#include "test_framework.h"
#include "cinema_pro_hdr/processor.h"

using namespace CinemaProHDR;

TEST(Processor_Initialization) {
    CphProcessor processor;
    CphParams params; // Use default parameters
    
    ASSERT_TRUE(processor.Initialize(params));
    
    return true;
}

TEST(Processor_InvalidParameters) {
    CphProcessor processor;
    CphParams params;
    
    // Set invalid parameters
    params.pivot_pq = -0.1f; // Invalid
    
    ASSERT_FALSE(processor.Initialize(params));
    
    // Check that error was logged
    std::string error = processor.GetLastError();
    ASSERT_FALSE(error.empty());
    
    return true;
}

TEST(Processor_ParameterValidation) {
    CphProcessor processor;
    CphParams valid_params;
    CphParams invalid_params;
    
    // Valid parameters
    ASSERT_TRUE(processor.ValidateParams(valid_params));
    
    // Invalid parameters
    invalid_params.gamma_s = 2.0f; // Out of range
    ASSERT_FALSE(processor.ValidateParams(invalid_params));
    
    return true;
}

TEST(Processor_BasicFrameProcessing) {
    CphProcessor processor;
    CphParams params;
    
    ASSERT_TRUE(processor.Initialize(params));
    
    // Create test image
    Image input(100, 100, 3);
    input.color_space = ColorSpace::BT2020_PQ;
    
    // Set some test pixel values
    float* pixel = input.GetPixel(50, 50);
    pixel[0] = 0.5f;
    pixel[1] = 0.7f;
    pixel[2] = 0.3f;
    
    Image output;
    ASSERT_TRUE(processor.ProcessFrame(input, output));
    
    // Check output validity
    ASSERT_TRUE(output.IsValid());
    ASSERT_EQ(input.width, output.width);
    ASSERT_EQ(input.height, output.height);
    ASSERT_EQ(input.channels, output.channels);
    
    return true;
}

TEST(Processor_StatisticsCollection) {
    CphProcessor processor;
    CphParams params;
    
    ASSERT_TRUE(processor.Initialize(params));
    
    // Process a frame
    Image input(50, 50, 3);
    input.color_space = ColorSpace::BT2020_PQ;
    
    // Fill with gradient values
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float* pixel = input.GetPixel(x, y);
            float value = (float)(x + y) / (input.width + input.height);
            pixel[0] = value;
            pixel[1] = value * 0.8f;
            pixel[2] = value * 0.6f;
        }
    }
    
    Image output;
    ASSERT_TRUE(processor.ProcessFrame(input, output));
    
    // Check statistics
    Statistics stats = processor.GetStatistics();
    ASSERT_TRUE(stats.IsValid());
    ASSERT_EQ(1, stats.frame_count);
    
    return true;
}

TEST(Processor_ErrorHandling) {
    CphProcessor processor;
    CphParams params;
    
    ASSERT_TRUE(processor.Initialize(params));
    
    // Try to process invalid image
    Image invalid_input;
    invalid_input.width = -1; // Invalid
    
    Image output;
    ASSERT_FALSE(processor.ProcessFrame(invalid_input, output));
    
    // Check error was logged
    std::vector<ErrorReport> errors = processor.GetErrorHistory();
    ASSERT_GT(errors.size(), 0);
    
    return true;
}

TEST(Processor_ModeSettings) {
    CphProcessor processor;
    CphParams params;
    
    ASSERT_TRUE(processor.Initialize(params));
    
    // Test mode settings
    processor.SetDeterministicMode(true);
    processor.SetDCIComplianceMode(true);
    
    // These should not cause errors
    ASSERT_TRUE(processor.GetLastError().empty());
    
    return true;
}