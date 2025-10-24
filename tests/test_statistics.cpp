#include "test_framework.h"
#include "cinema_pro_hdr/core.h"

using namespace CinemaProHDR;

TEST(Statistics_DefaultValues) {
    Statistics stats;
    
    ASSERT_EQ(0.0f, stats.pq_stats.min_pq);
    ASSERT_EQ(0.0f, stats.pq_stats.avg_pq);
    ASSERT_EQ(1.0f, stats.pq_stats.max_pq);
    ASSERT_EQ(0.0f, stats.pq_stats.variance);
    ASSERT_TRUE(stats.monotonic);
    ASSERT_TRUE(stats.c1_continuous);
    ASSERT_EQ(0.0f, stats.max_derivative_gap);
    ASSERT_EQ(0, stats.frame_count);
    
    return true;
}

TEST(Statistics_Reset) {
    Statistics stats;
    
    // Modify values
    stats.pq_stats.min_pq = 0.1f;
    stats.pq_stats.avg_pq = 0.5f;
    stats.pq_stats.max_pq = 0.9f;
    stats.pq_stats.variance = 0.1f;
    stats.monotonic = false;
    stats.c1_continuous = false;
    stats.max_derivative_gap = 0.01f;
    stats.frame_count = 100;
    
    // Reset
    stats.Reset();
    
    // Check reset values
    ASSERT_EQ(0.0f, stats.pq_stats.min_pq);
    ASSERT_EQ(0.0f, stats.pq_stats.avg_pq);
    ASSERT_EQ(1.0f, stats.pq_stats.max_pq);
    ASSERT_EQ(0.0f, stats.pq_stats.variance);
    ASSERT_TRUE(stats.monotonic);
    ASSERT_TRUE(stats.c1_continuous);
    ASSERT_EQ(0.0f, stats.max_derivative_gap);
    ASSERT_EQ(0, stats.frame_count);
    
    return true;
}

TEST(Statistics_Validation) {
    Statistics stats;
    
    // Valid statistics
    stats.pq_stats.min_pq = 0.1f;
    stats.pq_stats.avg_pq = 0.5f;
    stats.pq_stats.max_pq = 0.9f;
    stats.pq_stats.variance = 0.05f;
    stats.max_derivative_gap = 0.001f;
    stats.frame_count = 10;
    
    ASSERT_TRUE(stats.IsValid());
    
    return true;
}

TEST(Statistics_InvalidRanges) {
    Statistics stats;
    
    // Test invalid PQ ranges
    stats.pq_stats.min_pq = -0.1f; // Below 0
    ASSERT_FALSE(stats.IsValid());
    
    stats.pq_stats.min_pq = 0.1f;
    stats.pq_stats.max_pq = 1.1f; // Above 1
    ASSERT_FALSE(stats.IsValid());
    
    stats.pq_stats.max_pq = 0.9f;
    stats.pq_stats.avg_pq = 1.0f; // avg > max
    ASSERT_FALSE(stats.IsValid());
    
    return true;
}

TEST(Statistics_LogicalConsistency) {
    Statistics stats;
    
    // Test min > avg
    stats.pq_stats.min_pq = 0.6f;
    stats.pq_stats.avg_pq = 0.5f;
    stats.pq_stats.max_pq = 0.9f;
    ASSERT_FALSE(stats.IsValid());
    
    // Test avg > max
    stats.pq_stats.min_pq = 0.1f;
    stats.pq_stats.avg_pq = 0.8f;
    stats.pq_stats.max_pq = 0.7f;
    ASSERT_FALSE(stats.IsValid());
    
    // Test valid ordering
    stats.pq_stats.min_pq = 0.1f;
    stats.pq_stats.avg_pq = 0.5f;
    stats.pq_stats.max_pq = 0.9f;
    ASSERT_TRUE(stats.IsValid());
    
    return true;
}