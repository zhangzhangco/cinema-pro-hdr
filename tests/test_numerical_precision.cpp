#include "test_framework.h"
#include "cinema_pro_hdr/color_space.h"
#include <cmath>
#include <limits>

using namespace CinemaProHDR;

TEST(NumericalUtils_IsFinite) {
    ASSERT_TRUE(NumericalUtils::IsFinite(0.0f));
    ASSERT_TRUE(NumericalUtils::IsFinite(1.0f));
    ASSERT_TRUE(NumericalUtils::IsFinite(-1.0f));
    ASSERT_TRUE(NumericalUtils::IsFinite(1e-10f));
    ASSERT_TRUE(NumericalUtils::IsFinite(1e10f));
    
    ASSERT_FALSE(NumericalUtils::IsFinite(std::numeric_limits<float>::infinity()));
    ASSERT_FALSE(NumericalUtils::IsFinite(-std::numeric_limits<float>::infinity()));
    ASSERT_FALSE(NumericalUtils::IsFinite(std::numeric_limits<float>::quiet_NaN()));
    
    return true;
}

TEST(NumericalUtils_IsFiniteRGB) {
    float valid_rgb[3] = {0.5f, 0.7f, 0.3f};
    ASSERT_TRUE(NumericalUtils::IsFiniteRGB(valid_rgb));
    
    float invalid_rgb[3] = {0.5f, std::numeric_limits<float>::infinity(), 0.3f};
    ASSERT_FALSE(NumericalUtils::IsFiniteRGB(invalid_rgb));
    
    float nan_rgb[3] = {0.5f, 0.7f, std::numeric_limits<float>::quiet_NaN()};
    ASSERT_FALSE(NumericalUtils::IsFiniteRGB(nan_rgb));
    
    return true;
}

TEST(NumericalUtils_SaturateRGB) {
    float rgb[3] = {-0.5f, 1.5f, 0.5f};
    NumericalUtils::SaturateRGB(rgb);
    
    ASSERT_EQ(0.0f, rgb[0]);
    ASSERT_EQ(1.0f, rgb[1]);
    ASSERT_EQ(0.5f, rgb[2]);
    
    return true;
}

TEST(NumericalUtils_SafePow) {
    ASSERT_NEAR(8.0f, NumericalUtils::SafePow(2.0f, 3.0f), 1e-6f);
    ASSERT_NEAR(1.0f, NumericalUtils::SafePow(5.0f, 0.0f), 1e-6f);
    
    // Test edge cases
    ASSERT_EQ(0.0f, NumericalUtils::SafePow(-1.0f, 2.0f)); // Negative base
    ASSERT_EQ(0.0f, NumericalUtils::SafePow(0.0f, 2.0f));  // Zero base
    
    return true;
}

TEST(NumericalUtils_SafeLog) {
    ASSERT_NEAR(0.0f, NumericalUtils::SafeLog(1.0f), 1e-6f);
    ASSERT_NEAR(std::log(2.0f), NumericalUtils::SafeLog(2.0f), 1e-6f);
    
    // Test edge cases
    ASSERT_EQ(-10.0f, NumericalUtils::SafeLog(0.0f));  // Zero input
    ASSERT_EQ(-10.0f, NumericalUtils::SafeLog(-1.0f)); // Negative input
    
    return true;
}

TEST(NumericalUtils_SafeDivide) {
    ASSERT_NEAR(2.0f, NumericalUtils::SafeDivide(6.0f, 3.0f), 1e-6f);
    
    // Test division by zero
    ASSERT_EQ(0.0f, NumericalUtils::SafeDivide(5.0f, 0.0f, 0.0f));
    ASSERT_EQ(10.0f, NumericalUtils::SafeDivide(5.0f, 0.0f, 10.0f));
    
    // Test very small denominator
    ASSERT_EQ(0.0f, NumericalUtils::SafeDivide(5.0f, 1e-10f, 0.0f));
    
    return true;
}

TEST(NumericalUtils_SmoothStep) {
    // Test boundary values
    ASSERT_EQ(0.0f, NumericalUtils::SmoothStep(0.0f, 1.0f, -0.5f));
    ASSERT_EQ(0.0f, NumericalUtils::SmoothStep(0.0f, 1.0f, 0.0f));
    ASSERT_EQ(1.0f, NumericalUtils::SmoothStep(0.0f, 1.0f, 1.0f));
    ASSERT_EQ(1.0f, NumericalUtils::SmoothStep(0.0f, 1.0f, 1.5f));
    
    // Test middle value
    float mid = NumericalUtils::SmoothStep(0.0f, 1.0f, 0.5f);
    ASSERT_EQ(0.5f, mid);
    
    // Test smoothness (derivative should be 0 at edges)
    float near_start = NumericalUtils::SmoothStep(0.0f, 1.0f, 0.01f);
    float near_end = NumericalUtils::SmoothStep(0.0f, 1.0f, 0.99f);
    ASSERT_LT(near_start, 0.01f); // Should be very small
    ASSERT_GT(near_end, 0.99f);   // Should be very close to 1
    
    return true;
}

TEST(NumericalUtils_Mix) {
    ASSERT_EQ(5.0f, NumericalUtils::Mix(5.0f, 10.0f, 0.0f));
    ASSERT_EQ(10.0f, NumericalUtils::Mix(5.0f, 10.0f, 1.0f));
    ASSERT_EQ(7.5f, NumericalUtils::Mix(5.0f, 10.0f, 0.5f));
    
    // Test clamping
    ASSERT_EQ(5.0f, NumericalUtils::Mix(5.0f, 10.0f, -0.5f));
    ASSERT_EQ(10.0f, NumericalUtils::Mix(5.0f, 10.0f, 1.5f));
    
    return true;
}

TEST(NumericalUtils_RangeValidation) {
    ASSERT_TRUE(NumericalUtils::IsInRange(0.5f, 0.0f, 1.0f));
    ASSERT_TRUE(NumericalUtils::IsInRange(0.0f, 0.0f, 1.0f));
    ASSERT_TRUE(NumericalUtils::IsInRange(1.0f, 0.0f, 1.0f));
    
    ASSERT_FALSE(NumericalUtils::IsInRange(-0.1f, 0.0f, 1.0f));
    ASSERT_FALSE(NumericalUtils::IsInRange(1.1f, 0.0f, 1.0f));
    
    return true;
}

TEST(NumericalUtils_ClampToRange) {
    ASSERT_EQ(0.5f, NumericalUtils::ClampToRange(0.5f, 0.0f, 1.0f));
    ASSERT_EQ(0.0f, NumericalUtils::ClampToRange(-0.5f, 0.0f, 1.0f));
    ASSERT_EQ(1.0f, NumericalUtils::ClampToRange(1.5f, 0.0f, 1.0f));
    
    return true;
}