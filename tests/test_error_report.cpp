#include "test_framework.h"
#include "cinema_pro_hdr/core.h"

using namespace CinemaProHDR;

TEST(ErrorReport_Construction) {
    ErrorReport error(ErrorCode::RANGE_PIVOT, "Test error message");
    
    ASSERT_EQ(static_cast<int>(ErrorCode::RANGE_PIVOT), static_cast<int>(error.code));
    ASSERT_EQ("Test error message", error.message);
    ASSERT_TRUE(error.IsError());
    
    return true;
}

TEST(ErrorReport_SuccessCode) {
    ErrorReport success;
    
    ASSERT_EQ(static_cast<int>(ErrorCode::SUCCESS), static_cast<int>(success.code));
    ASSERT_FALSE(success.IsError());
    
    return true;
}

TEST(ErrorReport_ToString) {
    ErrorReport error(ErrorCode::RANGE_PIVOT, "Parameter out of range");
    error.field_name = "pivot_pq";
    error.invalid_value = 0.35f;
    error.action_taken = "CLAMP";
    error.clip_guid = "test-guid-123";
    error.timecode = "01:23:45:12";
    
    std::string error_string = error.ToString();
    
    // Check that key components are present in the string
    ASSERT_TRUE(error_string.find("WARN") != std::string::npos);
    ASSERT_TRUE(error_string.find("code=2") != std::string::npos); // RANGE_PIVOT is enum value 2
    ASSERT_TRUE(error_string.find("field=pivot_pq") != std::string::npos);
    ASSERT_TRUE(error_string.find("val=0.35") != std::string::npos);
    ASSERT_TRUE(error_string.find("action=CLAMP") != std::string::npos);
    ASSERT_TRUE(error_string.find("test-guid-123") != std::string::npos);
    ASSERT_TRUE(error_string.find("01:23:45:12") != std::string::npos);
    ASSERT_TRUE(error_string.find("Parameter out of range") != std::string::npos);
    
    return true;
}

TEST(ErrorReport_ErrorLevels) {
    // Test warning level
    ErrorReport warning(ErrorCode::RANGE_PIVOT, "Warning message");
    std::string warning_str = warning.ToString();
    ASSERT_TRUE(warning_str.find("[WARN]") != std::string::npos);
    
    // Test error level
    ErrorReport error(ErrorCode::NAN_INF, "Error message");
    std::string error_str = error.ToString();
    ASSERT_TRUE(error_str.find("[ERROR]") != std::string::npos);
    
    // Test info level (success)
    ErrorReport info(ErrorCode::SUCCESS, "Info message");
    std::string info_str = info.ToString();
    ASSERT_TRUE(info_str.find("[INFO]") != std::string::npos);
    
    return true;
}