#include "ExpressionProgram.h"

#include <charconv>
#include <cmath>
#include <numbers>
#include <system_error>

namespace spektrummer::model
{
namespace
{
constexpr std::size_t maximumSourceLength = 512;

[[nodiscard]] constexpr std::string_view messageFor (ExpressionErrorCode code) noexcept
{
    switch (code)
    {
        case ExpressionErrorCode::none: return {};
        case ExpressionErrorCode::emptyExpression: return "expression is empty";
        case ExpressionErrorCode::expressionTooLong: return "expression is too long";
        case ExpressionErrorCode::tooManyNodes: return "expression has too many nodes";
        case ExpressionErrorCode::nestingTooDeep: return "expression nesting is too deep";
        case ExpressionErrorCode::expectedExpression: return "expected expression";
        case ExpressionErrorCode::trailingToken: return "unexpected trailing token";
        case ExpressionErrorCode::unknownIdentifier: return "unknown identifier";
        case ExpressionErrorCode::unknownFunction: return "unknown function";
        case ExpressionErrorCode::wrongArgumentCount: return "wrong argument count";
        case ExpressionErrorCode::unsupportedSyntax: return "unsupported syntax";
        case ExpressionErrorCode::divisionByZero: return "division by zero";
        case ExpressionErrorCode::domainError: return "function domain error";
        case ExpressionErrorCode::nonFiniteResult: return "result is not finite";
        case ExpressionErrorCode::resultOutOfRange: return "result is out of range";
        case ExpressionErrorCode::invalidResultRange: return "result range is invalid";
        case ExpressionErrorCode::evaluationLimitExceeded: return "evaluation limit exceeded";
        case ExpressionErrorCode::invalidProgram: return "expression program is invalid";
    }

    return "expression error";
}

[[nodiscard]] constexpr ExpressionError makeError (ExpressionErrorCode code,
                                                    std::size_t offset) noexcept
{
    return { code, offset, messageFor (code) };
}

[[nodiscard]] constexpr ExpressionEvaluationResult evaluationSuccess (double value) noexcept
{
    return { value, true, {} };
}

[[nodiscard]] constexpr ExpressionEvaluationResult evaluationFailure (
    ExpressionErrorCode code,
    std::size_t offset) noexcept
{
    return { 0.0, false, makeError (code, offset) };
}

[[nodiscard]] constexpr bool isAsciiWhitespace (char character) noexcept
{
    return character == ' ' || character == '\t' || character == '\n'
        || character == '\r' || character == '\f' || character == '\v';
}

[[nodiscard]] constexpr bool isAsciiDigit (char character) noexcept
{
    return character >= '0' && character <= '9';
}

[[nodiscard]] constexpr bool isIdentifierStart (char character) noexcept
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z') || character == '_';
}

[[nodiscard]] constexpr bool isIdentifierContinuation (char character) noexcept
{
    return isIdentifierStart (character) || isAsciiDigit (character);
}

[[nodiscard]] constexpr bool isUnsupportedSyntaxToken (char character) noexcept
{
    return character == '=' || character == '?' || character == ':'
        || character == '&' || character == '|';
}
}

class ExpressionProgram::Compiler final
{
public:
    Compiler (std::string_view sourceToParse,
              ExpressionVariableSet variableSetToUse,
              ExpressionProgram& destination) noexcept
        : source (sourceToParse), variableSet (variableSetToUse), program (destination)
    {
    }

    [[nodiscard]] bool parse() noexcept
    {
        skipWhitespace();

        if (position == source.size())
        {
            fail (ExpressionErrorCode::emptyExpression, 0);
            return false;
        }

        const auto parsedRoot = parseLogicalOr();

        if (error.code != ExpressionErrorCode::none)
            return false;

        skipWhitespace();

        if (position != source.size())
        {
            const auto character = source[position];
            fail (isUnsupportedSyntaxToken (character)
                      ? ExpressionErrorCode::unsupportedSyntax
                      : ExpressionErrorCode::trailingToken,
                  position);
            return false;
        }

        program.root = parsedRoot;
        return true;
    }

    [[nodiscard]] ExpressionError getError() const noexcept
    {
        return error;
    }

private:
    static constexpr std::size_t maximumNestingDepth = 128;

    struct FunctionDescription
    {
        NodeKind kind = NodeKind::functionAbs;
        std::size_t arity = 0;
        bool found = false;
    };

    void skipWhitespace() noexcept
    {
        while (position < source.size() && isAsciiWhitespace (source[position]))
            ++position;
    }

    [[nodiscard]] bool consume (std::string_view token) noexcept
    {
        if (source.substr (position, token.size()) != token)
            return false;

        position += token.size();
        return true;
    }

    void fail (ExpressionErrorCode code, std::size_t offset) noexcept
    {
        if (error.code == ExpressionErrorCode::none)
            error = makeError (code, offset);
    }

    [[nodiscard]] NodeIndex addNode (NodeKind kind, std::size_t offset) noexcept
    {
        if (program.nodeCount >= maximumNodeCount)
        {
            fail (ExpressionErrorCode::tooManyNodes, offset);
            return invalidNode;
        }

        const auto index = program.nodeCount++;
        program.nodes[index].kind = kind;
        program.nodes[index].offset = offset;
        return index;
    }

    [[nodiscard]] NodeIndex parseLogicalOr() noexcept
    {
        auto left = parseLogicalAnd();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;

            if (! consume ("||"))
                break;

            const auto operation = addNode (NodeKind::logicalOr, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseLogicalAnd();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseLogicalAnd() noexcept
    {
        auto left = parseEquality();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;

            if (! consume ("&&"))
                break;

            const auto operation = addNode (NodeKind::logicalAnd, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseEquality();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseEquality() noexcept
    {
        auto left = parseComparison();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;
            auto kind = NodeKind::equal;

            if (consume ("=="))
                kind = NodeKind::equal;
            else if (consume ("!="))
                kind = NodeKind::notEqual;
            else
                break;

            const auto operation = addNode (kind, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseComparison();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseComparison() noexcept
    {
        auto left = parseAdditive();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;
            auto kind = NodeKind::less;

            if (consume ("<="))
                kind = NodeKind::lessEqual;
            else if (consume (">="))
                kind = NodeKind::greaterEqual;
            else if (consume ("<"))
                kind = NodeKind::less;
            else if (consume (">"))
                kind = NodeKind::greater;
            else
                break;

            const auto operation = addNode (kind, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseAdditive();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseAdditive() noexcept
    {
        auto left = parseMultiplicative();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;
            auto kind = NodeKind::add;

            if (consume ("+"))
                kind = NodeKind::add;
            else if (consume ("-"))
                kind = NodeKind::subtract;
            else
                break;

            const auto operation = addNode (kind, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseMultiplicative();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseMultiplicative() noexcept
    {
        auto left = parseUnary();

        while (error.code == ExpressionErrorCode::none)
        {
            skipWhitespace();
            const auto operatorOffset = position;
            auto kind = NodeKind::multiply;

            if (consume ("*"))
                kind = NodeKind::multiply;
            else if (consume ("/"))
                kind = NodeKind::divide;
            else
                break;

            const auto operation = addNode (kind, operatorOffset);

            if (operation == invalidNode)
                return invalidNode;

            program.nodes[operation].first = left;
            program.nodes[operation].second = parseUnary();
            left = operation;
        }

        return left;
    }

    [[nodiscard]] NodeIndex parseUnary() noexcept
    {
        skipWhitespace();
        const auto operatorOffset = position;
        auto kind = NodeKind::unaryPlus;
        bool hasOperator = true;

        if (consume ("+"))
            kind = NodeKind::unaryPlus;
        else if (consume ("-"))
            kind = NodeKind::unaryMinus;
        else if (consume ("!"))
            kind = NodeKind::logicalNot;
        else
            hasOperator = false;

        if (! hasOperator)
            return parsePrimary();

        const auto operation = addNode (kind, operatorOffset);

        if (operation == invalidNode)
            return invalidNode;

        program.nodes[operation].first = parseUnary();
        return operation;
    }

    [[nodiscard]] NodeIndex parsePrimary() noexcept
    {
        skipWhitespace();

        if (position == source.size())
        {
            fail (ExpressionErrorCode::expectedExpression, position);
            return invalidNode;
        }

        if (isAsciiDigit (source[position])
            || (source[position] == '.' && position + 1 < source.size()
                && isAsciiDigit (source[position + 1])))
            return parseNumber();

        if (isIdentifierStart (source[position]))
            return parseIdentifier();

        if (source[position] == '(')
        {
            const auto openingOffset = position;

            if (nestingDepth >= maximumNestingDepth)
            {
                fail (ExpressionErrorCode::nestingTooDeep, openingOffset);
                return invalidNode;
            }

            ++position;
            ++nestingDepth;
            const auto expression = parseLogicalOr();
            --nestingDepth;

            if (error.code != ExpressionErrorCode::none)
                return invalidNode;

            skipWhitespace();

            if (! consume (")"))
            {
                fail (position < source.size() && isUnsupportedSyntaxToken (source[position])
                          ? ExpressionErrorCode::unsupportedSyntax
                          : ExpressionErrorCode::expectedExpression,
                      position);
                return invalidNode;
            }

            return expression;
        }

        fail (ExpressionErrorCode::expectedExpression, position);
        return invalidNode;
    }

    [[nodiscard]] NodeIndex parseNumber() noexcept
    {
        const auto start = position;

        while (position < source.size() && isAsciiDigit (source[position]))
            ++position;

        if (position < source.size() && source[position] == '.')
        {
            ++position;

            while (position < source.size() && isAsciiDigit (source[position]))
                ++position;
        }

        if (position < source.size() && (source[position] == 'e' || source[position] == 'E'))
        {
            const auto exponentMarker = position++;

            if (position < source.size() && (source[position] == '+' || source[position] == '-'))
                ++position;

            const auto exponentDigits = position;

            while (position < source.size() && isAsciiDigit (source[position]))
                ++position;

            if (position == exponentDigits)
                position = exponentMarker;
        }

        double value = 0.0;
        const auto* first = source.data() + start;
        const auto* last = source.data() + position;
        const auto conversion = std::from_chars (first, last, value, std::chars_format::general);

        if (conversion.ec == std::errc::result_out_of_range)
        {
            fail (ExpressionErrorCode::nonFiniteResult, start);
            return invalidNode;
        }

        if (conversion.ec != std::errc {} || conversion.ptr != last || ! std::isfinite (value))
        {
            fail (ExpressionErrorCode::expectedExpression, start);
            return invalidNode;
        }

        const auto literal = addNode (NodeKind::literal, start);

        if (literal != invalidNode)
            program.nodes[literal].literalValue = value;

        return literal;
    }

    [[nodiscard]] NodeIndex parseIdentifier() noexcept
    {
        const auto start = position++;

        while (position < source.size() && isIdentifierContinuation (source[position]))
            ++position;

        const auto identifier = source.substr (start, position - start);
        skipWhitespace();

        if (position < source.size() && source[position] == '(')
            return parseFunction (identifier, start);

        if (identifier == "pi")
            return addNode (NodeKind::constantPi, start);

        if (identifier == "e")
            return addNode (NodeKind::constantE, start);

        if (identifier == "nyquist_hz")
            return addNode (NodeKind::variableNyquistHz, start);

        if (variableSet == ExpressionVariableSet::source)
        {
            if (identifier == "index")
                return addNode (NodeKind::variableIndex, start);

            if (identifier == "fundamental_hz")
                return addNode (NodeKind::variableFundamentalHz, start);
        }
        else if (identifier == "frequency_hz")
        {
            return addNode (NodeKind::variableFrequencyHz, start);
        }

        fail (ExpressionErrorCode::unknownIdentifier, start);
        return invalidNode;
    }

    [[nodiscard]] static constexpr FunctionDescription describeFunction (
        std::string_view identifier) noexcept
    {
        if (identifier == "if") return { NodeKind::functionIf, 3, true };
        if (identifier == "abs") return { NodeKind::functionAbs, 1, true };
        if (identifier == "min") return { NodeKind::functionMin, 2, true };
        if (identifier == "max") return { NodeKind::functionMax, 2, true };
        if (identifier == "clamp") return { NodeKind::functionClamp, 3, true };
        if (identifier == "pow") return { NodeKind::functionPow, 2, true };
        if (identifier == "exp") return { NodeKind::functionExp, 1, true };
        if (identifier == "log") return { NodeKind::functionLog, 1, true };
        if (identifier == "sin") return { NodeKind::functionSin, 1, true };
        if (identifier == "cos") return { NodeKind::functionCos, 1, true };
        return {};
    }

    [[nodiscard]] NodeIndex parseFunction (std::string_view identifier,
                                           std::size_t start) noexcept
    {
        const auto description = describeFunction (identifier);

        if (! description.found)
        {
            fail (ExpressionErrorCode::unknownFunction, start);
            return invalidNode;
        }

        ++position;
        const auto function = addNode (description.kind, start);

        if (function == invalidNode)
            return invalidNode;

        std::array<NodeIndex, 3> arguments { invalidNode, invalidNode, invalidNode };
        std::size_t argumentCount = 0;
        skipWhitespace();

        if (! consume (")"))
        {
            for (;;)
            {
                const auto argument = parseLogicalOr();

                if (error.code != ExpressionErrorCode::none)
                    return invalidNode;

                if (argumentCount < arguments.size())
                    arguments[argumentCount] = argument;

                ++argumentCount;
                skipWhitespace();

                if (consume (")"))
                    break;

                if (! consume (","))
                {
                    const auto code = position < source.size()
                                           && (source[position] == '=' || source[position] == '?'
                                               || source[position] == ':')
                                        ? ExpressionErrorCode::unsupportedSyntax
                                        : ExpressionErrorCode::expectedExpression;
                    fail (code, position);
                    return invalidNode;
                }
            }
        }

        if (argumentCount != description.arity)
        {
            fail (ExpressionErrorCode::wrongArgumentCount, start);
            return invalidNode;
        }

        program.nodes[function].first = arguments[0];
        program.nodes[function].second = arguments[1];
        program.nodes[function].third = arguments[2];
        return function;
    }

    std::string_view source;
    ExpressionVariableSet variableSet;
    ExpressionProgram& program;
    std::size_t position = 0;
    std::size_t nestingDepth = 0;
    ExpressionError error {};
};

ExpressionCompileResult ExpressionProgram::compile (std::string_view source,
                                                    ExpressionVariableSet variables) noexcept
{
    ExpressionCompileResult result;

    if (source.size() > maximumSourceLength)
    {
        result.error = makeError (ExpressionErrorCode::expressionTooLong,
                                  maximumSourceLength);
        return result;
    }

    Compiler compiler { source, variables, result.program };
    result.success = compiler.parse();
    result.error = compiler.getError();
    return result;
}

ExpressionEvaluationResult ExpressionProgram::evaluate (
    const ExpressionContext& context,
    ExpressionResultRange resultRange) const noexcept
{
    if (! std::isfinite (resultRange.minimum) || ! std::isfinite (resultRange.maximum)
        || resultRange.minimum > resultRange.maximum)
        return evaluationFailure (ExpressionErrorCode::invalidResultRange, 0);

    if (root == invalidNode || nodeCount == 0 || root >= nodeCount)
        return evaluationFailure (ExpressionErrorCode::invalidProgram, 0);

    auto remainingWork = static_cast<std::size_t> (nodeCount);
    auto result = evaluateNode (root, context, remainingWork);

    if (! result.success)
        return result;

    if (result.value < resultRange.minimum || result.value > resultRange.maximum)
        return evaluationFailure (ExpressionErrorCode::resultOutOfRange, nodes[root].offset);

    return result;
}

ExpressionEvaluationResult ExpressionProgram::evaluateNode (
    NodeIndex nodeIndex,
    const ExpressionContext& context,
    std::size_t& remainingWork) const noexcept
{
    if (remainingWork == 0)
        return evaluationFailure (ExpressionErrorCode::evaluationLimitExceeded, 0);

    if (nodeIndex >= nodeCount)
        return evaluationFailure (ExpressionErrorCode::invalidProgram, 0);

    --remainingWork;
    const auto& node = nodes[nodeIndex];

    const auto finiteValue = [&node] (double value) noexcept
    {
        return std::isfinite (value)
                 ? evaluationSuccess (value)
                 : evaluationFailure (ExpressionErrorCode::nonFiniteResult, node.offset);
    };

    switch (node.kind)
    {
        case NodeKind::literal: return evaluationSuccess (node.literalValue);
        case NodeKind::constantPi: return evaluationSuccess (std::numbers::pi);
        case NodeKind::constantE: return evaluationSuccess (std::numbers::e);
        case NodeKind::variableIndex: return finiteValue (context.index);
        case NodeKind::variableFundamentalHz: return finiteValue (context.fundamentalHz);
        case NodeKind::variableFrequencyHz: return finiteValue (context.frequencyHz);
        case NodeKind::variableNyquistHz: return finiteValue (context.nyquistHz);

        case NodeKind::unaryPlus:
        case NodeKind::unaryMinus:
        case NodeKind::logicalNot:
        {
            const auto operand = evaluateNode (node.first, context, remainingWork);

            if (! operand.success)
                return operand;

            if (node.kind == NodeKind::unaryPlus)
                return finiteValue (operand.value);

            if (node.kind == NodeKind::unaryMinus)
                return finiteValue (-operand.value);

            return evaluationSuccess (operand.value == 0.0 ? 1.0 : 0.0);
        }

        case NodeKind::logicalAnd:
        {
            const auto left = evaluateNode (node.first, context, remainingWork);

            if (! left.success || left.value == 0.0)
                return left.success ? evaluationSuccess (0.0) : left;

            const auto right = evaluateNode (node.second, context, remainingWork);
            return right.success ? evaluationSuccess (right.value == 0.0 ? 0.0 : 1.0) : right;
        }

        case NodeKind::logicalOr:
        {
            const auto left = evaluateNode (node.first, context, remainingWork);

            if (! left.success || left.value != 0.0)
                return left.success ? evaluationSuccess (1.0) : left;

            const auto right = evaluateNode (node.second, context, remainingWork);
            return right.success ? evaluationSuccess (right.value == 0.0 ? 0.0 : 1.0) : right;
        }

        case NodeKind::functionIf:
        {
            const auto condition = evaluateNode (node.first, context, remainingWork);

            if (! condition.success)
                return condition;

            return evaluateNode (condition.value != 0.0 ? node.second : node.third,
                                 context,
                                 remainingWork);
        }

        case NodeKind::functionAbs:
        case NodeKind::functionExp:
        case NodeKind::functionLog:
        case NodeKind::functionSin:
        case NodeKind::functionCos:
        {
            const auto argument = evaluateNode (node.first, context, remainingWork);

            if (! argument.success)
                return argument;

            if (node.kind == NodeKind::functionLog && argument.value <= 0.0)
                return evaluationFailure (ExpressionErrorCode::domainError, node.offset);

            if (node.kind == NodeKind::functionAbs) return finiteValue (std::fabs (argument.value));
            if (node.kind == NodeKind::functionExp) return finiteValue (std::exp (argument.value));
            if (node.kind == NodeKind::functionLog) return finiteValue (std::log (argument.value));
            if (node.kind == NodeKind::functionSin) return finiteValue (std::sin (argument.value));
            return finiteValue (std::cos (argument.value));
        }

        case NodeKind::functionClamp:
        {
            const auto value = evaluateNode (node.first, context, remainingWork);
            if (! value.success) return value;

            const auto minimum = evaluateNode (node.second, context, remainingWork);
            if (! minimum.success) return minimum;

            const auto maximum = evaluateNode (node.third, context, remainingWork);
            if (! maximum.success) return maximum;

            if (minimum.value > maximum.value)
                return evaluationFailure (ExpressionErrorCode::domainError, node.offset);

            return finiteValue (value.value < minimum.value ? minimum.value
                                                            : value.value > maximum.value
                                                                ? maximum.value
                                                                : value.value);
        }

        case NodeKind::add:
        case NodeKind::subtract:
        case NodeKind::multiply:
        case NodeKind::divide:
        case NodeKind::less:
        case NodeKind::lessEqual:
        case NodeKind::greater:
        case NodeKind::greaterEqual:
        case NodeKind::equal:
        case NodeKind::notEqual:
        case NodeKind::functionMin:
        case NodeKind::functionMax:
        case NodeKind::functionPow:
        {
            const auto left = evaluateNode (node.first, context, remainingWork);
            if (! left.success) return left;

            const auto right = evaluateNode (node.second, context, remainingWork);
            if (! right.success) return right;

            switch (node.kind)
            {
                case NodeKind::add: return finiteValue (left.value + right.value);
                case NodeKind::subtract: return finiteValue (left.value - right.value);
                case NodeKind::multiply: return finiteValue (left.value * right.value);

                case NodeKind::divide:
                    if (right.value == 0.0)
                        return evaluationFailure (ExpressionErrorCode::divisionByZero,
                                                  node.offset);
                    return finiteValue (left.value / right.value);

                case NodeKind::less: return evaluationSuccess (left.value < right.value ? 1.0 : 0.0);
                case NodeKind::lessEqual: return evaluationSuccess (left.value <= right.value ? 1.0 : 0.0);
                case NodeKind::greater: return evaluationSuccess (left.value > right.value ? 1.0 : 0.0);
                case NodeKind::greaterEqual: return evaluationSuccess (left.value >= right.value ? 1.0 : 0.0);
                case NodeKind::equal: return evaluationSuccess (left.value == right.value ? 1.0 : 0.0);
                case NodeKind::notEqual: return evaluationSuccess (left.value != right.value ? 1.0 : 0.0);
                case NodeKind::functionMin: return finiteValue (left.value < right.value ? left.value : right.value);
                case NodeKind::functionMax: return finiteValue (left.value > right.value ? left.value : right.value);

                case NodeKind::functionPow:
                    if ((left.value < 0.0 && std::trunc (right.value) != right.value)
                        || (left.value == 0.0 && right.value < 0.0))
                        return evaluationFailure (ExpressionErrorCode::domainError,
                                                  node.offset);
                    return finiteValue (std::pow (left.value, right.value));

                default: break;
            }

            break;
        }
    }

    return evaluationFailure (ExpressionErrorCode::invalidProgram, node.offset);
}
}
