#include "test_framework.h"
#include "cinema_pro_hdr/core.h"

using namespace CinemaProHDR;

TEST(Image_Construction) {
    Image img(1920, 1080, 3);
    
    ASSERT_EQ(1920, img.width);
    ASSERT_EQ(1080, img.height);
    ASSERT_EQ(3, img.channels);
    ASSERT_EQ(1920 * 1080 * 3, img.data.size());
    ASSERT_TRUE(img.IsValid());
    
    return true;
}

TEST(Image_PixelAccess) {
    Image img(10, 10, 3);
    
    // Test valid pixel access
    float* pixel = img.GetPixel(5, 5);
    ASSERT_TRUE(pixel != nullptr);
    
    // Set pixel values
    pixel[0] = 0.5f; // R
    pixel[1] = 0.7f; // G
    pixel[2] = 0.3f; // B
    
    // Read back values
    const float* const_pixel = img.GetPixel(5, 5);
    ASSERT_TRUE(const_pixel != nullptr);
    ASSERT_EQ(0.5f, const_pixel[0]);
    ASSERT_EQ(0.7f, const_pixel[1]);
    ASSERT_EQ(0.3f, const_pixel[2]);
    
    return true;
}

TEST(Image_BoundaryChecks) {
    Image img(10, 10, 3);
    
    // Test out-of-bounds access
    ASSERT_TRUE(img.GetPixel(-1, 5) == nullptr);
    ASSERT_TRUE(img.GetPixel(5, -1) == nullptr);
    ASSERT_TRUE(img.GetPixel(10, 5) == nullptr);
    ASSERT_TRUE(img.GetPixel(5, 10) == nullptr);
    
    // Test valid boundary access
    ASSERT_TRUE(img.GetPixel(0, 0) != nullptr);
    ASSERT_TRUE(img.GetPixel(9, 9) != nullptr);
    
    return true;
}

TEST(Image_Validation) {
    // Valid image
    Image valid_img(100, 100, 3);
    ASSERT_TRUE(valid_img.IsValid());
    
    // Invalid dimensions
    Image invalid_img;
    invalid_img.width = 0;
    invalid_img.height = 100;
    invalid_img.channels = 3;
    ASSERT_FALSE(invalid_img.IsValid());
    
    return true;
}

TEST(Image_Clear) {
    Image img(10, 10, 3);
    
    // Set some non-zero values
    float* pixel = img.GetPixel(5, 5);
    pixel[0] = 1.0f;
    pixel[1] = 0.8f;
    pixel[2] = 0.6f;
    
    // Clear the image
    img.Clear();
    
    // Check that all values are zero
    const float* cleared_pixel = img.GetPixel(5, 5);
    ASSERT_EQ(0.0f, cleared_pixel[0]);
    ASSERT_EQ(0.0f, cleared_pixel[1]);
    ASSERT_EQ(0.0f, cleared_pixel[2]);
    
    return true;
}