#pragma once

#include <QWidget>
#include <QProcess>
#include <QVector>
#include <QString>

class QLineEdit;
class QTableWidget;
class QPushButton;
class QToolButton;
class QCheckBox;
class QLabel;
class OutputPanel;
class ProcessRunner;

class PackagesPage : public QWidget
{
    Q_OBJECT
public:
    explicit PackagesPage(QWidget *parent = nullptr);
    void focusSearch();
    void searchQuery(const QString &q);

signals:
    void activity(const QString &message);

private:
    struct PkgRow {
        QString name;
        QString summaryOrVersion;
        QString repo;
    };

    static constexpr int WINDOW = 120;
    static constexpr int SHIFT = 40;
    static constexpr int STREAM_BATCH = 80;

    void doSearch();
    void loadAvailable();
    void loadInstalled();
    void installSelected();
    void removeSelected();
    void updateAll();
    void showInfo();
    void showHistory();
    QStringList selectedPackageNames() const;
    void setBusy(bool busy);
    void onOutput(const QString &text, bool isStdout);
    void onFinished(int code, QProcess::ExitStatus st);
    void onScroll(int value);

    void clearData();
    void beginListJob(bool searchStyle);
    void processStreamChunk(const QString &text);
    void flushPendingRows();
    bool parseListLine(const QString &line, PkgRow *out) const;
    bool parseSearchLine(const QString &line, PkgRow *out) const;
    void setTableRow(int row, const PkgRow &pkg);
    void appendTableRow(const PkgRow &pkg);
    void refillTable();
    void shiftWindowDown();
    void shiftWindowUp();
    void updateRangeLabel();
    void filterAllByQuery(const QString &q);

    QLineEdit *m_search = nullptr;
    QLabel *m_rangeLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_installBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
    QToolButton *m_moreBtn = nullptr;
    QCheckBox *m_installedOnly = nullptr;
    OutputPanel *m_output = nullptr;
    ProcessRunner *m_runner = nullptr;

    QVector<PkgRow> m_all;
    QVector<PkgRow> m_pending;
    int m_windowStart = 0;
    bool m_shifting = false;
    QString m_lineRemainder;

    bool m_listMode = false;
    bool m_searchStyle = false;
};
