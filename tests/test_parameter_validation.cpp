#include "test_framework.h"
#include "cinema_pro_hdr/processor.h"

using namespace CinemaProHDR;

TEST(ParamValidator_ValidPPRParams) {
    CphParams params;
    params.curve = CurveType::PPR;
    
    std::vector<ErrorReport> errors;
    ASSERT_TRUE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(0, errors.size());
    
    return true;
}

TEST(ParamValidator_ValidRLOGParams) {
    CphParams params;
    params.curve = CurveType::RLOG;
    
    std::vector<ErrorReport> errors;
    ASSERT_TRUE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(0, errors.size());
    
    return true;
}

TEST(ParamValidator_InvalidPivot) {
    CphParams params;
    params.pivot_pq = 0.04f; // Below minimum
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_GT(errors.size(), 0);
    
    // Check that pivot error is reported
    bool found_pivot_error = false;
    for (const auto& error : errors) {
        if (error.field_name == "pivot_pq") {
            found_pivot_error = true;
            break;
        }
    }
    ASSERT_TRUE(found_pivot_error);
    
    return true;
}

TEST(ParamValidator_InvalidPPRGamma) {
    CphParams params;
    params.curve = CurveType::PPR;
    params.gamma_s = 2.0f; // Above maximum
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidatePPRParams(params, errors));
    ASSERT_GT(errors.size(), 0);
    
    return true;
}

TEST(ParamValidator_InvalidRLOGParams) {
    CphParams params;
    params.curve = CurveType::RLOG;
    params.rlog_a = 0.5f; // Below minimum
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidateRLOGParams(params, errors));
    ASSERT_GT(errors.size(), 0);
    
    return true;
}

TEST(ParamValidator_MultiplErrors) {
    CphParams params;
    params.pivot_pq = -0.1f;    // Invalid
    params.gamma_s = 2.0f;      // Invalid
    params.black_lift = 0.05f;  // Invalid
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(3, errors.size()); // Should report all three errors
    
    return true;
}

TEST(ParamValidator_SoftKneeParams) {
    CphParams params;
    params.yknee = 0.9f;  // Below minimum
    params.alpha = 0.1f;  // Below minimum
    params.toe = 0.02f;   // Above maximum
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(3, errors.size());
    
    return true;
}

TEST(ParamValidator_SaturationParams) {
    CphParams params;
    params.sat_base = -0.5f; // Below minimum
    params.sat_hi = 3.0f;    // Above maximum
    
    std::vector<ErrorReport> errors;
    ASSERT_FALSE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(2, errors.size());
    
    return true;
}

TEST(ParamValidator_BoundaryValues) {
    CphParams params;
    
    // Test exact boundary values (should be valid)
    params.pivot_pq = 0.05f;    // Minimum
    params.gamma_s = 1.0f;      // Minimum
    params.gamma_h = 1.4f;      // Maximum
    params.shoulder_h = 3.0f;   // Maximum
    
    std::vector<ErrorReport> errors;
    ASSERT_TRUE(ParamValidator::ValidateCphParams(params, errors));
    ASSERT_EQ(0, errors.size());
    
    return true;
}