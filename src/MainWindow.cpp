#include "MainWindow.h"
#include "Settings.h"
#include "pages/PackagesPage.h"
#include "pages/ReposPage.h"
#include "pages/ContainersPage.h"
#include "pages/DriversPage.h"
#include "widgets/Sidebar.h"
#include "widgets/CommandPalette.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Oreon System Manager"));
    resize(1100, 700);
    setWindowIcon(QIcon(QStringLiteral(":/org/oreon/SystemManager/logo.png")));

    setStyleSheet(QStringLiteral(
        "QToolButton { padding: 4px 8px; }"
        "QPushButton { padding: 4px 12px; }"
    ));

    setupUi();
    setupMenus();
    setupPalette();
    setupShortcuts();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *lay = new QHBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_sidebar = new Sidebar(central);
    m_stack = new QStackedWidget(central);
    m_packages = new PackagesPage(m_stack);
    m_repos = new ReposPage(m_stack);
    m_containers = new ContainersPage(m_stack);
    m_drivers = new DriversPage(m_stack);
    m_stack->addWidget(m_packages);
    m_stack->addWidget(m_repos);
    m_stack->addWidget(m_containers);
    m_stack->addWidget(m_drivers);

    lay->addWidget(m_sidebar);
    lay->addWidget(m_stack, 1);
    setCentralWidget(central);

    // Collapse the QMainWindow status bar so no bottom chrome remains.
    auto *sb = new QStatusBar(this);
    setStatusBar(sb);
    sb->setVisible(false);
    sb->hide();
    sb->setFixedHeight(0);
    sb->setMaximumHeight(0);

    connect(m_sidebar, &Sidebar::pageRequested, this, &MainWindow::goPage);
    connect(m_packages, &PackagesPage::activity, this, &MainWindow::logActivity);
    connect(m_repos, &ReposPage::activity, this, &MainWindow::logActivity);
    connect(m_containers, &ContainersPage::activity, this, &MainWindow::logActivity);
    connect(m_drivers, &DriversPage::activity, this, &MainWindow::logActivity);

    m_activityDock = new QDockWidget(QStringLiteral("Activity log"), this);
    m_activityDock->setObjectName(QStringLiteral("activityDock"));
    m_activityList = new QListWidget(m_activityDock);
    m_activityDock->setWidget(m_activityList);
    addDockWidget(Qt::BottomDockWidgetArea, m_activityDock);
    m_activityDock->hide();
}

void MainWindow::setupMenus()
{
    auto *file = menuBar()->addMenu(QStringLiteral("File"));
    file->addAction(QStringLiteral("Command palette…"), QKeySequence(QStringLiteral("Ctrl+K")), this, [this]() {
        m_palette->openPalette();
    });
    file->addAction(QStringLiteral("Activity log"), this, [this]() {
        m_activityDock->setVisible(!m_activityDock->isVisible());
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Quit"), QKeySequence::Quit, this, &QWidget::close);

    auto *view = menuBar()->addMenu(QStringLiteral("View"));
    view->addAction(QStringLiteral("Packages"), QKeySequence(QStringLiteral("Ctrl+1")), this, [this]() { goPage(0); });
    view->addAction(QStringLiteral("Repositories"), QKeySequence(QStringLiteral("Ctrl+2")), this, [this]() { goPage(1); });
    view->addAction(QStringLiteral("Containers"), QKeySequence(QStringLiteral("Ctrl+3")), this, [this]() { goPage(2); });
    view->addAction(QStringLiteral("Drivers"), QKeySequence(QStringLiteral("Ctrl+4")), this, [this]() { goPage(3); });

    auto *help = menuBar()->addMenu(QStringLiteral("Help"));
    help->addAction(QStringLiteral("Settings"), this, &MainWindow::showSettings);
    help->addAction(QStringLiteral("About"), this, &MainWindow::showAbout);
}


void MainWindow::setupPalette()
{
    m_palette = new CommandPalette(this);
    QVector<PaletteAction> actions = {
        {QStringLiteral("page.packages"), QStringLiteral("Go to Packages"), QStringLiteral("Navigate")},
        {QStringLiteral("page.repos"), QStringLiteral("Go to Repositories"), QStringLiteral("Navigate")},
        {QStringLiteral("page.containers"), QStringLiteral("Go to Containers"), QStringLiteral("Navigate")},
        {QStringLiteral("page.drivers"), QStringLiteral("Go to Drivers"), QStringLiteral("Navigate")},
        {QStringLiteral("pkg.search"), QStringLiteral("Search packages"), QStringLiteral("Focus package search")},
        {QStringLiteral("pkg.update"), QStringLiteral("Update all packages"), QStringLiteral("pkexec dnf upgrade")},
        {QStringLiteral("repo.refresh"), QStringLiteral("Refresh repositories"), {}},
        {QStringLiteral("drivers.detect"), QStringLiteral("Detect drivers"), {}},
        {QStringLiteral("containers.refresh"), QStringLiteral("Refresh containers"), {}},
        {QStringLiteral("activity.toggle"), QStringLiteral("Toggle activity log"), {}},
        {QStringLiteral("settings"), QStringLiteral("Open settings"), {}},
        {QStringLiteral("about"), QStringLiteral("About Oreon System Manager"), {}},
    };
    m_palette->setActions(actions);
    connect(m_palette, &CommandPalette::actionTriggered, this, &MainWindow::handlePaletteAction);
}

void MainWindow::setupShortcuts()
{
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this, [this]() { m_palette->openPalette(); });
}

void MainWindow::goPage(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_stack->setCurrentIndex(index);
    m_sidebar->setCurrentIndex(index);
}

void MainWindow::logActivity(const QString &msg)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_activityList->insertItem(0, QStringLiteral("[%1] %2").arg(stamp, msg));
    while (m_activityList->count() > 500)
        delete m_activityList->takeItem(m_activityList->count() - 1);
}

void MainWindow::handlePaletteAction(const QString &id)
{
    if (id == QLatin1String("page.packages")) goPage(0);
    else if (id == QLatin1String("page.repos")) goPage(1);
    else if (id == QLatin1String("page.containers")) goPage(2);
    else if (id == QLatin1String("page.drivers")) goPage(3);
    else if (id == QLatin1String("pkg.search")) { goPage(0); m_packages->focusSearch(); }
    else if (id == QLatin1String("pkg.update")) { goPage(0); logActivity(QStringLiteral("Use Update all on Packages page")); }
    else if (id == QLatin1String("repo.refresh")) { goPage(1); m_repos->refresh(); }
    else if (id == QLatin1String("drivers.detect")) { goPage(3); m_drivers->detect(); }
    else if (id == QLatin1String("containers.refresh")) { goPage(2); m_containers->refreshAll(); }
    else if (id == QLatin1String("activity.toggle")) m_activityDock->setVisible(!m_activityDock->isVisible());
    else if (id == QLatin1String("settings")) showSettings();
    else if (id == QLatin1String("about")) showAbout();
    else if (id.startsWith(QLatin1String("search:"))) {
        goPage(0);
        m_packages->searchQuery(id.mid(7));
    }
}


void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("About Oreon System Manager"));
    box.setIconPixmap(QPixmap(QStringLiteral(":/org/oreon/SystemManager/logo.png"))
                          .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    box.setText(QStringLiteral(
        "<b>Oreon System Manager</b> 1.0.3<br>"
        "Packages, repos, containers, and drivers for Oreon.<br><br>"
        "License: GPL-3.0<br>"
        "App ID: org.oreon.SystemManager<br><br>"
        "<b>Keyboard shortcuts</b><br>"
        "Ctrl+K: command palette<br>"
        "Ctrl+1..4: switch pages<br>"
        "Ctrl+Q: quit<br>"));
    box.exec();
}

void MainWindow::showSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Settings"));
    dlg.resize(420, 220);
    auto *form = new QFormLayout(&dlg);

    auto *theme = new QComboBox(&dlg);
    theme->addItem(QStringLiteral("Breeze Light"), static_cast<int>(Settings::BreezeLight));
    theme->addItem(QStringLiteral("Breeze Dark"), static_cast<int>(Settings::BreezeDark));
    theme->setCurrentIndex(Settings::instance().theme() == Settings::BreezeDark ? 1 : 0);

    auto *confirm = new QCheckBox(QStringLiteral("Confirm before remove / destroy"), &dlg);
    confirm->setChecked(Settings::instance().confirmBeforeRemove());

    auto *term = new QLineEdit(Settings::instance().terminalCommand(), &dlg);
    term->setPlaceholderText(QStringLiteral("konsole"));

    form->addRow(QStringLiteral("Theme"), theme);
    form->addRow(QString(), confirm);
    form->addRow(QStringLiteral("Terminal command"), term);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        Settings::instance().setTheme(static_cast<Settings::Theme>(theme->currentData().toInt()));
        Settings::instance().setConfirmBeforeRemove(confirm->isChecked());
        Settings::instance().setTerminalCommand(term->text().trimmed().isEmpty()
            ? QStringLiteral("konsole") : term->text().trimmed());
        logActivity(QStringLiteral("Settings saved"));
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_K) {
        m_palette->openPalette();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
