#include "PackagesPage.h"
#include "ProcessRunner.h"
#include "Settings.h"
#include "widgets/OutputPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

PackagesPage::PackagesPage(QWidget *parent)
    : QWidget(parent)
{
    m_runner = new ProcessRunner(this);
    connect(m_runner, &ProcessRunner::outputReceived, this, &PackagesPage::onOutput);
    connect(m_runner, &ProcessRunner::finished, this, &PackagesPage::onFinished);
    connect(m_runner, &ProcessRunner::failedToStart, this, [this](const QString &e) {
        m_output->append(QStringLiteral("Failed to start: %1\n").arg(e));
        m_output->expand();
        setBusy(false);
        m_listMode = false;
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Packages"), this);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    root->addWidget(title);

    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search packages"));
    m_search->setClearButtonEnabled(true);
    m_installedOnly = new QCheckBox(QStringLiteral("Installed only"), this);
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(m_installedOnly);
    root->addLayout(searchRow);

    m_rangeLabel = new QLabel(QStringLiteral("Showing 0–0 of 0"), this);
    m_rangeLabel->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 12px;"));
    root->addWidget(m_rangeLabel);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Package"), QStringLiteral("Summary"), QStringLiteral("Repo")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setWordWrap(false);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_installBtn = new QPushButton(QStringLiteral("Install"), this);
    m_installBtn->setDefault(true);
    m_installBtn->setAutoDefault(true);
    m_removeBtn = new QPushButton(QStringLiteral("Remove"), this);
    m_moreBtn = new QToolButton(this);
    m_moreBtn->setText(QStringLiteral("More"));
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(m_moreBtn);
    moreMenu->addAction(QStringLiteral("Update all"), this, &PackagesPage::updateAll);
    moreMenu->addAction(QStringLiteral("Info"), this, &PackagesPage::showInfo);
    moreMenu->addAction(QStringLiteral("History"), this, &PackagesPage::showHistory);
    m_moreBtn->setMenu(moreMenu);
    actions->addWidget(m_installBtn);
    actions->addWidget(m_removeBtn);
    actions->addWidget(m_moreBtn);
    actions->addStretch(1);
    root->addLayout(actions);

    m_output = new OutputPanel(this);
    root->addWidget(m_output);
    connect(m_output, &OutputPanel::cancelRequested, this, [this]() {
        m_runner->cancel();
        setBusy(false);
        m_listMode = false;
        m_output->append(QStringLiteral("\n[cancelled]\n"));
    });

    connect(m_search, &QLineEdit::returnPressed, this, &PackagesPage::doSearch);
    connect(m_installedOnly, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            loadInstalled();
        else
            doSearch();
    });
    connect(m_installBtn, &QPushButton::clicked, this, &PackagesPage::installSelected);
    connect(m_removeBtn, &QPushButton::clicked, this, &PackagesPage::removeSelected);
    connect(m_table->verticalScrollBar(), &QScrollBar::valueChanged, this, &PackagesPage::onScroll);

    // Empty search on launch: load available packages immediately.
    loadAvailable();
}

void PackagesPage::focusSearch()
{
    m_search->setFocus();
    m_search->selectAll();
}

void PackagesPage::searchQuery(const QString &q)
{
    m_search->setText(q);
    doSearch();
}

void PackagesPage::setBusy(bool busy)
{
    m_installBtn->setEnabled(!busy);
    m_removeBtn->setEnabled(!busy);
    m_moreBtn->setEnabled(!busy);
    m_output->setBusy(busy);
}

void PackagesPage::clearData()
{
    m_all.clear();
    m_pending.clear();
    m_lineRemainder.clear();
    m_windowStart = 0;
    m_table->setRowCount(0);
    updateRangeLabel();
}

void PackagesPage::beginListJob(bool searchStyle)
{
    if (m_runner->isRunning())
        m_runner->cancel();
    clearData();
    m_listMode = true;
    m_searchStyle = searchStyle;
    m_output->clear();
    m_output->collapse();
    setBusy(true);
}

void PackagesPage::setTableRow(int row, const PkgRow &pkg)
{
    auto *pkgItem = new QTableWidgetItem(pkg.name);
    pkgItem->setToolTip(pkg.name);
    auto *sumItem = new QTableWidgetItem(pkg.summaryOrVersion);
    sumItem->setToolTip(pkg.summaryOrVersion);
    auto *repoItem = new QTableWidgetItem(pkg.repo);
    repoItem->setToolTip(pkg.repo);
    m_table->setItem(row, 0, pkgItem);
    m_table->setItem(row, 1, sumItem);
    m_table->setItem(row, 2, repoItem);
}

void PackagesPage::appendTableRow(const PkgRow &pkg)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    setTableRow(row, pkg);
}

void PackagesPage::updateRangeLabel()
{
    const int total = m_all.size();
    if (total == 0) {
        m_rangeLabel->setText(QStringLiteral("Showing 0–0 of 0"));
        return;
    }
    const int shown = m_table->rowCount();
    const int from = m_windowStart + 1;
    const int to = m_windowStart + shown;
    m_rangeLabel->setText(QStringLiteral("Showing %1–%2 of %3").arg(from).arg(to).arg(total));
}

void PackagesPage::refillTable()
{
    const int count = qMin(WINDOW, qMax(0, m_all.size() - m_windowStart));
    m_table->setRowCount(0);
    m_table->setRowCount(count);
    for (int i = 0; i < count; ++i)
        setTableRow(i, m_all.at(m_windowStart + i));
    updateRangeLabel();
}

void PackagesPage::flushPendingRows()
{
    if (m_pending.isEmpty())
        return;
    m_all += m_pending;
    m_pending.clear();

    const int shown = m_table->rowCount();
    if (shown < WINDOW) {
        const int room = WINDOW - shown;
        const int start = m_windowStart + shown;
        const int n = qMin(room, m_all.size() - start);
        for (int i = 0; i < n; ++i)
            appendTableRow(m_all.at(start + i));
    }
    updateRangeLabel();
}

bool PackagesPage::parseListLine(const QString &line, PkgRow *out) const
{
    const QString t = line.trimmed();
    if (ProcessRunner::isDnfNoise(t))
        return false;
    const QStringList fields = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 2 || !ProcessRunner::looksLikeNevra(fields.at(0)))
        return false;
    out->name = fields.at(0);
    out->summaryOrVersion = fields.value(1);
    out->repo = fields.value(2);
    return true;
}

bool PackagesPage::parseSearchLine(const QString &line, PkgRow *out) const
{
    const QString t = line.trimmed();
    if (ProcessRunner::isDnfNoise(t))
        return false;
    if (t.contains(QLatin1String(" : "))) {
        out->name = t.section(QLatin1String(" : "), 0, 0).trimmed();
        out->summaryOrVersion = t.section(QLatin1String(" : "), 1).trimmed();
        out->repo.clear();
        return !out->name.isEmpty();
    }
    if (t.contains(QLatin1Char('\t'))) {
        const auto parts = t.split(QLatin1Char('\t'));
        out->name = parts.value(0).trimmed();
        out->summaryOrVersion = parts.value(1).trimmed();
        out->repo.clear();
        return !out->name.isEmpty();
    }
    const QStringList fields = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (!fields.isEmpty() && ProcessRunner::looksLikeNevra(fields.at(0))) {
        out->name = fields.at(0);
        out->summaryOrVersion = fields.value(1);
        out->repo = fields.value(2);
        return true;
    }
    return false;
}

void PackagesPage::processStreamChunk(const QString &text)
{
    m_lineRemainder += text;
    int idx = 0;
    while ((idx = m_lineRemainder.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_lineRemainder.left(idx);
        m_lineRemainder = m_lineRemainder.mid(idx + 1);
        PkgRow row;
        const bool ok = m_searchStyle ? parseSearchLine(line, &row) : parseListLine(line, &row);
        if (ok)
            m_pending.append(row);
        if (m_pending.size() >= STREAM_BATCH)
            flushPendingRows();
    }
}

void PackagesPage::shiftWindowDown()
{
    const int windowEnd = m_windowStart + m_table->rowCount();
    if (windowEnd >= m_all.size())
        return;
    const int shift = qMin(SHIFT, m_all.size() - windowEnd);
    if (shift <= 0)
        return;

    m_shifting = true;
    auto *sb = m_table->verticalScrollBar();
    const int oldVal = sb->value();
    const int rowH = qMax(1, m_table->verticalHeader()->defaultSectionSize());

    // Remove top rows, append next rows from m_all.
    for (int i = 0; i < shift; ++i)
        m_table->removeRow(0);
    for (int i = 0; i < shift; ++i)
        appendTableRow(m_all.at(windowEnd + i));

    m_windowStart += shift;
    updateRangeLabel();

    sb->setValue(qMax(0, oldVal - shift * rowH));
    m_shifting = false;
}

void PackagesPage::shiftWindowUp()
{
    if (m_windowStart <= 0)
        return;
    const int shift = qMin(SHIFT, m_windowStart);
    if (shift <= 0)
        return;

    m_shifting = true;
    auto *sb = m_table->verticalScrollBar();
    const int oldVal = sb->value();
    const int rowH = qMax(1, m_table->verticalHeader()->defaultSectionSize());

    // Remove bottom rows, prepend previous rows from m_all.
    const int removeCount = qMin(shift, m_table->rowCount());
    for (int i = 0; i < removeCount; ++i)
        m_table->removeRow(m_table->rowCount() - 1);

    m_windowStart -= shift;
    for (int i = shift - 1; i >= 0; --i)
        m_table->insertRow(0);
    for (int i = 0; i < shift; ++i)
        setTableRow(i, m_all.at(m_windowStart + i));

    // Keep table at most WINDOW rows.
    while (m_table->rowCount() > WINDOW)
        m_table->removeRow(m_table->rowCount() - 1);

    updateRangeLabel();
    sb->setValue(qMin(sb->maximum(), oldVal + shift * rowH));
    m_shifting = false;
}

void PackagesPage::onScroll(int value)
{
    if (m_shifting || m_all.isEmpty())
        return;
    auto *sb = m_table->verticalScrollBar();
    if (value > sb->maximum() - 20)
        shiftWindowDown();
    else if (value < 20 && m_windowStart > 0)
        shiftWindowUp();
}

void PackagesPage::filterAllByQuery(const QString &q)
{
    if (q.isEmpty())
        return;
    const QString needle = q.toLower();
    QVector<PkgRow> filtered;
    filtered.reserve(m_all.size() / 4);
    for (const PkgRow &r : m_all) {
        if (r.name.toLower().contains(needle)
            || r.summaryOrVersion.toLower().contains(needle)
            || r.repo.toLower().contains(needle)) {
            filtered.append(r);
        }
    }
    m_all = std::move(filtered);
    m_windowStart = 0;
    refillTable();
}

QStringList PackagesPage::selectedPackageNames() const
{
    QStringList names;
    const auto ranges = m_table->selectedRanges();
    QSet<int> rows;
    for (const auto &r : ranges) {
        for (int i = r.topRow(); i <= r.bottomRow(); ++i)
            rows.insert(i);
    }
    for (int row : rows) {
        auto *item = m_table->item(row, 0);
        if (!item)
            continue;
        names << ProcessRunner::extractPackageName(item->text());
    }
    names.removeDuplicates();
    return names;
}

void PackagesPage::doSearch()
{
    if (m_installedOnly->isChecked()) {
        loadInstalled();
        return;
    }
    const QString q = m_search->text().trimmed();
    if (q.isEmpty()) {
        loadAvailable();
        return;
    }
    beginListJob(true);
    emit activity(QStringLiteral("Searching packages: %1").arg(q));
    m_runner->start(QStringLiteral("dnf"), {QStringLiteral("search"), q});
}

void PackagesPage::loadAvailable()
{
    beginListJob(false);
    emit activity(QStringLiteral("Listing available packages"));
    m_runner->start(QStringLiteral("dnf"), {QStringLiteral("list"), QStringLiteral("--available")});
}

void PackagesPage::loadInstalled()
{
    beginListJob(false);
    emit activity(QStringLiteral("Listing installed packages"));
    m_runner->start(QStringLiteral("dnf"), {QStringLiteral("list"), QStringLiteral("--installed")});
}

void PackagesPage::installSelected()
{
    const QStringList pkgs = selectedPackageNames();
    if (pkgs.isEmpty())
        return;
    if (m_runner->isRunning())
        return;
    m_listMode = false;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Install: %1").arg(pkgs.join(QLatin1Char(' '))));
    QStringList args = {QStringLiteral("dnf"), QStringLiteral("install"), QStringLiteral("--assumeyes")};
    args += pkgs;
    m_runner->startPkexec(args);
}

void PackagesPage::removeSelected()
{
    const QStringList pkgs = selectedPackageNames();
    if (pkgs.isEmpty())
        return;
    if (Settings::instance().confirmBeforeRemove()) {
        const auto r = QMessageBox::question(this, QStringLiteral("Remove packages"),
            QStringLiteral("Remove %1?").arg(pkgs.join(QLatin1String(", "))));
        if (r != QMessageBox::Yes)
            return;
    }
    if (m_runner->isRunning())
        return;
    m_listMode = false;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Remove: %1").arg(pkgs.join(QLatin1Char(' '))));
    QStringList args = {QStringLiteral("dnf"), QStringLiteral("remove"), QStringLiteral("--assumeyes")};
    args += pkgs;
    m_runner->startPkexec(args);
}

void PackagesPage::updateAll()
{
    if (m_runner->isRunning())
        return;
    m_listMode = false;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("dnf upgrade --assumeyes"));
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("upgrade"), QStringLiteral("--assumeyes")});
}

void PackagesPage::showInfo()
{
    const QStringList pkgs = selectedPackageNames();
    if (pkgs.isEmpty())
        return;
    if (m_runner->isRunning())
        return;
    m_listMode = false;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    QStringList args = {QStringLiteral("info")};
    args += pkgs;
    m_runner->start(QStringLiteral("dnf"), args);
}

void PackagesPage::showHistory()
{
    if (m_runner->isRunning())
        return;
    m_listMode = false;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    m_runner->start(QStringLiteral("dnf"), {QStringLiteral("history"), QStringLiteral("list"), QStringLiteral("--reverse")});
}

void PackagesPage::onOutput(const QString &text, bool isStdout)
{
    if (m_listMode) {
        if (isStdout)
            processStreamChunk(text);
        if (m_output->isExpanded())
            m_output->append(text);
        return;
    }
    m_output->append(text);
}

void PackagesPage::onFinished(int code, QProcess::ExitStatus st)
{
    if (m_listMode) {
        // Flush any trailing incomplete line + pending batch.
        if (!m_lineRemainder.trimmed().isEmpty()) {
            PkgRow row;
            const bool ok = m_searchStyle
                ? parseSearchLine(m_lineRemainder, &row)
                : parseListLine(m_lineRemainder, &row);
            if (ok)
                m_pending.append(row);
            m_lineRemainder.clear();
        }
        flushPendingRows();

        if (m_installedOnly->isChecked()) {
            const QString q = m_search->text().trimmed();
            if (!q.isEmpty())
                filterAllByQuery(q);
        }

        m_windowStart = 0;
        if (m_table->rowCount() == 0 && !m_all.isEmpty())
            refillTable();
        else
            updateRangeLabel();

        m_output->collapse();
        setBusy(false);
        m_listMode = false;
        return;
    }
    m_output->append(ProcessRunner::formatResult(code, st));
    if (code != 0 && (code == 126 || code == 127))
        m_output->append(QStringLiteral("Polkit prompt was cancelled, or the tool is missing.\n"));
    setBusy(false);
    m_listMode = false;
}
