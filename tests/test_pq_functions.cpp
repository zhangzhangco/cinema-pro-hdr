#include "test_framework.h"
#include "cinema_pro_hdr/color_space.h"

using namespace CinemaProHDR;

TEST(PQFunctions_KnownValues) {
    // Test known PQ values
    // PQ(100 nits) should be approximately 0.508
    float pq_100 = ColorSpaceConverter::PQ_OETF(100.0f);
    ASSERT_NEAR(0.508f, pq_100, 0.01f);
    
    // PQ(1000 nits) should be approximately 0.75
    float pq_1000 = ColorSpaceConverter::PQ_OETF(1000.0f);
    ASSERT_NEAR(0.75f, pq_1000, 0.01f);
    
    return true;
}

TEST(PQFunctions_MonotonicityCheck) {
    // PQ function should be monotonic
    float prev_pq = 0.0f;
    
    for (int i = 1; i <= 100; ++i) {
        float nits = i * 100.0f; // 100, 200, ..., 10000 nits
        float pq = ColorSpaceConverter::PQ_OETF(nits);
        
        ASSERT_GT(pq, prev_pq); // Should be strictly increasing
        prev_pq = pq;
    }
    
    return true;
}

TEST(PQFunctions_InverseMonotonicity) {
    // PQ EOTF should also be monotonic
    float prev_nits = 0.0f;
    
    for (int i = 1; i <= 100; ++i) {
        float pq = i * 0.01f; // 0.01, 0.02, ..., 1.0
        float nits = ColorSpaceConverter::PQ_EOTF(pq);
        
        ASSERT_GT(nits, prev_nits); // Should be strictly increasing
        prev_nits = nits;
    }
    
    return true;
}

TEST(PQFunctions_EdgeCases) {
    // Test negative input
    ASSERT_EQ(0.0f, ColorSpaceConverter::PQ_EOTF(-0.1f));
    ASSERT_EQ(0.0f, ColorSpaceConverter::PQ_OETF(-100.0f));
    
    // Test values above maximum
    float high_pq = ColorSpaceConverter::PQ_EOTF(1.1f);
    ASSERT_NEAR(10000.0f, high_pq, 1.0f);
    
    float high_nits = ColorSpaceConverter::PQ_OETF(15000.0f);
    ASSERT_NEAR(1.0f, high_nits, 0.01f);
    
    return true;
}

TEST(PQFunctions_PrecisionTest) {
    // Test precision around common values
    float test_nits[] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 4000.0f, 10000.0f};
    
    for (float nits : test_nits) {
        float pq = ColorSpaceConverter::PQ_OETF(nits);
        float nits_back = ColorSpaceConverter::PQ_EOTF(pq);
        
        // Allow for some numerical precision loss
        float relative_error = std::abs(nits - nits_back) / nits;
        ASSERT_LT(relative_error, 5e-5f); // Relax precision for numerical stability in PQ functions
    }
    
    return true;
}