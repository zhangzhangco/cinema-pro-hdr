#include "test_framework.h"
#include "cinema_pro_hdr/core.h"

using namespace CinemaProHDR;

TEST(CphParams_DefaultValues) {
    CphParams params;
    
    ASSERT_TRUE(params.IsValid());
    ASSERT_EQ(static_cast<int>(CurveType::PPR), static_cast<int>(params.curve));
    ASSERT_GT(params.pivot_pq, 0.05f);
    ASSERT_LT(params.pivot_pq, 0.30f);
    
    return true;
}

TEST(CphParams_ValidationRanges) {
    CphParams params;
    
    // Test pivot range
    params.pivot_pq = 0.04f; // Below minimum
    ASSERT_FALSE(params.IsValid());
    
    params.pivot_pq = 0.31f; // Above maximum
    ASSERT_FALSE(params.IsValid());
    
    params.pivot_pq = 0.18f; // Valid
    ASSERT_TRUE(params.IsValid());
    
    return true;
}

TEST(CphParams_ClampToValidRange) {
    CphParams params;
    
    // Set invalid values
    params.pivot_pq = -0.1f;
    params.gamma_s = 2.0f;
    params.gamma_h = 0.5f;
    params.shoulder_h = 5.0f;
    
    ASSERT_FALSE(params.IsValid());
    
    // Clamp to valid range
    params.ClampToValidRange();
    
    ASSERT_TRUE(params.IsValid());
    ASSERT_EQ(0.05f, params.pivot_pq);
    ASSERT_EQ(1.6f, params.gamma_s);
    ASSERT_EQ(0.8f, params.gamma_h);
    ASSERT_EQ(3.0f, params.shoulder_h);
    
    return true;
}

TEST(CphParams_PPRParameters) {
    CphParams params;
    params.curve = CurveType::PPR;
    
    // Test gamma_s range
    params.gamma_s = 0.9f; // Below minimum
    ASSERT_FALSE(params.IsValid());
    
    params.gamma_s = 1.7f; // Above maximum
    ASSERT_FALSE(params.IsValid());
    
    params.gamma_s = 1.3f; // Valid
    ASSERT_TRUE(params.IsValid());
    
    return true;
}

TEST(CphParams_RLOGParameters) {
    CphParams params;
    params.curve = CurveType::RLOG;
    
    // Test rlog_a range
    params.rlog_a = 0.5f; // Below minimum
    ASSERT_FALSE(params.IsValid());
    
    params.rlog_a = 17.0f; // Above maximum
    ASSERT_FALSE(params.IsValid());
    
    params.rlog_a = 8.0f; // Valid
    ASSERT_TRUE(params.IsValid());
    
    return true;
}