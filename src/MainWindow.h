#pragma once

#include <QMainWindow>
#include <QDateTime>
#include <QKeyEvent>

class QStackedWidget;
class QListWidget;
class QDockWidget;
class Sidebar;
class CommandPalette;
class PackagesPage;
class ReposPage;
class ContainersPage;
class DriversPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void setupPalette();
    void setupShortcuts();
    void showAbout();
    void showSettings();
    void logActivity(const QString &msg);
    void handlePaletteAction(const QString &id);
    void goPage(int index);

    Sidebar *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    PackagesPage *m_packages = nullptr;
    ReposPage *m_repos = nullptr;
    ContainersPage *m_containers = nullptr;
    DriversPage *m_drivers = nullptr;
    CommandPalette *m_palette = nullptr;
    QDockWidget *m_activityDock = nullptr;
    QListWidget *m_activityList = nullptr;
};
