#include <QAbstractButton>
#include <QButtonGroup>
#include <QSignalSpy>
#include <QTest>
#include "widgets/sidebar.h"

class TestSidebar : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToFirstPage();
    void emitsCorrectIndexOnClick();
    void onlyOneButtonCheckedAtATime();
};

void TestSidebar::defaultsToFirstPage()
{
    Sidebar sidebar;
    auto *group = sidebar.findChild<QButtonGroup *>();
    QVERIFY(group != nullptr);
    QVERIFY(group->button(0)->isChecked());
    for (int i = 1; i < 4; ++i)
        QVERIFY(!group->button(i)->isChecked());
}

void TestSidebar::emitsCorrectIndexOnClick()
{
    Sidebar sidebar;
    auto *group = sidebar.findChild<QButtonGroup *>();
    QVERIFY(group != nullptr);

    QSignalSpy spy(&sidebar, &Sidebar::pageRequested);

    for (int i = 1; i < 4; ++i) {
        group->button(i)->click();
        QCOMPARE(spy.count(), i);
        QCOMPARE(spy.last().first().toInt(), i);
    }
    // Back to first
    group->button(0)->click();
    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.last().first().toInt(), 0);
}

void TestSidebar::onlyOneButtonCheckedAtATime()
{
    Sidebar sidebar;
    auto *group = sidebar.findChild<QButtonGroup *>();
    QVERIFY(group != nullptr);

    for (int active = 0; active < 4; ++active) {
        group->button(active)->click();
        for (int j = 0; j < 4; ++j) {
            if (j == active)
                QVERIFY(group->button(j)->isChecked());
            else
                QVERIFY(!group->button(j)->isChecked());
        }
    }
}

QTEST_MAIN(TestSidebar)
#include "test_sidebar.moc"
