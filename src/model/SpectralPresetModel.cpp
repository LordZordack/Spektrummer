#include "SpectralPresetModel.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

namespace spektrummer::model
{
namespace
{
[[nodiscard]] ModelErrorCode numericError (double value,
                                           double minimum,
                                           double maximum) noexcept
{
    if (! std::isfinite (value))
        return ModelErrorCode::nonFiniteValue;

    if (value < minimum || value > maximum)
        return ModelErrorCode::valueOutOfRange;

    return ModelErrorCode::none;
}

[[nodiscard]] constexpr CheckedNumericResult numericSuccess (double value) noexcept
{
    return { value, true, ModelErrorCode::none };
}

[[nodiscard]] constexpr CheckedNumericResult numericFailure (ModelErrorCode error) noexcept
{
    return { 0.0, false, error };
}

void setValidationFailure (
    SpectralPresetValidationResult& result,
    ModelErrorCode code,
    ModelField field,
    std::size_t sourceIndex = SpectralModelError::noIndex,
    std::size_t layerIndex = SpectralModelError::noIndex,
    std::size_t pointIndex = SpectralModelError::noIndex,
    ExpressionError expressionError = {}) noexcept
{
    result.preset.reset();
    result.error = { code, field, sourceIndex, layerIndex, pointIndex, expressionError };
}
}

std::span<const FreeCurvePoint> ValidatedFreeCurve::points() const noexcept
{
    return { pointStorage.data(), pointCount };
}

std::span<const ValidatedSourceDefinition> ValidatedSpectralPreset::sources() const noexcept
{
    return { sourceStorage.data(), sourceCount };
}

std::span<const ValidatedTransferLayer> ValidatedSpectralPreset::transferLayers() const noexcept
{
    return { transferLayerStorage.data(), transferLayerCount };
}

CheckedNumericResult ratioToCents (double ratio) noexcept
{
    const auto error = numericError (ratio, minimumTrackedRatio, maximumTrackedRatio);

    if (error != ModelErrorCode::none)
        return numericFailure (error);

    const auto cents = 1200.0 * std::log2 (ratio);
    return std::isfinite (cents) ? numericSuccess (cents)
                                 : numericFailure (ModelErrorCode::valueOutOfRange);
}

CheckedNumericResult centsToRatio (double cents) noexcept
{
    if (! std::isfinite (cents))
        return numericFailure (ModelErrorCode::nonFiniteValue);

    const auto minimumCents = 1200.0 * std::log2 (minimumTrackedRatio);
    const auto maximumCents = 1200.0 * std::log2 (maximumTrackedRatio);

    if (cents < minimumCents || cents > maximumCents)
        return numericFailure (ModelErrorCode::valueOutOfRange);

    const auto ratio = std::exp2 (cents / 1200.0);

    if (! std::isfinite (ratio) || ratio < minimumTrackedRatio || ratio > maximumTrackedRatio)
        return numericFailure (ModelErrorCode::valueOutOfRange);

    return numericSuccess (ratio);
}

CheckedNumericResult harmonicRatio (std::size_t index,
                                    double inharmonicStretchB) noexcept
{
    if (index < minimumHarmonicCount || index > maximumHarmonicCount)
        return numericFailure (ModelErrorCode::invalidIndex);

    const auto error = numericError (inharmonicStretchB,
                                     minimumInharmonicStretchB,
                                     maximumInharmonicStretchB);

    if (error != ModelErrorCode::none)
        return numericFailure (error);

    const auto harmonic = static_cast<double> (index);
    const auto ratio = harmonic
                     * std::sqrt (1.0 + inharmonicStretchB * harmonic * harmonic);
    return std::isfinite (ratio) ? numericSuccess (ratio)
                                 : numericFailure (ModelErrorCode::valueOutOfRange);
}

CheckedNumericResult harmonicLevelDb (std::size_t index,
                                      const HarmonicGenerator& generator) noexcept
{
    if (generator.count < minimumHarmonicCount || generator.count > maximumHarmonicCount)
        return numericFailure (ModelErrorCode::countOutOfRange);

    if (index < minimumHarmonicCount || index > generator.count)
        return numericFailure (ModelErrorCode::invalidIndex);

    for (const auto [value, minimum, maximum] : std::array {
             std::array { generator.baseLevelDb, minimumSourceLevelDb, maximumSourceLevelDb },
             std::array { generator.rollOffDbPerOctave,
                          minimumRollOffDbPerOctave,
                          maximumRollOffDbPerOctave },
             std::array { generator.evenOffsetDb, minimumEvenOffsetDb, maximumEvenOffsetDb },
             std::array { generator.inharmonicStretchB,
                          minimumInharmonicStretchB,
                          maximumInharmonicStretchB } })
    {
        const auto error = numericError (value, minimum, maximum);

        if (error != ModelErrorCode::none)
            return numericFailure (error);
    }

    const auto harmonic = static_cast<double> (index);
    const auto evenOffset = index % 2 == 0 ? generator.evenOffsetDb : 0.0;
    const auto level = generator.baseLevelDb
                     + generator.rollOffDbPerOctave * std::log2 (harmonic)
                     + evenOffset;

    if (! std::isfinite (level))
        return numericFailure (ModelErrorCode::valueOutOfRange);

    return numericSuccess (std::clamp (level, minimumSourceLevelDb, maximumSourceLevelDb));
}

SpectralPresetValidationResult::SpectralPresetValidationResult (
    ValidationKey,
    const SpectralPresetDraft& draft) noexcept
{
    if (draft.sources.size() > maximumSourceDefinitions)
    {
        setValidationFailure (*this, ModelErrorCode::capacityExceeded,
                              ModelField::sourceDefinitionCount);
        return;
    }

    if (draft.transferLayers.size() > maximumTransferLayers)
    {
        setValidationFailure (*this, ModelErrorCode::capacityExceeded,
                              ModelField::transferLayerCount);
        return;
    }

    preset.emplace (ValidatedSpectralPreset::ConstructionKey {});
    auto& validated = *preset;
    std::size_t expandedPartialCount = 0;

    const auto sourceScalarFails = [this] (double value,
                                           double minimum,
                                           double maximum,
                                           ModelField field,
                                           std::size_t sourceIndex) noexcept
    {
        const auto error = numericError (value, minimum, maximum);

        if (error == ModelErrorCode::none)
            return false;

        setValidationFailure (*this, error, field, sourceIndex);
        return true;
    };

    for (std::size_t sourceIndex = 0; sourceIndex < draft.sources.size(); ++sourceIndex)
    {
        const auto& source = draft.sources[sourceIndex];
        auto declaredCount = std::size_t { 1 };

        if (const auto* tracked = std::get_if<TrackedPartial> (&source))
        {
            if (sourceScalarFails (tracked->ratio,
                                   minimumTrackedRatio,
                                   maximumTrackedRatio,
                                   ModelField::ratio,
                                   sourceIndex))
                return;

            if (sourceScalarFails (tracked->levelDb,
                                   minimumSourceLevelDb,
                                   maximumSourceLevelDb,
                                   ModelField::levelDb,
                                   sourceIndex))
                return;

            validated.sourceStorage[sourceIndex] = *tracked;
        }
        else if (const auto* fixed = std::get_if<FixedPartial> (&source))
        {
            if (sourceScalarFails (fixed->frequencyHz,
                                   minimumModelFrequencyHz,
                                   maximumModelFrequencyHz,
                                   ModelField::frequencyHz,
                                   sourceIndex))
                return;

            if (sourceScalarFails (fixed->levelDb,
                                   minimumSourceLevelDb,
                                   maximumSourceLevelDb,
                                   ModelField::levelDb,
                                   sourceIndex))
                return;

            validated.sourceStorage[sourceIndex] = *fixed;
        }
        else if (const auto* harmonic = std::get_if<HarmonicGenerator> (&source))
        {
            if (harmonic->count < minimumHarmonicCount
                || harmonic->count > maximumHarmonicCount)
            {
                setValidationFailure (*this, ModelErrorCode::countOutOfRange,
                                      ModelField::count, sourceIndex);
                return;
            }

            const auto harmonicFields = std::array {
                std::pair { ModelField::baseLevelDb,
                            std::array { harmonic->baseLevelDb,
                                         minimumSourceLevelDb,
                                         maximumSourceLevelDb } },
                std::pair { ModelField::rollOffDbPerOctave,
                            std::array { harmonic->rollOffDbPerOctave,
                                         minimumRollOffDbPerOctave,
                                         maximumRollOffDbPerOctave } },
                std::pair { ModelField::evenOffsetDb,
                            std::array { harmonic->evenOffsetDb,
                                         minimumEvenOffsetDb,
                                         maximumEvenOffsetDb } },
                std::pair { ModelField::inharmonicStretchB,
                            std::array { harmonic->inharmonicStretchB,
                                         minimumInharmonicStretchB,
                                         maximumInharmonicStretchB } }
            };

            for (const auto& [field, range] : harmonicFields)
                if (sourceScalarFails (range[0], range[1], range[2], field, sourceIndex))
                    return;

            declaredCount = harmonic->count;
            validated.sourceStorage[sourceIndex] = *harmonic;
        }
        else if (const auto* formula = std::get_if<FormulaGenerator> (&source))
        {
            if (formula->mode != FormulaFrequencyMode::trackedRatio
                && formula->mode != FormulaFrequencyMode::fixedHz)
            {
                setValidationFailure (*this, ModelErrorCode::invalidEnumValue,
                                      ModelField::formulaFrequencyMode, sourceIndex);
                return;
            }

            if (formula->count < minimumHarmonicCount
                || formula->count > maximumHarmonicCount)
            {
                setValidationFailure (*this, ModelErrorCode::countOutOfRange,
                                      ModelField::count, sourceIndex);
                return;
            }

            auto& destination = validated.sourceStorage[sourceIndex]
                                    .emplace<ValidatedFormulaGenerator>();
            destination.mode = formula->mode;
            destination.count = formula->count;

            const auto frequency = ExpressionProgram::compile (
                formula->frequencyExpression, ExpressionVariableSet::source);

            if (! frequency.success)
            {
                setValidationFailure (*this, ModelErrorCode::expressionCompileFailed,
                                      ModelField::frequencyExpression,
                                      sourceIndex,
                                      SpectralModelError::noIndex,
                                      SpectralModelError::noIndex,
                                      frequency.error);
                return;
            }

            destination.frequencyProgram = frequency.program;
            const auto level = ExpressionProgram::compile (
                formula->levelExpression, ExpressionVariableSet::source);

            if (! level.success)
            {
                setValidationFailure (*this, ModelErrorCode::expressionCompileFailed,
                                      ModelField::levelExpression,
                                      sourceIndex,
                                      SpectralModelError::noIndex,
                                      SpectralModelError::noIndex,
                                      level.error);
                return;
            }

            destination.levelProgram = level.program;
            declaredCount = formula->count;
        }
        else
        {
            setValidationFailure (*this, ModelErrorCode::invalidEnumValue,
                                  ModelField::none, sourceIndex);
            return;
        }

        if (declaredCount > maximumExpandedPartials - expandedPartialCount)
        {
            setValidationFailure (*this, ModelErrorCode::capacityExceeded,
                                  ModelField::expandedPartialCount, sourceIndex);
            return;
        }

        expandedPartialCount += declaredCount;
        validated.sourceCount = sourceIndex + 1;
    }

    const auto layerScalarFails = [this] (
        double value,
        double minimum,
        double maximum,
        ModelField field,
        std::size_t layerIndex,
        std::size_t pointIndex = SpectralModelError::noIndex) noexcept
    {
        const auto error = numericError (value, minimum, maximum);

        if (error == ModelErrorCode::none)
            return false;

        setValidationFailure (*this, error, field, SpectralModelError::noIndex,
                              layerIndex, pointIndex);
        return true;
    };

    for (std::size_t layerIndex = 0; layerIndex < draft.transferLayers.size(); ++layerIndex)
    {
        const auto& layer = draft.transferLayers[layerIndex];
        auto& destinationLayer = validated.transferLayerStorage[layerIndex];
        destinationLayer.enabled = layer.enabled;

        if (const auto* curve = std::get_if<FreeCurve> (&layer.definition))
        {
            if (curve->points.empty() || curve->points.size() > maximumFreeCurvePoints)
            {
                setValidationFailure (*this, ModelErrorCode::countOutOfRange,
                                      ModelField::freeCurvePointCount,
                                      SpectralModelError::noIndex,
                                      layerIndex);
                return;
            }

            auto& destination = destinationLayer.definition.emplace<ValidatedFreeCurve>();

            for (std::size_t pointIndex = 0; pointIndex < curve->points.size(); ++pointIndex)
            {
                const auto& point = curve->points[pointIndex];

                if (layerScalarFails (point.frequencyHz,
                                      minimumModelFrequencyHz,
                                      maximumModelFrequencyHz,
                                      ModelField::frequencyHz,
                                      layerIndex,
                                      pointIndex))
                    return;

                if (layerScalarFails (point.responseDb,
                                      minimumTransferResponseDb,
                                      maximumTransferResponseDb,
                                      ModelField::responseDb,
                                      layerIndex,
                                      pointIndex))
                    return;

                if (pointIndex > 0
                    && point.frequencyHz <= curve->points[pointIndex - 1].frequencyHz)
                {
                    setValidationFailure (
                        *this,
                        ModelErrorCode::frequenciesNotStrictlyIncreasing,
                        ModelField::frequencyHz,
                        SpectralModelError::noIndex,
                        layerIndex,
                        pointIndex);
                    return;
                }

                destination.pointStorage[pointIndex] = point;
                destination.pointCount = pointIndex + 1;
            }
        }
        else if (const auto* peak = std::get_if<PeakNotch> (&layer.definition))
        {
            for (const auto& [value, minimum, maximum, field] : std::array {
                     std::tuple { peak->frequencyHz, minimumModelFrequencyHz,
                                  maximumModelFrequencyHz, ModelField::frequencyHz },
                     std::tuple { peak->gainDb, minimumTransferGainDb,
                                  maximumTransferGainDb, ModelField::gainDb },
                     std::tuple { peak->q, minimumTransferQ,
                                  maximumTransferQ, ModelField::q } })
                if (layerScalarFails (value, minimum, maximum, field, layerIndex))
                    return;

            destinationLayer.definition = *peak;
        }
        else if (const auto* shelf = std::get_if<Shelf> (&layer.definition))
        {
            if (shelf->kind != ShelfKind::low && shelf->kind != ShelfKind::high)
            {
                setValidationFailure (*this, ModelErrorCode::invalidEnumValue,
                                      ModelField::shelfKind,
                                      SpectralModelError::noIndex,
                                      layerIndex);
                return;
            }

            for (const auto& [value, minimum, maximum, field] : std::array {
                     std::tuple { shelf->frequencyHz, minimumModelFrequencyHz,
                                  maximumModelFrequencyHz, ModelField::frequencyHz },
                     std::tuple { shelf->gainDb, minimumTransferGainDb,
                                  maximumTransferGainDb, ModelField::gainDb },
                     std::tuple { shelf->q, minimumTransferQ,
                                  maximumTransferQ, ModelField::q } })
                if (layerScalarFails (value, minimum, maximum, field, layerIndex))
                    return;

            destinationLayer.definition = *shelf;
        }
        else if (const auto* tilt = std::get_if<Tilt> (&layer.definition))
        {
            if (layerScalarFails (tilt->frequencyHz,
                                  minimumModelFrequencyHz,
                                  maximumModelFrequencyHz,
                                  ModelField::frequencyHz,
                                  layerIndex))
                return;

            if (layerScalarFails (tilt->slopeDbPerOctave,
                                  minimumTiltSlopeDbPerOctave,
                                  maximumTiltSlopeDbPerOctave,
                                  ModelField::slopeDbPerOctave,
                                  layerIndex))
                return;

            destinationLayer.definition = *tilt;
        }
        else if (const auto* formula = std::get_if<TransferFormula> (&layer.definition))
        {
            const auto compiled = ExpressionProgram::compile (
                formula->expression, ExpressionVariableSet::transfer);

            if (! compiled.success)
            {
                setValidationFailure (*this, ModelErrorCode::expressionCompileFailed,
                                      ModelField::transferExpression,
                                      SpectralModelError::noIndex,
                                      layerIndex,
                                      SpectralModelError::noIndex,
                                      compiled.error);
                return;
            }

            auto& destination = destinationLayer.definition
                                    .emplace<ValidatedTransferFormula>();
            destination.program = compiled.program;
        }
        else
        {
            setValidationFailure (*this, ModelErrorCode::invalidEnumValue,
                                  ModelField::none,
                                  SpectralModelError::noIndex,
                                  layerIndex);
            return;
        }

        validated.transferLayerCount = layerIndex + 1;
    }

}

SpectralPresetValidationResult validatePreset (const SpectralPresetDraft& draft) noexcept
{
    return SpectralPresetValidationResult (
        SpectralPresetValidationResult::ValidationKey {}, draft);
}
}
