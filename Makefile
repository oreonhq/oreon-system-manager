.PHONY: all build release clean install uninstall run help

BINARY := oreon-system-manager
PREFIX ?= /usr
DESTDIR ?=
BUILD_DIR ?= build
JOBS ?= $(shell nproc)

help:
	@echo "Available targets:"
	@echo "  make build       - Debug cmake build"
	@echo "  make release     - Release cmake build"
	@echo "  make run         - Build and run"
	@echo "  make clean       - Remove build dir"
	@echo "  make install     - Install to PREFIX (default /usr)"
	@echo "  make uninstall   - Remove installed files"
	@echo ""
	@echo "Variables: PREFIX DESTDIR BUILD_DIR JOBS"

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) -j$(JOBS)

release:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j$(JOBS)

run: build
	./$(BUILD_DIR)/$(BINARY)

clean:
	rm -rf $(BUILD_DIR)

install: release
	install -Dm755 $(BUILD_DIR)/$(BINARY) $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	install -Dm644 assets/logo.png \
		$(DESTDIR)$(PREFIX)/share/icons/hicolor/200x200/apps/$(BINARY).png
	install -Dm644 packaging/$(BINARY).desktop \
		$(DESTDIR)$(PREFIX)/share/applications/$(BINARY).desktop
	install -Dm644 packaging/org.oreon.SystemManager.policy \
		$(DESTDIR)$(PREFIX)/share/polkit-1/actions/org.oreon.SystemManager.policy
	@echo "Installed to $(PREFIX)"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/200x200/apps/$(BINARY).png
	rm -f $(DESTDIR)$(PREFIX)/share/applications/$(BINARY).desktop
	rm -f $(DESTDIR)$(PREFIX)/share/polkit-1/actions/org.oreon.SystemManager.policy
	@echo "Uninstalled from $(PREFIX)"
