#include <QtTest/QtTest>
#include "../../../src/databus/DataBus.h"
#include "../../../src/databus/Topic.h"

using namespace MotorStudio;

class TestDataBus : public QObject {
    Q_OBJECT

private:
    TopicId m_topicA = 0;
    TopicId m_topicB = 0;

private slots:
    void initTestCase()
    {
        auto& reg = TopicRegistry::instance();
        m_topicA = reg.registerTopic("TestA");
        m_topicB = reg.registerTopic("TestB");
    }

    void testPublishSingle()
    {
        auto& bus = DataBus::instance();
        bus.publish(m_topicA, 3.14f, 1000);

        auto val = bus.latestValue(m_topicA);
        QVERIFY(val.has_value());
        QCOMPARE(val.value(), 3.14f);
    }

    void testLatestValueForUnknownTopic()
    {
        auto& bus = DataBus::instance();
        auto val = bus.latestValue(99999);
        QVERIFY(!val.has_value());
    }

    void testSubscribeReceivesData()
    {
        auto& bus = DataBus::instance();
        int callCount = 0;
        float lastValue = 0;

        auto subId = bus.subscribe(m_topicA, [&](const DataPoint& dp) {
            callCount++;
            lastValue = dp.value;
        });

        bus.publish(m_topicA, 42.0f, 2000);
        QCOMPARE(callCount, 1);
        QCOMPARE(lastValue, 42.0f);

        bus.unsubscribe(subId);
    }

    void testUnsubscribeStopsReceiving()
    {
        auto& bus = DataBus::instance();
        int callCount = 0;

        auto subId = bus.subscribe(m_topicB, [&](const DataPoint&) {
            callCount++;
        });

        bus.publish(m_topicB, 1.0f);
        QCOMPARE(callCount, 1);

        bus.unsubscribe(subId);
        bus.publish(m_topicB, 2.0f);
        QCOMPARE(callCount, 1);  // Should NOT increment
    }

    void testSubscribeMultiple()
    {
        auto& bus = DataBus::instance();
        int callCount = 0;

        auto subId = bus.subscribeMultiple({m_topicA, m_topicB}, [&](const DataPoint&) {
            callCount++;
        });

        bus.publish(m_topicA, 1.0f);
        bus.publish(m_topicB, 2.0f);
        QCOMPARE(callCount, 2);

        bus.unsubscribe(subId);
    }

    void testPublishFrame()
    {
        auto& bus = DataBus::instance();
        std::vector<TopicId> ids = {m_topicA, m_topicB};
        QVector<float> values = {10.0f, 20.0f};

        bus.publishFrame(ids, values, 3000);

        QCOMPARE(bus.latestValue(m_topicA).value(), 10.0f);
        QCOMPARE(bus.latestValue(m_topicB).value(), 20.0f);
    }

    void testPublishBatch()
    {
        auto& bus = DataBus::instance();
        std::vector<DataPoint> points = {
            {m_topicA, 100.0f, 4000},
            {m_topicB, 200.0f, 4000},
        };

        bus.publishBatch(points);

        QCOMPARE(bus.latestValue(m_topicA).value(), 100.0f);
        QCOMPARE(bus.latestValue(m_topicB).value(), 200.0f);
    }

    void testSubscriberCount()
    {
        auto& bus = DataBus::instance();
        size_t before = bus.subscriberCount();

        auto subId = bus.subscribe(m_topicA, [](const DataPoint&) {});
        QCOMPARE(bus.subscriberCount(), before + 1);

        bus.unsubscribe(subId);
        QCOMPARE(bus.subscriberCount(), before);
    }
};

QTEST_MAIN(TestDataBus)
#include "test_databus.moc"
