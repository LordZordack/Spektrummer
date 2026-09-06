#pragma once

#include "ExpressionProgram.h"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace spektrummer::model
{
inline constexpr std::size_t maximumSourceDefinitions = 64;
inline constexpr std::size_t maximumExpandedPartials = 256;
inline constexpr std::size_t maximumTransferLayers = 32;
inline constexpr std::size_t maximumFreeCurvePoints = 64;

inline constexpr double minimumTrackedRatio = 1.0e-6;
inline constexpr double maximumTrackedRatio = 65536.0;
inline constexpr double minimumModelFrequencyHz = 1.0;
inline constexpr double maximumModelFrequencyHz = 192000.0;
inline constexpr double minimumSourceLevelDb = -96.0;
inline constexpr double maximumSourceLevelDb = 0.0;
inline constexpr std::size_t minimumHarmonicCount = 1;
inline constexpr std::size_t maximumHarmonicCount = maximumExpandedPartials;
inline constexpr double minimumRollOffDbPerOctave = -48.0;
inline constexpr double maximumRollOffDbPerOctave = 12.0;
inline constexpr double minimumEvenOffsetDb = -96.0;
inline constexpr double maximumEvenOffsetDb = 24.0;
inline constexpr double minimumInharmonicStretchB = 0.0;
inline constexpr double maximumInharmonicStretchB = 0.01;
inline constexpr double minimumTransferResponseDb = -96.0;
inline constexpr double maximumTransferResponseDb = 24.0;
inline constexpr double minimumTransferGainDb = -24.0;
inline constexpr double maximumTransferGainDb = 24.0;
inline constexpr double minimumTransferQ = 0.1;
inline constexpr double maximumTransferQ = 20.0;
inline constexpr double minimumTiltSlopeDbPerOctave = -24.0;
inline constexpr double maximumTiltSlopeDbPerOctave = 24.0;

enum class FormulaFrequencyMode
{
    trackedRatio,
    fixedHz
};

struct TrackedPartial
{
    double ratio = 1.0;
    double levelDb = 0.0;
};

struct FixedPartial
{
    double frequencyHz = minimumModelFrequencyHz;
    double levelDb = 0.0;
};

struct HarmonicGenerator
{
    std::size_t count = minimumHarmonicCount;
    double baseLevelDb = 0.0;
    double rollOffDbPerOctave = 0.0;
    double evenOffsetDb = 0.0;
    double inharmonicStretchB = 0.0;
};

struct FormulaGenerator
{
    FormulaFrequencyMode mode = FormulaFrequencyMode::trackedRatio;
    std::size_t count = minimumHarmonicCount;
    std::string frequencyExpression;
    std::string levelExpression;
};

using SourceDefinition = std::variant<TrackedPartial,
                                      FixedPartial,
                                      HarmonicGenerator,
                                      FormulaGenerator>;

struct FreeCurvePoint
{
    double frequencyHz = minimumModelFrequencyHz;
    double responseDb = 0.0;
};

struct FreeCurve
{
    std::vector<FreeCurvePoint> points;
};

struct PeakNotch
{
    double frequencyHz = minimumModelFrequencyHz;
    double gainDb = 0.0;
    double q = 1.0;
};

enum class ShelfKind
{
    low,
    high
};

struct Shelf
{
    ShelfKind kind = ShelfKind::low;
    double frequencyHz = minimumModelFrequencyHz;
    double gainDb = 0.0;
    double q = 1.0;
};

struct Tilt
{
    double frequencyHz = minimumModelFrequencyHz;
    double slopeDbPerOctave = 0.0;
};

struct TransferFormula
{
    std::string expression;
};

using TransferDefinition = std::variant<FreeCurve,
                                        PeakNotch,
                                        Shelf,
                                        Tilt,
                                        TransferFormula>;

struct TransferLayer
{
    bool enabled = true;
    TransferDefinition definition;
};

struct SpectralPresetDraft
{
    std::vector<SourceDefinition> sources;
    std::vector<TransferLayer> transferLayers;
};

enum class ModelErrorCode
{
    none,
    nonFiniteValue,
    valueOutOfRange,
    countOutOfRange,
    capacityExceeded,
    frequenciesNotStrictlyIncreasing,
    expressionCompileFailed,
    invalidEnumValue,
    invalidIndex
};

enum class ModelField
{
    none,
    sourceDefinitionCount,
    expandedPartialCount,
    transferLayerCount,
    ratio,
    frequencyHz,
    levelDb,
    count,
    baseLevelDb,
    rollOffDbPerOctave,
    evenOffsetDb,
    inharmonicStretchB,
    formulaFrequencyMode,
    frequencyExpression,
    levelExpression,
    freeCurvePointCount,
    responseDb,
    gainDb,
    q,
    shelfKind,
    slopeDbPerOctave,
    transferExpression
};

struct SpectralModelError
{
    static constexpr std::size_t noIndex = std::numeric_limits<std::size_t>::max();

    ModelErrorCode code = ModelErrorCode::none;
    ModelField field = ModelField::none;
    std::size_t sourceIndex = noIndex;
    std::size_t layerIndex = noIndex;
    std::size_t pointIndex = noIndex;
    ExpressionError expressionError {};
};

struct CheckedNumericResult
{
    double value = 0.0;
    bool success = false;
    ModelErrorCode error = ModelErrorCode::none;
};

struct ValidatedFormulaGenerator
{
    FormulaFrequencyMode mode = FormulaFrequencyMode::trackedRatio;
    std::size_t count = minimumHarmonicCount;
    ExpressionProgram frequencyProgram;
    ExpressionProgram levelProgram;
};

struct SpectralPresetValidationResult;

class ValidatedFreeCurve final
{
public:
    ValidatedFreeCurve() noexcept = default;

    [[nodiscard]] std::span<const FreeCurvePoint> points() const noexcept;

private:
    friend struct SpectralPresetValidationResult;
    friend class ValidatedSpectralPreset;
    friend SpectralPresetValidationResult validatePreset (const SpectralPresetDraft&) noexcept;

    std::array<FreeCurvePoint, maximumFreeCurvePoints> pointStorage {};
    std::size_t pointCount = 0;
};

struct ValidatedTransferFormula
{
    ExpressionProgram program;
};

using ValidatedSourceDefinition = std::variant<TrackedPartial,
                                               FixedPartial,
                                               HarmonicGenerator,
                                               ValidatedFormulaGenerator>;

using ValidatedTransferDefinition = std::variant<ValidatedFreeCurve,
                                                 PeakNotch,
                                                 Shelf,
                                                 Tilt,
                                                 ValidatedTransferFormula>;

struct ValidatedTransferLayer
{
    bool enabled = true;
    ValidatedTransferDefinition definition;
};

class ValidatedSpectralPreset final
{
private:
    struct ConstructionKey
    {
    };

public:
    explicit ValidatedSpectralPreset (ConstructionKey) noexcept {}

    [[nodiscard]] std::span<const ValidatedSourceDefinition> sources() const noexcept;
    [[nodiscard]] std::span<const ValidatedTransferLayer> transferLayers() const noexcept;

private:
    friend struct SpectralPresetValidationResult;
    friend SpectralPresetValidationResult validatePreset (const SpectralPresetDraft&) noexcept;

    std::array<ValidatedSourceDefinition, maximumSourceDefinitions> sourceStorage {};
    std::array<ValidatedTransferLayer, maximumTransferLayers> transferLayerStorage {};
    std::size_t sourceCount = 0;
    std::size_t transferLayerCount = 0;
};

struct SpectralPresetValidationResult
{
    SpectralPresetValidationResult() noexcept = default;

    std::optional<ValidatedSpectralPreset> preset;
    SpectralModelError error;

private:
    struct ValidationKey
    {
    };

    SpectralPresetValidationResult (ValidationKey,
                                    const SpectralPresetDraft& draft) noexcept;

    friend SpectralPresetValidationResult validatePreset (
        const SpectralPresetDraft& draft) noexcept;
};

[[nodiscard]] CheckedNumericResult ratioToCents (double ratio) noexcept;
[[nodiscard]] CheckedNumericResult centsToRatio (double cents) noexcept;
[[nodiscard]] CheckedNumericResult harmonicRatio (std::size_t index,
                                                  double inharmonicStretchB) noexcept;
[[nodiscard]] CheckedNumericResult harmonicLevelDb (
    std::size_t index,
    const HarmonicGenerator& generator) noexcept;

[[nodiscard]] SpectralPresetValidationResult validatePreset (
    const SpectralPresetDraft& draft) noexcept;
}
