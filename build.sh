#!/usr/bin/env bash
set -euo pipefail

cargo build --release

if [[ $# -eq 0 ]]; then
    echo "Build complete: target/release/oreon-system-manager"
    echo "Install system-wide with: sudo ./build.sh /usr"
    exit 0
fi

INSTALL_PREFIX="$1"

# Install 200x200 PNG icon
install -Dm644 assets/logo.png \
    "${DESTDIR:-}${INSTALL_PREFIX}/share/icons/hicolor/200x200/apps/oreon-system-manager.png"

# Install desktop file
install -Dm644 packaging/oreon-system-manager.desktop \
    "${DESTDIR:-}${INSTALL_PREFIX}/share/applications/oreon-system-manager.desktop"

# Install binary
install -Dm755 target/release/oreon-system-manager \
    "${DESTDIR:-}${INSTALL_PREFIX}/bin/oreon-system-manager"

echo "Installed to ${INSTALL_PREFIX}"
echo "Run 'gtk-update-icon-cache' to refresh the icon cache"
