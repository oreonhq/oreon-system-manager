// Integration tests: exercise page logic against mock binaries.
// MOCK_DIR is defined by CMake to the absolute path of tests/mocks/.
// We prepend it to PATH so QProcess picks up our scripts instead of the
// real dnf/docker when they exist on the host.

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTest>
#include "pages/containerpage.h"
#include "pages/packagepage.h"
#include "pages/repopage.h"

static void prependMockDir()
{
    const QByteArray current = qgetenv("PATH");
    qputenv("PATH", QByteArray(MOCK_DIR) + ":" + current);
}

class TestIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { prependMockDir(); }

    void packageSearch_populatesList();
    void repoLoad_populatesList();
    void dockerRefresh_populatesList();
};

void TestIntegration::packageSearch_populatesList()
{
    PackagePage page;
    page.show();

    auto *searchBar = page.findChild<QLineEdit *>();
    QVERIFY(searchBar != nullptr);

    auto buttons = page.findChildren<QPushButton *>();
    QPushButton *searchBtn = nullptr;
    for (auto *b : buttons) {
        if (b->text() == "Search") { searchBtn = b; break; }
    }
    QVERIFY(searchBtn != nullptr);

    auto *list = page.findChild<QListWidget *>();
    QVERIFY(list != nullptr);

    searchBar->setText("vim");
    searchBtn->click();

    // Allow the mock process to start and produce output.
    QVERIFY(QTest::qWaitFor([list] { return list->count() > 0; }, 3000));
    QVERIFY(list->count() > 0);
    // Mock dnf always includes "vim" in its search output.
    bool found = false;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->text().contains("vim", Qt::CaseInsensitive)) { found = true; break; }
    QVERIFY(found);
}

void TestIntegration::repoLoad_populatesList()
{
    // RepoPage calls loadRepos() in its constructor.
    RepoPage page;
    page.show();

    auto *list = page.findChild<QListWidget *>();
    QVERIFY(list != nullptr);

    QVERIFY(QTest::qWaitFor([list] { return list->count() > 0; }, 3000));
    QVERIFY(list->count() > 0);
}

void TestIntegration::dockerRefresh_populatesList()
{
    ContainerPage page;
    page.show();

    auto lists = page.findChildren<QListWidget *>();
    QVERIFY(lists.size() >= 1);
    QListWidget *dockerList = lists.at(0);

    QVERIFY(QTest::qWaitFor([dockerList] { return dockerList->count() > 0; }, 3000));
    QVERIFY(dockerList->count() > 0);
}

QTEST_MAIN(TestIntegration)
#include "test_integration.moc"
