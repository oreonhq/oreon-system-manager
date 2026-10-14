#include "DriversPage.h"
#include "ProcessRunner.h"
#include "widgets/OutputPanel.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

DriversPage::DriversPage(QWidget *parent)
    : QWidget(parent)
{
    m_runner = new ProcessRunner(this);
    connect(m_runner, &ProcessRunner::outputReceived, this, &DriversPage::onOutput);
    connect(m_runner, &ProcessRunner::finished, this, &DriversPage::onFinished);
    connect(m_runner, &ProcessRunner::failedToStart, this, [this](const QString &e) {
        m_output->append(QStringLiteral("Failed to start: %1\n").arg(e));
        m_output->expand();
        setBusy(false);
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Drivers"), this);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 600;"));
    root->addWidget(title);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Hardware"), QStringLiteral("Suggested package")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_detectBtn = new QPushButton(QStringLiteral("Detect"), this);
    m_installBtn = new QPushButton(QStringLiteral("Install"), this);
    m_installBtn->setEnabled(false);
    m_showModulesBtn = new QToolButton(this);
    m_showModulesBtn->setText(QStringLiteral("Show modules"));
    m_showModulesBtn->setCheckable(true);
    m_showModulesBtn->setChecked(false);
    m_showModulesBtn->setAutoRaise(true);
    actions->addWidget(m_detectBtn);
    actions->addWidget(m_installBtn);
    actions->addWidget(m_showModulesBtn);
    actions->addStretch(1);
    root->addLayout(actions);

    m_modulesLabel = new QLabel(QStringLiteral("Loaded modules"), this);
    m_modulesLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px;"));
    root->addWidget(m_modulesLabel);
    m_modules = new QPlainTextEdit(this);
    m_modules->setReadOnly(true);
    m_modules->setMaximumHeight(120);
    m_modules->setFont(QFont(QStringLiteral("monospace")));
    root->addWidget(m_modules);
    setModulesVisible(false);

    m_output = new OutputPanel(this);
    root->addWidget(m_output);
    connect(m_output, &OutputPanel::cancelRequested, this, [this]() {
        m_runner->cancel();
        setBusy(false);
    });

    connect(m_detectBtn, &QPushButton::clicked, this, &DriversPage::detect);
    connect(m_installBtn, &QPushButton::clicked, this, &DriversPage::installSelected);
    connect(m_showModulesBtn, &QToolButton::toggled, this, [this](bool on) {
        setModulesVisible(on);
        if (on && m_modules->toPlainText().isEmpty())
            loadModules();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_installBtn->setEnabled(m_table->currentRow() >= 0);
    });

    detect();
}

void DriversPage::setModulesVisible(bool visible)
{
    m_modulesLabel->setVisible(visible);
    m_modules->setVisible(visible);
}

void DriversPage::setBusy(bool busy)
{
    m_detectBtn->setEnabled(!busy);
    m_showModulesBtn->setEnabled(!busy);
    m_installBtn->setEnabled(!busy && m_table->currentRow() >= 0);
    m_output->setBusy(busy);
}

QString DriversPage::selectedPackage() const
{
    const int row = m_table->currentRow();
    if (row < 0) return {};
    auto *it = m_table->item(row, 1);
    return it ? it->text() : QString();
}

void DriversPage::detect()
{
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::Detect;
    m_table->setRowCount(0);
    m_output->clear();
    m_output->collapse();
    setBusy(true);
    emit activity(QStringLiteral("Detecting hardware"));

    const QString script = QStringLiteral(
        "set -o pipefail\n"
        "lspci -nn 2>/dev/null | awk 'BEGIN{IGNORECASE=1}\n"
        "/vga|3d|display/ {\n"
        "  line=tolower($0);\n"
        "  if (line ~ /nvidia/) print \"NVIDIA graphics | akmod-nvidia\";\n"
        "  else if (line ~ /amd|ati/) print \"AMD graphics | mesa-dri-drivers\";\n"
        "  else if (line ~ /intel/) print \"Intel graphics | mesa-dri-drivers\";\n"
        "  else print \"Unknown graphics device | mesa-dri-drivers\";\n"
        "}\n"
        "/network.*(wireless|wifi)|wireless|wi-fi/ {\n"
        "  line=tolower($0);\n"
        "  if (line ~ /broadcom|bcm/) print \"Broadcom WiFi | akmod-wl\";\n"
        "  else if (line ~ /intel/) print \"Intel WiFi | iwlwifi-mvm-firmware\";\n"
        "  else if (line ~ /realtek/) print \"Realtek WiFi | NetworkManager-wifi\";\n"
        "  else print \"WiFi device | NetworkManager-wifi\";\n"
        "}\n"
        "/bluetooth/ {\n"
        "  line=tolower($0);\n"
        "  if (line ~ /intel/) print \"Intel Bluetooth | bluez\";\n"
        "  else print \"Bluetooth device | bluez\";\n"
        "}\n"
        "' | sort -u\n");
    m_runner->start(QStringLiteral("bash"), {QStringLiteral("-c"), script});
}

void DriversPage::installSelected()
{
    const QString pkg = selectedPackage();
    if (pkg.isEmpty()) return;
    m_mode = Mode::Install;
    m_output->clear();
    m_output->expand();
    setBusy(true);
    emit activity(QStringLiteral("Install driver %1").arg(pkg));
    m_runner->startPkexec({QStringLiteral("dnf"), QStringLiteral("install"),
                           QStringLiteral("--assumeyes"), pkg});
}

void DriversPage::loadModules()
{
    if (m_runner->isRunning())
        m_runner->cancel();
    m_buffer.clear();
    m_mode = Mode::Modules;
    m_modules->clear();
    setBusy(true);
    const QString script = QStringLiteral(
        "lsmod | awk 'BEGIN{IGNORECASE=1} "
        "NR==1 || /nvidia|nouveau|amdgpu|radeon|i915|iwlwifi|wl|btusb|bluetooth|snd_/ {print}' | head -n 40");
    m_runner->start(QStringLiteral("bash"), {QStringLiteral("-c"), script});
}

void DriversPage::onOutput(const QString &text, bool isStdout)
{
    if (m_mode == Mode::Modules) {
        if (isStdout)
            m_modules->appendPlainText(text.trimmed());
        else if (m_output->isExpanded())
            m_output->append(text);
        return;
    }
    if (m_mode == Mode::Detect) {
        if (isStdout)
            m_buffer += text;
        if (m_output->isExpanded())
            m_output->append(text);
        return;
    }
    m_output->append(text);
}

void DriversPage::onFinished(int code, QProcess::ExitStatus st)
{
    if (m_mode == Mode::Detect) {
        for (const QString &line : ProcessRunner::parseListOutput(m_buffer)) {
            QString hw = line;
            QString pkg;
            if (line.contains(QLatin1String(" | "))) {
                hw = line.section(QLatin1String(" | "), 0, 0).trimmed();
                pkg = line.section(QLatin1String(" | "), 1).trimmed();
            }
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(hw));
            m_table->setItem(row, 1, new QTableWidgetItem(pkg));
        }
        if (m_table->rowCount() == 0) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(QStringLiteral("No matching PCI devices found")));
            m_table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("mesa-dri-drivers")));
        }
        m_output->collapse();
        setBusy(false);
        m_mode = Mode::Idle;
        return;
    }
    if (m_mode == Mode::Modules) {
        setBusy(false);
        m_mode = Mode::Idle;
        return;
    }
    m_output->append(ProcessRunner::formatResult(code, st));
    if (code == 126 || code == 127)
        m_output->append(QStringLiteral("Polkit cancelled or tool missing.\n"));
    setBusy(false);
    m_mode = Mode::Idle;
}
