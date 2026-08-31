use crate::pages::package_page::format_result;
use crate::process::{self, extract_repo_id, parse_list_output, repo_is_enabled, ProcessRequest};
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
    pub struct RepoPage(ObjectSubclass<imp::Imp>)
        @extends gtk4::Box, gtk4::Widget, @implements gtk4::Orientable, gtk4::Accessible, gtk4::Buildable, gtk4::ConstraintTarget;
}

mod imp {
    use super::*;
    use gtk4::subclass::prelude::*;
    use std::cell::RefCell;

    pub struct Imp {
        pub repo_list: RefCell<Option<StringList>>,
        pub toggle_btn: RefCell<Option<Button>>,
        pub output: RefCell<Option<CollapsibleOutput>>,
        pub selected_index: RefCell<Option<u32>>,
    }

    impl Default for Imp {
        fn default() -> Self {
            Self {
                repo_list: RefCell::new(None),
                toggle_btn: RefCell::new(None),
                output: RefCell::new(None),
                selected_index: RefCell::new(None),
            }
        }
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Imp {
        const NAME: &'static str = "OreonRepoPage";
        type Type = super::RepoPage;
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

impl RepoPage {
    pub fn new() -> Self {
        let obj: Self = glib::Object::new();
        let box_ref: &gtk4::Box = obj.upcast_ref();

        let title = Label::new(Some("Repositories"));
        title.set_widget_name("pageTitle");
        title.set_halign(gtk4::Align::Start);
        box_ref.append(&title);

        let sub = Label::new(Some("Enable, disable, and refresh DNF repositories."));
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
        let toggle_btn = Button::with_label("Enable / Disable");
        let refresh_btn = Button::with_label("Refresh");
        toggle_btn.add_css_class("suggested-action");
        toggle_btn.set_sensitive(false);
        action_row.append(&toggle_btn);
        let stretch = gtk4::Box::new(Orientation::Horizontal, 0);
        stretch.set_hexpand(true);
        action_row.append(&stretch);
        action_row.append(&refresh_btn);
        box_ref.append(&action_row);
        box_ref.append(&gtk4::Box::new(Orientation::Vertical, 14));

        let output = CollapsibleOutput::new();
        box_ref.append(&output);

        *obj.imp().repo_list.borrow_mut() = Some(model.clone());
        *obj.imp().toggle_btn.borrow_mut() = Some(toggle_btn.clone());
        *obj.imp().output.borrow_mut() = Some(output.clone());

        let obj_c = obj.clone();
        refresh_btn.connect_clicked(move |_| obj_c.load_repos());
        let obj_c = obj.clone();
        toggle_btn.connect_clicked(move |_| obj_c.on_toggle_repo());

        let obj_c = obj.clone();
        selection_model.connect_selection_changed(move |sel, _, _| {
            *obj_c.imp().selected_index.borrow_mut() = Some(sel.selected());
            let has = sel.selected_item().is_some();
            if let Some(ref b) = *obj_c.imp().toggle_btn.borrow() {
                b.set_sensitive(has);
            }
        });

        obj.load_repos();
        obj
    }

    fn selected_text(&self) -> Option<String> {
        let imp = self.imp();
        let model = imp.repo_list.borrow();
        let idx = imp.selected_index.borrow();
        if let (Some(model), Some(idx)) = (model.as_ref(), *idx) {
            if (idx as usize) < model.n_items() as usize {
                return Some(model.string(idx).map(|s| s.to_string()).unwrap_or_default());
            }
        }
        None
    }

    fn load_repos(&self) {
        let imp = self.imp();
        if let Some(m) = imp.repo_list.borrow().as_ref() {
            m.splice(0, m.n_items(), &[]);
        }
        if let Some(ref o) = *imp.output.borrow() {
            o.clear();
        }

        let request = ProcessRequest::new("dnf", &["--assumeyes", "repolist", "all", "--quiet"]);
        let output = imp.output.borrow().clone();
        let model = imp.repo_list.borrow().clone();
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

    fn on_toggle_repo(&self) {
        if let Some(text) = self.selected_text() {
            let repo_id = extract_repo_id(&text);
            let enabled = repo_is_enabled(&text);
            let action = if enabled {
                "--disablerepo"
            } else {
                "--enablerepo"
            };
            let imp = self.imp();
            if let Some(ref o) = *imp.output.borrow() {
                o.expand();
            }
            self.run_dnf_config(&[action, &repo_id]);
        }
    }

    fn run_dnf_config(&self, args: &[&str]) {
        let imp = self.imp();
        let mut full = vec!["dnf", "config-manager"];
        full.extend(args);
        let request = ProcessRequest::new("pkexec", &full);
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

impl Default for RepoPage {
    fn default() -> Self {
        Self::new()
    }
}
