#include "Sidebar.h"
#include <QFrame>
#include <QListWidgetItem>
#include <QSize>

#include <QListWidget>
#include <QVBoxLayout>

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(160);
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 12, 4, 12);
    lay->setSpacing(4);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(2);
    m_list->addItem(QStringLiteral("Packages"));
    m_list->addItem(QStringLiteral("Repositories"));
    m_list->addItem(QStringLiteral("Containers"));
    m_list->addItem(QStringLiteral("Drivers"));
    m_list->setCurrentRow(0);
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        it->setSizeHint(QSize(it->sizeHint().width(), 32));
    }
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background: transparent;"
        "  outline: none;"
        "  border: none;"
        "}"
        "QListWidget::item {"
        "  padding: 6px 10px;"
        "  border-radius: 6px;"
        "  margin: 1px 0;"
        "}"
        "QListWidget::item:selected {"
        "  background: rgba(61, 174, 233, 0.22);"
        "  color: palette(text);"
        "}"
        "QListWidget::item:hover:!selected {"
        "  background: rgba(0, 0, 0, 0.06);"
        "}"
    ));
    connect(m_list, &QListWidget::currentRowChanged, this, &Sidebar::pageRequested);
    lay->addWidget(m_list, 1);
}

void Sidebar::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_list->count())
        m_list->setCurrentRow(index);
}

int Sidebar::currentIndex() const
{
    return m_list->currentRow();
}
