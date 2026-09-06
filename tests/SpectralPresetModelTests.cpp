#include <JuceHeader.h>

#include "model/SpectralPresetModel.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using spektrummer::model::CheckedNumericResult;
using spektrummer::model::FixedPartial;
using spektrummer::model::FormulaFrequencyMode;
using spektrummer::model::FormulaGenerator;
using spektrummer::model::FreeCurve;
using spektrummer::model::FreeCurvePoint;
using spektrummer::model::HarmonicGenerator;
using spektrummer::model::ModelErrorCode;
using spektrummer::model::ModelField;
using spektrummer::model::PeakNotch;
using spektrummer::model::Shelf;
using spektrummer::model::ShelfKind;
using spektrummer::model::SourceDefinition;
using spektrummer::model::SpectralModelError;
using spektrummer::model::SpectralPresetDraft;
using spektrummer::model::SpectralPresetValidationResult;
using spektrummer::model::Tilt;
using spektrummer::model::TrackedPartial;
using spektrummer::model::TransferDefinition;
using spektrummer::model::TransferFormula;
using spektrummer::model::TransferLayer;
using spektrummer::model::ValidatedFormulaGenerator;
using spektrummer::model::ValidatedFreeCurve;
using spektrummer::model::ValidatedSourceDefinition;
using spektrummer::model::ValidatedSpectralPreset;
using spektrummer::model::ValidatedTransferDefinition;
using spektrummer::model::ValidatedTransferFormula;
using spektrummer::model::ValidatedTransferLayer;
using spektrummer::model::centsToRatio;
using spektrummer::model::harmonicLevelDb;
using spektrummer::model::harmonicRatio;
using spektrummer::model::maximumExpandedPartials;
using spektrummer::model::maximumFreeCurvePoints;
using spektrummer::model::maximumSourceDefinitions;
using spektrummer::model::maximumTransferLayers;
using spektrummer::model::ratioToCents;
using spektrummer::model::validatePreset;

using spektrummer::model::ExpressionContext;
using spektrummer::model::ExpressionErrorCode;
using spektrummer::model::ExpressionEvaluationResult;
using spektrummer::model::ExpressionResultRange;

static_assert (maximumSourceDefinitions == 64);
static_assert (maximumExpandedPartials == 256);
static_assert (maximumTransferLayers == 32);
static_assert (maximumFreeCurvePoints == 64);

static_assert (std::variant_size_v<SourceDefinition> == 4);
static_assert (std::is_same_v<std::variant_alternative_t<0, SourceDefinition>, TrackedPartial>);
static_assert (std::is_same_v<std::variant_alternative_t<1, SourceDefinition>, FixedPartial>);
static_assert (std::is_same_v<std::variant_alternative_t<2, SourceDefinition>, HarmonicGenerator>);
static_assert (std::is_same_v<std::variant_alternative_t<3, SourceDefinition>, FormulaGenerator>);
static_assert (std::variant_size_v<TransferDefinition> == 5);
static_assert (std::is_same_v<std::variant_alternative_t<0, TransferDefinition>, FreeCurve>);
static_assert (std::is_same_v<std::variant_alternative_t<1, TransferDefinition>, PeakNotch>);
static_assert (std::is_same_v<std::variant_alternative_t<2, TransferDefinition>, Shelf>);
static_assert (std::is_same_v<std::variant_alternative_t<3, TransferDefinition>, Tilt>);
static_assert (std::is_same_v<std::variant_alternative_t<4, TransferDefinition>, TransferFormula>);
static_assert (std::variant_size_v<ValidatedSourceDefinition> == 4);
static_assert (std::is_same_v<std::variant_alternative_t<0, ValidatedSourceDefinition>,
                              TrackedPartial>);
static_assert (std::is_same_v<std::variant_alternative_t<1, ValidatedSourceDefinition>,
                              FixedPartial>);
static_assert (std::is_same_v<std::variant_alternative_t<2, ValidatedSourceDefinition>,
                              HarmonicGenerator>);
static_assert (std::is_same_v<std::variant_alternative_t<3, ValidatedSourceDefinition>,
                              ValidatedFormulaGenerator>);
static_assert (std::variant_size_v<ValidatedTransferDefinition> == 5);
static_assert (std::is_same_v<std::variant_alternative_t<0, ValidatedTransferDefinition>,
                              ValidatedFreeCurve>);
static_assert (std::is_same_v<std::variant_alternative_t<1, ValidatedTransferDefinition>,
                              PeakNotch>);
static_assert (std::is_same_v<std::variant_alternative_t<2, ValidatedTransferDefinition>, Shelf>);
static_assert (std::is_same_v<std::variant_alternative_t<3, ValidatedTransferDefinition>, Tilt>);
static_assert (std::is_same_v<std::variant_alternative_t<4, ValidatedTransferDefinition>,
                              ValidatedTransferFormula>);

static_assert (std::is_same_v<decltype (validatePreset (
                                  std::declval<const SpectralPresetDraft&>())),
                              SpectralPresetValidationResult>);
static_assert (noexcept (validatePreset (std::declval<const SpectralPresetDraft&>())));
static_assert (std::is_same_v<decltype (ratioToCents (0.0)), CheckedNumericResult>);
static_assert (std::is_same_v<decltype (centsToRatio (0.0)), CheckedNumericResult>);
static_assert (std::is_same_v<decltype (harmonicRatio (std::size_t {}, 0.0)),
                              CheckedNumericResult>);
static_assert (std::is_same_v<decltype (harmonicLevelDb (
                                  std::size_t {},
                                  std::declval<const HarmonicGenerator&>())),
                              CheckedNumericResult>);
static_assert (noexcept (ratioToCents (0.0)));
static_assert (noexcept (centsToRatio (0.0)));
static_assert (noexcept (harmonicRatio (std::size_t {}, 0.0)));
static_assert (noexcept (harmonicLevelDb (
    std::size_t {}, std::declval<const HarmonicGenerator&>())));
static_assert (std::is_same_v<decltype (std::declval<SpectralPresetValidationResult>().preset),
                              std::optional<ValidatedSpectralPreset>>);
static_assert (std::is_same_v<decltype (std::declval<SpectralPresetValidationResult>().error),
                              SpectralModelError>);
static_assert (! std::is_default_constructible_v<ValidatedSpectralPreset>);
static_assert (std::is_same_v<decltype (std::declval<const ValidatedSpectralPreset&>().sources()),
                              std::span<const ValidatedSourceDefinition>>);
static_assert (std::is_same_v<decltype (
                                  std::declval<const ValidatedSpectralPreset&>().transferLayers()),
                              std::span<const ValidatedTransferLayer>>);
static_assert (noexcept (std::declval<const ValidatedSpectralPreset&>().sources()));
static_assert (noexcept (std::declval<const ValidatedSpectralPreset&>().transferLayers()));
static_assert (std::is_same_v<decltype (std::declval<const ValidatedFreeCurve&>().points()),
                              std::span<const FreeCurvePoint>>);
static_assert (noexcept (std::declval<const ValidatedFreeCurve&>().points()));

constexpr auto noIndex = SpectralModelError::noIndex;
constexpr auto negativeInfinity = -std::numeric_limits<double>::infinity();
constexpr auto positiveInfinity = std::numeric_limits<double>::infinity();
constexpr auto quietNaN = std::numeric_limits<double>::quiet_NaN();

constexpr ExpressionContext sourceContext {
    .index = 3.0,
    .fundamentalHz = 110.0,
    .frequencyHz = 0.0,
    .nyquistHz = 24000.0
};

constexpr ExpressionContext transferContext {
    .index = 0.0,
    .fundamentalHz = 0.0,
    .frequencyHz = 1000.0,
    .nyquistHz = 24000.0
};

constexpr ExpressionResultRange trackedRatioRange { .minimum = 1.0e-6,
                                                    .maximum = 65536.0 };
constexpr ExpressionResultRange fixedFrequencyRange { .minimum = 1.0,
                                                      .maximum = 192000.0 };
constexpr ExpressionResultRange sourceLevelRange { .minimum = -96.0, .maximum = 0.0 };
constexpr ExpressionResultRange transferResponseRange { .minimum = -96.0,
                                                        .maximum = 24.0 };

[[nodiscard]] TrackedPartial validTrackedPartial()
{
    return { .ratio = 1.0, .levelDb = -12.0 };
}

[[nodiscard]] FixedPartial validFixedPartial()
{
    return { .frequencyHz = 440.0, .levelDb = -12.0 };
}

[[nodiscard]] HarmonicGenerator validHarmonicGenerator()
{
    return { .count = 16,
             .baseLevelDb = -6.0,
             .rollOffDbPerOctave = -6.0,
             .evenOffsetDb = -3.0,
             .inharmonicStretchB = 0.001 };
}

[[nodiscard]] FormulaGenerator validFormulaGenerator (
    FormulaFrequencyMode mode = FormulaFrequencyMode::trackedRatio)
{
    return { .mode = mode,
             .count = 8,
             .frequencyExpression = mode == FormulaFrequencyMode::trackedRatio
                                        ? "index"
                                        : "fundamental_hz * index",
             .levelExpression = "-6 * log(index)" };
}

[[nodiscard]] PeakNotch validPeakNotch()
{
    return { .frequencyHz = 1000.0, .gainDb = -6.0, .q = 1.0 };
}

[[nodiscard]] Shelf validShelf (ShelfKind kind = ShelfKind::low)
{
    return { .kind = kind, .frequencyHz = 1000.0, .gainDb = 6.0, .q = 1.0 };
}

[[nodiscard]] Tilt validTilt()
{
    return { .frequencyHz = 1000.0, .slopeDbPerOctave = -3.0 };
}

[[nodiscard]] TransferFormula validTransferFormula()
{
    return { .expression = "frequency_hz / nyquist_hz" };
}

[[nodiscard]] SpectralPresetDraft draftWithSource (SourceDefinition source)
{
    SpectralPresetDraft draft;
    draft.sources.push_back (std::move (source));
    return draft;
}

[[nodiscard]] SpectralPresetDraft draftWithLayer (TransferDefinition definition,
                                                  bool enabled = true)
{
    SpectralPresetDraft draft;
    draft.transferLayers.push_back (
        TransferLayer { .enabled = enabled, .definition = std::move (definition) });
    return draft;
}

class SpectralPresetModelTests final : public juce::UnitTest
{
public:
    SpectralPresetModelTests() : juce::UnitTest ("Spectral preset model") {}

    void runTest() override
    {
        testPublicDraftAndValidationContract();
        testTrackedPartialBoundaries();
        testFixedPartialBoundaries();
        testRatioAndCentsConversions();
        testHarmonicGeneratorBoundaries();
        testHarmonicHelpers();
        testFormulaGenerators();
        testFreeCurves();
        testParametricTransferDefinitions();
        testTransferFormulas();
        testCollectionAndExpansionLimits();
        testValidatedPublicationAndAtomicFailures();
    }

private:
    void expectSuccess (const SpectralPresetDraft& draft)
    {
        const auto result = validatePreset (draft);
        expect (result.preset.has_value());
        expect (result.error.code == ModelErrorCode::none);
        expect (result.error.field == ModelField::none);
        expectEquals (result.error.sourceIndex, noIndex);
        expectEquals (result.error.layerIndex, noIndex);
        expectEquals (result.error.pointIndex, noIndex);
        expect (result.error.expressionError.code == ExpressionErrorCode::none);
        expectEquals (result.error.expressionError.offset, std::size_t {});
    }

    void expectFailure (const SpectralPresetDraft& draft,
                        ModelErrorCode code,
                        ModelField field,
                        std::size_t sourceIndex = noIndex,
                        std::size_t layerIndex = noIndex,
                        std::size_t pointIndex = noIndex,
                        ExpressionErrorCode expressionCode = ExpressionErrorCode::none,
                        std::size_t expressionOffset = 0)
    {
        const auto result = validatePreset (draft);
        expect (! result.preset.has_value());
        expect (result.error.code == code);
        expect (result.error.field == field);
        expectEquals (result.error.sourceIndex, sourceIndex);
        expectEquals (result.error.layerIndex, layerIndex);
        expectEquals (result.error.pointIndex, pointIndex);
        expect (result.error.expressionError.code == expressionCode);
        expectEquals (result.error.expressionError.offset, expressionOffset);
    }

    void expectSourceFailure (SourceDefinition source,
                              ModelErrorCode code,
                              ModelField field)
    {
        expectFailure (draftWithSource (std::move (source)), code, field, 0);
    }

    void expectLayerFailure (TransferDefinition definition,
                             ModelErrorCode code,
                             ModelField field,
                             std::size_t pointIndex = noIndex)
    {
        expectFailure (draftWithLayer (std::move (definition)), code, field,
                       noIndex, 0, pointIndex);
    }

    void expectNumericSuccess (CheckedNumericResult result,
                               double expected,
                               double tolerance = 0.0)
    {
        expect (result.success);
        expect (result.error == ModelErrorCode::none);

        if (result.success)
        {
            if (tolerance == 0.0)
                expectEquals (result.value, expected);
            else
                expectWithinAbsoluteError (result.value, expected, tolerance);
        }
    }

    void expectNumericFailure (CheckedNumericResult result, ModelErrorCode code)
    {
        expect (! result.success);
        expect (result.error == code);
    }

    void expectEvaluationSuccess (const ExpressionEvaluationResult& result,
                                  double expected,
                                  double tolerance = 1.0e-12)
    {
        expect (result.success);
        expect (result.error.code == ExpressionErrorCode::none);

        if (result.success)
            expectWithinAbsoluteError (result.value, expected, tolerance);
    }

    void expectEvaluationFailure (const ExpressionEvaluationResult& result,
                                  ExpressionErrorCode code,
                                  std::size_t offset)
    {
        expect (! result.success);
        expect (result.error.code == code);
        expectEquals (result.error.offset, offset);
    }

    void testPublicDraftAndValidationContract()
    {
        beginTest ("Draft variants and enum alternatives are publicly constructible");
        SpectralPresetDraft draft;
        draft.sources = {
            validTrackedPartial(),
            validFixedPartial(),
            validHarmonicGenerator(),
            validFormulaGenerator()
        };
        draft.transferLayers = {
            { .enabled = true,
              .definition = FreeCurve { .points = { { 20.0, -3.0 }, { 20000.0, 3.0 } } } },
            { .enabled = false, .definition = validPeakNotch() },
            { .enabled = true, .definition = validShelf (ShelfKind::low) },
            { .enabled = true, .definition = validTilt() },
            { .enabled = false, .definition = validTransferFormula() }
        };

        expect (std::holds_alternative<TrackedPartial> (draft.sources[0]));
        expect (std::holds_alternative<FixedPartial> (draft.sources[1]));
        expect (std::holds_alternative<HarmonicGenerator> (draft.sources[2]));
        expect (std::holds_alternative<FormulaGenerator> (draft.sources[3]));
        expect (std::holds_alternative<FreeCurve> (draft.transferLayers[0].definition));
        expect (std::holds_alternative<PeakNotch> (draft.transferLayers[1].definition));
        expect (std::holds_alternative<Shelf> (draft.transferLayers[2].definition));
        expect (std::holds_alternative<Tilt> (draft.transferLayers[3].definition));
        expect (std::holds_alternative<TransferFormula> (draft.transferLayers[4].definition));
        expect (FormulaFrequencyMode::trackedRatio != FormulaFrequencyMode::fixedHz);
        expect (ShelfKind::low != ShelfKind::high);
        expectSuccess (draft);

        beginTest ("Empty presets are valid bounded identity models");
        expectSuccess (SpectralPresetDraft {});
    }

    void testTrackedPartialBoundaries()
    {
        beginTest ("Tracked partial ratio and level endpoints are inclusive");
        for (const auto ratio : std::array { 1.0e-6, 65536.0 })
            for (const auto level : std::array { -96.0, 0.0 })
                expectSuccess (draftWithSource (TrackedPartial { ratio, level }));

        beginTest ("Tracked partial rejects immediate ratio outsiders exactly");
        for (const auto ratio : std::array {
                 std::nextafter (1.0e-6, negativeInfinity),
                 std::nextafter (65536.0, positiveInfinity) })
            expectSourceFailure (TrackedPartial { ratio, -12.0 },
                                 ModelErrorCode::valueOutOfRange, ModelField::ratio);

        beginTest ("Tracked partial rejects immediate level outsiders exactly");
        for (const auto level : std::array {
                 std::nextafter (-96.0, negativeInfinity),
                 std::nextafter (0.0, positiveInfinity) })
            expectSourceFailure (TrackedPartial { 1.0, level },
                                 ModelErrorCode::valueOutOfRange, ModelField::levelDb);

        beginTest ("Tracked partial rejects every non-finite scalar at its field");
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            expectSourceFailure (TrackedPartial { value, -12.0 },
                                 ModelErrorCode::nonFiniteValue, ModelField::ratio);
            expectSourceFailure (TrackedPartial { 1.0, value },
                                 ModelErrorCode::nonFiniteValue, ModelField::levelDb);
        }
    }

    void testFixedPartialBoundaries()
    {
        beginTest ("Fixed partial frequency and level endpoints are inclusive");
        for (const auto frequency : std::array { 1.0, 192000.0 })
            for (const auto level : std::array { -96.0, 0.0 })
                expectSuccess (draftWithSource (FixedPartial { frequency, level }));

        beginTest ("Fixed partial rejects immediate frequency outsiders exactly");
        for (const auto frequency : std::array {
                 std::nextafter (1.0, negativeInfinity),
                 std::nextafter (192000.0, positiveInfinity) })
            expectSourceFailure (FixedPartial { frequency, -12.0 },
                                 ModelErrorCode::valueOutOfRange, ModelField::frequencyHz);

        beginTest ("Fixed partial rejects immediate level outsiders exactly");
        for (const auto level : std::array {
                 std::nextafter (-96.0, negativeInfinity),
                 std::nextafter (0.0, positiveInfinity) })
            expectSourceFailure (FixedPartial { 440.0, level },
                                 ModelErrorCode::valueOutOfRange, ModelField::levelDb);

        beginTest ("Fixed partial rejects every non-finite scalar at its field");
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            expectSourceFailure (FixedPartial { value, -12.0 },
                                 ModelErrorCode::nonFiniteValue, ModelField::frequencyHz);
            expectSourceFailure (FixedPartial { 440.0, value },
                                 ModelErrorCode::nonFiniteValue, ModelField::levelDb);
        }
    }

    void testRatioAndCentsConversions()
    {
        beginTest ("Ratio-to-cents uses exactly 1200 times log2");
        for (const auto ratio : std::array { 1.0e-6, 0.5, 1.0, 2.0, 65536.0 })
            expectNumericSuccess (ratioToCents (ratio), 1200.0 * std::log2 (ratio));

        beginTest ("Cents-to-ratio uses exactly exp2 of cents divided by 1200");
        const auto minimumCents = 1200.0 * std::log2 (1.0e-6);
        const auto maximumCents = 1200.0 * std::log2 (65536.0);
        for (const auto cents : std::array { minimumCents, -1200.0, 0.0, 1200.0,
                                            maximumCents })
            expectNumericSuccess (centsToRatio (cents), std::exp2 (cents / 1200.0));

        beginTest ("Ratio and cents conversions round-trip across the full contract");
        for (const auto ratio : std::array { 1.0e-6, 0.125, 0.5, 1.0, 2.0, 17.25,
                                             65536.0 })
        {
            const auto cents = ratioToCents (ratio);
            expect (cents.success);
            if (cents.success)
                expectNumericSuccess (centsToRatio (cents.value), ratio,
                                      std::abs (ratio) * 1.0e-12);
        }

        beginTest ("Ratio conversion rejects finite values outside the tracked contract");
        for (const auto ratio : std::array {
                 -1.0,
                 0.0,
                 std::nextafter (1.0e-6, negativeInfinity),
                 std::nextafter (65536.0, positiveInfinity) })
            expectNumericFailure (ratioToCents (ratio), ModelErrorCode::valueOutOfRange);

        beginTest ("Cents conversion rejects results outside the tracked contract");
        expectNumericFailure (centsToRatio (std::nextafter (minimumCents, negativeInfinity)),
                              ModelErrorCode::valueOutOfRange);
        expectNumericFailure (centsToRatio (std::nextafter (maximumCents, positiveInfinity)),
                              ModelErrorCode::valueOutOfRange);
        expectNumericFailure (centsToRatio (1.0e9), ModelErrorCode::valueOutOfRange);

        beginTest ("Both checked conversions reject every non-finite input");
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            expectNumericFailure (ratioToCents (value), ModelErrorCode::nonFiniteValue);
            expectNumericFailure (centsToRatio (value), ModelErrorCode::nonFiniteValue);
        }
    }

    void testHarmonicGeneratorBoundaries()
    {
        beginTest ("Harmonic generator accepts every scalar endpoint");
        auto generator = validHarmonicGenerator();
        for (const auto count : std::array<std::size_t, 2> { 1, 256 })
        {
            generator.count = count;
            expectSuccess (draftWithSource (generator));
        }
        generator = validHarmonicGenerator();
        for (const auto value : std::array { -96.0, 0.0 })
        {
            generator.baseLevelDb = value;
            expectSuccess (draftWithSource (generator));
        }
        generator = validHarmonicGenerator();
        for (const auto value : std::array { -48.0, 12.0 })
        {
            generator.rollOffDbPerOctave = value;
            expectSuccess (draftWithSource (generator));
        }
        generator = validHarmonicGenerator();
        for (const auto value : std::array { -96.0, 24.0 })
        {
            generator.evenOffsetDb = value;
            expectSuccess (draftWithSource (generator));
        }
        generator = validHarmonicGenerator();
        for (const auto value : std::array { 0.0, 0.01 })
        {
            generator.inharmonicStretchB = value;
            expectSuccess (draftWithSource (generator));
        }

        beginTest ("Harmonic generator rejects immediate count outsiders");
        for (const auto count : std::array<std::size_t, 2> { 0, 257 })
        {
            generator = validHarmonicGenerator();
            generator.count = count;
            expectSourceFailure (generator, ModelErrorCode::countOutOfRange, ModelField::count);
        }

        testHarmonicScalarBoundary (
            ModelField::baseLevelDb, -96.0, 0.0,
            [] (HarmonicGenerator& value, double scalar) { value.baseLevelDb = scalar; });
        testHarmonicScalarBoundary (
            ModelField::rollOffDbPerOctave, -48.0, 12.0,
            [] (HarmonicGenerator& value, double scalar) {
                value.rollOffDbPerOctave = scalar;
            });
        testHarmonicScalarBoundary (
            ModelField::evenOffsetDb, -96.0, 24.0,
            [] (HarmonicGenerator& value, double scalar) { value.evenOffsetDb = scalar; });
        testHarmonicScalarBoundary (
            ModelField::inharmonicStretchB, 0.0, 0.01,
            [] (HarmonicGenerator& value, double scalar) {
                value.inharmonicStretchB = scalar;
            });
    }

    template <typename Setter>
    void testHarmonicScalarBoundary (ModelField field,
                                     double minimum,
                                     double maximum,
                                     Setter set)
    {
        beginTest ("Harmonic scalar rejects immediate outsiders and non-finite values");
        for (const auto value : std::array {
                 std::nextafter (minimum, negativeInfinity),
                 std::nextafter (maximum, positiveInfinity) })
        {
            auto generator = validHarmonicGenerator();
            set (generator, value);
            expectSourceFailure (generator, ModelErrorCode::valueOutOfRange, field);
        }

        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            auto generator = validHarmonicGenerator();
            set (generator, value);
            expectSourceFailure (generator, ModelErrorCode::nonFiniteValue, field);
        }
    }

    void testHarmonicHelpers()
    {
        beginTest ("Harmonic ratio is one-based and uses the exact stretch equation");
        for (const auto index : std::array<std::size_t, 4> { 1, 2, 17, 256 })
            for (const auto stretch : std::array { 0.0, 0.01 })
            {
                const auto harmonic = static_cast<double> (index);
                const auto expected = harmonic
                                    * std::sqrt (1.0 + stretch * harmonic * harmonic);
                expectNumericSuccess (
                    harmonicRatio (index, stretch), expected, std::abs (expected) * 1.0e-12);
            }

        beginTest ("Harmonic ratio rejects invalid indices and stretch inputs");
        expectNumericFailure (harmonicRatio (0, 0.0), ModelErrorCode::invalidIndex);
        expectNumericFailure (harmonicRatio (257, 0.0), ModelErrorCode::invalidIndex);
        expectNumericFailure (harmonicRatio (1, std::nextafter (0.0, negativeInfinity)),
                              ModelErrorCode::valueOutOfRange);
        expectNumericFailure (harmonicRatio (1, std::nextafter (0.01, positiveInfinity)),
                              ModelErrorCode::valueOutOfRange);
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
            expectNumericFailure (harmonicRatio (1, value), ModelErrorCode::nonFiniteValue);

        beginTest ("Harmonic levels use log2 and apply even offset only to even indices");
        const auto generator = validHarmonicGenerator();
        expectNumericSuccess (harmonicLevelDb (1, generator), -6.0);
        expectNumericSuccess (harmonicLevelDb (2, generator), -15.0);
        expectNumericSuccess (harmonicLevelDb (3, generator),
                              -6.0 - 6.0 * std::log2 (3.0));

        beginTest ("Harmonic level clamps only after roll-off and even offset");
        auto high = validHarmonicGenerator();
        high.baseLevelDb = 0.0;
        high.rollOffDbPerOctave = 12.0;
        high.evenOffsetDb = 24.0;
        expectNumericSuccess (harmonicLevelDb (2, high), 0.0);
        auto low = validHarmonicGenerator();
        low.baseLevelDb = -96.0;
        low.rollOffDbPerOctave = -48.0;
        low.evenOffsetDb = -96.0;
        expectNumericSuccess (harmonicLevelDb (2, low), -96.0);

        beginTest ("Harmonic level rejects indices outside the declared generator");
        expectNumericFailure (harmonicLevelDb (0, generator), ModelErrorCode::invalidIndex);
        expectNumericFailure (harmonicLevelDb (generator.count + 1, generator),
                              ModelErrorCode::invalidIndex);

        beginTest ("Harmonic level rejects every invalid input field");
        auto invalid = generator;
        invalid.count = 0;
        expectNumericFailure (harmonicLevelDb (1, invalid), ModelErrorCode::countOutOfRange);
        invalid = generator;
        invalid.baseLevelDb = quietNaN;
        expectNumericFailure (harmonicLevelDb (1, invalid), ModelErrorCode::nonFiniteValue);
        invalid = generator;
        invalid.rollOffDbPerOctave = positiveInfinity;
        expectNumericFailure (harmonicLevelDb (1, invalid), ModelErrorCode::nonFiniteValue);
        invalid = generator;
        invalid.evenOffsetDb = 25.0;
        expectNumericFailure (harmonicLevelDb (2, invalid), ModelErrorCode::valueOutOfRange);
        invalid = generator;
        invalid.inharmonicStretchB = quietNaN;
        expectNumericFailure (harmonicLevelDb (1, invalid), ModelErrorCode::nonFiniteValue);
        invalid = generator;
        invalid.count = 257;
        expectNumericFailure (harmonicLevelDb (1, invalid), ModelErrorCode::countOutOfRange);
    }

    void testFormulaGenerators()
    {
        beginTest ("Both formula frequency modes accept count endpoints");
        for (const auto mode : std::array { FormulaFrequencyMode::trackedRatio,
                                           FormulaFrequencyMode::fixedHz })
            for (const auto count : std::array<std::size_t, 2> { 1, 256 })
            {
                auto formula = validFormulaGenerator (mode);
                formula.count = count;
                expectSuccess (draftWithSource (formula));
            }

        beginTest ("Formula generator rejects immediate count outsiders");
        for (const auto count : std::array<std::size_t, 2> { 0, 257 })
        {
            auto formula = validFormulaGenerator();
            formula.count = count;
            expectSourceFailure (formula, ModelErrorCode::countOutOfRange, ModelField::count);
        }

        beginTest ("Source formulas compile separately with source-only variables");
        auto formula = validFormulaGenerator();
        formula.frequencyExpression = "index + fundamental_hz / nyquist_hz";
        formula.levelExpression = "-index";
        auto result = validatePreset (draftWithSource (formula));
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto sources = result.preset->sources();
            expectEquals (sources.size(), std::size_t { 1 });
            if (sources.size() == 1)
                if (const auto* validated = std::get_if<ValidatedFormulaGenerator> (&sources[0]))
                {
                    expect (validated->mode == FormulaFrequencyMode::trackedRatio);
                    expectEquals (validated->count, std::size_t { 8 });
                    expectEvaluationSuccess (
                        validated->frequencyProgram.evaluate (sourceContext, trackedRatioRange),
                        3.0 + 110.0 / 24000.0);
                    expectEvaluationSuccess (
                        validated->levelProgram.evaluate (sourceContext, sourceLevelRange), -3.0);
                }
                else
                {
                    expect (false, "Validated source must retain the formula variant");
                }
        }

        beginTest ("Tracked and fixed formula programs enforce distinct frequency ranges");
        for (const auto mode : std::array { FormulaFrequencyMode::trackedRatio,
                                           FormulaFrequencyMode::fixedHz })
        {
            auto boundaryFormula = validFormulaGenerator (mode);
            boundaryFormula.frequencyExpression = mode == FormulaFrequencyMode::trackedRatio
                                                      ? "if(index == 1, 0.000001, 65536)"
                                                      : "if(index == 1, 1, 192000)";
            boundaryFormula.levelExpression = "if(index == 1, -96, 0)";
            const auto validatedResult = validatePreset (draftWithSource (boundaryFormula));
            expect (validatedResult.preset.has_value());
            if (! validatedResult.preset)
                continue;

            const auto* validated = std::get_if<ValidatedFormulaGenerator> (
                &validatedResult.preset->sources()[0]);
            expect (validated != nullptr);
            if (validated == nullptr)
                continue;

            const auto frequencyRange = mode == FormulaFrequencyMode::trackedRatio
                                            ? trackedRatioRange
                                            : fixedFrequencyRange;
            auto context = sourceContext;
            context.index = 1.0;
            expectEvaluationSuccess (
                validated->frequencyProgram.evaluate (context, frequencyRange),
                frequencyRange.minimum);
            expectEvaluationSuccess (
                validated->levelProgram.evaluate (context, sourceLevelRange), -96.0);
            context.index = 2.0;
            expectEvaluationSuccess (
                validated->frequencyProgram.evaluate (context, frequencyRange),
                frequencyRange.maximum);
            expectEvaluationSuccess (
                validated->levelProgram.evaluate (context, sourceLevelRange), 0.0);
        }

        beginTest ("Formula range, domain, and non-finite failures remain evaluator errors");
        for (const auto expression : std::array<std::string, 4> {
                 "0", "65537", "log(-1)", "exp(10000)" })
        {
            auto invalidResultFormula = validFormulaGenerator();
            invalidResultFormula.frequencyExpression = expression;
            const auto validatedResult = validatePreset (draftWithSource (invalidResultFormula));
            expect (validatedResult.preset.has_value());
            if (! validatedResult.preset)
                continue;
            const auto* validated = std::get_if<ValidatedFormulaGenerator> (
                &validatedResult.preset->sources()[0]);
            expect (validated != nullptr);
            if (validated == nullptr)
                continue;

            const auto evaluation = validated->frequencyProgram.evaluate (sourceContext,
                                                                           trackedRatioRange);
            if (expression == "0" || expression == "65537")
                expectEvaluationFailure (evaluation, ExpressionErrorCode::resultOutOfRange, 0);
            else if (expression == "log(-1)")
                expectEvaluationFailure (evaluation, ExpressionErrorCode::domainError, 0);
            else
                expectEvaluationFailure (evaluation, ExpressionErrorCode::nonFiniteResult, 0);
        }

        beginTest ("Fixed-Hz formula results reject values outside their own range");
        for (const auto expression : std::array<std::string, 2> { "0", "192001" })
        {
            auto invalidResultFormula = validFormulaGenerator (FormulaFrequencyMode::fixedHz);
            invalidResultFormula.frequencyExpression = expression;
            const auto validatedResult = validatePreset (draftWithSource (invalidResultFormula));
            expect (validatedResult.preset.has_value());
            if (! validatedResult.preset)
                continue;
            const auto* validated = std::get_if<ValidatedFormulaGenerator> (
                &validatedResult.preset->sources()[0]);
            expect (validated != nullptr);
            if (validated != nullptr)
                expectEvaluationFailure (
                    validated->frequencyProgram.evaluate (sourceContext, fixedFrequencyRange),
                    ExpressionErrorCode::resultOutOfRange, 0);
        }

        beginTest ("Formula level overflow is an error and is never clamped");
        auto overflowingLevel = validFormulaGenerator();
        overflowingLevel.levelExpression = "1";
        result = validatePreset (draftWithSource (overflowingLevel));
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto* validated = std::get_if<ValidatedFormulaGenerator> (
                &result.preset->sources()[0]);
            expect (validated != nullptr);
            if (validated != nullptr)
                expectEvaluationFailure (
                    validated->levelProgram.evaluate (sourceContext, sourceLevelRange),
                    ExpressionErrorCode::resultOutOfRange, 0);
        }

        beginTest ("Formula compilation reports exact source field and expression error");
        auto malformed = validFormulaGenerator();
        malformed.frequencyExpression = "1 +";
        expectFailure (draftWithSource (malformed), ModelErrorCode::expressionCompileFailed,
                       ModelField::frequencyExpression, 0, noIndex, noIndex,
                       ExpressionErrorCode::expectedExpression, 3);
        malformed = validFormulaGenerator();
        malformed.levelExpression = "1 +";
        expectFailure (draftWithSource (malformed), ModelErrorCode::expressionCompileFailed,
                       ModelField::levelExpression, 0, noIndex, noIndex,
                       ExpressionErrorCode::expectedExpression, 3);
        malformed = validFormulaGenerator();
        malformed.frequencyExpression = "frequency_hz";
        expectFailure (draftWithSource (malformed), ModelErrorCode::expressionCompileFailed,
                       ModelField::frequencyExpression, 0, noIndex, noIndex,
                       ExpressionErrorCode::unknownIdentifier, 0);
        malformed = validFormulaGenerator();
        malformed.levelExpression = "frequency_hz";
        expectFailure (draftWithSource (malformed), ModelErrorCode::expressionCompileFailed,
                       ModelField::levelExpression, 0, noIndex, noIndex,
                       ExpressionErrorCode::unknownIdentifier, 0);
    }

    void testFreeCurves()
    {
        beginTest ("Free curves accept one point and all 64 ordered points");
        expectSuccess (draftWithLayer (FreeCurve { .points = { { 1.0, -96.0 } } }));
        FreeCurve maximumCurve;
        for (auto index = std::size_t {}; index < maximumFreeCurvePoints; ++index)
        {
            const auto fraction = static_cast<double> (index)
                                / static_cast<double> (maximumFreeCurvePoints - 1);
            maximumCurve.points.push_back (
                { 1.0 + fraction * 191999.0, -96.0 + fraction * 120.0 });
        }
        expectSuccess (draftWithLayer (maximumCurve));

        beginTest ("Validated free curves preserve endpoint points and fixed-capacity count");
        const auto result = validatePreset (draftWithLayer (maximumCurve));
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto layers = result.preset->transferLayers();
            expectEquals (layers.size(), std::size_t { 1 });
            if (layers.size() == 1)
                if (const auto* curve = std::get_if<ValidatedFreeCurve> (&layers[0].definition))
                {
                    const auto points = curve->points();
                    expectEquals (points.size(), maximumFreeCurvePoints);
                    if (! points.empty())
                    {
                        expectEquals (points.front().frequencyHz, 1.0);
                        expectEquals (points.front().responseDb, -96.0);
                        expectEquals (points.back().frequencyHz, 192000.0);
                        expectEquals (points.back().responseDb, 24.0);
                    }
                }
                else
                {
                    expect (false, "Validated transfer must retain the free-curve variant");
                }
        }

        beginTest ("Free curves reject zero and 65 points before indexing a point");
        expectLayerFailure (FreeCurve {}, ModelErrorCode::countOutOfRange,
                            ModelField::freeCurvePointCount);
        auto excessiveCurve = maximumCurve;
        excessiveCurve.points.push_back ({ 192001.0, 0.0 });
        expectLayerFailure (excessiveCurve, ModelErrorCode::countOutOfRange,
                            ModelField::freeCurvePointCount);

        beginTest ("Free-curve frequency endpoints are inclusive and outsiders are exact");
        expectSuccess (draftWithLayer (FreeCurve {
            .points = { { 1.0, 0.0 }, { 192000.0, 0.0 } }
        }));
        for (const auto frequency : std::array {
                 std::nextafter (1.0, negativeInfinity),
                 std::nextafter (192000.0, positiveInfinity) })
            expectLayerFailure (FreeCurve { .points = { { frequency, 0.0 } } },
                                ModelErrorCode::valueOutOfRange, ModelField::frequencyHz, 0);

        beginTest ("Free-curve response endpoints are inclusive and outsiders are exact");
        expectSuccess (draftWithLayer (FreeCurve {
            .points = { { 100.0, -96.0 }, { 1000.0, 24.0 } }
        }));
        for (const auto response : std::array {
                 std::nextafter (-96.0, negativeInfinity),
                 std::nextafter (24.0, positiveInfinity) })
            expectLayerFailure (FreeCurve { .points = { { 100.0, response } } },
                                ModelErrorCode::valueOutOfRange, ModelField::responseDb, 0);

        beginTest ("Free curves reject non-finite point fields at exact point indices");
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            expectLayerFailure (FreeCurve { .points = { { value, 0.0 } } },
                                ModelErrorCode::nonFiniteValue, ModelField::frequencyHz, 0);
            expectLayerFailure (FreeCurve { .points = { { 100.0, value } } },
                                ModelErrorCode::nonFiniteValue, ModelField::responseDb, 0);
        }

        beginTest ("Free-curve frequencies must be strictly increasing and unique");
        expectLayerFailure (FreeCurve {
            .points = { { 100.0, 0.0 }, { 100.0, 1.0 } }
        }, ModelErrorCode::frequenciesNotStrictlyIncreasing, ModelField::frequencyHz, 1);
        expectLayerFailure (FreeCurve {
            .points = { { 100.0, 0.0 }, { 99.0, 1.0 } }
        }, ModelErrorCode::frequenciesNotStrictlyIncreasing, ModelField::frequencyHz, 1);
    }

    void testParametricTransferDefinitions()
    {
        beginTest ("Peak/notch accepts all frequency, gain, and Q endpoints");
        testPeakOrShelfEndpoints (false, ShelfKind::low);

        beginTest ("Low and high shelves accept all frequency, gain, and Q endpoints");
        testPeakOrShelfEndpoints (true, ShelfKind::low);
        testPeakOrShelfEndpoints (true, ShelfKind::high);

        beginTest ("Peak/notch rejects every immediate outsider and non-finite scalar");
        testPeakOrShelfFailures (false, ShelfKind::low);

        beginTest ("Both shelf kinds reject every immediate outsider and non-finite scalar");
        testPeakOrShelfFailures (true, ShelfKind::low);
        testPeakOrShelfFailures (true, ShelfKind::high);

        beginTest ("Tilt accepts frequency and signed-slope endpoints");
        for (const auto frequency : std::array { 1.0, 192000.0 })
            expectSuccess (draftWithLayer (Tilt { frequency, -3.0 }));
        for (const auto slope : std::array { -24.0, 24.0 })
            expectSuccess (draftWithLayer (Tilt { 1000.0, slope }));

        beginTest ("Tilt rejects immediate outsiders and non-finite fields exactly");
        for (const auto frequency : std::array {
                 std::nextafter (1.0, negativeInfinity),
                 std::nextafter (192000.0, positiveInfinity) })
            expectLayerFailure (Tilt { frequency, 0.0 }, ModelErrorCode::valueOutOfRange,
                                ModelField::frequencyHz);
        for (const auto slope : std::array {
                 std::nextafter (-24.0, negativeInfinity),
                 std::nextafter (24.0, positiveInfinity) })
            expectLayerFailure (Tilt { 1000.0, slope }, ModelErrorCode::valueOutOfRange,
                                ModelField::slopeDbPerOctave);
        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            expectLayerFailure (Tilt { value, 0.0 }, ModelErrorCode::nonFiniteValue,
                                ModelField::frequencyHz);
            expectLayerFailure (Tilt { 1000.0, value }, ModelErrorCode::nonFiniteValue,
                                ModelField::slopeDbPerOctave);
        }
    }

    void testPeakOrShelfEndpoints (bool shelf, ShelfKind kind)
    {
        const auto validate = [this, shelf, kind] (double frequency, double gain, double q)
        {
            if (shelf)
                expectSuccess (draftWithLayer (Shelf { kind, frequency, gain, q }));
            else
                expectSuccess (draftWithLayer (PeakNotch { frequency, gain, q }));
        };

        for (const auto frequency : std::array { 1.0, 192000.0 })
            validate (frequency, 0.0, 1.0);
        for (const auto gain : std::array { -24.0, 24.0 })
            validate (1000.0, gain, 1.0);
        for (const auto q : std::array { 0.1, 20.0 })
            validate (1000.0, 0.0, q);
    }

    void testPeakOrShelfFailures (bool shelf, ShelfKind kind)
    {
        const auto reject = [this, shelf, kind] (double frequency,
                                                 double gain,
                                                 double q,
                                                 ModelErrorCode code,
                                                 ModelField field)
        {
            if (shelf)
                expectLayerFailure (Shelf { kind, frequency, gain, q }, code, field);
            else
                expectLayerFailure (PeakNotch { frequency, gain, q }, code, field);
        };

        for (const auto frequency : std::array {
                 std::nextafter (1.0, negativeInfinity),
                 std::nextafter (192000.0, positiveInfinity) })
            reject (frequency, 0.0, 1.0, ModelErrorCode::valueOutOfRange,
                    ModelField::frequencyHz);
        for (const auto gain : std::array {
                 std::nextafter (-24.0, negativeInfinity),
                 std::nextafter (24.0, positiveInfinity) })
            reject (1000.0, gain, 1.0, ModelErrorCode::valueOutOfRange,
                    ModelField::gainDb);
        for (const auto q : std::array {
                 std::nextafter (0.1, negativeInfinity),
                 std::nextafter (20.0, positiveInfinity) })
            reject (1000.0, 0.0, q, ModelErrorCode::valueOutOfRange, ModelField::q);

        for (const auto value : std::array { quietNaN, negativeInfinity, positiveInfinity })
        {
            reject (value, 0.0, 1.0, ModelErrorCode::nonFiniteValue,
                    ModelField::frequencyHz);
            reject (1000.0, value, 1.0, ModelErrorCode::nonFiniteValue,
                    ModelField::gainDb);
            reject (1000.0, 0.0, value, ModelErrorCode::nonFiniteValue, ModelField::q);
        }
    }

    void testTransferFormulas()
    {
        beginTest ("Transfer formulas compile only transfer variables");
        auto formula = validTransferFormula();
        formula.expression = "frequency_hz / nyquist_hz";
        auto result = validatePreset (draftWithLayer (formula));
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto* validated = std::get_if<ValidatedTransferFormula> (
                &result.preset->transferLayers()[0].definition);
            expect (validated != nullptr);
            if (validated != nullptr)
                expectEvaluationSuccess (
                    validated->program.evaluate (transferContext, transferResponseRange),
                    1000.0 / 24000.0);
        }

        for (const auto expression : std::array<std::string, 2> { "index", "fundamental_hz" })
        {
            formula.expression = expression;
            expectFailure (draftWithLayer (formula), ModelErrorCode::expressionCompileFailed,
                           ModelField::transferExpression, noIndex, 0, noIndex,
                           ExpressionErrorCode::unknownIdentifier, 0);
        }
        formula.expression = "1 +";
        expectFailure (draftWithLayer (formula), ModelErrorCode::expressionCompileFailed,
                       ModelField::transferExpression, noIndex, 0, noIndex,
                       ExpressionErrorCode::expectedExpression, 3);

        beginTest ("Transfer formula response endpoints are inclusive");
        formula.expression = "if(frequency_hz < nyquist_hz, -96, 24)";
        result = validatePreset (draftWithLayer (formula));
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto* validated = std::get_if<ValidatedTransferFormula> (
                &result.preset->transferLayers()[0].definition);
            expect (validated != nullptr);
            if (validated != nullptr)
            {
                expectEvaluationSuccess (
                    validated->program.evaluate (transferContext, transferResponseRange), -96.0);
                auto atNyquist = transferContext;
                atNyquist.frequencyHz = atNyquist.nyquistHz;
                expectEvaluationSuccess (
                    validated->program.evaluate (atNyquist, transferResponseRange), 24.0);
            }
        }

        beginTest ("Transfer formula domain, non-finite, and range failures are rejected");
        for (const auto& testCase : std::array {
                 FormulaErrorCase { "log(-1)", ExpressionErrorCode::domainError },
                 FormulaErrorCase { "exp(10000)", ExpressionErrorCode::nonFiniteResult },
                 FormulaErrorCase { "-97", ExpressionErrorCode::resultOutOfRange },
                 FormulaErrorCase { "25", ExpressionErrorCode::resultOutOfRange } })
        {
            formula.expression = testCase.expression;
            result = validatePreset (draftWithLayer (formula));
            expect (result.preset.has_value());
            if (! result.preset)
                continue;
            const auto* validated = std::get_if<ValidatedTransferFormula> (
                &result.preset->transferLayers()[0].definition);
            expect (validated != nullptr);
            if (validated != nullptr)
                expectEvaluationFailure (
                    validated->program.evaluate (transferContext, transferResponseRange),
                    testCase.code, 0);
        }
    }

    void testCollectionAndExpansionLimits()
    {
        beginTest ("Source-definition capacity accepts 64 and rejects 65 before traversal");
        SpectralPresetDraft maximumSources;
        maximumSources.sources.assign (maximumSourceDefinitions, validTrackedPartial());
        expectSuccess (maximumSources);
        auto excessiveSources = maximumSources;
        excessiveSources.sources.push_back (TrackedPartial { quietNaN, -12.0 });
        expectFailure (excessiveSources, ModelErrorCode::capacityExceeded,
                       ModelField::sourceDefinitionCount);

        beginTest ("Transfer-layer capacity accepts 32 and rejects 33 before traversal");
        SpectralPresetDraft maximumLayers;
        maximumLayers.transferLayers.assign (
            maximumTransferLayers,
            TransferLayer { .enabled = true, .definition = validPeakNotch() });
        expectSuccess (maximumLayers);
        auto excessiveLayers = maximumLayers;
        excessiveLayers.transferLayers.push_back (
            { .enabled = false, .definition = PeakNotch { quietNaN, 0.0, 1.0 } });
        expectFailure (excessiveLayers, ModelErrorCode::capacityExceeded,
                       ModelField::transferLayerCount);

        beginTest ("Declared expansion accepts exactly 256 partials");
        SpectralPresetDraft exactExpansion;
        auto generator = validHarmonicGenerator();
        generator.count = 255;
        exactExpansion.sources = { generator, validTrackedPartial() };
        expectSuccess (exactExpansion);

        beginTest ("Tracked and fixed partials each contribute one declared partial");
        SpectralPresetDraft mixedExpansion;
        auto firstGenerator = validFormulaGenerator();
        firstGenerator.count = 127;
        auto secondGenerator = validHarmonicGenerator();
        secondGenerator.count = 127;
        mixedExpansion.sources = {
            firstGenerator, secondGenerator, validTrackedPartial(), validFixedPartial()
        };
        expectSuccess (mixedExpansion);

        beginTest ("The source that reaches 257 reports exact expansion overflow location");
        auto excessiveExpansion = exactExpansion;
        excessiveExpansion.sources.push_back (validFixedPartial());
        expectFailure (excessiveExpansion, ModelErrorCode::capacityExceeded,
                       ModelField::expandedPartialCount, 2);

        beginTest ("Pathological declared counts are rejected before overflow arithmetic");
        SpectralPresetDraft overflowShaped;
        generator.count = std::numeric_limits<std::size_t>::max();
        overflowShaped.sources = { validTrackedPartial(), generator };
        expectFailure (overflowShaped, ModelErrorCode::countOutOfRange,
                       ModelField::count, 1);
    }

    void testValidatedPublicationAndAtomicFailures()
    {
        beginTest ("Successful validation preserves source variants and canonical order");
        SpectralPresetDraft draft;
        draft.sources = {
            TrackedPartial { 2.0, -1.0 },
            FixedPartial { 220.0, -2.0 },
            HarmonicGenerator { 3, -3.0, -4.0, -5.0, 0.006 },
            FormulaGenerator { FormulaFrequencyMode::fixedHz, 4,
                               "fundamental_hz * index", "-index" }
        };
        draft.transferLayers = {
            { .enabled = false, .definition = PeakNotch { 101.0, -1.0, 0.5 } },
            { .enabled = true, .definition = Shelf { ShelfKind::high, 202.0, 2.0, 0.7 } },
            { .enabled = false, .definition = Tilt { 303.0, 3.0 } }
        };
        const auto result = validatePreset (draft);
        expect (result.preset.has_value());
        if (result.preset)
        {
            const auto sources = result.preset->sources();
            expectEquals (sources.size(), std::size_t { 4 });
            if (sources.size() == 4)
            {
                const auto* tracked = std::get_if<TrackedPartial> (&sources[0]);
                const auto* fixed = std::get_if<FixedPartial> (&sources[1]);
                const auto* harmonic = std::get_if<HarmonicGenerator> (&sources[2]);
                const auto* formula = std::get_if<ValidatedFormulaGenerator> (&sources[3]);
                expect (tracked != nullptr);
                expect (fixed != nullptr);
                expect (harmonic != nullptr);
                expect (formula != nullptr);
                if (tracked != nullptr)
                    expectEquals (tracked->ratio, 2.0);
                if (fixed != nullptr)
                    expectEquals (fixed->frequencyHz, 220.0);
                if (harmonic != nullptr)
                    expectEquals (harmonic->count, std::size_t { 3 });
                if (formula != nullptr)
                    expect (formula->mode == FormulaFrequencyMode::fixedHz);
            }

            const auto layers = result.preset->transferLayers();
            expectEquals (layers.size(), std::size_t { 3 });
            if (layers.size() == 3)
            {
                expect (! layers[0].enabled);
                expect (layers[1].enabled);
                expect (! layers[2].enabled);
                const auto* peak = std::get_if<PeakNotch> (&layers[0].definition);
                const auto* shelf = std::get_if<Shelf> (&layers[1].definition);
                const auto* tilt = std::get_if<Tilt> (&layers[2].definition);
                expect (peak != nullptr);
                expect (shelf != nullptr);
                expect (tilt != nullptr);
                if (peak != nullptr)
                    expectEquals (peak->frequencyHz, 101.0);
                if (shelf != nullptr)
                {
                    expect (shelf->kind == ShelfKind::high);
                    expectEquals (shelf->frequencyHz, 202.0);
                }
                if (tilt != nullptr)
                    expectEquals (tilt->frequencyHz, 303.0);
            }
        }

        beginTest ("Disabled layers retain order but still require valid payloads");
        SpectralPresetDraft disabledInvalid;
        disabledInvalid.transferLayers = {
            { .enabled = true, .definition = validPeakNotch() },
            { .enabled = false, .definition = PeakNotch { 1000.0, 0.0, 0.0 } },
            { .enabled = true, .definition = validTilt() }
        };
        expectFailure (disabledInvalid, ModelErrorCode::valueOutOfRange, ModelField::q,
                       noIndex, 1);

        beginTest ("Invalid early, middle, or late sources never publish valid siblings");
        for (const auto invalidIndex : std::array<std::size_t, 3> { 0, 1, 2 })
        {
            SpectralPresetDraft atomic;
            atomic.sources = {
                TrackedPartial { 1.0, -1.0 },
                TrackedPartial { 2.0, -2.0 },
                TrackedPartial { 3.0, -3.0 }
            };
            std::get<TrackedPartial> (atomic.sources[invalidIndex]).levelDb = quietNaN;
            expectFailure (atomic, ModelErrorCode::nonFiniteValue, ModelField::levelDb,
                           invalidIndex);
        }

        beginTest ("Invalid early, middle, or late layers never publish valid siblings");
        for (const auto invalidIndex : std::array<std::size_t, 3> { 0, 1, 2 })
        {
            SpectralPresetDraft atomic;
            atomic.transferLayers = {
                { .enabled = true, .definition = validPeakNotch() },
                { .enabled = false, .definition = validPeakNotch() },
                { .enabled = true, .definition = validPeakNotch() }
            };
            std::get<PeakNotch> (atomic.transferLayers[invalidIndex].definition).q = quietNaN;
            expectFailure (atomic, ModelErrorCode::nonFiniteValue, ModelField::q,
                           noIndex, invalidIndex);
        }

        beginTest ("A bad middle free-curve point reports every exact location atomically");
        SpectralPresetDraft pointAtomic;
        pointAtomic.transferLayers = {
            { .enabled = true, .definition = validPeakNotch() },
            { .enabled = true,
              .definition = FreeCurve {
                  .points = { { 10.0, 0.0 }, { 100.0, quietNaN }, { 1000.0, 0.0 } }
              } },
            { .enabled = true, .definition = validTilt() }
        };
        expectFailure (pointAtomic, ModelErrorCode::nonFiniteValue, ModelField::responseDb,
                       noIndex, 1, 1);
    }

    struct FormulaErrorCase
    {
        std::string expression;
        ExpressionErrorCode code = ExpressionErrorCode::none;
    };
};

SpectralPresetModelTests spectralPresetModelTests;
}
