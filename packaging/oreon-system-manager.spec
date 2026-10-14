Name:           oreon-system-manager
Version:        1.0.3
Release:        1%{?dist}
Summary:        Qt6 system manager for Oreon packages, repos, containers, and drivers
License:        GPL-3.0-or-later
URL:            https://oreonhq.com
Source0:        https://tarballs.oreonhq.com/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtsvg-devel

Requires:       qt6-qtbase
Requires:       polkit
Requires:       dnf
Requires:       (breeze or plasma-breeze or kf6-breeze or qt6ct)
Recommends:     docker
Recommends:      distrobox
Recommends:      flatpak

%description
Manage DNF packages and repositories, Docker and Distrobox containers, Flatpak,
and hardware driver suggestions with a Qt6 GUI.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install
# cmake already installs binary/desktop/icon/policy; ensure paths exist
install -Dm644 packaging/oreon-system-manager.desktop \
  %{buildroot}%{_datadir}/applications/oreon-system-manager.desktop
install -Dm644 assets/logo.png \
  %{buildroot}%{_datadir}/icons/hicolor/200x200/apps/oreon-system-manager.png
install -Dm644 packaging/org.oreon.SystemManager.policy \
  %{buildroot}%{_datadir}/polkit-1/actions/org.oreon.SystemManager.policy

%post
# desktop mime / icon cache; nothing destructive
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f %{_datadir}/icons/hicolor >/dev/null 2>&1 || :
fi
if command -v update-mime-database >/dev/null 2>&1; then
  update-mime-database %{_datadir}/mime >/dev/null 2>&1 || :
fi
echo "Oreon System Manager %{version} ready. Launch from the menu or run oreon-system-manager."

%postun
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f %{_datadir}/icons/hicolor >/dev/null 2>&1 || :
fi

%files
%license LICENSE
%doc README.md
%{_bindir}/oreon-system-manager
%{_datadir}/applications/oreon-system-manager.desktop
%{_datadir}/icons/hicolor/*/apps/oreon-system-manager.png
%{_datadir}/polkit-1/actions/org.oreon.SystemManager.policy

%changelog
%autochangelog
