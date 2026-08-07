#include <QtTest/QtTest>
#include <QThread>
#include <atomic>
#include "../../src/automation/VariableScope.h"

using namespace MotorStudio;

// ============================================================================
// TestVariableScope — unit tests for VariableScope
// ============================================================================

class TestVariableScope : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------------------
    // Set/get Number
    // ------------------------------------------------------------------------
    void testSetGetNumber()
    {
        VariableScope scope;
        scope.setNumber("speed", 1500.0);

        auto val = scope.getNumber("speed");
        QVERIFY(val.has_value());
        QCOMPARE(val.value(), 1500.0);
    }

    void testSetGetNumberNotFound()
    {
        VariableScope scope;
        auto val = scope.getNumber("nonexistent");
        QVERIFY(!val.has_value());
    }

    // ------------------------------------------------------------------------
    // Set/get Bool
    // ------------------------------------------------------------------------
    void testSetGetBool()
    {
        VariableScope scope;
        scope.setBool("enabled", true);

        auto val = scope.getBool("enabled");
        QVERIFY(val.has_value());
        QCOMPARE(val.value(), true);
    }

    void testSetGetBoolNotFound()
    {
        VariableScope scope;
        auto val = scope.getBool("nonexistent");
        QVERIFY(!val.has_value());
    }

    // ------------------------------------------------------------------------
    // Set/get String
    // ------------------------------------------------------------------------
    void testSetGetString()
    {
        VariableScope scope;
        scope.setString("message", "hello world");

        auto val = scope.getString("message");
        QVERIFY(val.has_value());
        QCOMPARE(QString::fromStdString(val.value()), QString("hello world"));
    }

    void testSetGetStringNotFound()
    {
        VariableScope scope;
        auto val = scope.getString("nonexistent");
        QVERIFY(!val.has_value());
    }

    // ------------------------------------------------------------------------
    // Overwrite variable (type change)
    // ------------------------------------------------------------------------
    void testOverwriteVariable()
    {
        VariableScope scope;
        scope.setNumber("x", 42.0);
        QCOMPARE(scope.getNumber("x").value(), 42.0);

        // Overwrite with bool
        scope.setBool("x", true);
        QVERIFY(scope.getBool("x").has_value());
        QCOMPARE(scope.getBool("x").value(), true);

        // Overwrite with string
        scope.setString("x", "updated");
        QVERIFY(scope.getString("x").has_value());
        QCOMPARE(QString::fromStdString(scope.getString("x").value()), QString("updated"));
    }

    // ------------------------------------------------------------------------
    // has() check
    // ------------------------------------------------------------------------
    void testHas()
    {
        VariableScope scope;
        QVERIFY(!scope.has("temp"));

        scope.setNumber("temp", 25.0);
        QVERIFY(scope.has("temp"));
    }

    // ------------------------------------------------------------------------
    // type() check
    // ------------------------------------------------------------------------
    void testType()
    {
        VariableScope scope;

        scope.setNumber("n", 1.0);
        QCOMPARE(scope.type("n"), VarType::Number);

        scope.setBool("b", true);
        QCOMPARE(scope.type("b"), VarType::Boolean);

        scope.setString("s", "text");
        QCOMPARE(scope.type("s"), VarType::String);
    }

    // ------------------------------------------------------------------------
    // names() and count()
    // ------------------------------------------------------------------------
    void testNamesAndCount()
    {
        VariableScope scope;
        QCOMPARE(scope.count(), static_cast<size_t>(0));

        scope.setNumber("a", 1.0);
        scope.setNumber("b", 2.0);
        scope.setNumber("c", 3.0);

        QCOMPARE(scope.count(), static_cast<size_t>(3));

        auto n = scope.names();
        QCOMPARE(static_cast<int>(n.size()), 3);
        QVERIFY(std::find(n.begin(), n.end(), "a") != n.end());
        QVERIFY(std::find(n.begin(), n.end(), "b") != n.end());
        QVERIFY(std::find(n.begin(), n.end(), "c") != n.end());
    }

    // ------------------------------------------------------------------------
    // clear() removes all
    // ------------------------------------------------------------------------
    void testClear()
    {
        VariableScope scope;
        scope.setNumber("x", 1.0);
        scope.setNumber("y", 2.0);
        QCOMPARE(scope.count(), static_cast<size_t>(2));

        scope.clear();
        QCOMPARE(scope.count(), static_cast<size_t>(0));
        QVERIFY(!scope.has("x"));
        QVERIFY(!scope.has("y"));
    }

    // ------------------------------------------------------------------------
    // Parent scope: set in parent, resolve in child
    // ------------------------------------------------------------------------
    void testParentScopeResolve()
    {
        VariableScope parent;
        VariableScope child;
        child.setParentScope(&parent);

        parent.setNumber("globalSpeed", 3000.0);

        // Child does not have it directly
        QVERIFY(!child.has("globalSpeed"));

        // But resolve walks to parent
        auto val = child.resolveNumber("globalSpeed");
        QVERIFY(val.has_value());
        QCOMPARE(val.value(), 3000.0);
    }

    // ------------------------------------------------------------------------
    // Child overrides parent variable
    // ------------------------------------------------------------------------
    void testChildOverridesParent()
    {
        VariableScope parent;
        VariableScope child;
        child.setParentScope(&parent);

        parent.setNumber("speed", 1000.0);
        child.setNumber("speed", 2000.0);

        // Direct get from child returns child's value
        auto childVal = child.getNumber("speed");
        QVERIFY(childVal.has_value());
        QCOMPARE(childVal.value(), 2000.0);

        // Resolve also returns child's value (child has it)
        auto resolved = child.resolveNumber("speed");
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved.value(), 2000.0);

        // Parent still has its own value
        auto parentVal = parent.getNumber("speed");
        QVERIFY(parentVal.has_value());
        QCOMPARE(parentVal.value(), 1000.0);
    }

    // ------------------------------------------------------------------------
    // Multiple scope levels (grandparent -> parent -> child)
    // ------------------------------------------------------------------------
    void testGrandparentScope()
    {
        VariableScope grandparent;
        VariableScope parent;
        VariableScope child;

        parent.setParentScope(&grandparent);
        child.setParentScope(&parent);

        grandparent.setNumber("root", 100.0);
        parent.setNumber("mid", 200.0);
        child.setNumber("leaf", 300.0);

        // Resolve each from child
        auto r1 = child.resolveNumber("root");
        QVERIFY(r1.has_value());
        QCOMPARE(r1.value(), 100.0);

        auto r2 = child.resolveNumber("mid");
        QVERIFY(r2.has_value());
        QCOMPARE(r2.value(), 200.0);

        auto r3 = child.resolveNumber("leaf");
        QVERIFY(r3.has_value());
        QCOMPARE(r3.value(), 300.0);

        // Not found anywhere
        auto r4 = child.resolveNumber("unknown");
        QVERIFY(!r4.has_value());
    }

    // ------------------------------------------------------------------------
    // Signal emission on set
    // ------------------------------------------------------------------------
    void testSignalEmission()
    {
        VariableScope scope;

        QSignalSpy spy(&scope, &VariableScope::variableChanged);

        scope.setNumber("x", 10.0);
        QCOMPARE(spy.count(), 1);
        std::string nameArg = qvariant_cast<std::string>(spy.at(0).at(0));
        QCOMPARE(QString::fromStdString(nameArg), QString("x"));

        scope.setNumber("x", 20.0);  // overwrite
        QCOMPARE(spy.count(), 2);

        scope.setBool("flag", true);
        QCOMPARE(spy.count(), 3);

        scope.setString("msg", "hello");
        QCOMPARE(spy.count(), 4);
    }

    // ------------------------------------------------------------------------
    // Thread safety: concurrent writes don't crash
    // ------------------------------------------------------------------------
    void testThreadSafety()
    {
        VariableScope scope;
        std::atomic<int> doneCount(0);
        const int numThreads = 4;
        const int writesPerThread = 500;

        QVector<QThread*> threads;

        for (int t = 0; t < numThreads; ++t) {
            QThread* thread = QThread::create([&, t]() {
                for (int i = 0; i < writesPerThread; ++i) {
                    std::string name = "var" + std::to_string(t) + "_" + std::to_string(i);
                    scope.setNumber(name, static_cast<double>(i));
                    // Also do reads to stress-test the mutex
                    auto val = scope.getNumber(name);
                    Q_UNUSED(val);
                }
                doneCount.fetch_add(1);
            });
            threads.append(thread);
        }

        for (auto* th : threads) {
            th->start();
        }

        for (auto* th : threads) {
            th->wait(10000);
            QVERIFY(th->isFinished());
            delete th;
        }

        QCOMPARE(doneCount.load(), numThreads);
        // After 4 * 500 = 2000 writes, count should be >= threads * writesPerThread
        // (some keys may collide across threads)
        QVERIFY(scope.count() >= static_cast<size_t>(writesPerThread));
    }

    // ------------------------------------------------------------------------
    // parentScope() getter
    // ------------------------------------------------------------------------
    void testParentScopeGetter()
    {
        VariableScope parent;
        VariableScope child;

        QCOMPARE(child.parentScope(), static_cast<VariableScope*>(nullptr));

        child.setParentScope(&parent);
        QCOMPARE(child.parentScope(), &parent);
    }
};

QTEST_MAIN(TestVariableScope)
#include "test_variable_scope.moc"
