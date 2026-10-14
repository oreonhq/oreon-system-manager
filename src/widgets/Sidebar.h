#pragma once

#include <QWidget>

class QListWidget;

class Sidebar : public QWidget
{
    Q_OBJECT
public:
    explicit Sidebar(QWidget *parent = nullptr);
    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void pageRequested(int index);

private:
    QListWidget *m_list = nullptr;
};
