#include "ReposPage.h"
#include "ProcessRunner.h"
#include "widgets/OutputPanel.h"

#include <QAbstractItemView>
#include <QColor>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

ReposPage::ReposPage(QWidget *parent)
    : QWidget(parent)
{
    m_runner = new ProcessRunner(this);
    connect(m_runner, &ProcessRunner::outputReceived, this, &ReposPage::onOutput);
    connect(m_runner, &ProcessRunner::finished, this, &ReposPage::onFinished);
    connect(m_runner, &ProcessRunner::failedToStart, this, [this](const QString &e) {
        m_output->append(QStringLiteral("Failed to start: %1\n").arg(e));
        m_output->expand();
        setBusy(false);
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Repositories"), this);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    root->addWidget(title);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Repo ID"), QStringLiteral("Name"), QStringLiteral("Status")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_toggleBtn = new QPushButton(QStringLiteral("Toggle"), this);
    m_addBtn = new QPushButton(QStringLiteral("Add"), this);
    m_refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    m_moreBtn = new QToolButton(this);
    m_moreBtn->setText(QStringLiteral("More"));
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(m_moreBtn);
    moreMenu->addAction(QStringLiteral("Enable"), this, &ReposPage::enableSelected);
    moreMenu->addAction(QStringLiteral("Disable"), this, &ReposPage::disableSelected);
    moreMenu->addAction(QStringLiteral("Remove"), this, &ReposPage::removeRepo);
    m_moreBtn->setMenu(moreMenu);
    actions->addWidget(m_toggleBtn);
    actions->addWidget(m_addBtn);
    actions->addWidget(m_refreshBtn);
    actions->addWidget(m_moreBtn);
    actions->addStretch(1);
    root->addLayout(actions);

    m_output = new OutputPanel(this);
    root->addWidget(m_output);
    connect(m_output, &OutputPanel::cancelRequested, this, [this]() {
        m_runner->cancel();
        setBusy(false);
    });

    connect(m_refreshBtn, &QPushButton::clicked, this, &ReposPage::loadRepos);
    connect(m_toggleBtn, &QPushButton::clicked, this, &ReposPage::toggleSelected);
    connect(m_addBtn, &QPushButton::clicked, this, &ReposPage::addRepo);

    loadRepos();
}

void ReposPage::refresh() { loadRepos(); }

void ReposPage::setBusy(bool busy)
{
    for (auto *b : {m_toggleBtn, m_addBtn, m_refreshBtn})
        b->setEnabled(!busy);
    m_moreBtn->setEnabled(!busy);
    m_output->setBusy(busy);
}

QString ReposPage::selectedRepoId() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return {};
    auto *item = m_table->item(row, 0);
    return item ? item->text() : QString();
}

bool ReposPage::selectedEnabled() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return false;
    auto *item = m_table->item(row, 2);
    return item && item->text() == QLatin1String("enabled");
}

void ReposPage::loadRepos()
{
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::List;
    m_triedFallback = false;
    m_table->setRowCount(0);
    m_output->clear();
    m_output->collapse();
    setBusy(true);
    emit activity(QStringLiteral("Listing repositories"));
    m_runner->start(QStringLiteral("dnf"), {QStringLiteral("repo"), QStringLiteral("list"), QStringLiteral("--all")});
}

void ReposPage::toggleSelected()
{
    if (selectedEnabled())
        disableSelected();
    else
        enableSelected();
}

void ReposPage::enableSelected()
{
    const QString id = selectedRepoId();
    if (id.isEmpty())
        return;
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Enable repo %1").arg(id));
    m_pendingRepoId = id;
    m_pendingEnable = true;
    m_triedSetopt = false;
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("config-manager"),
                           QStringLiteral("--set-enabled"), id});
}

void ReposPage::disableSelected()
{
    const QString id = selectedRepoId();
    if (id.isEmpty())
        return;
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Disable repo %1").arg(id));
    m_pendingRepoId = id;
    m_pendingEnable = false;
    m_triedSetopt = false;
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("config-manager"),
                           QStringLiteral("--set-disabled"), id});
}

void ReposPage::addRepo()
{
    bool ok = false;
    const QString url = QInputDialog::getText(this, QStringLiteral("Add repository"),
        QStringLiteral("URL to a .repo file:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || url.isEmpty())
        return;
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Add repo from %1").arg(url));
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("config-manager"),
                           QStringLiteral("--add-repo"), url});
}

void ReposPage::removeRepo()
{
    const QString id = selectedRepoId();
    if (id.isEmpty())
        return;
    const auto r = QMessageBox::question(this, QStringLiteral("Remove repository"),
        QStringLiteral("Remove custom repo file for '%1'? This uses rm on /etc/yum.repos.d/%1.repo").arg(id));
    if (r != QMessageBox::Yes)
        return;
    m_mode = Mode::Action;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Remove repo %1").arg(id));
    const QString path = QStringLiteral("/etc/yum.repos.d/%1.repo").arg(id);
    m_runner->startPkexec({QStringLiteral("rm"), QStringLiteral("-f"), path});
}

void ReposPage::onOutput(const QString &text, bool isStdout)
{
    if (m_mode == Mode::List) {
        if (isStdout)
            m_buffer += text;
        if (m_output->isExpanded())
            m_output->append(text);
        return;
    }
    m_output->append(text);
}

void ReposPage::onFinished(int code, QProcess::ExitStatus st)
{
    if (m_mode == Mode::List) {
        if (code != 0 && !m_triedFallback) {
            m_triedFallback = true;
            m_buffer.clear();
            m_runner->start(QStringLiteral("dnf"), {QStringLiteral("repolist"), QStringLiteral("--all")});
            return;
        }
        const QStringList lines = ProcessRunner::parseListOutput(m_buffer);
        m_table->setRowCount(0);
        for (const QString &line : lines) {
            const QString id = ProcessRunner::extractRepoId(line);
            if (id.isEmpty())
                continue;
            QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            QString status = QStringLiteral("unknown");
            QString name;
            if (!fields.isEmpty()) {
                const QString last = fields.last();
                if (last == QLatin1String("enabled") || last == QLatin1String("disabled")) {
                    status = last;
                    fields.removeLast();
                }
                if (!fields.isEmpty())
                    fields.removeFirst(); // drop id
                name = fields.join(QLatin1Char(' '));
            }
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(id));
            m_table->setItem(row, 1, new QTableWidgetItem(name));
            auto *badge = new QTableWidgetItem(status);
            if (status == QLatin1String("enabled"))
                badge->setForeground(QColor(39, 174, 96));
            else if (status == QLatin1String("disabled"))
                badge->setForeground(QColor(149, 165, 166));
            m_table->setItem(row, 2, badge);
        }
        m_output->collapse();
        setBusy(false);
        return;
    } else if (code == 0) {
        m_pendingRepoId.clear();
        loadRepos();
        return;
    } else if (code != 0 && !m_pendingRepoId.isEmpty() && !m_triedSetopt
               && code != 126 && code != 127) {
        m_triedSetopt = true;
        const QString opt = QStringLiteral("%1.enabled=%2")
            .arg(m_pendingRepoId, m_pendingEnable ? QStringLiteral("1") : QStringLiteral("0"));
        m_output->append(QStringLiteral("Retrying with dnf5 setopt %1\n").arg(opt));
        m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("config-manager"),
                               QStringLiteral("setopt"), opt});
        return;
    }
    m_output->append(ProcessRunner::formatResult(code, st));
    if (code == 126 || code == 127)
        m_output->append(QStringLiteral("Polkit cancelled or tool missing.\n"));
    m_pendingRepoId.clear();
    setBusy(false);
}
