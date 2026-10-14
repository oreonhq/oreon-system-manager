#include "ContainersPage.h"
#include "ProcessRunner.h"
#include "Settings.h"
#include "widgets/OutputPanel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QRegularExpression>

ContainersPage::ContainersPage(QWidget *parent)
    : QWidget(parent)
{
    m_runner = new ProcessRunner(this);
    connect(m_runner, &ProcessRunner::outputReceived, this, &ContainersPage::onOutput);
    connect(m_runner, &ProcessRunner::finished, this, &ContainersPage::onFinished);
    connect(m_runner, &ProcessRunner::failedToStart, this, [this](const QString &e) {
        m_output->append(QStringLiteral("Failed to start: %1\n").arg(e));
        m_output->expand();
        setBusy(false);
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Containers"), this);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    root->addWidget(title);

    m_tabs = new QTabWidget(this);
    buildDockerTab();
    buildDistroboxTab();
    buildFlatpakTab();
    root->addWidget(m_tabs, 1);

    m_output = new OutputPanel(this);
    root->addWidget(m_output);
    connect(m_output, &OutputPanel::cancelRequested, this, [this]() {
        m_runner->cancel();
        setBusy(false);
    });

    refreshAll();
}

bool ContainersPage::toolExists(const QString &name) const
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

void ContainersPage::buildDockerTab()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(8);

    m_dockerEmpty = new QWidget(page);
    auto *emptyLay = new QHBoxLayout(m_dockerEmpty);
    emptyLay->setContentsMargins(0, 0, 0, 0);
    auto *emptyLbl = new QLabel(QStringLiteral("Docker not installed."), m_dockerEmpty);
    emptyLbl->setStyleSheet(QStringLiteral("color: gray;"));
    emptyLay->addWidget(emptyLbl);
    auto *installBtn = new QPushButton(QStringLiteral("Install"), m_dockerEmpty);
    connect(installBtn, &QPushButton::clicked, this, &ContainersPage::installDocker);
    emptyLay->addWidget(installBtn);
    emptyLay->addStretch(1);
    lay->addWidget(m_dockerEmpty);

    m_dockerTable = new QTableWidget(0, 3, page);
    m_dockerTable->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Status"), QStringLiteral("Image")});
    m_dockerTable->horizontalHeader()->setStretchLastSection(false);
    m_dockerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_dockerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_dockerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_dockerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dockerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dockerTable->setAlternatingRowColors(true);
    m_dockerTable->verticalHeader()->setVisible(false);
    m_dockerTable->setShowGrid(false);
    lay->addWidget(m_dockerTable, 1);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto *start = new QPushButton(QStringLiteral("Start"), page);
    auto *stop = new QPushButton(QStringLiteral("Stop"), page);
    auto *remove = new QPushButton(QStringLiteral("Remove"), page);
    connect(start, &QPushButton::clicked, this, &ContainersPage::dockerStart);
    connect(stop, &QPushButton::clicked, this, &ContainersPage::dockerStop);
    connect(remove, &QPushButton::clicked, this, &ContainersPage::dockerRemove);
    auto *more = new QToolButton(page);
    more->setText(QStringLiteral("More"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(QStringLiteral("Create"), this, &ContainersPage::dockerCreate);
    menu->addAction(QStringLiteral("Pull"), this, &ContainersPage::dockerPull);
    menu->addAction(QStringLiteral("Logs"), this, &ContainersPage::dockerLogs);
    menu->addAction(QStringLiteral("Restart"), this, &ContainersPage::dockerRestart);
    menu->addAction(QStringLiteral("Refresh"), this, &ContainersPage::refreshDocker);
    more->setMenu(menu);
    row->addWidget(start);
    row->addWidget(stop);
    row->addWidget(remove);
    row->addWidget(more);
    row->addStretch(1);
    lay->addLayout(row);

    m_dockerHint = new QLabel(page); // kept for compatibility; unused visually
    m_dockerHint->hide();

    m_tabs->addTab(page, QStringLiteral("Docker"));
}

void ContainersPage::buildDistroboxTab()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(8);

    m_distroEmpty = new QWidget(page);
    auto *emptyLay = new QHBoxLayout(m_distroEmpty);
    emptyLay->setContentsMargins(0, 0, 0, 0);
    auto *emptyLbl = new QLabel(QStringLiteral("Distrobox not installed."), m_distroEmpty);
    emptyLbl->setStyleSheet(QStringLiteral("color: gray;"));
    emptyLay->addWidget(emptyLbl);
    auto *installBtn = new QPushButton(QStringLiteral("Install"), m_distroEmpty);
    connect(installBtn, &QPushButton::clicked, this, &ContainersPage::installDistrobox);
    emptyLay->addWidget(installBtn);
    emptyLay->addStretch(1);
    lay->addWidget(m_distroEmpty);

    m_distroTable = new QTableWidget(0, 4, page);
    m_distroTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Name"), QStringLiteral("Status"), QStringLiteral("Image")});
    m_distroTable->horizontalHeader()->setStretchLastSection(false);
    m_distroTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_distroTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_distroTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_distroTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_distroTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_distroTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_distroTable->setAlternatingRowColors(true);
    m_distroTable->verticalHeader()->setVisible(false);
    m_distroTable->setShowGrid(false);
    lay->addWidget(m_distroTable, 1);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto *enter = new QPushButton(QStringLiteral("Enter"), page);
    auto *stop = new QPushButton(QStringLiteral("Stop"), page);
    auto *remove = new QPushButton(QStringLiteral("Delete"), page);
    connect(enter, &QPushButton::clicked, this, &ContainersPage::distroEnter);
    connect(stop, &QPushButton::clicked, this, &ContainersPage::distroStop);
    connect(remove, &QPushButton::clicked, this, &ContainersPage::distroRemove);
    auto *more = new QToolButton(page);
    more->setText(QStringLiteral("More"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(QStringLiteral("Create"), this, &ContainersPage::distroCreate);
    menu->addAction(QStringLiteral("Export"), this, &ContainersPage::distroExport);
    menu->addAction(QStringLiteral("Refresh"), this, &ContainersPage::refreshDistrobox);
    more->setMenu(menu);
    row->addWidget(enter);
    row->addWidget(stop);
    row->addWidget(remove);
    row->addWidget(more);
    row->addStretch(1);
    lay->addLayout(row);

    m_distroHint = new QLabel(page);
    m_distroHint->hide();

    m_tabs->addTab(page, QStringLiteral("Distrobox"));
}

void ContainersPage::buildFlatpakTab()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(8);

    m_flatpakHint = new QLabel(page);
    m_flatpakHint->setWordWrap(true);
    m_flatpakHint->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_flatpakHint);

    m_flatpakTable = new QTableWidget(0, 3, page);
    m_flatpakTable->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Application ID"), QStringLiteral("Version")});
    m_flatpakTable->horizontalHeader()->setStretchLastSection(false);
    m_flatpakTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_flatpakTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_flatpakTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_flatpakTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_flatpakTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_flatpakTable->setAlternatingRowColors(true);
    m_flatpakTable->verticalHeader()->setVisible(false);
    m_flatpakTable->setShowGrid(false);
    lay->addWidget(m_flatpakTable, 1);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    auto *install = new QPushButton(QStringLiteral("Install"), page);
    auto *uninstall = new QPushButton(QStringLiteral("Uninstall"), page);
    connect(install, &QPushButton::clicked, this, &ContainersPage::flatpakInstall);
    connect(uninstall, &QPushButton::clicked, this, &ContainersPage::flatpakUninstall);
    auto *more = new QToolButton(page);
    more->setText(QStringLiteral("More"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addAction(QStringLiteral("Refresh"), this, &ContainersPage::refreshFlatpak);
    more->setMenu(menu);
    row->addWidget(install);
    row->addWidget(uninstall);
    row->addWidget(more);
    row->addStretch(1);
    lay->addLayout(row);

    m_tabs->addTab(page, QStringLiteral("Flatpak"));
}

void ContainersPage::refreshAll()
{
    refreshDocker();
}

void ContainersPage::setBusy(bool busy)
{
    m_output->setBusy(busy);
}

QString ContainersPage::selectedDockerName() const
{
    const int row = m_dockerTable->currentRow();
    if (row < 0) return {};
    auto *it = m_dockerTable->item(row, 0);
    return it ? it->text() : QString();
}

QString ContainersPage::selectedDistroName() const
{
    const int row = m_distroTable->currentRow();
    if (row < 0) return {};
    auto *it = m_distroTable->item(row, 1);
    return it ? it->text() : QString();
}

QString ContainersPage::selectedFlatpakId() const
{
    const int row = m_flatpakTable->currentRow();
    if (row < 0) return {};
    auto *it = m_flatpakTable->item(row, 1);
    return it ? it->text() : QString();
}

void ContainersPage::refreshDocker()
{
    const bool has = toolExists(QStringLiteral("docker"));
    m_dockerEmpty->setVisible(!has);
    m_dockerTable->setVisible(has);
    if (!has) {
        refreshDistrobox();
        return;
    }
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::DockerList;
    m_dockerTable->setRowCount(0);
    m_output->collapse();
    setBusy(true);
    m_runner->start(QStringLiteral("docker"),
                    {QStringLiteral("ps"), QStringLiteral("-a"),
                     QStringLiteral("--format"), QStringLiteral("{{.Names}}\t{{.Status}}\t{{.Image}}")});
}

void ContainersPage::refreshDistrobox()
{
    const bool has = toolExists(QStringLiteral("distrobox"));
    m_distroEmpty->setVisible(!has);
    m_distroTable->setVisible(has);
    if (!has) {
        refreshFlatpak();
        return;
    }
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::DistroList;
    m_distroTable->setRowCount(0);
    m_output->collapse();
    setBusy(true);
    m_runner->start(QStringLiteral("distrobox"), {QStringLiteral("list")});
}

void ContainersPage::refreshFlatpak()
{
    const bool has = toolExists(QStringLiteral("flatpak"));
    m_flatpakHint->setText(has ? QString()
                               : QStringLiteral("Flatpak is not installed."));
    m_flatpakHint->setVisible(!has);
    m_flatpakTable->setEnabled(has);
    if (!has) {
        setBusy(false);
        return;
    }
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::FlatpakList;
    m_flatpakTable->setRowCount(0);
    m_output->collapse();
    setBusy(true);
    m_runner->start(QStringLiteral("flatpak"),
                    {QStringLiteral("list"), QStringLiteral("--app"),
                     QStringLiteral("--columns=name,application,version")});
}

void ContainersPage::installDocker()
{
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Install docker"));
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("install"), QStringLiteral("--assumeyes"),
                           QStringLiteral("docker")});
}

void ContainersPage::installDistrobox()
{
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Install distrobox"));
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("install"), QStringLiteral("--assumeyes"),
                           QStringLiteral("distrobox")});
}

void ContainersPage::dockerStart()
{
    const QString n = selectedDockerName();
    if (n.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("docker start %1").arg(n));
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("start"), n});
}
void ContainersPage::dockerStop()
{
    const QString n = selectedDockerName();
    if (n.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("docker stop %1").arg(n));
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("stop"), n});
}
void ContainersPage::dockerRestart()
{
    const QString n = selectedDockerName();
    if (n.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("restart"), n});
}
void ContainersPage::dockerRemove()
{
    const QString n = selectedDockerName();
    if (n.isEmpty()) return;
    if (Settings::instance().confirmBeforeRemove()) {
        if (QMessageBox::question(this, QStringLiteral("Remove container"),
                QStringLiteral("Remove container '%1'?").arg(n)) != QMessageBox::Yes)
            return;
    }
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("docker rm %1").arg(n));
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("rm"), QStringLiteral("-f"), n});
}
void ContainersPage::dockerCreate()
{
    bool ok = false;
    const QString image = QInputDialog::getText(this, QStringLiteral("Create container"),
        QStringLiteral("Image name:"), QLineEdit::Normal, QStringLiteral("fedora:latest"), &ok).trimmed();
    if (!ok || image.isEmpty()) return;
    const QString name = QInputDialog::getText(this, QStringLiteral("Create container"),
        QStringLiteral("Container name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("docker create %1").arg(name));
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("create"), QStringLiteral("--name"), name, image});
}
void ContainersPage::dockerPull()
{
    bool ok = false;
    const QString image = QInputDialog::getText(this, QStringLiteral("Pull image"),
        QStringLiteral("Image:"), QLineEdit::Normal, QStringLiteral("fedora:latest"), &ok).trimmed();
    if (!ok || image.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("docker pull %1").arg(image));
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("pull"), image});
}
void ContainersPage::dockerLogs()
{
    const QString n = selectedDockerName();
    if (n.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    m_runner->start(QStringLiteral("docker"), {QStringLiteral("logs"), QStringLiteral("--tail"), QStringLiteral("200"), n});
}

void ContainersPage::distroCreate()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Create Distrobox"),
        QStringLiteral("Name:"), QLineEdit::Normal, QStringLiteral("fedora"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const QString image = QInputDialog::getText(this, QStringLiteral("Create Distrobox"),
        QStringLiteral("Image:"), QLineEdit::Normal,
        QStringLiteral("registry.fedoraproject.org/fedora-toolbox:latest"), &ok).trimmed();
    if (!ok || image.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("distrobox create %1").arg(name));
    m_runner->start(QStringLiteral("distrobox"), {QStringLiteral("create"), QStringLiteral("-n"), name, QStringLiteral("-i"), image});
}
void ContainersPage::distroEnter()
{
    const QString n = selectedDistroName();
    if (n.isEmpty()) return;
    QString term = Settings::instance().terminalCommand();
    if (QStandardPaths::findExecutable(term).isEmpty()) {
        for (const QString &c : {QStringLiteral("konsole"), QStringLiteral("gnome-terminal"), QStringLiteral("xterm")}) {
            if (!QStandardPaths::findExecutable(c).isEmpty()) { term = c; break; }
        }
    }
    if (QStandardPaths::findExecutable(term).isEmpty()) {
        m_output->expand();
        m_output->append(QStringLiteral("No terminal found. Set one in Settings.\n"));
        return;
    }
    QStringList args;
    if (term.contains(QLatin1String("gnome-terminal")))
        args << QStringLiteral("--") << QStringLiteral("distrobox") << QStringLiteral("enter") << n;
    else if (term.contains(QLatin1String("konsole")))
        args << QStringLiteral("-e") << QStringLiteral("distrobox") << QStringLiteral("enter") << n;
    else
        args << QStringLiteral("-e") << QStringLiteral("distrobox enter ") + n;
    QProcess::startDetached(term, args);
    emit activity(QStringLiteral("Opened terminal for %1").arg(n));
}
void ContainersPage::distroStop()
{
    const QString n = selectedDistroName();
    if (n.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    m_runner->start(QStringLiteral("distrobox"), {QStringLiteral("stop"), n});
}
void ContainersPage::distroRemove()
{
    const QString n = selectedDistroName();
    if (n.isEmpty()) return;
    if (Settings::instance().confirmBeforeRemove()) {
        if (QMessageBox::question(this, QStringLiteral("Remove Distrobox"),
                QStringLiteral("Remove '%1'?").arg(n)) != QMessageBox::Yes)
            return;
    }
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("distrobox rm %1").arg(n));
    m_runner->start(QStringLiteral("distrobox"), {QStringLiteral("rm"), QStringLiteral("-f"), n});
}
void ContainersPage::distroExport()
{
    const QString n = selectedDistroName();
    if (n.isEmpty()) return;
    bool ok = false;
    const QString app = QInputDialog::getText(this, QStringLiteral("Export app"),
        QStringLiteral("App binary name inside the box:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || app.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    m_runner->start(QStringLiteral("distrobox"),
                    {QStringLiteral("enter"), n, QStringLiteral("--"),
                     QStringLiteral("distrobox-export"), QStringLiteral("--app"), app});
}

void ContainersPage::flatpakInstall()
{
    bool ok = false;
    const QString app = QInputDialog::getText(this, QStringLiteral("Install Flatpak"),
        QStringLiteral("Application ID:"), QLineEdit::Normal, QStringLiteral("org.mozilla.firefox"), &ok).trimmed();
    if (!ok || app.isEmpty()) return;
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    emit activity(QStringLiteral("flatpak install %1").arg(app));
    m_runner->start(QStringLiteral("flatpak"),
                    {QStringLiteral("install"), QStringLiteral("-y"), QStringLiteral("flathub"), app});
}
void ContainersPage::flatpakUninstall()
{
    const QString id = selectedFlatpakId();
    if (id.isEmpty()) return;
    if (Settings::instance().confirmBeforeRemove()) {
        if (QMessageBox::question(this, QStringLiteral("Uninstall Flatpak"),
                QStringLiteral("Uninstall '%1'?").arg(id)) != QMessageBox::Yes)
            return;
    }
    m_mode = Mode::Action; m_output->clear(); m_output->expand(); setBusy(true);
    m_runner->start(QStringLiteral("flatpak"), {QStringLiteral("uninstall"), QStringLiteral("-y"), id});
}

void ContainersPage::onOutput(const QString &text, bool isStdout)
{
    if (m_mode == Mode::DockerList || m_mode == Mode::DistroList || m_mode == Mode::FlatpakList) {
        if (isStdout)
            m_buffer += text;
        if (m_output->isExpanded())
            m_output->append(text);
        return;
    }
    m_output->append(text);
}

void ContainersPage::onFinished(int code, QProcess::ExitStatus st)
{
    if (m_mode == Mode::DockerList) {
        for (const QString &line : ProcessRunner::parseDockerPsOutput(m_buffer)) {
            const QStringList p = line.split(QLatin1Char('\t'));
            const int row = m_dockerTable->rowCount();
            m_dockerTable->insertRow(row);
            m_dockerTable->setItem(row, 0, new QTableWidgetItem(p.value(0)));
            m_dockerTable->setItem(row, 1, new QTableWidgetItem(p.value(1)));
            m_dockerTable->setItem(row, 2, new QTableWidgetItem(p.value(2)));
        }
        setBusy(false);
        refreshDistrobox();
        return;
    }
    if (m_mode == Mode::DistroList) {
        for (const QString &line : ProcessRunner::parseDistroboxListOutput(m_buffer)) {
            QStringList p;
            if (line.contains(QLatin1Char('|'))) {
                for (const QString &part : line.split(QLatin1Char('|')))
                    p << part.trimmed();
            } else {
                p = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            }
            const int row = m_distroTable->rowCount();
            m_distroTable->insertRow(row);
            m_distroTable->setItem(row, 0, new QTableWidgetItem(p.value(0)));
            m_distroTable->setItem(row, 1, new QTableWidgetItem(p.value(1)));
            m_distroTable->setItem(row, 2, new QTableWidgetItem(p.value(2)));
            m_distroTable->setItem(row, 3, new QTableWidgetItem(p.value(3)));
        }
        setBusy(false);
        refreshFlatpak();
        return;
    }
    if (m_mode == Mode::FlatpakList) {
        for (const QString &raw : m_buffer.split(QLatin1Char('\n'))) {
            const QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1String("Name")))
                continue;
            QStringList p = line.split(QRegularExpression(QStringLiteral("\\s{2,}")));
            if (p.size() < 2)
                p = line.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
            if (p.size() < 2)
                continue;
            const int row = m_flatpakTable->rowCount();
            m_flatpakTable->insertRow(row);
            m_flatpakTable->setItem(row, 0, new QTableWidgetItem(p.value(0)));
            m_flatpakTable->setItem(row, 1, new QTableWidgetItem(p.value(1)));
            m_flatpakTable->setItem(row, 2, new QTableWidgetItem(p.value(2)));
        }
        setBusy(false);
        m_mode = Mode::Idle;
        return;
    }
    m_output->append(ProcessRunner::formatResult(code, st));
    setBusy(false);
    m_mode = Mode::Idle;
    if (code == 0)
        refreshAll();
}
