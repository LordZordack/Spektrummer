#include <JuceHeader.h>

#include "TestAllocationCounter.h"
#include "model/ExpressionProgram.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
using spektrummer::model::ExpressionCompileResult;
using spektrummer::model::ExpressionContext;
using spektrummer::model::ExpressionError;
using spektrummer::model::ExpressionErrorCode;
using spektrummer::model::ExpressionEvaluationResult;
using spektrummer::model::ExpressionProgram;
using spektrummer::model::ExpressionResultRange;
using spektrummer::model::ExpressionVariableSet;
using spektrummer::test::ScopedAllocationCounter;

static_assert (std::is_same_v<
               decltype (ExpressionProgram::compile (
                   std::declval<std::string_view>(),
                   std::declval<ExpressionVariableSet>())),
               ExpressionCompileResult>);
static_assert (noexcept (ExpressionProgram::compile (
    std::declval<std::string_view>(),
    std::declval<ExpressionVariableSet>())));
static_assert (std::is_same_v<
               decltype (std::declval<const ExpressionProgram&>().evaluate (
                   std::declval<const ExpressionContext&>(),
                   std::declval<ExpressionResultRange>())),
               ExpressionEvaluationResult>);
static_assert (noexcept (std::declval<const ExpressionProgram&>().evaluate (
    std::declval<const ExpressionContext&>(),
    std::declval<ExpressionResultRange>())));
static_assert (std::is_same_v<
               decltype (std::declval<ExpressionCompileResult>().error),
               ExpressionError>);
static_assert (std::is_same_v<
               decltype (std::declval<ExpressionEvaluationResult>().error),
               ExpressionError>);

constexpr ExpressionContext sourceContext {
    .index = 3.0,
    .fundamentalHz = 110.0,
    .frequencyHz = 440.0,
    .nyquistHz = 24000.0
};

constexpr ExpressionContext transferContext {
    .index = 7.0,
    .fundamentalHz = 220.0,
    .frequencyHz = 880.0,
    .nyquistHz = 24000.0
};

constexpr ExpressionResultRange unrestrictedRange {
    .minimum = -std::numeric_limits<double>::max(),
    .maximum = std::numeric_limits<double>::max()
};

struct EvaluationObservation
{
    ExpressionEvaluationResult result;
    std::size_t allocations = 0;
};

[[nodiscard]] EvaluationObservation observeEvaluation (
    const ExpressionProgram& program,
    const ExpressionContext& context,
    ExpressionResultRange range = unrestrictedRange) noexcept
{
    ScopedAllocationCounter allocationCounter;
    const auto result = program.evaluate (context, range);
    const auto allocations = allocationCounter.stop();
    return { result, allocations };
}

class ExpressionProgramTests final : public juce::UnitTest
{
public:
    ExpressionProgramTests() : juce::UnitTest ("Expression program") {}

    void runTest() override
    {
        testPrecedenceAndOperators();
        testConstantsAndFunctions();
        testVariableSets();
        testLazyDeterministicAllocationFreeEvaluation();
        testParseFailuresAndLimits();
        testFunctionArities();
        testEvaluationFailuresAndRanges();
    }

private:
    void expectEvaluation (std::string_view expression,
                           double expected,
                           ExpressionVariableSet variables = ExpressionVariableSet::source,
                           const ExpressionContext& context = sourceContext,
                           ExpressionResultRange range = unrestrictedRange,
                           double tolerance = 1.0e-12)
    {
        const auto compiled = ExpressionProgram::compile (expression, variables);
        expect (compiled.success, std::string { expression });

        if (! compiled.success)
            return;

        expect (compiled.error.code == ExpressionErrorCode::none);
        const auto observation = observeEvaluation (compiled.program, context, range);
        expect (observation.result.success, std::string { expression });
        expectEquals (observation.allocations, std::size_t {});

        if (observation.result.success)
        {
            expect (observation.result.error.code == ExpressionErrorCode::none);
            expectWithinAbsoluteError (observation.result.value, expected, tolerance);
        }
    }

    void expectCompileFailure (std::string_view expression,
                               ExpressionVariableSet variables,
                               ExpressionErrorCode code,
                               std::size_t offset)
    {
        const auto compiled = ExpressionProgram::compile (expression, variables);
        expect (! compiled.success, std::string { expression });
        expect (compiled.error.code == code, std::string { expression });
        expectEquals (compiled.error.offset, offset, std::string { expression });
    }

    void expectEvaluationFailure (std::string_view expression,
                                  ExpressionErrorCode code,
                                  std::size_t offset,
                                  ExpressionResultRange range = unrestrictedRange)
    {
        const auto compiled = ExpressionProgram::compile (expression,
                                                          ExpressionVariableSet::source);
        expect (compiled.success, std::string { expression });

        if (! compiled.success)
            return;

        expectEvaluationFailure (compiled.program, sourceContext, code, offset, range, expression);
    }

    void expectEvaluationFailure (const ExpressionProgram& program,
                                  const ExpressionContext& context,
                                  ExpressionErrorCode code,
                                  std::size_t offset,
                                  ExpressionResultRange range,
                                  std::string_view description)
    {
        const auto observation = observeEvaluation (program, context, range);
        expect (! observation.result.success, std::string { description });
        expect (observation.result.error.code == code, std::string { description });
        expectEquals (observation.result.error.offset, offset, std::string { description });
        expectEquals (observation.allocations, std::size_t {}, std::string { description });
    }

    void testPrecedenceAndOperators()
    {
        beginTest ("The complete precedence chain is stable");
        expectEvaluation ("1 || 0 && 0", 1.0);
        expectEvaluation ("0 == 1 < 2", 0.0);
        expectEvaluation ("1 < 2 + 3 * 4", 1.0);
        expectEvaluation ("-2 * 3 + 10", 4.0);
        expectEvaluation ("(1 + 2) * 3", 9.0);

        beginTest ("Numeric literals and unary operators evaluate exactly");
        expectEvaluation ("0.5 + .25 + 1e1", 10.75);
        expectEvaluation ("+2 + -3", -1.0);
        expectEvaluation ("!0", 1.0);
        expectEvaluation ("!5", 0.0);

        beginTest ("Arithmetic operators evaluate exactly");
        expectEvaluation ("8 + 3", 11.0);
        expectEvaluation ("8 - 3", 5.0);
        expectEvaluation ("8 * 3", 24.0);
        expectEvaluation ("8 / 4", 2.0);

        beginTest ("Comparisons and equality normalize to booleans");
        for (const auto& testCase : std::array {
                 ValueCase { "1 < 2", 1.0 },
                 ValueCase { "2 < 1", 0.0 },
                 ValueCase { "2 <= 2", 1.0 },
                 ValueCase { "3 > 2", 1.0 },
                 ValueCase { "2 >= 2", 1.0 },
                 ValueCase { "2 == 2", 1.0 },
                 ValueCase { "2 != 2", 0.0 } })
            expectEvaluation (testCase.expression, testCase.expected);

        beginTest ("Logical operators normalize every truthy operand");
        expectEvaluation ("-2 && 3", 1.0);
        expectEvaluation ("0 && 3", 0.0);
        expectEvaluation ("0 || -2", 1.0);
        expectEvaluation ("0 || 0", 0.0);
    }

    void testConstantsAndFunctions()
    {
        beginTest ("Named constants have stable values");
        expectEvaluation ("pi", std::numbers::pi, ExpressionVariableSet::source,
                          sourceContext, unrestrictedRange, 1.0e-15);
        expectEvaluation ("e", std::numbers::e, ExpressionVariableSet::source,
                          sourceContext, unrestrictedRange, 1.0e-15);

        beginTest ("The exact built-in function set evaluates with fixed arities");
        for (const auto& testCase : std::array {
                 ValueCase { "if(1, 2, 3)", 2.0 },
                 ValueCase { "abs(-3)", 3.0 },
                 ValueCase { "min(4, 2)", 2.0 },
                 ValueCase { "max(4, 2)", 4.0 },
                 ValueCase { "clamp(9, 1, 5)", 5.0 },
                 ValueCase { "pow(2, 3)", 8.0 },
                 ValueCase { "exp(1)", std::numbers::e },
                 ValueCase { "log(e)", 1.0 },
                 ValueCase { "sin(pi / 2)", 1.0 },
                 ValueCase { "cos(0)", 1.0 } })
            expectEvaluation (testCase.expression, testCase.expected, ExpressionVariableSet::source,
                              sourceContext, unrestrictedRange, 1.0e-12);
    }

    void testVariableSets()
    {
        beginTest ("Source expressions expose only source variables");
        expectEvaluation ("index", 3.0, ExpressionVariableSet::source, sourceContext);
        expectEvaluation ("fundamental_hz", 110.0,
                          ExpressionVariableSet::source, sourceContext);
        expectEvaluation ("nyquist_hz", 24000.0,
                          ExpressionVariableSet::source, sourceContext);
        expectCompileFailure ("frequency_hz", ExpressionVariableSet::source,
                              ExpressionErrorCode::unknownIdentifier, 0);

        beginTest ("Transfer expressions expose only transfer variables");
        expectEvaluation ("frequency_hz", 880.0,
                          ExpressionVariableSet::transfer, transferContext);
        expectEvaluation ("nyquist_hz", 24000.0,
                          ExpressionVariableSet::transfer, transferContext);
        expectCompileFailure ("index", ExpressionVariableSet::transfer,
                              ExpressionErrorCode::unknownIdentifier, 0);
        expectCompileFailure ("fundamental_hz", ExpressionVariableSet::transfer,
                              ExpressionErrorCode::unknownIdentifier, 0);
        expectCompileFailure ("unknown", ExpressionVariableSet::source,
                              ExpressionErrorCode::unknownIdentifier, 0);
    }

    void testLazyDeterministicAllocationFreeEvaluation()
    {
        beginTest ("if and logical operators do not evaluate inactive branches");
        for (const auto& testCase : std::array {
                 ValueCase { "if(0, 1 / 0, 42)", 42.0 },
                 ValueCase { "if(1, 42, 1 / 0)", 42.0 },
                 ValueCase { "0 && (1 / 0)", 0.0 },
                 ValueCase { "1 || (1 / 0)", 1.0 } })
            expectEvaluation (testCase.expression, testCase.expected);

        beginTest ("Repeated evaluation is deterministic and allocation-free");
        const auto compiled = ExpressionProgram::compile (
            "sin(index) + fundamental_hz / nyquist_hz",
            ExpressionVariableSet::source);
        expect (compiled.success);

        if (! compiled.success)
            return;

        std::array<double, 16> values {};
        std::array<std::size_t, 16> allocations {};

        for (auto index = std::size_t {}; index < values.size(); ++index)
        {
            const auto observation = observeEvaluation (compiled.program, sourceContext);
            expect (observation.result.success);
            values[index] = observation.result.value;
            allocations[index] = observation.allocations;
        }

        for (auto index = std::size_t {}; index < values.size(); ++index)
        {
            expectEquals (values[index], values.front());
            expectEquals (allocations[index], std::size_t {});
        }
    }

    void testParseFailuresAndLimits()
    {
        beginTest ("Malformed input reports stable codes and zero-based offsets");
        expectCompileFailure ("", ExpressionVariableSet::source,
                              ExpressionErrorCode::emptyExpression, 0);
        expectCompileFailure ("1 +", ExpressionVariableSet::source,
                              ExpressionErrorCode::expectedExpression, 3);
        expectCompileFailure ("1 + * 2", ExpressionVariableSet::source,
                              ExpressionErrorCode::expectedExpression, 4);
        expectCompileFailure ("1 2", ExpressionVariableSet::source,
                              ExpressionErrorCode::trailingToken, 2);
        expectCompileFailure ("sqrt(4)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unknownFunction, 0);
        expectCompileFailure ("index = 1", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 6);
        expectCompileFailure ("1 ? 2 : 3", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 2);
        expectCompileFailure ("(index = 1)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 7);
        expectCompileFailure ("(1 ? 2 : 3)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 3);
        expectCompileFailure ("(1 & 2)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 3);
        expectCompileFailure ("(1 | 2)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 3);
        expectCompileFailure ("abs(1 & 2)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 6);
        expectCompileFailure ("abs(1 | 2)", ExpressionVariableSet::source,
                              ExpressionErrorCode::unsupportedSyntax, 6);

        beginTest ("The source length limit accepts 512 and rejects 513 characters");
        const std::string maximumLengthExpression (512, ' ');
        auto acceptedLength = maximumLengthExpression;
        acceptedLength.front() = '1';
        expect (ExpressionProgram::compile (acceptedLength,
                                            ExpressionVariableSet::source).success);

        auto rejectedLength = acceptedLength;
        rejectedLength.push_back (' ');
        expectCompileFailure (rejectedLength, ExpressionVariableSet::source,
                              ExpressionErrorCode::expressionTooLong, 512);

        beginTest ("The node limit accepts 128 and rejects 129 parsed nodes");
        const std::string maximumNodeExpression = std::string (127, '-') + "1";
        expect (ExpressionProgram::compile (maximumNodeExpression,
                                            ExpressionVariableSet::source).success);
        const std::string excessiveNodeExpression = std::string (128, '-') + "1";
        expectCompileFailure (excessiveNodeExpression, ExpressionVariableSet::source,
                              ExpressionErrorCode::tooManyNodes, 128);

        beginTest ("The nesting limit accepts 128 and rejects 129 parenthesis groups");
        const std::string maximumNestingExpression = std::string (128, '(') + "1"
                                                   + std::string (128, ')');
        expect (ExpressionProgram::compile (maximumNestingExpression,
                                            ExpressionVariableSet::source).success);
        const std::string excessiveNestingExpression = std::string (129, '(') + "1"
                                                     + std::string (129, ')');
        expectCompileFailure (excessiveNestingExpression, ExpressionVariableSet::source,
                              ExpressionErrorCode::nestingTooDeep, 128);
    }

    void testFunctionArities()
    {
        beginTest ("Every built-in function rejects a non-exact arity");
        for (const auto expression : std::array<std::string_view, 20> {
                 "if(1, 2)", "if(1, 2, 3, 4)",
                 "abs()", "abs(1, 2)",
                 "min(1)", "min(1, 2, 3)",
                 "max(1)", "max(1, 2, 3)",
                 "clamp(1, 2)", "clamp(1, 2, 3, 4)",
                 "pow(2)", "pow(2, 3, 4)",
                 "exp()", "exp(1, 2)",
                 "log()", "log(1, 2)",
                 "sin()", "sin(1, 2)",
                 "cos()", "cos(1, 2)" })
            expectCompileFailure (expression, ExpressionVariableSet::source,
                                  ExpressionErrorCode::wrongArgumentCount, 0);
    }

    void testEvaluationFailuresAndRanges()
    {
        beginTest ("Evaluation failures have stable codes, offsets, and zero allocations");
        expectEvaluationFailure ("1 / 0", ExpressionErrorCode::divisionByZero, 2);
        expectEvaluationFailure ("1 / -0", ExpressionErrorCode::divisionByZero, 2);
        expectEvaluationFailure ("log(0)", ExpressionErrorCode::domainError, 0);
        expectEvaluationFailure ("log(-1)", ExpressionErrorCode::domainError, 0);
        expectEvaluationFailure ("pow(-1, 0.5)", ExpressionErrorCode::domainError, 0);
        expectEvaluationFailure ("pow(0, -1)", ExpressionErrorCode::domainError, 0);
        expectEvaluationFailure ("clamp(1, 2, 1)", ExpressionErrorCode::domainError, 0);
        expectEvaluationFailure ("exp(10000)", ExpressionErrorCode::nonFiniteResult, 0);
        expectEvaluationFailure ("1e308 * 1e308", ExpressionErrorCode::nonFiniteResult, 6);

        constexpr ExpressionResultRange unitRange { .minimum = 0.0, .maximum = 1.0 };
        expectEvaluationFailure ("-0.01", ExpressionErrorCode::resultOutOfRange, 0, unitRange);
        expectEvaluationFailure ("1.01", ExpressionErrorCode::resultOutOfRange, 0, unitRange);

        beginTest ("Caller-supplied result ranges include both endpoints");
        expectEvaluation ("0", 0.0, ExpressionVariableSet::source,
                          sourceContext, unitRange);
        expectEvaluation ("1", 1.0, ExpressionVariableSet::source,
                          sourceContext, unitRange);

        beginTest ("Invalid caller-supplied result ranges fail without allocating");
        const auto constantProgram = ExpressionProgram::compile ("0", ExpressionVariableSet::source);
        expect (constantProgram.success);
        if (constantProgram.success)
        {
            constexpr auto negativeInfinity = -std::numeric_limits<double>::infinity();
            constexpr auto positiveInfinity = std::numeric_limits<double>::infinity();
            constexpr auto quietNaN = std::numeric_limits<double>::quiet_NaN();

            for (const auto range : std::array {
                     ExpressionResultRange { .minimum = 1.0, .maximum = 0.0 },
                     ExpressionResultRange { .minimum = quietNaN, .maximum = 1.0 },
                     ExpressionResultRange { .minimum = negativeInfinity, .maximum = 1.0 },
                     ExpressionResultRange { .minimum = positiveInfinity, .maximum = 1.0 },
                     ExpressionResultRange { .minimum = 0.0, .maximum = quietNaN },
                     ExpressionResultRange { .minimum = 0.0, .maximum = negativeInfinity },
                     ExpressionResultRange { .minimum = 0.0, .maximum = positiveInfinity } })
                expectEvaluationFailure (constantProgram.program, sourceContext,
                                         ExpressionErrorCode::invalidResultRange, 0, range,
                                         "invalid result range");
        }

        beginTest ("Non-finite evaluated context variables fail without allocating");
        struct VariableCase
        {
            std::string_view expression;
            ExpressionVariableSet variables;
            ExpressionContext context;
            double ExpressionContext::* member;
        };

        for (const auto& variableCase : std::array {
                 VariableCase { "index", ExpressionVariableSet::source,
                                sourceContext, &ExpressionContext::index },
                 VariableCase { "fundamental_hz", ExpressionVariableSet::source,
                                sourceContext, &ExpressionContext::fundamentalHz },
                 VariableCase { "frequency_hz", ExpressionVariableSet::transfer,
                                transferContext, &ExpressionContext::frequencyHz },
                 VariableCase { "nyquist_hz", ExpressionVariableSet::transfer,
                                transferContext, &ExpressionContext::nyquistHz } })
        {
            const auto compiled = ExpressionProgram::compile (variableCase.expression,
                                                              variableCase.variables);
            expect (compiled.success, std::string { variableCase.expression });
            if (! compiled.success)
                continue;

            for (const auto nonFinite : std::array {
                     std::numeric_limits<double>::quiet_NaN(),
                     -std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity() })
            {
                auto context = variableCase.context;
                context.*variableCase.member = nonFinite;
                expectEvaluationFailure (compiled.program, context,
                                         ExpressionErrorCode::nonFiniteResult, 0,
                                         unrestrictedRange, variableCase.expression);
            }
        }

        beginTest ("A default expression program fails without allocating");
        expectEvaluationFailure (ExpressionProgram {}, sourceContext,
                                 ExpressionErrorCode::invalidProgram, 0,
                                 unrestrictedRange, "default expression program");
    }

    struct ValueCase
    {
        std::string_view expression;
        double expected = 0.0;
    };
};

ExpressionProgramTests expressionProgramTests;
}
