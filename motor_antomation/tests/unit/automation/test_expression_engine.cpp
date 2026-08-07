#include <QtTest/QtTest>
#include "../../src/automation/ExpressionEngine.h"
#include <cmath>
#include <limits>
#include <unordered_map>

using namespace MotorStudio;

// ============================================================================
// TestExpressionEngine — comprehensive unit tests for ExpressionEngine
// ============================================================================

class TestExpressionEngine : public QObject {
    Q_OBJECT

private:
    /// Helper: build a ValueProvider backed by two simple maps.
    ValueProvider makeProvider(
        const std::unordered_map<std::string, double>& vars,
        const std::unordered_map<std::string, double>& channels) const
    {
        ValueProvider vp;
        vp.getVariable = [vars](const std::string& name) -> std::optional<double> {
            auto it = vars.find(name);
            if (it != vars.end()) return it->second;
            return std::nullopt;
        };
        vp.getChannel = [channels](const std::string& name) -> std::optional<double> {
            auto it = channels.find(name);
            if (it != channels.end()) return it->second;
            return std::nullopt;
        };
        return vp;
    }

    /// Empty provider (returns nullopt for everything).
    ValueProvider emptyProvider() const
    {
        ValueProvider vp;
        vp.getVariable = [](const std::string&) -> std::optional<double> {
            return std::nullopt;
        };
        vp.getChannel = [](const std::string&) -> std::optional<double> {
            return std::nullopt;
        };
        return vp;
    }

private slots:
    // ========================================================================
    // Number literals
    // ========================================================================
    void testIntegerLiteral()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("42", vp).value(), 42.0);
    }

    void testFloatLiteral()
    {
        auto vp = emptyProvider();
        auto result = ExpressionEngine::evaluate("3.14", vp);
        QVERIFY(result.has_value());
        QVERIFY(std::abs(result.value() - 3.14) < 1e-9);
    }

    void testNegativeNumberLiteral()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("-42", vp).value(), -42.0);
    }

    void testNegativeFloatLiteral()
    {
        auto vp = emptyProvider();
        auto result = ExpressionEngine::evaluate("-3.14", vp);
        QVERIFY(result.has_value());
        QVERIFY(std::abs(result.value() + 3.14) < 1e-9);
    }

    void testZeroLiteral()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("0", vp).value(), 0.0);
    }

    // ========================================================================
    // Simple arithmetic
    // ========================================================================
    void testAddition()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("2 + 3", vp).value(), 5.0);
    }

    void testSubtraction()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("10 - 3", vp).value(), 7.0);
    }

    void testMultiplication()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("4 * 5", vp).value(), 20.0);
    }

    void testDivision()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("10 / 2", vp).value(), 5.0);
    }

    void testModulo()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("10 % 3", vp).value(), 1.0);
    }

    void testPrecedenceAddMul()
    {
        auto vp = emptyProvider();
        // 2 + 3 * 4 = 2 + 12 = 14 (multiplication before addition)
        QCOMPARE(ExpressionEngine::evaluate("2 + 3 * 4", vp).value(), 14.0);
    }

    void testPrecedenceMulAdd()
    {
        auto vp = emptyProvider();
        // 3 * 4 + 2 = 12 + 2 = 14
        QCOMPARE(ExpressionEngine::evaluate("3 * 4 + 2", vp).value(), 14.0);
    }

    void testPrecedenceSubDiv()
    {
        auto vp = emptyProvider();
        // 10 - 4 / 2 = 10 - 2 = 8
        QCOMPARE(ExpressionEngine::evaluate("10 - 4 / 2", vp).value(), 8.0);
    }

    // ========================================================================
    // Parentheses
    // ========================================================================
    void testParenthesesOverride()
    {
        auto vp = emptyProvider();
        // (2 + 3) * 4 = 5 * 4 = 20
        QCOMPARE(ExpressionEngine::evaluate("(2 + 3) * 4", vp).value(), 20.0);
    }

    void testNestedParentheses()
    {
        auto vp = emptyProvider();
        // ((2 + 3) * (4 + 1)) / 5 = (5 * 5) / 5 = 25 / 5 = 5
        QCOMPARE(ExpressionEngine::evaluate("((2 + 3) * (4 + 1)) / 5", vp).value(), 5.0);
    }

    void testParenthesesWithUnaryMinus()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("-(1 + 2)", vp).value(), -3.0);
    }

    // ========================================================================
    // Comparison operators
    // ========================================================================
    void testGreaterThanTrue()
    {
        auto vp = emptyProvider();
        // 5 > 3 → 1.0 (true)
        QCOMPARE(ExpressionEngine::evaluate("5 > 3", vp).value(), 1.0);
    }

    void testGreaterThanFalse()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("3 > 5", vp).value(), 0.0);
    }

    void testLessThan()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("2 < 10", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("10 < 2", vp).value(), 0.0);
    }

    void testEqualToTrue()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("5 == 5", vp).value(), 1.0);
    }

    void testEqualToFalse()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("5 == 3", vp).value(), 0.0);
    }

    void testNotEqual()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("5 != 3", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("5 != 5", vp).value(), 0.0);
    }

    void testGreaterOrEqual()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("5 >= 3", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("5 >= 5", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("3 >= 5", vp).value(), 0.0);
    }

    void testLessOrEqual()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("3 <= 5", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("5 <= 5", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("5 <= 3", vp).value(), 0.0);
    }

    // ========================================================================
    // Logical operators
    // ========================================================================
    void testAndBothTrue()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("1 && 1", vp).value(), 1.0);
    }

    void testAndFalse()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("1 && 0", vp).value(), 0.0);
        QCOMPARE(ExpressionEngine::evaluate("0 && 1", vp).value(), 0.0);
        QCOMPARE(ExpressionEngine::evaluate("0 && 0", vp).value(), 0.0);
    }

    void testOrTrue()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("1 || 0", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("0 || 1", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("1 || 1", vp).value(), 1.0);
    }

    void testOrFalse()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("0 || 0", vp).value(), 0.0);
    }

    void testNotOnTrue()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("!1", vp).value(), 0.0);
    }

    void testNotOnFalse()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("!0", vp).value(), 1.0);
    }

    void testDoubleNot()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("!!1", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("!!0", vp).value(), 0.0);
    }

    void testAndOrPrecedence()
    {
        auto vp = emptyProvider();
        // && binds tighter than || :  1 || 0 && 0  =  1 || 0  =  1
        QCOMPARE(ExpressionEngine::evaluate("1 || 0 && 0", vp).value(), 1.0);
        // 0 || 1 && 0  =  0 || 0  =  0
        QCOMPARE(ExpressionEngine::evaluate("0 || 1 && 0", vp).value(), 0.0);
    }

    // ========================================================================
    // Variable references ($varName)
    // ========================================================================
    void testVariableSimple()
    {
        auto vp = makeProvider({{"x", 10.0}}, {});
        QCOMPARE(ExpressionEngine::evaluate("$x", vp).value(), 10.0);
    }

    void testVariableArithmetic()
    {
        auto vp = makeProvider({{"x", 10.0}, {"y", 5.0}}, {});
        QCOMPARE(ExpressionEngine::evaluate("$x + $y", vp).value(), 15.0);
        QCOMPARE(ExpressionEngine::evaluate("$x - $y", vp).value(), 5.0);
        QCOMPARE(ExpressionEngine::evaluate("$x * $y", vp).value(), 50.0);
        QCOMPARE(ExpressionEngine::evaluate("$x / $y", vp).value(), 2.0);
    }

    void testVariableChinese()
    {
        // Verify UTF-8 variable names work (design doc example)
        auto vp = makeProvider({
            {"温度", 85.0},
            {"转速", 1400.0}
        }, {});
        // $温度 < 85 && $转速 > 1400
        QCOMPARE(ExpressionEngine::evaluate(
            "$温度 < 85 && $转速 > 1400", vp).value(), 0.0);
        // Change values so both conditions are true
        auto vp2 = makeProvider({
            {"温度", 80.0},
            {"转速", 1500.0}
        }, {});
        QCOMPARE(ExpressionEngine::evaluate(
            "$温度 < 85 && $转速 > 1400", vp2).value(), 1.0);
    }

    void testVariableUnknown()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("$noSuchVar", vp).has_value());
    }

    void testTrailingDollar()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("$", vp).has_value());
    }

    // ========================================================================
    // Channel references (channel:name)
    // ========================================================================
    void testChannelSimple()
    {
        auto vp = makeProvider({}, {{"Ia", 5.0}});
        QCOMPARE(ExpressionEngine::evaluate("channel:Ia", vp).value(), 5.0);
    }

    void testChannelArithmetic()
    {
        auto vp = makeProvider({}, {
            {"Ia", 1.0},
            {"Ib", 2.0},
            {"Ic", 3.0}
        });
        // channel:Ia + channel:Ib + channel:Ic = 1 + 2 + 3 = 6
        QCOMPARE(ExpressionEngine::evaluate(
            "channel:Ia + channel:Ib + channel:Ic", vp).value(), 6.0);
    }

    void testChannelCompare()
    {
        auto vp = makeProvider({}, {{"Ia", 7.0}});
        // channel:Ia > 0
        QCOMPARE(ExpressionEngine::evaluate("channel:Ia > 0", vp).value(), 1.0);
        // channel:Ia < 0
        QCOMPARE(ExpressionEngine::evaluate("channel:Ia < 0", vp).value(), 0.0);
    }

    void testChannelUnknown()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("channel:NoSuchChannel", vp).has_value());
    }

    void testTrailingChannelColon()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("channel:", vp).has_value());
    }

    void testDocumentExample1()
    {
        // (channel:Ia + channel:Ib + channel:Ic) / 3 > 10
        auto vp = makeProvider({}, {
            {"Ia", 12.0},
            {"Ib", 15.0},
            {"Ic", 9.0}
        });
        // (12 + 15 + 9) / 3 = 36 / 3 = 12 > 10 → true
        QCOMPARE(ExpressionEngine::evaluate(
            "(channel:Ia + channel:Ib + channel:Ic) / 3 > 10", vp).value(), 1.0);
    }

    void testDocumentExample2()
    {
        // $avg_current >= 5.0 || $timeout
        auto vp = makeProvider({
            {"avg_current", 4.0},
            {"timeout", 0.0}
        }, {});
        // 4.0 >= 5.0 is false, 0.0 is false → false
        QCOMPARE(ExpressionEngine::evaluate(
            "$avg_current >= 5.0 || $timeout", vp).value(), 0.0);

        // Now set timeout to 1.0 → true
        auto vp2 = makeProvider({
            {"avg_current", 4.0},
            {"timeout", 1.0}
        }, {});
        QCOMPARE(ExpressionEngine::evaluate(
            "$avg_current >= 5.0 || $timeout", vp2).value(), 1.0);
    }

    // ========================================================================
    // Function calls
    // ========================================================================
    void testFuncMin()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("min(3, 5)", vp).value(), 3.0);
        QCOMPARE(ExpressionEngine::evaluate("min(-1, 0, 5)", vp).value(), -1.0);
    }

    void testFuncMax()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("max(3, 5)", vp).value(), 5.0);
        QCOMPARE(ExpressionEngine::evaluate("max(1, 2, 3)", vp).value(), 3.0);
    }

    void testFuncAbs()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("abs(-5)", vp).value(), 5.0);
        QCOMPARE(ExpressionEngine::evaluate("abs(3.14)", vp).value(), 3.14);
        QCOMPARE(ExpressionEngine::evaluate("abs(0)", vp).value(), 0.0);
    }

    void testFuncAvg()
    {
        auto vp = emptyProvider();
        // avg(1, 2, 3, 4) = 10 / 4 = 2.5
        QCOMPARE(ExpressionEngine::evaluate("avg(1,2,3,4)", vp).value(), 2.5);
    }

    void testFuncNested()
    {
        auto vp = emptyProvider();
        // max(min(1, 10), 5) = max(1, 5) = 5
        QCOMPARE(ExpressionEngine::evaluate("max(min(1, 10), 5)", vp).value(), 5.0);
    }

    void testFuncWithExpressions()
    {
        auto vp = emptyProvider();
        // min(2 + 3, 4 * 2) = min(5, 8) = 5
        QCOMPARE(ExpressionEngine::evaluate("min(2 + 3, 4 * 2)", vp).value(), 5.0);
    }

    void testFuncNoArgs()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("min()", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("avg()", vp).has_value());
    }

    void testFuncAbsWrongArgCount()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("abs(1,2)", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("abs()", vp).has_value());
    }

    void testFuncUnknown()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("unknown(1)", vp).has_value());
    }

    // ========================================================================
    // Complex expressions
    // ========================================================================
    void testComplexArithmeticChain()
    {
        auto vp = emptyProvider();
        // 1 + 2 * 3 - 4 / 2 = 1 + 6 - 2 = 5
        QCOMPARE(ExpressionEngine::evaluate("1 + 2 * 3 - 4 / 2", vp).value(), 5.0);
    }

    void testComparisonWithArithmetic()
    {
        auto vp = emptyProvider();
        // 2 + 3 == 5 → true
        QCOMPARE(ExpressionEngine::evaluate("2 + 3 == 5", vp).value(), 1.0);
        // 2 * 3 > 4 + 1 → 6 > 5 → true
        QCOMPARE(ExpressionEngine::evaluate("2 * 3 > 4 + 1", vp).value(), 1.0);
    }

    void testLogicWithComparison()
    {
        auto vp = emptyProvider();
        // (5 > 3) && (2 < 4) → true && true → true
        QCOMPARE(ExpressionEngine::evaluate("5 > 3 && 2 < 4", vp).value(), 1.0);
        QCOMPARE(ExpressionEngine::evaluate("5 > 3 && 2 > 4", vp).value(), 0.0);
    }

    void testMixedVariableChannel()
    {
        auto vp = makeProvider(
            {{"threshold", 10.0}},
            {{"Ia", 8.0}, {"Ib", 12.0}}
        );
        // channel:Ia > $threshold → 8 > 10 → false → 0.0
        QCOMPARE(ExpressionEngine::evaluate(
            "channel:Ia > $threshold", vp).value(), 0.0);
        // channel:Ib > $threshold → 12 > 10 → true → 1.0
        QCOMPARE(ExpressionEngine::evaluate(
            "channel:Ib > $threshold", vp).value(), 1.0);
    }

    // ========================================================================
    // Edge cases
    // ========================================================================
    void testEmptyString()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("", vp).has_value());
    }

    void testWhitespaceOnly()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("   \t  \n  ", vp).has_value());
    }

    void testDivisionByZero()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("5 / 0", vp).has_value());
    }

    void testModuloByZero()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("5 % 0", vp).has_value());
    }

    void testUnbalancedParens()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("(2 + 3", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("2 + 3)", vp).has_value());
    }

    void testInvalidSyntax()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("+", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("2 +", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("* 3", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("2 & 3", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("2 | 3", vp).has_value());
        QVERIFY(!ExpressionEngine::evaluate("==", vp).has_value());
    }

    void testBareIdentifier()
    {
        auto vp = emptyProvider();
        // "channel" by itself (no ':') is a bare identifier — invalid
        QVERIFY(!ExpressionEngine::evaluate("channel", vp).has_value());
    }

    void testDoubleOperator()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluate("2 ** 3", vp).has_value());
    }

    // ========================================================================
    // Parse-then-evaluate (cached AST)
    // ========================================================================
    void testParseCache()
    {
        auto ast = ExpressionEngine::parse("$x + $y");
        QVERIFY(ast != nullptr);

        auto vp1 = makeProvider({{"x", 10.0}, {"y", 20.0}}, {});
        QCOMPARE(ExpressionEngine::evaluateAst(ast.get(), vp1).value(), 30.0);

        // Re-evaluate with different values
        auto vp2 = makeProvider({{"x", 100.0}, {"y", 200.0}}, {});
        QCOMPARE(ExpressionEngine::evaluateAst(ast.get(), vp2).value(), 300.0);
    }

    void testParseRejectsInvalid()
    {
        auto ast = ExpressionEngine::parse("2 +");
        QVERIFY(ast == nullptr);

        auto ast2 = ExpressionEngine::parse(")2(");
        QVERIFY(ast2 == nullptr);
    }

    void testEvaluateAstNullNode()
    {
        auto vp = emptyProvider();
        QVERIFY(!ExpressionEngine::evaluateAst(nullptr, vp).has_value());
    }

    // ========================================================================
    // Unary minus with non-literal
    // ========================================================================
    void testUnaryMinusOnVariable()
    {
        auto vp = makeProvider({{"x", 5.0}}, {});
        QCOMPARE(ExpressionEngine::evaluate("-$x", vp).value(), -5.0);
    }

    void testUnaryMinusOnParens()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("-(3 + 2)", vp).value(), -5.0);
    }

    void testUnaryMinusOnFuncCall()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("-abs(5)", vp).value(), -5.0);
    }

    // ========================================================================
    // Large numbers and precision
    // ========================================================================
    void testLargeNumbers()
    {
        auto vp = emptyProvider();
        auto result = ExpressionEngine::evaluate("999999.999 + 0.001", vp);
        QVERIFY(result.has_value());
        QVERIFY(std::abs(result.value() - 1000000.0) < 1e-6);
    }

    void testNegativeResult()
    {
        auto vp = emptyProvider();
        QCOMPARE(ExpressionEngine::evaluate("3 - 10", vp).value(), -7.0);
    }

    // ========================================================================
    // Channel ref with underscore in name
    // ========================================================================
    void testChannelWithUnderscore()
    {
        auto vp = makeProvider({}, {{"avg_current", 7.5}});
        QCOMPARE(ExpressionEngine::evaluate("channel:avg_current", vp).value(), 7.5);
    }
};

// ============================================================================
// Qt Test main
// ============================================================================
QTEST_MAIN(TestExpressionEngine)
#include "test_expression_engine.moc"
