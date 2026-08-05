#include <QtTest>
#include <QCoreApplication>

// 测试骨架
class CoreTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void testPlaceholder() { QVERIFY(true); }
};

QTEST_MAIN(CoreTest)
#include "test_main.moc"