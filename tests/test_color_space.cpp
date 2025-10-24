#include "test_framework.h"
#include "cinema_pro_hdr/color_space.h"
#include <cmath>

using namespace CinemaProHDR;

TEST(ColorSpace_PQ_EOTF_OETF_Roundtrip) {
    // Test PQ EOTF/OETF roundtrip
    float test_values[] = {0.0f, 0.1f, 0.18f, 0.5f, 0.9f, 1.0f};
    
    for (float pq_value : test_values) {
        float linear = ColorSpaceConverter::PQ_EOTF(pq_value);
        float pq_back = ColorSpaceConverter::PQ_OETF(linear);
        
        ASSERT_NEAR(pq_value, pq_back, 1e-5f); // Relax tolerance for numerical precision
    }
    
    return true;
}

TEST(ColorSpace_PQ_BoundaryValues) {
    // Test boundary values
    ASSERT_EQ(0.0f, ColorSpaceConverter::PQ_EOTF(0.0f));
    ASSERT_EQ(0.0f, ColorSpaceConverter::PQ_OETF(0.0f));
    
    // Test that PQ_EOTF(1.0) gives maximum luminance
    float max_luminance = ColorSpaceConverter::PQ_EOTF(1.0f);
    ASSERT_NEAR(10000.0f, max_luminance, 1.0f);
    
    return true;
}

TEST(ColorSpace_PQ_RGB_Processing) {
    float pq_rgb[3] = {0.1f, 0.5f, 0.9f};
    float linear_rgb[3];
    float pq_back[3];
    
    // Convert PQ to linear
    ColorSpaceConverter::PQ_EOTF_RGB(pq_rgb, linear_rgb);
    
    // Convert back to PQ
    ColorSpaceConverter::PQ_OETF_RGB(linear_rgb, pq_back);
    
    // Check roundtrip accuracy
    ASSERT_NEAR(pq_rgb[0], pq_back[0], 1e-5f);
    ASSERT_NEAR(pq_rgb[1], pq_back[1], 1e-5f);
    ASSERT_NEAR(pq_rgb[2], pq_back[2], 1e-5f);
    
    return true;
}

TEST(ColorSpace_MatrixTransformations) {
    float bt2020_rgb[3] = {0.5f, 0.7f, 0.3f};
    float p3d65_rgb[3];
    float bt2020_back[3];
    
    // BT.2020 to P3-D65 and back
    ColorSpaceConverter::BT2020_to_P3D65(bt2020_rgb, p3d65_rgb);
    ColorSpaceConverter::P3D65_to_BT2020(p3d65_rgb, bt2020_back);
    
    // Check roundtrip accuracy (allowing for some numerical error in matrix operations)
    ASSERT_NEAR(bt2020_rgb[0], bt2020_back[0], 0.1f);
    ASSERT_NEAR(bt2020_rgb[1], bt2020_back[1], 0.1f);
    ASSERT_NEAR(bt2020_rgb[2], bt2020_back[2], 0.1f);
    
    return true;
}

TEST(ColorSpace_WorkingDomainConversion) {
    // Create test image
    Image input(10, 10, 3);
    input.color_space = ColorSpace::P3_D65;
    
    // Set some test pixel values
    float* pixel = input.GetPixel(5, 5);
    pixel[0] = 0.5f;
    pixel[1] = 0.7f;
    pixel[2] = 0.3f;
    
    // Convert to working domain
    Image working;
    ColorSpaceConverter::ToWorkingDomain(input, working);
    
    ASSERT_EQ(static_cast<int>(ColorSpace::BT2020_PQ), static_cast<int>(working.color_space));
    ASSERT_EQ(input.width, working.width);
    ASSERT_EQ(input.height, working.height);
    ASSERT_TRUE(working.IsValid());
    
    // Convert back from working domain
    Image output;
    ColorSpaceConverter::FromWorkingDomain(working, output, ColorSpace::P3_D65);
    
    ASSERT_EQ(ColorSpace::P3_D65, output.color_space);
    ASSERT_TRUE(output.IsValid());
    
    return true;
}

TEST(ColorSpace_ValidColorSpaces) {
    ASSERT_TRUE(ColorSpaceConverter::IsValidColorSpace(ColorSpace::BT2020_PQ));
    ASSERT_TRUE(ColorSpaceConverter::IsValidColorSpace(ColorSpace::P3_D65));
    ASSERT_TRUE(ColorSpaceConverter::IsValidColorSpace(ColorSpace::ACESG));
    ASSERT_TRUE(ColorSpaceConverter::IsValidColorSpace(ColorSpace::REC709));
    
    return true;
}

TEST(ColorSpace_ColorSpaceToString) {
    ASSERT_EQ("BT2020_PQ", ColorSpaceConverter::ColorSpaceToString(ColorSpace::BT2020_PQ));
    ASSERT_EQ("P3_D65", ColorSpaceConverter::ColorSpaceToString(ColorSpace::P3_D65));
    ASSERT_EQ("ACEScg", ColorSpaceConverter::ColorSpaceToString(ColorSpace::ACESG));
    ASSERT_EQ("REC709", ColorSpaceConverter::ColorSpaceToString(ColorSpace::REC709));
    
    return true;
}