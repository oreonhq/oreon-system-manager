use crate::pages::package_page::format_result;
use crate::process::{self, parse_list_output, ProcessRequest};
use crate::widgets::collapsible_output::CollapsibleOutput;
use glib::subclass::prelude::ObjectSubclassIsExt;
use gtk4::glib;
use gtk4::prelude::*;
use gtk4::ListView;
use gtk4::{
    Button, Label, Orientation, PolicyType, ScrolledWindow, SignalListItemFactory, StringList,
    StringObject,
};

glib::wrapper! {
    pub struct DriversPage(ObjectSubclass<imp::Imp>)
        @extends gtk4::Box, gtk4::Widget, @implements gtk4::Orientable, gtk4::Accessible, gtk4::Buildable, gtk4::ConstraintTarget;
}

mod imp {
    use super::*;
    use gtk4::subclass::prelude::*;
    use std::cell::RefCell;

    pub struct Imp {
        pub driver_list: RefCell<Option<StringList>>,
        pub detect_btn: RefCell<Option<Button>>,
        pub install_btn: RefCell<Option<Button>>,
        pub output: RefCell<Option<CollapsibleOutput>>,
        pub selected_index: RefCell<Option<u32>>,
    }

    impl Default for Imp {
        fn default() -> Self {
            Self {
                driver_list: RefCell::new(None),
                detect_btn: RefCell::new(None),
                install_btn: RefCell::new(None),
                output: RefCell::new(None),
                selected_index: RefCell::new(None),
            }
        }
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Imp {
        const NAME: &'static str = "OreonDriversPage";
        type Type = super::DriversPage;
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

impl DriversPage {
    pub fn new() -> Self {
        let obj: Self = glib::Object::new();
        let box_ref: &gtk4::Box = obj.upcast_ref();

        let title = Label::new(Some("Drivers"));
        title.set_widget_name("pageTitle");
        title.set_halign(gtk4::Align::Start);
        box_ref.append(&title);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 4));

        let sub = Label::new(Some("Detect hardware and install the appropriate drivers."));
        sub.set_widget_name("pageSubtitle");
        sub.set_halign(gtk4::Align::Start);
        sub.set_wrap(true);
        box_ref.append(&sub);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 18));

        let card = gtk4::Frame::new(None);
        card.set_vexpand(true);
        let model = StringList::new(&[]);
        let selection_model = gtk4::SingleSelection::new(Some(model.clone()));

        let factory = SignalListItemFactory::new();
        factory.connect_setup(move |_f, item| {
            let label = Label::new(None);
            label.set_halign(gtk4::Align::Start);
            label.set_margin_start(16);
            label.set_margin_end(16);
            label.set_margin_top(7);
            label.set_margin_bottom(7);
            item.set_child(Some(&label));
        });
        factory.connect_bind(move |_f, item| {
            let label = item
                .child()
                .and_then(|w| w.downcast::<Label>().ok())
                .unwrap();
            if let Some(so) = item.item().and_then(|i| i.downcast::<StringObject>().ok()) {
                label.set_label(&so.string());
            }
        });

        let list_view = ListView::new(Some(selection_model.clone()), Some(factory));
        let scrolled = ScrolledWindow::new();
        scrolled.set_child(Some(&list_view));
        scrolled.set_policy(PolicyType::Automatic, PolicyType::Automatic);
        card.set_child(Some(&scrolled));
        box_ref.append(&card);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 10));

        let action_row = gtk4::Box::new(Orientation::Horizontal, 8);
        let detect_btn = Button::with_label("Detect Hardware");
        let install_btn = Button::with_label("Install Driver");
        detect_btn.add_css_class("suggested-action");
        install_btn.add_css_class("suggested-action");
        install_btn.set_sensitive(false);
        action_row.append(&detect_btn);
        action_row.append(&install_btn);
        let stretch = gtk4::Box::new(Orientation::Horizontal, 0);
        stretch.set_hexpand(true);
        action_row.append(&stretch);
        box_ref.append(&action_row);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 14));

        let output = CollapsibleOutput::new();
        box_ref.append(&output);

        *obj.imp().driver_list.borrow_mut() = Some(model.clone());
        *obj.imp().detect_btn.borrow_mut() = Some(detect_btn.clone());
        *obj.imp().install_btn.borrow_mut() = Some(install_btn.clone());
        *obj.imp().output.borrow_mut() = Some(output.clone());

        let obj_c = obj.clone();
        detect_btn.connect_clicked(move |_| obj_c.detect_drivers());
        let obj_c = obj.clone();
        install_btn.connect_clicked(move |_| obj_c.on_install_driver());

        let obj_c = obj.clone();
        selection_model.connect_selection_changed(move |sel, _, _| {
            *obj_c.imp().selected_index.borrow_mut() = Some(sel.selected());
            let has = sel.selected_item().is_some();
            if let Some(ref b) = *obj_c.imp().install_btn.borrow() {
                b.set_sensitive(has);
            }
        });

        obj
    }

    fn selected_text(&self) -> Option<String> {
        let imp = self.imp();
        let model = imp.driver_list.borrow();
        let idx = imp.selected_index.borrow();
        if let (Some(m), Some(i)) = (model.as_ref(), *idx) {
            if (i as usize) < m.n_items() as usize {
                return Some(m.string(i).map(|s| s.to_string()).unwrap_or_default());
            }
        }
        None
    }

    fn detect_drivers(&self) {
        let imp = self.imp();
        if let Some(m) = imp.driver_list.borrow().as_ref() {
            m.splice(0, m.n_items(), &[]);
        }
        if let Some(ref o) = *imp.output.borrow() {
            o.clear();
            o.expand();
            o.append("Detecting hardware\u{2026}");
        }
        let request = ProcessRequest::new(
            "bash",
            &["-c", "set -o pipefail; lspci -nn | awk 'BEGIN{IGNORECASE=1} /vga|3d|display/ {line=tolower($0); if (line ~ /nvidia/) print \"NVIDIA graphics | akmod-nvidia\"; else if (line ~ /amd|ati/) print \"AMD graphics | mesa-dri-drivers\"; else if (line ~ /intel/) print \"Intel graphics | mesa-dri-drivers\"; else print \"Unknown graphics device | mesa-dri-drivers\"}' | sort -u"],
        );
        let output = imp.output.borrow().clone();
        let model = imp.driver_list.borrow().clone();
        let output_c = output.clone();
        let model_c = model.clone();
        process::run_process(
            request,
            true,
            move |is_list, text| {
                if let Some(ref o) = output_c {
                    o.append(text);
                }
                if is_list {
                    if let Some(ref m) = model_c {
                        for item in parse_list_output(text) {
                            m.append(&item);
                        }
                    }
                }
            },
            move |code| {
                if let Some(ref o) = output {
                    o.append(&format_result(code));
                }
            },
        );
    }

    fn on_install_driver(&self) {
        if let Some(text) = self.selected_text() {
            let pkg = text.split('|').nth(1).map(str::trim).unwrap_or("");
            if pkg.is_empty() {
                return;
            }
            let imp = self.imp();
            if let Some(ref o) = *imp.output.borrow() {
                o.clear();
                o.expand();
            }
            self.run_command("pkexec", &["dnf", "install", "-y", pkg]);
        }
    }

    fn run_command(&self, program: &str, args: &[&str]) {
        let imp = self.imp();
        let request = ProcessRequest::new(program, args);
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

impl Default for DriversPage {
    fn default() -> Self {
        Self::new()
    }
}
