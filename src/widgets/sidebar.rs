use crate::resources;
use gtk4::glib;
use gtk4::prelude::*;
use gtk4::{Align, Box as GtkBox, Image, Label, ListBox, ListBoxRow, Orientation};
use std::cell::RefCell;
use std::rc::Rc;

type PageCallback = Rc<RefCell<Option<Box<dyn Fn(u32)>>>>;

glib::wrapper! {
    pub struct Sidebar(ObjectSubclass<imp::Imp>)
        @extends gtk4::Box, gtk4::Widget, @implements gtk4::Orientable, gtk4::Accessible, gtk4::Buildable, gtk4::ConstraintTarget;
}

mod imp {
    use super::*;
    use gtk4::subclass::prelude::*;

    pub struct Imp;

    impl Default for Imp {
        fn default() -> Self {
            Self
        }
    }

    #[glib::object_subclass]
    impl ObjectSubclass for Imp {
        const NAME: &'static str = "OreonSidebar";
        type Type = super::Sidebar;
        type ParentType = gtk4::Box;
    }

    impl ObjectImpl for Imp {
        fn constructed(&self) {
            let obj = self.obj();
            obj.set_orientation(Orientation::Vertical);
            obj.set_spacing(0);
        }
    }

    impl BoxImpl for Imp {}
    impl WidgetImpl for Imp {}
}

struct NavItem {
    icon: &'static str,
    label: &'static str,
}

impl Sidebar {
    pub fn new() -> Self {
        let obj: Self = glib::Object::new();
        obj.set_widget_name("sidebar");
        obj.set_width_request(200);
        obj.set_vexpand(true);

        // --- Header ---
        let header = GtkBox::new(Orientation::Horizontal, 8);
        header.set_margin_start(14);
        header.set_margin_end(14);
        header.set_margin_top(12);
        header.set_margin_bottom(12);

        // Logo from embedded resource
        let logo = load_logo_image(24);
        let logo_label = Label::new(Some("Oreon"));
        logo_label.add_css_class("title-header");
        logo_label.set_halign(Align::Start);

        header.append(&logo);
        header.append(&logo_label);
        obj.upcast_ref::<gtk4::Box>().append(&header);

        // --- Nav list ---
        let list = ListBox::new();
        list.add_css_class("navigation-sidebar");
        list.set_selection_mode(gtk4::SelectionMode::Browse);
        list.set_hexpand(false);

        let nav_items = [
            NavItem {
                icon: "system-software-install-symbolic",
                label: "Packages",
            },
            NavItem {
                icon: "folder-symbolic",
                label: "Repositories",
            },
            NavItem {
                icon: "computer-symbolic",
                label: "Containers",
            },
            NavItem {
                icon: "preferences-system-symbolic",
                label: "Drivers",
            },
        ];

        for item in nav_items.iter() {
            let row = ListBoxRow::new();
            row.set_height_request(36);

            let content = GtkBox::new(Orientation::Horizontal, 10);
            content.set_margin_start(10);
            content.set_margin_end(10);
            content.set_margin_top(6);
            content.set_margin_bottom(6);
            content.set_halign(Align::Start);

            let icon = Image::from_icon_name(item.icon);
            icon.set_icon_size(gtk4::IconSize::Normal);

            let label = Label::new(Some(item.label));

            content.append(&icon);
            content.append(&label);
            row.set_child(Some(&content));
            list.append(&row);
        }

        // Select first row
        list.select_row(list.row_at_index(0).as_ref());

        obj.upcast_ref::<gtk4::Box>().append(&list);

        // Spacer
        let stretch = GtkBox::new(Orientation::Vertical, 0);
        stretch.set_vexpand(true);
        obj.upcast_ref::<gtk4::Box>().append(&stretch);

        obj
    }

    pub fn connect_page_requested<F: Fn(u32) + 'static>(&self, f: F) {
        let callback: PageCallback = Rc::new(RefCell::new(None));

        // Find the ListBox child
        let mut child = self.first_child();
        while let Some(c) = child {
            if let Some(list) = c.downcast_ref::<ListBox>() {
                let cb1 = callback.clone();
                list.connect_row_selected(move |_, row| {
                    if let Some(row) = row {
                        let idx = row.index() as u32;
                        if let Some(ref cb) = *cb1.borrow() {
                            cb(idx);
                        }
                    }
                });
                break;
            }
            child = c.next_sibling();
        }

        *callback.borrow_mut() = Some(Box::new(f));
    }
}

/// Load the logo from the embedded GResource as an `Image` widget.
fn load_logo_image(size: i32) -> Image {
    let texture = gtk4::gdk::Texture::from_resource(resources::LOGO_RESOURCE_PATH);
    let img = Image::from_paintable(Some(&texture));
    img.set_pixel_size(size);
    img
}
