use crate::process::{self, extract_package_name, parse_dnf_list_output, parse_dnf_search_output, ProcessRequest};
use crate::widgets::collapsible_output::CollapsibleOutput;
use glib::subclass::prelude::ObjectSubclassIsExt;
use gtk4::glib;
use gtk4::prelude::*;
use gtk4::ListView;
use gtk4::{
    Button, Entry, Label, Orientation, PolicyType, ScrolledWindow, SignalListItemFactory,
    StringList, StringObject,
};
use std::cell::RefCell;
use std::rc::Rc;

glib::wrapper! {
    pub struct PackagePage(ObjectSubclass<imp::Imp>)
        @extends gtk4::Box, gtk4::Widget, @implements gtk4::Orientable, gtk4::Accessible, gtk4::Buildable, gtk4::ConstraintTarget;
}

mod imp {
    use super::*;
    use gtk4::subclass::prelude::*;
    use std::cell::RefCell;

    pub struct Imp {
        pub search_bar: RefCell<Option<Entry>>,
        pub package_list: RefCell<Option<StringList>>,
        pub install_btn: RefCell<Option<Button>>,
        pub remove_btn: RefCell<Option<Button>>,
        pub output: RefCell<Option<CollapsibleOutput>>,
        pub selected_index: RefCell<Option<u32>>,
    }

    impl Default for Imp {
        fn default() -> Self {
            Self {
                search_bar: RefCell::new(None),
                package_list: RefCell::new(None),
                install_btn: RefCell::new(None),
                remove_btn: RefCell::new(None),
                output: RefCell::new(None),
                selected_index: RefCell::new(None),
            }
        }
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Imp {
        const NAME: &'static str = "OreonPackagePage";
        type Type = super::PackagePage;
        type ParentType = gtk4::Box;
    }

    impl ObjectImpl for Imp {
        fn constructed(&self) {
            let obj = self.obj();
            obj.set_orientation(Orientation::Vertical);
            obj.set_margin_start(24);
            obj.set_margin_end(24);
            obj.set_margin_top(24);
            obj.set_margin_bottom(24);
            obj.set_spacing(0);
        }
    }

    impl BoxImpl for Imp {}
    impl WidgetImpl for Imp {}
}

fn make_list_factory() -> SignalListItemFactory {
    let factory = SignalListItemFactory::new();
    factory.connect_setup(move |_f, item| {
        let row = gtk4::Box::new(Orientation::Horizontal, 16);
        row.set_margin_start(16);
        row.set_margin_end(16);
        row.set_margin_top(7);
        row.set_margin_bottom(7);

        let package = Label::new(None);
        package.set_halign(gtk4::Align::Start);
        package.set_hexpand(true);
        package.set_xalign(0.0);

        let version = Label::new(None);
        version.set_halign(gtk4::Align::Start);
        version.set_xalign(0.0);
        version.set_width_chars(24);

        let repository = Label::new(None);
        repository.set_halign(gtk4::Align::Start);
        repository.set_xalign(0.0);
        repository.set_width_chars(16);

        row.append(&package);
        row.append(&version);
        row.append(&repository);
        item.set_child(Some(&row));
    });
    factory.connect_bind(move |_f, item| {
        let row = item.child().and_then(|w| w.downcast::<gtk4::Box>().ok()).unwrap();
        let package = row.first_child().and_then(|w| w.downcast::<Label>().ok()).unwrap();
        let version = package.next_sibling().and_then(|w| w.downcast::<Label>().ok()).unwrap();
        let repository = version.next_sibling().and_then(|w| w.downcast::<Label>().ok()).unwrap();
        if let Some(string_obj) = item.item().and_then(|i| i.downcast::<StringObject>().ok()) {
            let value = string_obj.string();
            if let Some((name, description)) = value.split_once(" : ") {
                package.set_label(name);
                version.set_label(description);
                repository.set_label("");
            } else {
                let mut fields = value.split_whitespace();
                package.set_label(fields.next().unwrap_or(""));
                version.set_label(fields.next().unwrap_or(""));
                repository.set_label(fields.next().unwrap_or(""));
            }
        }
    });
    factory
}

impl PackagePage {
    pub fn new() -> Self {
        let obj: Self = glib::Object::new();
        let box_ref: &gtk4::Box = obj.upcast_ref();

        let title = Label::new(Some("Packages"));
        title.set_widget_name("pageTitle");
        title.set_halign(gtk4::Align::Start);
        box_ref.append(&title);

        let sub = Label::new(Some("Search, install, and remove DNF packages."));
        sub.set_widget_name("pageSubtitle");
        sub.set_halign(gtk4::Align::Start);
        sub.set_wrap(true);
        box_ref.append(&sub);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 18));

        let search_row = gtk4::Box::new(Orientation::Horizontal, 8);
        let search_bar = Entry::new();
        search_bar.set_placeholder_text(Some("Search packages\u{2026}"));
        search_bar.set_hexpand(true);
        search_bar.set_icon_from_icon_name(
            gtk4::EntryIconPosition::Primary,
            Some("system-search-symbolic"),
        );
        let search_btn = Button::with_label("Search");
        search_btn.add_css_class("suggested-action");
        search_row.append(&search_bar);
        search_row.append(&search_btn);
        box_ref.append(&search_row);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 12));

        let card = gtk4::Frame::new(None);
        card.set_vexpand(true);
        let model = StringList::new(&[]);
        let selection_model = gtk4::MultiSelection::new(Some(model.clone()));
        let factory = make_list_factory();
        let list_view = ListView::new(Some(selection_model.clone()), Some(factory));
        let list_box = gtk4::Box::new(Orientation::Vertical, 0);
        let headers = gtk4::Box::new(Orientation::Horizontal, 16);
        headers.set_margin_start(16);
        headers.set_margin_end(16);
        headers.set_margin_top(8);
        headers.set_margin_bottom(8);
        let package_header = Label::new(Some("Package"));
        package_header.set_halign(gtk4::Align::Start);
        package_header.set_hexpand(true);
        package_header.set_xalign(0.0);
        let version_header = Label::new(Some("Version"));
        version_header.set_halign(gtk4::Align::Start);
        version_header.set_xalign(0.0);
        version_header.set_width_chars(24);
        let repository_header = Label::new(Some("Repository"));
        repository_header.set_halign(gtk4::Align::Start);
        repository_header.set_xalign(0.0);
        repository_header.set_width_chars(16);
        for header in [&package_header, &version_header, &repository_header] {
            header.add_css_class("heading");
            headers.append(header);
        }
        let scrolled = ScrolledWindow::new();
        scrolled.set_child(Some(&list_view));
        scrolled.set_policy(PolicyType::Automatic, PolicyType::Automatic);
        scrolled.set_vexpand(true);
        list_box.append(&headers);
        list_box.append(&gtk4::Separator::new(Orientation::Horizontal));
        list_box.append(&scrolled);
        card.set_child(Some(&list_box));
        box_ref.append(&card);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 10));

        let action_row = gtk4::Box::new(Orientation::Horizontal, 8);
        let install_btn = Button::with_label("Install");
        let remove_btn = Button::with_label("Remove");
        install_btn.add_css_class("suggested-action");
        remove_btn.add_css_class("destructive-action");
        install_btn.set_sensitive(false);
        remove_btn.set_sensitive(false);
        action_row.append(&install_btn);
        action_row.append(&remove_btn);
        let stretch = gtk4::Box::new(Orientation::Horizontal, 0);
        stretch.set_hexpand(true);
        action_row.append(&stretch);
        box_ref.append(&action_row);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 14));

        let output = CollapsibleOutput::new();
        box_ref.append(&output);

        *obj.imp().search_bar.borrow_mut() = Some(search_bar.clone());
        *obj.imp().package_list.borrow_mut() = Some(model.clone());
        *obj.imp().install_btn.borrow_mut() = Some(install_btn.clone());
        *obj.imp().remove_btn.borrow_mut() = Some(remove_btn.clone());
        *obj.imp().output.borrow_mut() = Some(output.clone());

        let obj_c = obj.clone();
        search_btn.connect_clicked(move |_| obj_c.on_search());
        let obj_c = obj.clone();
        search_bar.connect_activate(move |_| obj_c.on_search());
        let obj_c = obj.clone();
        install_btn.connect_clicked(move |_| obj_c.on_install());
        let obj_c = obj.clone();
        remove_btn.connect_clicked(move |_| obj_c.on_remove());

        let obj_c = obj.clone();
        selection_model.connect_selection_changed(move |sel, _, _| {
            let first = (0..sel.n_items()).find(|index| sel.is_selected(*index));
            *obj_c.imp().selected_index.borrow_mut() = first;
            let has = first.is_some();
            if let Some(ref b) = *obj_c.imp().install_btn.borrow() {
                b.set_sensitive(has);
            }
            if let Some(ref b) = *obj_c.imp().remove_btn.borrow() {
                b.set_sensitive(has);
            }
        });

        obj.load_initial_packages();
        obj
    }

    fn load_initial_packages(&self) {
        let imp = self.imp();
        let model = imp.package_list.borrow().clone();
        let model_c = model.clone();
        let pending = Rc::new(RefCell::new(String::new()));
        let pending_c = pending.clone();
        process::run_process(
            ProcessRequest::new(
                "bash",
                &[
                    "-c",
                    "dnf --assumeyes list available --quiet | head -n 100",
                ],
            ),
            true,
            move |is_list, text| {
                if is_list {
                    if let Some(ref m) = model_c {
                        let mut data = pending_c.borrow_mut();
                        data.push_str(text);
                        while let Some(newline) = data.find('\n') {
                            let line = data[..newline].to_string();
                            data.drain(..=newline);
                            for item in parse_dnf_list_output(&line) {
                                m.append(&item);
                            }
                        }
                    }
                }
            },
            move |_| {
                if let Some(ref m) = model {
                    let data = pending.borrow();
                    for item in parse_dnf_list_output(&data) {
                        m.append(&item);
                    }
                }
            },
        );
    }

    fn selected_text(&self) -> Option<String> {
        let imp = self.imp();
        let model = imp.package_list.borrow();
        let idx = imp.selected_index.borrow();
        if let (Some(model), Some(idx)) = (model.as_ref(), *idx) {
            if (idx as usize) < model.n_items() as usize {
                return Some(model.string(idx).map(|s| s.to_string()).unwrap_or_default());
            }
        }
        None
    }

    fn on_search(&self) {
        let imp = self.imp();
        let query = imp
            .search_bar
            .borrow()
            .as_ref()
            .map(|e| e.text().trim().to_string())
            .unwrap_or_default();
        if query.is_empty() {
            return;
        }
        if let Some(m) = imp.package_list.borrow().as_ref() {
            m.splice(0, m.n_items(), &[]);
        }
        if let Some(ref o) = *imp.output.borrow() {
            o.clear();
        }

        let request = ProcessRequest::new("dnf", &["--assumeyes", "search", "--quiet", &query]);
        let output = imp.output.borrow().clone();
        let model = imp.package_list.borrow().clone();
        let output_c = output.clone();
        let model_c = model.clone();
        let pending = Rc::new(RefCell::new(String::new()));
        let pending_c = pending.clone();
        process::run_process(
            request,
            true,
            move |is_list, text| {
                if let Some(ref o) = output_c {
                    o.append(text);
                }
                if is_list {
                    if let Some(ref m) = model_c {
                        let mut data = pending_c.borrow_mut();
                        data.push_str(text);
                        let complete = data.rsplit_once('\n').map(|(_, rest)| rest.to_string());
                        if let Some(rest) = complete {
                            let ready = data[..data.len() - rest.len()].to_string();
                            *data = rest;
                            for item in parse_dnf_search_output(&ready) {
                                m.append(&item);
                            }
                        }
                    }
                }
            },
            move |code| {
                if let Some(ref m) = model {
                    let mut data = pending.borrow_mut();
                    if !data.trim().is_empty() {
                        for item in parse_dnf_search_output(&data) {
                            m.append(&item);
                        }
                        data.clear();
                    }
                }
                if let Some(ref o) = output {
                    o.append(&format_result(code));
                }
            },
        );
    }

    fn on_install(&self) {
        if let Some(text) = self.selected_text() {
            let pkg = extract_package_name(&text);
            if pkg.is_empty() {
                return;
            }
            let imp = self.imp();
            if let Some(ref o) = *imp.output.borrow() {
                o.clear();
                o.expand();
            }
            self.run_dnf(&["install", "-y", &pkg]);
        }
    }

    fn on_remove(&self) {
        if let Some(text) = self.selected_text() {
            let pkg = extract_package_name(&text);
            if pkg.is_empty() {
                return;
            }
            let imp = self.imp();
            if let Some(ref o) = *imp.output.borrow() {
                o.clear();
                o.expand();
            }
            self.run_dnf(&["remove", "-y", &pkg]);
        }
    }

    fn run_dnf(&self, args: &[&str]) {
        let imp = self.imp();
        let (program, full_args): (&str, Vec<&str>) =
            if args.first() == Some(&"search") || args.first() == Some(&"list") {
                ("dnf", args.to_vec())
            } else {
                let mut v = vec!["dnf"];
                v.extend(args);
                ("pkexec", v)
            };
        let request = ProcessRequest::new(program, &full_args);
        let output = imp.output.borrow().clone();
        let output_c = output.clone();
        process::run_process(
            request,
            false,
            move |_, text| {
                if let Some(ref o) = output_c {
                    o.append(text);
                }
            },
            move |code| {
                if let Some(ref o) = output {
                    o.append(&format_result(code));
                }
            },
        );
    }
}

pub fn format_result(code: Option<i32>) -> String {
    match code {
        Some(0) => "\n[Done]".to_string(),
        Some(c) => format!("\n[Failed \u{2014} exit code {}]", c),
        None => "\n[Failed \u{2014} process error]".to_string(),
    }
}

impl Default for PackagePage {
    fn default() -> Self {
        Self::new()
    }
}
