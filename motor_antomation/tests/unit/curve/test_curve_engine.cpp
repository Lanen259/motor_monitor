#include <QtTest/QtTest>
#include "../../src/curve/CurveEngine.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace MotorStudio;

class TestCurveEngine : public QObject {
    Q_OBJECT

private:
    CurveEngine* m_engine = nullptr;
    uint32_t m_topic1 = 1;
    uint32_t m_topic2 = 2;

private slots:
    void initTestCase()
    {
        m_engine = new CurveEngine();
    }

    void cleanupTestCase()
    {
        delete m_engine;
    }

    void testAddChannel()
    {
        m_engine->addChannel(m_topic1, 1000);
        QVERIFY(m_engine->hasChannel(m_topic1));
        QCOMPARE(m_engine->channelCount(), size_t(1));
    }

    void testAddDuplicateChannel()
    {
        size_t before = m_engine->channelCount();
        m_engine->addChannel(m_topic1, 500);  // Should not duplicate
        QCOMPARE(m_engine->channelCount(), before);
    }

    void testAppendAndRead()
    {
        m_engine->addChannel(m_topic2, 10000);

        for (int i = 0; i < 100; ++i) {
            m_engine->append(m_topic2, i * 1000ULL, static_cast<float>(i));
        }

        auto* ch = m_engine->channel(m_topic2);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->count(), size_t(100));

        auto all = ch->allPoints();
        QCOMPARE(all.size(), size_t(100));
        QCOMPARE(all[0].first, 0ULL);
        QCOMPARE(all[0].second, 0.0f);
        QCOMPARE(all[99].first, 99000ULL);
        QCOMPARE(all[99].second, 99.0f);
    }

    void testDataRange()
    {
        auto range = m_engine->dataRange(m_topic2);
        QCOMPARE(range.minVal, 0.0f);
        QCOMPARE(range.maxVal, 99.0f);
    }

    void testChannelIds()
    {
        auto ids = m_engine->channelIds();
        QVERIFY(ids.size() >= 2);
    }

    void testDownsample()
    {
        auto result = m_engine->downsample(m_topic2, 10);
        QVERIFY(result.size() <= 10);
        QVERIFY(result.size() >= 2);  // LTTB always returns first+last+intermediate
    }

    void testDownsampleInsufficientPoints()
    {
        // Should return all points when target > data count
        auto result = m_engine->downsample(m_topic2, 200);
        QCOMPARE(result.size(), size_t(100));
    }

    void testRemoveChannel()
    {
        m_engine->addChannel(999, 100);
        QVERIFY(m_engine->hasChannel(999));
        m_engine->removeChannel(999);
        QVERIFY(!m_engine->hasChannel(999));
    }

    void testAppendViaDataPoint()
    {
        m_engine->addChannel(888, 1000);
        DataPoint dp(888, 3.14f, 5000);
        m_engine->append(dp);

        auto* ch = m_engine->channel(888);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->count(), size_t(1));
        QCOMPARE(ch->allPoints()[0].second, 3.14f);
    }

    void testRingBufferWrapAround()
    {
        uint32_t tid = 777;
        m_engine->addChannel(tid, 10);  // Small capacity

        // Write 20 values, capacity is 10 → should wrap
        for (int i = 0; i < 20; ++i) {
            m_engine->append(tid, i * 1000ULL, static_cast<float>(i));
        }

        auto* ch = m_engine->channel(tid);
        QCOMPARE(ch->count(), size_t(10));  // Only 10 retained

        auto all = ch->allPoints();
        QCOMPARE(all.size(), size_t(10));
        // Oldest should be index 10 (wrapped)
        QCOMPARE(all[0].second, 10.0f);
        QCOMPARE(all[9].second, 19.0f);
    }

    // WI-010: Verify LTTB downsample on large datasets
    void testLTTBDownsampleLarge()
    {
        uint32_t tid = 1001;
        // Use default 100000-point capacity
        m_engine->addChannel(tid);

        // Write 50000 sinusoidal points at 1kHz-equivalent timestamps
        const size_t N = 50000;
        for (size_t i = 0; i < N; ++i) {
            uint64_t ts = i * 1000ULL;  // 1us interval → 1kHz
            float val = std::sin(static_cast<double>(i) * 0.001) * 10.0f;
            m_engine->append(tid, ts, val);
        }

        auto* ch = m_engine->channel(tid);
        QCOMPARE(ch->count(), N);

        // Downsample to 800 points (typical widget pixel width)
        size_t target = 800;
        auto result = m_engine->downsample(tid, target);
        QVERIFY(result.size() <= target);
        QVERIFY(result.size() >= 2);

        // First and last preserved
        QCOMPARE(result.front().second, ch->allPoints().front().second);
        QCOMPARE(result.back().second, ch->allPoints().back().second);

        // All values within original range
        for (const auto& p : result) {
            QVERIFY(p.second >= -10.0f && p.second <= 10.0f);
        }

        // Timestamps must be non-decreasing
        uint64_t prevTs = 0;
        for (const auto& p : result) {
            QVERIFY(p.first >= prevTs);
            prevTs = p.first;
        }
    }

    // WI-010: Verify setCapacity resize works and preserves data
    void testSetCapacity()
    {
        uint32_t tid = 2001;
        m_engine->addChannel(tid, 100);

        // Write 50 points
        for (int i = 0; i < 50; ++i) {
            m_engine->append(tid, i * 1000ULL, static_cast<float>(i));
        }

        auto* ch = m_engine->channel(tid);
        QCOMPARE(ch->count(), size_t(50));
        QCOMPARE(ch->capacity(), size_t(100));

        // Shrink capacity: keep only newest 20 points
        m_engine->setCapacity(tid, 20);
        QCOMPARE(ch->capacity(), size_t(20));
        QCOMPARE(ch->count(), size_t(20));
        auto pts = ch->allPoints();
        // Newest 20 points: 30..49
        QCOMPARE(pts.front().second, 30.0f);
        QCOMPARE(pts.back().second, 49.0f);

        // Expand capacity: data should stay the same
        m_engine->setCapacity(tid, 200);
        QCOMPARE(ch->capacity(), size_t(200));
        QCOMPARE(ch->count(), size_t(20));
        auto pts2 = ch->allPoints();
        QCOMPARE(pts2.front().second, 30.0f);
        QCOMPARE(pts2.back().second, 49.0f);

        // setCapacity(0) is a no-op
        m_engine->setCapacity(tid, 0);
        QCOMPARE(ch->capacity(), size_t(200));
    }

    // WI-010: Verify multi-threaded append does not corrupt data
    void testMultiThreadedAppend()
    {
        uint32_t tid = 3001;
        m_engine->addChannel(tid, 50000);

        const int writersPerThread = 10000;
        const int numThreads = 4;
        std::atomic<int> readyCount{0};
        std::atomic<bool> startFlag{false};

        auto writer = [&](int threadId) {
            readyCount++;
            while (!startFlag.load()) {
                // spin-wait
            }
            float base = static_cast<float>(threadId * writersPerThread);
            for (int i = 0; i < writersPerThread; ++i) {
                // Use threadId to avoid timestamp collisions
                uint64_t ts = static_cast<uint64_t>(threadId) * 1000000000ULL + i;
                m_engine->append(tid, ts, base + static_cast<float>(i));
            }
        };

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back(writer, t);
        }

        // Wait for all threads ready, then start
        while (readyCount.load() < numThreads) {}
        startFlag.store(true);

        for (auto& t : threads) {
            t.join();
        }

        auto* ch = m_engine->channel(tid);
        QCOMPARE(ch->totalWritten(), size_t(writersPerThread * numThreads));

        // Verify all points are readable and valid
        auto all = ch->allPoints();
        QCOMPARE(all.size(), ch->count());
        QVERIFY(all.size() > 0);
    }
};

QTEST_MAIN(TestCurveEngine)
#include "test_curve_engine.moc"
