#pragma once

#include <QDialog>
#include <QString>
#include <QVector>
#include <QKeyEvent>

class QLineEdit;
class QListWidget;

struct PaletteAction {
    QString id;
    QString title;
    QString subtitle;
};

class CommandPalette : public QDialog
{
    Q_OBJECT
public:
    explicit CommandPalette(QWidget *parent = nullptr);
    void setActions(const QVector<PaletteAction> &actions);
    void openPalette();

signals:
    void actionTriggered(const QString &id);

private:
    void filterList(const QString &text);
    void activateCurrent();

protected:
    void keyPressEvent(QKeyEvent *e) override;

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QVector<PaletteAction> m_actions;
};
