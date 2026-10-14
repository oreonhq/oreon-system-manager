#include "CommandPalette.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

CommandPalette::CommandPalette(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Command palette"));
    setModal(true);
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    resize(520, 360);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Search packages, jump pages, run actions…"));
    m_list = new QListWidget(this);
    lay->addWidget(m_input);
    lay->addWidget(m_list, 1);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::filterList);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        activateCurrent();
    });
    connect(m_input, &QLineEdit::returnPressed, this, &CommandPalette::activateCurrent);
}

void CommandPalette::setActions(const QVector<PaletteAction> &actions)
{
    m_actions = actions;
    filterList(m_input->text());
}

void CommandPalette::openPalette()
{
    m_input->clear();
    filterList(QString());
    show();
    raise();
    activateWindow();
    m_input->setFocus();
}

void CommandPalette::filterList(const QString &text)
{
    m_list->clear();
    const QString q = text.trimmed().toLower();
    if (!q.isEmpty()) {
        auto *search = new QListWidgetItem(QStringLiteral("Search packages: %1").arg(text.trimmed()), m_list);
        search->setData(Qt::UserRole, QStringLiteral("search:") + text.trimmed());
        search->setToolTip(QStringLiteral("Run dnf search"));
    }
    for (const PaletteAction &a : m_actions) {
        if (!q.isEmpty()
            && !a.title.toLower().contains(q)
            && !a.subtitle.toLower().contains(q)
            && !a.id.toLower().contains(q))
            continue;
        auto *item = new QListWidgetItem(a.title, m_list);
        item->setData(Qt::UserRole, a.id);
        if (!a.subtitle.isEmpty())
            item->setToolTip(a.subtitle);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPalette::activateCurrent()
{
    auto *item = m_list->currentItem();
    QString id;
    if (item)
        id = item->data(Qt::UserRole).toString();
    else if (!m_input->text().trimmed().isEmpty())
        id = QStringLiteral("search:") + m_input->text().trimmed();
    else
        return;
    hide();
    emit actionTriggered(id);
}

void CommandPalette::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (e->key() == Qt::Key_Down) {
        m_list->setCurrentRow(qMin(m_list->currentRow() + 1, m_list->count() - 1));
        return;
    }
    if (e->key() == Qt::Key_Up) {
        m_list->setCurrentRow(qMax(m_list->currentRow() - 1, 0));
        return;
    }
    QDialog::keyPressEvent(e);
}
