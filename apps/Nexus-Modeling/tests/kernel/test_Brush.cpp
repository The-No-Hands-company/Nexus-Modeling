#include <gtest/gtest.h>

#include <nexus/sculpt/Brush.h>

#include <cmath>
#include <limits>

using namespace nexus::sculpt;

TEST(BrushFalloff, ConstantReturnsOne)
{
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Constant, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Constant, 0.5f), 1.0f);
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Constant, 1.0f), 1.0f);
}

TEST(BrushFalloff, LinearDecreasesToZero)
{
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Linear, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Linear, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(evaluateFalloff(FalloffShape::Linear, 1.0f), 0.0f);
}

TEST(BrushFalloff, SmoothIsCubicSmoothstep)
{
    float v0 = evaluateFalloff(FalloffShape::Smooth, 0.0f);
    float v1 = evaluateFalloff(FalloffShape::Smooth, 1.0f);
    EXPECT_FLOAT_EQ(v0, 1.0f);
    EXPECT_FLOAT_EQ(v1, 0.0f);

    float mid = evaluateFalloff(FalloffShape::Smooth, 0.5f);
    EXPECT_GT(mid, 0.0f);
    EXPECT_LT(mid, 1.0f);
}

TEST(BrushFalloff, SharpDecaysQuadratically)
{
    float v0 = evaluateFalloff(FalloffShape::Sharp, 0.0f);
    float v1 = evaluateFalloff(FalloffShape::Sharp, 1.0f);
    EXPECT_FLOAT_EQ(v0, 1.0f);
    EXPECT_FLOAT_EQ(v1, 0.0f);

    float mid = evaluateFalloff(FalloffShape::Sharp, 0.5f);
    EXPECT_FLOAT_EQ(mid, 0.25f);
}

TEST(BrushFalloff, ClampedToUnitRange)
{
    float lo = evaluateFalloff(FalloffShape::Smooth, -0.3f);
    float hi = evaluateFalloff(FalloffShape::Smooth, 1.5f);
    EXPECT_FLOAT_EQ(lo, 1.0f);
    EXPECT_FLOAT_EQ(hi, 0.0f);
}

TEST(BrushFalloff, AllShapesReachZeroAtOne)
{
    for (auto shape : {FalloffShape::Constant, FalloffShape::Linear,
                       FalloffShape::Smooth, FalloffShape::Sharp}) {
        EXPECT_GE(evaluateFalloff(shape, 1.0f), 0.0f);
    }
}

TEST(BrushParams, DefaultsAreSane)
{
    BrushParams p;
    EXPECT_GT(p.radius, 0.0f);
    EXPECT_GT(p.strength, 0.0f);
    EXPECT_LE(p.strength, 1.0f);
    EXPECT_FLOAT_EQ(p.maxPerVertexDisplacement, 0.0f);
    EXPECT_TRUE(p.useVertexNormal);
}

TEST(BrushSample, DefaultPressureIsOne)
{
    BrushSample s;
    EXPECT_FLOAT_EQ(s.pressure, 1.0f);
    EXPECT_EQ(s.sequence, 0u);
}
