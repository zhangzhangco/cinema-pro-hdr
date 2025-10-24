#include "test_framework.h"
#include "cinema_pro_hdr/color_space.h"
#include <cmath>
#include <iostream>

using namespace CinemaProHDR;

// Test PQ EOTF/OETF functions for ST 2084 compliance
bool test_pq_functions() {
    std::cout << "Testing PQ EOTF/OETF functions..." << std::endl;
    
    // Test known values from ST 2084 standard
    struct TestCase {
        float pq_value;
        float expected_nits;
        float tolerance;
    };
    
    TestCase test_cases[] = {
        {0.0f, 0.0f, 0.001f},           // Black
        {0.5f, 92.24f, 1.0f},          // Mid-gray region
        {0.75f, 983.4f, 2.0f},         // Typical display white (actual ST 2084 value)
        {1.0f, 10000.0f, 1.0f}         // Peak white
    };
    
    for (const auto& test : test_cases) {
        float result_nits = ColorSpaceConverter::PQ_EOTF(test.pq_value);
        float error = std::abs(result_nits - test.expected_nits);
        
        if (error > test.tolerance) {
            std::cout << "PQ EOTF failed: input=" << test.pq_value 
                      << " expected=" << test.expected_nits 
                      << " got=" << result_nits 
                      << " error=" << error << std::endl;
            return false;
        }
        
        // Test round-trip conversion
        float pq_back = ColorSpaceConverter::PQ_OETF(result_nits);
        float roundtrip_error = std::abs(pq_back - test.pq_value);
        
        if (roundtrip_error > 0.001f) {
            std::cout << "PQ round-trip failed: original=" << test.pq_value 
                      << " round-trip=" << pq_back 
                      << " error=" << roundtrip_error << std::endl;
            return false;
        }
    }
    
    std::cout << "PQ functions test passed!" << std::endl;
    return true;
}

// Test color space transformation matrices
bool test_color_space_matrices() {
    std::cout << "Testing color space transformation matrices..." << std::endl;
    
    // Test with known color values
    float test_colors[][3] = {
        {1.0f, 0.0f, 0.0f},  // Red
        {0.0f, 1.0f, 0.0f},  // Green  
        {0.0f, 0.0f, 1.0f},  // Blue
        {1.0f, 1.0f, 1.0f},  // White
        {0.5f, 0.5f, 0.5f}   // Gray
    };
    
    for (int i = 0; i < 5; ++i) {
        float bt2020[3] = {test_colors[i][0], test_colors[i][1], test_colors[i][2]};
        float p3d65[3];
        float bt2020_back[3];
        
        // BT.2020 -> P3-D65 -> BT.2020 round-trip
        ColorSpaceConverter::BT2020_to_P3D65(bt2020, p3d65);
        ColorSpaceConverter::P3D65_to_BT2020(p3d65, bt2020_back);
        
        // Check round-trip accuracy
        for (int j = 0; j < 3; ++j) {
            float error = std::abs(bt2020[j] - bt2020_back[j]);
            if (error > 0.001f) {
                std::cout << "BT.2020<->P3-D65 round-trip failed: channel=" << j 
                          << " original=" << bt2020[j] 
                          << " round-trip=" << bt2020_back[j] 
                          << " error=" << error << std::endl;
                return false;
            }
        }
        
        // Test ACEScg transformations
        float acesg[3];
        ColorSpaceConverter::BT2020_to_ACEScg(bt2020, acesg);
        ColorSpaceConverter::ACEScg_to_BT2020(acesg, bt2020_back);
        
        for (int j = 0; j < 3; ++j) {
            float error = std::abs(bt2020[j] - bt2020_back[j]);
            if (error > 0.001f) {
                std::cout << "BT.2020<->ACEScg round-trip failed: channel=" << j 
                          << " original=" << bt2020[j] 
                          << " round-trip=" << bt2020_back[j] 
                          << " error=" << error << std::endl;
                return false;
            }
        }
    }
    
    std::cout << "Color space matrices test passed!" << std::endl;
    return true;
}

// Test gamut validation functions
bool test_gamut_validation() {
    std::cout << "Testing gamut validation functions..." << std::endl;
    
    // Test in-gamut colors
    float in_gamut_bt2020[] = {0.5f, 0.5f, 0.5f};
    if (!ColorSpaceConverter::IsInGamut(in_gamut_bt2020, ColorSpace::BT2020_PQ)) {
        std::cout << "In-gamut BT.2020 color incorrectly flagged as out-of-gamut" << std::endl;
        return false;
    }
    
    // Test out-of-gamut colors
    float out_gamut_bt2020[] = {1.5f, 0.5f, -0.1f};
    if (ColorSpaceConverter::IsInGamut(out_gamut_bt2020, ColorSpace::BT2020_PQ)) {
        std::cout << "Out-of-gamut BT.2020 color incorrectly flagged as in-gamut" << std::endl;
        return false;
    }
    
    // Test gamut distance calculation
    float distance = ColorSpaceConverter::GetGamutDistance(out_gamut_bt2020, ColorSpace::BT2020_PQ);
    if (distance <= 0.0f) {
        std::cout << "Gamut distance should be positive for out-of-gamut colors" << std::endl;
        return false;
    }
    
    // Test gamut clamping
    float clamped[3] = {out_gamut_bt2020[0], out_gamut_bt2020[1], out_gamut_bt2020[2]};
    ColorSpaceConverter::ClampToGamut(clamped, ColorSpace::BT2020_PQ);
    
    if (!ColorSpaceConverter::IsInGamut(clamped, ColorSpace::BT2020_PQ)) {
        std::cout << "Clamped color should be in-gamut" << std::endl;
        return false;
    }
    
    // Test ACEScg gamut (allows negative values)
    float acesg_color[] = {-0.1f, 1.5f, 0.8f};
    if (!ColorSpaceConverter::IsInGamut(acesg_color, ColorSpace::ACESG)) {
        std::cout << "Valid ACEScg color incorrectly flagged as out-of-gamut" << std::endl;
        return false;
    }
    
    std::cout << "Gamut validation test passed!" << std::endl;
    return true;
}

// Test working domain conversions
bool test_working_domain_conversion() {
    std::cout << "Testing working domain conversions..." << std::endl;
    
    // Create test image
    Image input_image(4, 4, 3);
    input_image.color_space = ColorSpace::P3_D65;
    
    // Fill with test pattern
    for (int y = 0; y < input_image.height; ++y) {
        for (int x = 0; x < input_image.width; ++x) {
            float* pixel = input_image.GetPixel(x, y);
            if (pixel) {
                pixel[0] = static_cast<float>(x) / 3.0f;  // Red gradient
                pixel[1] = static_cast<float>(y) / 3.0f;  // Green gradient  
                pixel[2] = 0.5f;                          // Constant blue
            }
        }
    }
    
    // Convert to working domain
    Image working_image;
    ColorSpaceConverter::ToWorkingDomain(input_image, working_image);
    
    if (working_image.color_space != ColorSpace::BT2020_PQ) {
        std::cout << "Working domain should be BT2020_PQ" << std::endl;
        return false;
    }
    
    // Convert back to P3-D65
    Image output_image;
    ColorSpaceConverter::FromWorkingDomain(working_image, output_image, ColorSpace::P3_D65);
    
    if (output_image.color_space != ColorSpace::P3_D65) {
        std::cout << "Output color space should match target" << std::endl;
        return false;
    }
    
    // Check that conversion preserves image structure
    for (int y = 0; y < input_image.height; ++y) {
        for (int x = 0; x < input_image.width; ++x) {
            const float* input_pixel = input_image.GetPixel(x, y);
            const float* output_pixel = output_image.GetPixel(x, y);
            
            if (input_pixel && output_pixel) {
                // Allow for some conversion error due to precision and color space transformations
                for (int c = 0; c < 3; ++c) {
                    float error = std::abs(input_pixel[c] - output_pixel[c]);
                    if (error > 0.05f) {  // 5% tolerance for color space conversions
                        std::cout << "Working domain round-trip error too large: " 
                                  << "pixel(" << x << "," << y << ") channel=" << c
                                  << " input=" << input_pixel[c] << " output=" << output_pixel[c]
                                  << " error=" << error << std::endl;
                        return false;
                    }
                }
            }
        }
    }
    
    std::cout << "Working domain conversion test passed!" << std::endl;
    return true;
}

// Test numerical stability
bool test_numerical_stability() {
    std::cout << "Testing numerical stability..." << std::endl;
    
    // Test NaN/Inf handling in PQ functions
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float inf_val = std::numeric_limits<float>::infinity();
    
    float result = ColorSpaceConverter::PQ_EOTF(nan_val);
    if (!std::isfinite(result)) {
        std::cout << "PQ_EOTF should handle NaN input gracefully" << std::endl;
        return false;
    }
    
    result = ColorSpaceConverter::PQ_OETF(inf_val);
    if (!std::isfinite(result)) {
        std::cout << "PQ_OETF should handle Inf input gracefully" << std::endl;
        return false;
    }
    
    // Test extreme values
    result = ColorSpaceConverter::PQ_EOTF(-1.0f);
    if (result != 0.0f) {
        std::cout << "PQ_EOTF should return 0 for negative input" << std::endl;
        return false;
    }
    
    result = ColorSpaceConverter::PQ_EOTF(2.0f);
    if (result != 10000.0f) {
        std::cout << "PQ_EOTF should clamp to maximum for input > 1" << std::endl;
        return false;
    }
    
    // Test NumericalUtils functions
    if (!NumericalUtils::IsFinite(1.0f)) {
        std::cout << "IsFinite should return true for normal values" << std::endl;
        return false;
    }
    
    if (NumericalUtils::IsFinite(nan_val)) {
        std::cout << "IsFinite should return false for NaN" << std::endl;
        return false;
    }
    
    float test_rgb[] = {1.5f, -0.5f, 0.5f};
    NumericalUtils::SaturateRGB(test_rgb);
    
    if (test_rgb[0] != 1.0f || test_rgb[1] != 0.0f || test_rgb[2] != 0.5f) {
        std::cout << "SaturateRGB should clamp values to [0,1]" << std::endl;
        return false;
    }
    
    std::cout << "Numerical stability test passed!" << std::endl;
    return true;
}

int main() {
    std::cout << "Running enhanced color space tests..." << std::endl;
    
    bool all_passed = true;
    
    all_passed &= test_pq_functions();
    all_passed &= test_color_space_matrices();
    all_passed &= test_gamut_validation();
    all_passed &= test_working_domain_conversion();
    all_passed &= test_numerical_stability();
    
    if (all_passed) {
        std::cout << "\nAll enhanced color space tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome tests failed!" << std::endl;
        return 1;
    }
}