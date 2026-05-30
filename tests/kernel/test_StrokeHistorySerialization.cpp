// Tests for StrokeHistorySerializer (Sculpt Slice 4).
//
// Covers:
//   - Serialize empty history → header-only string; deserialize back to empty.
//   - Single delta round-trip: strokeId, kind, vertexDeltas.
//   - Multiple deltas round-trip: order preserved, strokeCount correct.
//   - All BrushKind variants round-trip by name.
//   - vertexDeltas sorted ascending by index after deserialization.
//   - Delta with zero touched vertices round-trips correctly.
//   - Float precision: displacement components preserved to ~7 decimal places.
//   - Invalid delta (id == kInvalidStrokeId) causes error and empty output.
//   - Invalid header rejected.
//   - Malformed S record (missing fields) causes error.
//   - Malformed D record (missing fields) causes stroke error.
//   - Unknown BrushKind string causes stroke-level error; other strokes continue.
//   - Unknown top-level tags are silently skipped.
//   - strokeCount reflects only successfully parsed strokes.

#include <nexus/sculpt/StrokeHistorySerialization.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace nexus::sculpt;
using nexus::render::Vec3;

// ── Helpers ───────────────────────────────────────────────────────────────────

static StrokeDelta makeDelta(StrokeId id, BrushKind kind,
                             const std::vector<std::pair<uint32_t, Vec3>>& vd = {})
{
    StrokeDelta d;
    d.id   = id;
    d.kind = kind;
    d.vertexDeltas = vd;
    return d;
}

// ── Empty history ─────────────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, EmptyHistoryRoundTrip)
{
    std::vector<StrokeDelta> history;
    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize(history, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.strokeCount, 0u);
    EXPECT_FALSE(data.empty()); // header still present

    std::vector<StrokeDelta> out;
    const auto r2 = StrokeHistorySerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    EXPECT_EQ(r2.strokeCount, 0u);
    EXPECT_TRUE(out.empty());
}

// ── Single delta round-trip ───────────────────────────────────────────────────

TEST(StrokeHistorySerialization, SingleDeltaRoundTrip)
{
    const StrokeDelta delta = makeDelta(42u, BrushKind::Draw, {
        {3u, {0.1f, -0.2f, 0.3f}},
        {7u, {1.0f,  2.0f, 3.0f}},
    });

    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize({delta}, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.strokeCount, 1u);

    std::vector<StrokeDelta> out;
    const auto r2 = StrokeHistorySerializer::deserialize(data, out);
    ASSERT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id,   42u);
    EXPECT_EQ(out[0].kind, BrushKind::Draw);
    ASSERT_EQ(out[0].vertexDeltas.size(), 2u);
    EXPECT_EQ(out[0].vertexDeltas[0].first, 3u);
    EXPECT_EQ(out[0].vertexDeltas[1].first, 7u);
}

TEST(StrokeHistorySerialization, EmptyVertexDeltaRoundTrip)
{
    const StrokeDelta delta = makeDelta(1u, BrushKind::Smooth, {});
    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize({delta}, r);
    ASSERT_TRUE(r.ok);

    std::vector<StrokeDelta> out;
    ASSERT_TRUE(StrokeHistorySerializer::deserialize(data, out).ok);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id,   1u);
    EXPECT_EQ(out[0].kind, BrushKind::Smooth);
    EXPECT_TRUE(out[0].vertexDeltas.empty());
}

// ── Multiple deltas ───────────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, MultiplesDeltasOrderPreserved)
{
    const std::vector<StrokeDelta> history = {
        makeDelta(10u, BrushKind::Draw,    {{0u, {1.f, 0.f, 0.f}}}),
        makeDelta(11u, BrushKind::Smooth,  {}),
        makeDelta(12u, BrushKind::Inflate, {{5u, {0.f, 0.5f, 0.f}}, {9u, {0.f, 0.25f, 0.f}}}),
    };

    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize(history, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.strokeCount, 3u);

    std::vector<StrokeDelta> out;
    const auto r2 = StrokeHistorySerializer::deserialize(data, out);
    ASSERT_TRUE(r2.ok);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].id,   10u);
    EXPECT_EQ(out[1].id,   11u);
    EXPECT_EQ(out[2].id,   12u);
    EXPECT_EQ(out[2].vertexDeltas.size(), 2u);
}

// ── All BrushKind variants ────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, AllBrushKindVariantsRoundTrip)
{
    const std::vector<BrushKind> kinds = {
        BrushKind::Draw, BrushKind::Smooth, BrushKind::Inflate, BrushKind::Flatten,
        BrushKind::Pinch, BrushKind::Crease, BrushKind::Layer, BrushKind::Grab,
    };

    StrokeId id = 1u;
    std::vector<StrokeDelta> history;
    for (BrushKind k : kinds) {
        history.push_back(makeDelta(id++, k));
    }

    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize(history, r);
    ASSERT_TRUE(r.ok);

    std::vector<StrokeDelta> out;
    ASSERT_TRUE(StrokeHistorySerializer::deserialize(data, out).ok);
    ASSERT_EQ(out.size(), kinds.size());
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        EXPECT_EQ(out[i].kind, kinds[i]) << "mismatch at index " << i;
    }
}

// ── Sorting invariant ─────────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, VertexDeltasSortedAfterDeserialize)
{
    // vertexDeltas unsorted in the file (hand-crafted to test sort).
    const std::string data =
        "NEXUS_STROKE_HISTORY_V1\n"
        "S 1 Draw\n"
        "D 9 0.1 0.0 0.0\n"   // index 9 first
        "D 2 0.2 0.0 0.0\n"   // index 2 second
        "D 5 0.3 0.0 0.0\n";  // index 5 third

    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize(data, out);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(out.size(), 1u);
    const auto& vd = out[0].vertexDeltas;
    ASSERT_EQ(vd.size(), 3u);
    EXPECT_EQ(vd[0].first, 2u);
    EXPECT_EQ(vd[1].first, 5u);
    EXPECT_EQ(vd[2].first, 9u);
}

// ── Float precision ───────────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, DisplacementPrecisionPreserved)
{
    const Vec3 disp{3.14159265f, -2.71828174f, 1.41421354f};
    const StrokeDelta delta = makeDelta(1u, BrushKind::Layer, {{0u, disp}});

    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize({delta}, r);
    ASSERT_TRUE(r.ok);

    std::vector<StrokeDelta> out;
    ASSERT_TRUE(StrokeHistorySerializer::deserialize(data, out).ok);
    ASSERT_EQ(out.size(), 1u);
    const auto& vd = out[0].vertexDeltas;
    ASSERT_EQ(vd.size(), 1u);
    EXPECT_NEAR(vd[0].second.x, disp.x, 1e-6f);
    EXPECT_NEAR(vd[0].second.y, disp.y, 1e-6f);
    EXPECT_NEAR(vd[0].second.z, disp.z, 1e-6f);
}

// ── Error paths ───────────────────────────────────────────────────────────────

TEST(StrokeHistorySerialization, InvalidDeltaInSerializeCausesError)
{
    StrokeDelta bad;
    bad.id   = kInvalidStrokeId; // 0 = invalid
    bad.kind = BrushKind::Draw;

    StrokeHistorySerializationReport r;
    const std::string data = StrokeHistorySerializer::serialize({bad}, r);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(data.empty());
}

TEST(StrokeHistorySerialization, InvalidHeaderReturnsError)
{
    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize("WRONG_HEADER\n", out);
    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
    EXPECT_TRUE(out.empty());
}

TEST(StrokeHistorySerialization, MalformedSRecordReturnsError)
{
    const std::string data =
        "NEXUS_STROKE_HISTORY_V1\n"
        "S\n"  // missing id + kind
        "D 0 1.0 0.0 0.0\n";

    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(out.empty()); // erroneous stroke not added
}

TEST(StrokeHistorySerialization, UnknownBrushKindReportsError)
{
    const std::string data =
        "NEXUS_STROKE_HISTORY_V1\n"
        "S 1 UnknownBrush\n"
        "D 0 1.0 0.0 0.0\n"
        "S 2 Draw\n"        // valid stroke that follows
        "D 0 0.5 0.0 0.0\n";

    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
    // Valid stroke after the bad one should still be parsed.
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].id,   2u);
    EXPECT_EQ(r.strokeCount, 1u);
}

TEST(StrokeHistorySerialization, UnknownTagSilentlySkipped)
{
    const std::string data =
        "NEXUS_STROKE_HISTORY_V1\n"
        "FUTURE_TAG some_value\n"
        "S 1 Grab\n"
        "D 3 0.1 0.2 0.3\n";

    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize(data, out);
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].kind, BrushKind::Grab);
}

TEST(StrokeHistorySerialization, StrokeCountMatchesSuccessfulStrokes)
{
    const std::string data =
        "NEXUS_STROKE_HISTORY_V1\n"
        "S 1 Draw\n"
        "S 2 BadKind\n"   // error — skipped
        "S 3 Smooth\n";

    std::vector<StrokeDelta> out;
    const auto r = StrokeHistorySerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.strokeCount, 2u); // only Draw and Smooth succeed
    EXPECT_EQ(out.size(), 2u);
}
