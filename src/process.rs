use std::sync::mpsc;

/// Configuration for a process to run
pub struct ProcessRequest {
    pub program: String,
    pub args: Vec<String>,
}

impl ProcessRequest {
    pub fn new(program: &str, args: &[&str]) -> Self {
        ProcessRequest {
            program: program.to_string(),
            args: args.iter().map(|s| s.to_string()).collect(),
        }
    }
}

enum ProcessMessage {
    Output { is_stdout: bool, text: String },
    Done(Option<i32>),
}

/// Run a process asynchronously. `on_output` is called with each chunk of
/// stdout/stderr as it arrives, and `on_done` is called when the process
/// finishes with the exit code.
pub fn run_process<F, D>(request: ProcessRequest, is_list_cmd: bool, on_output: F, on_done: D)
where
    F: Fn(bool, &str) + 'static,
    D: Fn(Option<i32>) + 'static,
{
    let program = request.program;
    let args = request.args;

    let (sender, receiver) = mpsc::channel::<ProcessMessage>();

    // Spawn the process in a thread
    std::thread::spawn(move || {
        use std::io::Read;
        use std::process::{Command, Stdio};

        let mut child = match Command::new(&program)
            .args(&args)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
        {
            Ok(c) => c,
            Err(e) => {
                let _ = sender.send(ProcessMessage::Output {
                    is_stdout: false,
                    text: format!("Failed to start: {}\n", e),
                });
                let _ = sender.send(ProcessMessage::Done(None));
                return;
            }
        };

        let mut stdout = child.stdout.take().unwrap();
        let mut stderr = child.stderr.take().unwrap();

        let err_sender = sender.clone();
        let err_handle = std::thread::spawn(move || {
            let mut buf = [0u8; 4096];
            loop {
                match stderr.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        let text = String::from_utf8_lossy(&buf[..n]).to_string();
                        let _ = err_sender.send(ProcessMessage::Output {
                            is_stdout: false,
                            text,
                        });
                    }
                    Err(_) => break,
                }
            }
        });

        let mut buf = [0u8; 4096];
        loop {
            match stdout.read(&mut buf) {
                Ok(0) => break,
                Ok(n) => {
                    let text = String::from_utf8_lossy(&buf[..n]).to_string();
                    let _ = sender.send(ProcessMessage::Output {
                        is_stdout: true,
                        text,
                    });
                }
                Err(_) => break,
            }
        }

        let _ = err_handle.join();
        let exit_code = child.wait().ok().and_then(|s| s.code());
        let _ = sender.send(ProcessMessage::Done(exit_code));
    });

    // Poll the channel on the main loop
    let is_list = is_list_cmd;
    glib::source::idle_add_local(move || {
        for _ in 0..32 {
            let Ok(msg) = receiver.try_recv() else {
                break;
            };
            match msg {
                ProcessMessage::Output { is_stdout, text } => {
                    if is_stdout {
                        on_output(is_list, &text);
                    } else {
                        on_output(false, &text);
                    }
                }
                ProcessMessage::Done(exit_code) => {
                    on_done(exit_code);
                    return glib::ControlFlow::Break;
                }
            }
        }
        glib::ControlFlow::Continue
    });
}

/// Parse dnf search output: skip lines starting with "=" and empty lines
pub fn parse_dnf_search_output(data: &str) -> Vec<String> {
    data.lines()
        .map(|l| l.trim())
        .filter(|l| {
            !l.is_empty()
                && !l.starts_with('=')
                && !l.starts_with("Last metadata expiration")
                && l.contains(" : ")
        })
        .map(|l| l.to_string())
        .collect()
}

pub fn parse_dnf_list_output(data: &str) -> Vec<String> {
    data.lines()
        .map(str::trim)
        .filter(|line| {
            !line.is_empty()
                && !line.starts_with("Available Packages")
                && !line.starts_with("Installed Packages")
                && !line.starts_with("Last metadata")
                && line.split_whitespace().count() >= 3
                && line.contains('.')
        })
        .map(ToString::to_string)
        .collect()
}

/// Parse generic list output (repos, drivers, etc.)
pub fn parse_list_output(data: &str) -> Vec<String> {
    data.lines()
        .map(|l| l.trim())
        .filter(|l| {
            !l.is_empty()
                && !l.starts_with("repo id")
                && !l.starts_with("Repo-id")
                && !l.starts_with("CONTAINER")
                && !l.starts_with("ID")
        })
        .map(|l| l.to_string())
        .collect()
}

/// Parse docker ps output (tab-separated: name, status, image)
pub fn parse_docker_ps_output(data: &str) -> Vec<String> {
    data.lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .map(|l| l.to_string())
        .collect()
}

/// Parse distrobox list output (pipe-separated columns)
pub fn parse_distrobox_list_output(data: &str) -> Vec<String> {
    data.lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty() && !l.starts_with("ID") && !l.starts_with("---"))
        .map(|l| l.to_string())
        .collect()
}

/// Extract package name from a dnf search result line.
/// e.g. "vim.x86_64 : Vi IMproved" -> "vim"
pub fn extract_package_name(line: &str) -> String {
    line.split_whitespace()
        .next()
        .unwrap_or("")
        .split('.')
        .next()
        .unwrap_or("")
        .to_string()
}

/// Extract repo id from a dnf repolist line.
/// e.g. "fedora    Fedora 40 - aarch64    enabled" -> "fedora"
pub fn extract_repo_id(line: &str) -> String {
    line.split_whitespace().next().unwrap_or("").to_string()
}

/// Check if a repo line says "enabled"
pub fn repo_is_enabled(line: &str) -> bool {
    line.split_whitespace().last() == Some("enabled")
}

/// Extract container name from a docker ps line (tab-separated, first column)
pub fn extract_docker_name(line: &str) -> String {
    line.split('\t').next().unwrap_or("").trim().to_string()
}

/// Extract container name from a distrobox list line (pipe-separated, first column after header)
pub fn extract_distrobox_name(line: &str) -> String {
    line.split('|')
        .nth(1)
        .or_else(|| line.split_whitespace().next())
        .unwrap_or("")
        .trim()
        .to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_dnf_search() {
        let input = "vim.x86_64 : Vi IMproved — enhanced vi editor\n=== Name Matched ===\nvim-enhanced.x86_64 : A version of the VIM editor\n";
        let result = parse_dnf_search_output(input);
        assert_eq!(result.len(), 2);
        assert!(result[0].contains("vim.x86_64"));
        assert!(result[1].contains("vim-enhanced.x86_64"));
    }

    #[test]
    fn test_parse_list_output() {
        let input = "fedora    Fedora 40    enabled\nupdates    Updates    disabled\n\n";
        let result = parse_list_output(input);
        assert_eq!(result.len(), 2);
    }

    #[test]
    fn test_extract_package_name() {
        assert_eq!(extract_package_name("vim.x86_64 : Vi IMproved"), "vim");
        assert_eq!(
            extract_package_name("vim-enhanced.x86_64 : enhanced vi"),
            "vim-enhanced"
        );
    }

    #[test]
    fn test_extract_repo_id() {
        assert_eq!(extract_repo_id("fedora    Fedora 40    enabled"), "fedora");
        assert_eq!(
            extract_repo_id("updates-testing    Test Updates    disabled"),
            "updates-testing"
        );
    }

    #[test]
    fn test_repo_is_enabled() {
        assert!(repo_is_enabled("fedora    Fedora 40    enabled"));
        assert!(!repo_is_enabled(
            "updates-testing    Test Updates    disabled"
        ));
    }

    #[test]
    fn test_extract_docker_name() {
        assert_eq!(
            extract_docker_name("web-server\tUp 3 hours\tnotginx:latest"),
            "web-server"
        );
    }

    #[test]
    fn test_extract_distrobox_name() {
        assert_eq!(
            extract_distrobox_name(
                "1   | fedora-toolbox  | running | registry.fedoraproject.org/fedora-toolbox:40"
            ),
            "fedora-toolbox"
        );
    }
}
