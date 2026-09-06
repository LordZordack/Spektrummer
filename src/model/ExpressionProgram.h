#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace spektrummer::model
{
enum class ExpressionVariableSet
{
    source,
    transfer
};

enum class ExpressionErrorCode
{
    none,
    emptyExpression,
    expressionTooLong,
    tooManyNodes,
    nestingTooDeep,
    expectedExpression,
    trailingToken,
    unknownIdentifier,
    unknownFunction,
    wrongArgumentCount,
    unsupportedSyntax,
    divisionByZero,
    domainError,
    nonFiniteResult,
    resultOutOfRange,
    invalidResultRange,
    evaluationLimitExceeded,
    invalidProgram
};

struct ExpressionError
{
    ExpressionErrorCode code = ExpressionErrorCode::none;
    std::size_t offset = 0;
    std::string_view message {};
};

struct ExpressionContext
{
    double index = 0.0;
    double fundamentalHz = 0.0;
    double frequencyHz = 0.0;
    double nyquistHz = 0.0;
};

struct ExpressionResultRange
{
    double minimum = 0.0;
    double maximum = 0.0;
};

struct ExpressionEvaluationResult
{
    double value = 0.0;
    bool success = false;
    ExpressionError error {};
};

struct ExpressionCompileResult;

class ExpressionProgram final
{
public:
    ExpressionProgram() noexcept = default;

    [[nodiscard]] static ExpressionCompileResult compile (
        std::string_view source,
        ExpressionVariableSet variables) noexcept;

    [[nodiscard]] ExpressionEvaluationResult evaluate (
        const ExpressionContext& context,
        ExpressionResultRange resultRange) const noexcept;

private:
    static constexpr std::size_t maximumNodeCount = 128;
    using NodeIndex = std::uint16_t;
    static constexpr NodeIndex invalidNode = static_cast<NodeIndex> (maximumNodeCount);

    enum class NodeKind : std::uint8_t
    {
        literal,
        constantPi,
        constantE,
        variableIndex,
        variableFundamentalHz,
        variableFrequencyHz,
        variableNyquistHz,
        unaryPlus,
        unaryMinus,
        logicalNot,
        add,
        subtract,
        multiply,
        divide,
        less,
        lessEqual,
        greater,
        greaterEqual,
        equal,
        notEqual,
        logicalAnd,
        logicalOr,
        functionIf,
        functionAbs,
        functionMin,
        functionMax,
        functionClamp,
        functionPow,
        functionExp,
        functionLog,
        functionSin,
        functionCos
    };

    struct Node
    {
        NodeKind kind = NodeKind::literal;
        NodeIndex first = invalidNode;
        NodeIndex second = invalidNode;
        NodeIndex third = invalidNode;
        std::size_t offset = 0;
        double literalValue = 0.0;
    };

    class Compiler;

    [[nodiscard]] ExpressionEvaluationResult evaluateNode (
        NodeIndex nodeIndex,
        const ExpressionContext& context,
        std::size_t& remainingWork) const noexcept;

    std::array<Node, maximumNodeCount> nodes {};
    NodeIndex root = invalidNode;
    NodeIndex nodeCount = 0;
};

struct ExpressionCompileResult
{
    ExpressionProgram program {};
    bool success = false;
    ExpressionError error {};
};
}
