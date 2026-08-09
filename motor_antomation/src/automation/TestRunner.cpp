#include "TestRunner.h"
#include "ReportGenerator.h"

#include <QDebug>
#include <QMetaObject>
#include <QDir>

namespace MotorStudio {

struct TestRunner::Impl {
    AutomationEngine* engine = nullptr;
    QThread* workerThread = nullptr;
    bool running = false;
    TestCase currentTestCase;
    QString lastHtmlReport;
    QString lastCsvReport;
};

TestRunner::TestRunner(AutomationEngine* engine, QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>())
{
    d->engine = engine;

    // Create worker thread
    d->workerThread = new QThread(this);
    d->workerThread->setObjectName("AutomationWorker");

    // engine 可为 null（见 TestRunner(nullptr) 用例）：仅在非空时迁移线程。
    if (engine) {
        // Move engine to worker thread (engine must have no parent, or parent must be on same thread)
        engine->moveToThread(d->workerThread);

        // Connect engine's testCompleted to our slot so we can emit runnerFinished
        connect(engine, &AutomationEngine::testCompleted,
                this, &TestRunner::onEngineTestCompleted,
                Qt::QueuedConnection);
    }

    // Start the worker thread's event loop
    d->workerThread->start();
}

TestRunner::~TestRunner()
{
    // Move engine back to the calling thread so the owner can destroy it safely.
    // QObject::moveToThread must be invoked from the thread that OWNS the object
    // (the worker thread here), so we run the move-back on the engine's own thread
    // via a blocking queued call. Calling it from the destructor's thread directly
    // is a no-op + "not the object's thread" warning, and leaves the engine bound
    // to a QThread that is about to be deleted → segfault.
    if (d->engine && d->workerThread && d->workerThread->isRunning()) {
        QObject* engine = d->engine;
        QThread* destThread = QThread::currentThread();
        QMetaObject::invokeMethod(d->engine, [engine, destThread]() {
            engine->moveToThread(destThread);
        }, Qt::BlockingQueuedConnection);
    }
    // Graceful shutdown of worker thread
    if (d->workerThread) {
        d->workerThread->quit();
        d->workerThread->wait(3000);
    }
}

void TestRunner::runAsync(const TestCase& testCase)
{
    if (d->running) {
        qWarning() << "TestRunner: Test already running";
        return;
    }

    if (!d->engine) {
        qWarning() << "TestRunner: No engine set";
        // 异步投递失败结果：runAsync 的契约是"结果经信号后续送达"，
        // 同步发射会让调用方的 QSignalSpy::wait() 错过信号。
        QMetaObject::invokeMethod(this, [this]() {
            emit runnerFinished(TestResult{false, "", "No engine", -1, std::chrono::milliseconds(0), {}});
        }, Qt::QueuedConnection);
        return;
    }

    d->running = true;
    d->currentTestCase = testCase;
    d->lastHtmlReport.clear();
    d->lastCsvReport.clear();

    // Set the test case on the engine before invoking run().
    // This is safe because run() is queued and hasn't started on the worker thread yet.
    d->engine->setCurrentTestCase(testCase);

    // Invoke run() on the engine's worker thread via queued connection
    QMetaObject::invokeMethod(d->engine, "run", Qt::QueuedConnection);
}

void TestRunner::stop()
{
    if (!d->running) return;

    // Invoke stop on the engine (thread-safe via signal/slot or invokeMethod)
    QMetaObject::invokeMethod(d->engine, "stop", Qt::QueuedConnection);
}

bool TestRunner::isRunning() const
{
    return d->running;
}

QString TestRunner::lastHtmlReport() const { return d->lastHtmlReport; }
QString TestRunner::lastCsvReport() const { return d->lastCsvReport; }

void TestRunner::onEngineTestCompleted(const TestResult& result)
{
    d->running = false;

    // --- Generate reports automatically ---
    const QString reportsDir = "./reports";
    QDir().mkpath(reportsDir); // ensure directory exists (ReportGenerator also does this)

    const TestCase& tc = d->currentTestCase;

    d->lastHtmlReport = ReportGenerator::generateHtml(result, tc, reportsDir);
    if (d->lastHtmlReport.isEmpty()) {
        qWarning() << "TestRunner: Failed to generate HTML report";
    }

    d->lastCsvReport = ReportGenerator::generateCsv(result, tc, reportsDir);
    if (d->lastCsvReport.isEmpty()) {
        qWarning() << "TestRunner: Failed to generate CSV report";
    }

    // Emit runnerFinished first, then reportGenerated
    emit runnerFinished(result);
    emit reportGenerated(d->lastHtmlReport, d->lastCsvReport);
}

} // namespace MotorStudio
