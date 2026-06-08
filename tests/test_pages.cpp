// Smoke tests: each page must construct, show, and not crash.
// The QProcesses they spawn will silently fail (no dnf/docker on CI) —
// that is fine; we are only verifying the widget layer here.

#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QTest>
#include <QTextEdit>
#include "mainwindow.h"
#include "pages/containerpage.h"
#include "pages/driverspage.h"
#include "pages/packagepage.h"
#include "pages/repopage.h"

class TestPages : public QObject
{
    Q_OBJECT

private slots:
    void packagePageConstructs();
    void repoPageConstructs();
    void containerPageConstructs();
    void driversPageConstructs();
    void mainWindowHasFourPages();
};

void TestPages::packagePageConstructs()
{
    PackagePage page;
    page.show();
    QVERIFY(page.isVisible());
    QVERIFY(page.findChild<QListWidget *>() != nullptr);
    QVERIFY(page.findChild<QTextEdit *>()   != nullptr);
}

void TestPages::repoPageConstructs()
{
    RepoPage page;
    page.show();
    QVERIFY(page.isVisible());
    QVERIFY(page.findChild<QListWidget *>() != nullptr);
}

void TestPages::containerPageConstructs()
{
    ContainerPage page;
    page.show();
    QVERIFY(page.isVisible());
    // Two lists: one for Docker, one for Distrobox
    QCOMPARE(page.findChildren<QListWidget *>().size(), 2);
}

void TestPages::driversPageConstructs()
{
    DriversPage page;
    page.show();
    QVERIFY(page.isVisible());
    QVERIFY(page.findChild<QListWidget *>() != nullptr);
}

void TestPages::mainWindowHasFourPages()
{
    MainWindow window;
    window.show();
    QVERIFY(window.isVisible());
    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->count(), 4);
}

QTEST_MAIN(TestPages)
#include "test_pages.moc"
