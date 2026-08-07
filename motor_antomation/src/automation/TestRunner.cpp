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

    // Move engine to worker thread (engine must have no parent, or parent must be on same thread)
    // Since engine was created without parent, this is safe
    engine->moveToThread(d->workerThread);

    // Connect engine's testCompleted to our slot so we can emit runnerFinished
    connect(engine, &AutomationEngine::testCompleted,
            this, &TestRunner::onEngineTestCompleted,
            Qt::QueuedConnection);

    // Start the worker thread's event loop
    d->workerThread->start();
}

TestRunner::~TestRunner()
{
    // Move engine back to current thread before quitting worker,
    // preventing "QObject destroyed in wrong thread" warnings.
    if (d->engine && d->workerThread && d->workerThread->isRunning()) {
        d->engine->moveToThread(QThread::currentThread());
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
        emit runnerFinished(TestResult{false, "", "No engine", -1, std::chrono::milliseconds(0), {}});
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
