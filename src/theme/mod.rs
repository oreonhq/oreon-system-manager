use std::sync::{Mutex, OnceLock};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ThemeMode {
    System,
    Light,
    Dark,
}

impl ThemeMode {
    pub fn from_str(s: &str) -> Self {
        match s.trim() {
            "light" => ThemeMode::Light,
            "dark" => ThemeMode::Dark,
            _ => ThemeMode::System,
        }
    }

    pub fn as_str(&self) -> &'static str {
        match self {
            ThemeMode::System => "system",
            ThemeMode::Light => "light",
            ThemeMode::Dark => "dark",
        }
    }

    pub fn label(&self) -> &'static str {
        match self {
            ThemeMode::System => "System",
            ThemeMode::Light => "Light",
            ThemeMode::Dark => "Dark",
        }
    }

    pub fn icon(&self) -> &'static str {
        match self {
            ThemeMode::System => "preferences-desktop-display-symbolic",
            ThemeMode::Light => "weather-clear-symbolic",
            ThemeMode::Dark => "weather-clear-night-symbolic",
        }
    }
}

#[derive(Clone, Debug)]
pub struct AppSettings {
    pub theme_mode: ThemeMode,
    pub confirm_remove: bool,
    pub auto_expand_output: bool,
}

impl Default for AppSettings {
    fn default() -> Self {
        AppSettings {
            theme_mode: ThemeMode::System,
            confirm_remove: true,
            auto_expand_output: true,
        }
    }
}

pub struct SettingsManager {
    settings: AppSettings,
    /// The original gtk-theme-name captured at startup, before we override it.
    system_theme_name: Option<String>,
    /// Whether the original system theme is dark.
    system_is_dark: bool,
}

impl SettingsManager {
    fn new() -> Self {
        let system_theme_name = gtk4::Settings::default()
            .and_then(|s| s.gtk_theme_name())
            .map(|t| t.to_string());
        let system_is_dark = detect_theme_is_dark(&system_theme_name);
        SettingsManager {
            settings: AppSettings::default(),
            system_theme_name,
            system_is_dark,
        }
    }

    pub fn instance() -> &'static Mutex<SettingsManager> {
        static INSTANCE: OnceLock<Mutex<SettingsManager>> = OnceLock::new();
        INSTANCE.get_or_init(|| Mutex::new(SettingsManager::new()))
    }

    pub fn get(&self) -> &AppSettings {
        &self.settings
    }

    pub fn theme_mode(&self) -> ThemeMode {
        self.settings.theme_mode
    }

    pub fn is_dark(&self) -> bool {
        match self.settings.theme_mode {
            ThemeMode::Dark => true,
            ThemeMode::Light => false,
            ThemeMode::System => self.system_is_dark,
        }
    }

    pub fn apply_theme(&mut self, mode: ThemeMode) {
        self.settings.theme_mode = mode;
        self.apply_gtk_theme();
        self.save();
    }

    pub fn set_confirm_remove(&mut self, val: bool) {
        self.settings.confirm_remove = val;
        self.save();
    }

    pub fn set_auto_expand_output(&mut self, val: bool) {
        self.settings.auto_expand_output = val;
        self.save();
    }

    pub fn restore(&mut self) {
        self.settings = load_settings();
        self.apply_gtk_theme();
        apply_base_css();
    }

    /// Apply the correct GTK theme name based on the current mode.
    /// For Light/Dark we set `gtk-theme-name` to a dark or light variant
    /// of the detected system theme. For System we restore the original.
    fn apply_gtk_theme(&self) {
        let Some(settings) = gtk4::Settings::default() else {
            return;
        };

        let dark = self.is_dark();

        // set_gtk_application_prefer_dark_theme works as a hint for some
        // themes but is not enough on its own. We also set gtk-theme-name
        // to an explicit dark/light variant when we can derive one.
        settings.set_gtk_application_prefer_dark_theme(dark);

        if self.settings.theme_mode == ThemeMode::System {
            // Restore original system theme
            if let Some(ref orig) = self.system_theme_name {
                settings.set_gtk_theme_name(Some(orig));
            }
            return;
        }

        // Try to derive the counterpart theme name.
        if let Some(ref orig) = self.system_theme_name {
            if let Some(target) = derive_theme_name(orig, dark) {
                settings.set_gtk_theme_name(Some(&target));
            }
        }
    }

    fn save(&self) {
        if let Some(config_dir) = dirs_config_path() {
            let _ = std::fs::create_dir_all(&config_dir);
            let content = format!(
                "theme={}\nconfirm_remove={}\nauto_expand_output={}\n",
                self.settings.theme_mode.as_str(),
                self.settings.confirm_remove,
                self.settings.auto_expand_output,
            );
            let _ = std::fs::write(config_dir.join("settings.conf"), content);
        }
    }
}

/// Heuristic: detect whether a theme name / color-scheme is dark.
fn detect_theme_is_dark(theme_name: &Option<String>) -> bool {
    if let Some(ref name) = theme_name {
        let lower = name.to_lowercase();
        if lower.contains("dark") || lower.contains("night") || lower.contains("noir") {
            return true;
        }
        if lower.contains("light") || lower.contains("breeze") && !lower.contains("dark") {
            // Breeze (without "dark") is light by default
            return false;
        }
    }
    // Fallback: check the GTK settings property
    if let Some(settings) = gtk4::Settings::default() {
        settings.is_gtk_application_prefer_dark_theme()
    } else {
        false
    }
}

/// Given an original theme name and a desired dark/light state, try to
/// produce the counterpart theme name.
///
/// Works for common KDE/GNOME themes:
///   "Breeze"            <-> "Breeze-Dark"
///   "adwaira"           <-> "adwaira-dark"
///   "Catppuccin-Mocha"  -> for light: "Catppuccin-Latte"
///   etc.
fn derive_theme_name(original: &str, want_dark: bool) -> Option<String> {
    let lower = original.to_lowercase();

    // If the current name already matches the desired state, keep it
    let already_dark = lower.contains("dark")
        || lower.contains("night")
        || lower.contains("noir")
        || lower.contains("mocha")
        || lower.contains("macchiato")
        || lower.contains("frappe")
        || lower.contains("nord");
    let already_light = lower.contains("light") || lower.contains("latte");

    if want_dark && already_dark && !already_light {
        return Some(original.to_string());
    }
    if !want_dark && already_light {
        return Some(original.to_string());
    }
    if !want_dark && !already_dark {
        return Some(original.to_string());
    }
    if want_dark && !already_dark {
        // Try to produce a dark variant
        return Some(derive_dark_variant(original));
    }
    if !want_dark && already_dark {
        // Try to produce a light variant
        return Some(derive_light_variant(original));
    }

    None
}

fn derive_dark_variant(original: &str) -> String {
    // Breeze -> Breeze-Dark
    if original.eq_ignore_ascii_case("Breeze") {
        return "Breeze-Dark".to_string();
    }
    // If it contains "light", replace with "dark"
    let lower = original.to_lowercase();
    if lower.contains("light") {
        return original.replace("light", "Dark").replace("Light", "Dark");
    }
    if lower.contains("latte") {
        return original.replace("latte", "Mocha").replace("Latte", "Mocha");
    }
    // Generic: append -Dark
    format!("{}-Dark", original)
}

fn derive_light_variant(original: &str) -> String {
    // Breeze-Dark -> Breeze
    if original.eq_ignore_ascii_case("Breeze-Dark") {
        return "Breeze".to_string();
    }
    let lower = original.to_lowercase();
    if lower.contains("dark") {
        return original
            .replace("dark", "light")
            .replace("Dark", "Light")
            .replace("DARK", "LIGHT");
    }
    if lower.contains("mocha") || lower.contains("macchiato") || lower.contains("frappe") {
        return original
            .replace("mocha", "latte")
            .replace("Mocha", "Latte")
            .replace("macchiato", "latte")
            .replace("Macchiato", "Latte")
            .replace("frappe", "latte")
            .replace("Frappe", "Latte");
    }
    if lower.contains("night") {
        return original.replace("night", "day").replace("Night", "Day");
    }
    // Generic: strip -Dark suffix or append -Light
    if lower.ends_with("-dark") {
        return original
            .trim_end_matches("-Dark")
            .trim_end_matches("-dark")
            .to_string();
    }
    format!("{}-Light", original)
}

fn load_settings() -> AppSettings {
    let mut s = AppSettings::default();
    if let Some(config_dir) = dirs_config_path() {
        if let Ok(content) = std::fs::read_to_string(config_dir.join("settings.conf")) {
            for line in content.lines() {
                let line = line.trim();
                if let Some(rest) = line.strip_prefix("theme=") {
                    s.theme_mode = ThemeMode::from_str(rest);
                } else if let Some(rest) = line.strip_prefix("confirm_remove=") {
                    s.confirm_remove = rest.trim() == "true";
                } else if let Some(rest) = line.strip_prefix("auto_expand_output=") {
                    s.auto_expand_output = rest.trim() == "true";
                }
            }
        }
        // Migrate old single-theme file
        if s.theme_mode == ThemeMode::System {
            if let Ok(old) = std::fs::read_to_string(config_dir.join("theme")) {
                s.theme_mode = ThemeMode::from_str(&old);
            }
        }
    }
    s
}

fn dirs_config_path() -> Option<std::path::PathBuf> {
    std::env::var("XDG_CONFIG_HOME")
        .map(std::path::PathBuf::from)
        .or_else(|_| std::env::var("HOME").map(|h| std::path::PathBuf::from(h).join(".config")))
        .ok()
        .map(|p| p.join("oreon-system-manager"))
}

/// Minimal CSS — only branding accents. Everything else uses the system GTK theme.
fn apply_base_css() {
    let css = r#"
#pageTitle {
    font-size: 20pt;
    font-weight: 700;
}
#pageSubtitle {
    font-size: 11pt;
}
#terminal {
    font-family: "JetBrains Mono", "Hack", "Fira Code", "Menlo", "Cascadia Code", "Consolas", "Courier New", monospace;
    font-size: 10pt;
}
#terminal text {
    background-color: transparent;
}

/* Sidebar nav — remove background, use only text color + left indicator */
#sidebar row {
    background-color: transparent;
    background-image: none;
    box-shadow: none;
    border-radius: 6px;
    transition: all 120ms ease;
}
#sidebar row:hover {
    background-color: alpha(currentColor, 0.06);
}
#sidebar row:selected {
    background-color: transparent;
    background-image: none;
    box-shadow: none;
}
#sidebar row:selected label {
    color: @theme_fg_color;
    opacity: 1;
}
#sidebar row:selected image {
    color: @theme_fg_color;
    opacity: 1;
}
#sidebar row:selected label {
    font-weight: 600;
}
"#;
    let provider = gtk4::CssProvider::new();
    provider.load_from_data(css);
    if let Some(display) = gtk4::gdk::Display::default() {
        gtk4::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk4::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_theme_mode_from_str() {
        assert_eq!(ThemeMode::from_str("light"), ThemeMode::Light);
        assert_eq!(ThemeMode::from_str("dark"), ThemeMode::Dark);
        assert_eq!(ThemeMode::from_str("system"), ThemeMode::System);
        assert_eq!(ThemeMode::from_str("  dark  "), ThemeMode::Dark);
        assert_eq!(ThemeMode::from_str("unknown"), ThemeMode::System);
    }

    #[test]
    fn test_theme_mode_as_str() {
        assert_eq!(ThemeMode::System.as_str(), "system");
        assert_eq!(ThemeMode::Light.as_str(), "light");
        assert_eq!(ThemeMode::Dark.as_str(), "dark");
    }

    #[test]
    fn test_derive_dark_variant() {
        assert_eq!(derive_dark_variant("Breeze"), "Breeze-Dark");
        assert_eq!(derive_dark_variant("Breeze-Light"), "Breeze-Dark");
        assert_eq!(derive_dark_variant("Catppuccin-Latte"), "Catppuccin-Mocha");
        assert_eq!(derive_dark_variant("Adwaita"), "Adwaita-Dark");
    }

    #[test]
    fn test_derive_light_variant() {
        assert_eq!(derive_light_variant("Breeze-Dark"), "Breeze");
        assert_eq!(derive_light_variant("Adwaita-dark"), "Adwaita-light");
        assert_eq!(derive_light_variant("Catppuccin-Mocha"), "Catppuccin-Latte");
        assert_eq!(
            derive_light_variant("Catppuccin-Macchiato"),
            "Catppuccin-Latte"
        );
        assert_eq!(
            derive_light_variant("Catppuccin-Frappe"),
            "Catppuccin-Latte"
        );
        assert_eq!(derive_light_variant("Nordic-night"), "Nordic-day");
    }

    #[test]
    fn test_derive_theme_name_keep_same() {
        // Already dark, want dark → keep
        assert_eq!(
            derive_theme_name("Breeze-Dark", true),
            Some("Breeze-Dark".to_string())
        );
        // Already light, want light → keep
        assert_eq!(
            derive_theme_name("Breeze-Light", false),
            Some("Breeze-Light".to_string())
        );
        // Not dark, want light → keep
        assert_eq!(
            derive_theme_name("Breeze", false),
            Some("Breeze".to_string())
        );
    }

    #[test]
    fn test_derive_theme_name_switch() {
        // Light → dark
        assert_eq!(
            derive_theme_name("Breeze", true),
            Some("Breeze-Dark".to_string())
        );
        // Dark → light
        assert_eq!(
            derive_theme_name("Breeze-Dark", false),
            Some("Breeze".to_string())
        );
    }
}
