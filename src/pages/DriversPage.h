#pragma once

#include <QProcess>
#include <QWidget>

class QTableWidget;
class QPushButton;
class QToolButton;
class QPlainTextEdit;
class QLabel;
class OutputPanel;
class ProcessRunner;

class DriversPage : public QWidget
{
    Q_OBJECT
public:
    explicit DriversPage(QWidget *parent = nullptr);
    void detect();

signals:
    void activity(const QString &message);

private:
    void installSelected();
    void loadModules();
    void setBusy(bool busy);
    void onOutput(const QString &text, bool isStdout);
    void onFinished(int code, QProcess::ExitStatus st);
    QString selectedPackage() const;
    void setModulesVisible(bool visible);

    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_modules = nullptr;
    QLabel *m_modulesLabel = nullptr;
    QPushButton *m_detectBtn = nullptr;
    QPushButton *m_installBtn = nullptr;
    QToolButton *m_showModulesBtn = nullptr;
    OutputPanel *m_output = nullptr;
    ProcessRunner *m_runner = nullptr;
    QString m_buffer;
    enum class Mode { Idle, Detect, Modules, Install } m_mode = Mode::Idle;
};
