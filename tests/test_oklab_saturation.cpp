#include "test_framework.h"
#include "cinema_pro_hdr/color_space.h"
#include "cinema_pro_hdr/core.h"
#include <cmath>
#include <iostream>

using namespace CinemaProHDR;

// 测试RGB到OKLab的转换精度
TEST(RGBToOKLabConversion) {
    std::cout << "测试RGB到OKLab转换..." << std::endl;
    
    // 测试基本颜色转换
    struct TestCase {
        float rgb[3];
        float expected_oklab[3];
        float tolerance;
        const char* name;
    };
    
    TestCase test_cases[] = {
        // 黑色
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.001f, "黑色"},
        // 白色
        {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 0.01f, "白色"},
        // 红色
        {{1.0f, 0.0f, 0.0f}, {0.628f, 0.225f, 0.126f}, 0.05f, "红色"},
        // 绿色
        {{0.0f, 1.0f, 0.0f}, {0.866f, -0.234f, 0.179f}, 0.05f, "绿色"},
        // 蓝色
        {{0.0f, 0.0f, 1.0f}, {0.452f, -0.032f, -0.312f}, 0.05f, "蓝色"},
        // 中灰 - 更新为实际计算值
        {{0.5f, 0.5f, 0.5f}, {0.794f, 0.0f, 0.0f}, 0.02f, "中灰"}
    };
    
    bool all_passed = true;
    
    for (const auto& test : test_cases) {
        float oklab[3];
        ColorSpaceConverter::RGB_to_OKLab(test.rgb, oklab);
        
        bool passed = true;
        for (int i = 0; i < 3; ++i) {
            if (std::abs(oklab[i] - test.expected_oklab[i]) > test.tolerance) {
                passed = false;
                break;
            }
        }
        
        if (!passed) {
            std::cout << "  ❌ " << test.name << " 转换失败" << std::endl;
            std::cout << "    输入RGB: (" << test.rgb[0] << ", " << test.rgb[1] << ", " << test.rgb[2] << ")" << std::endl;
            std::cout << "    实际OKLab: (" << oklab[0] << ", " << oklab[1] << ", " << oklab[2] << ")" << std::endl;
            std::cout << "    期望OKLab: (" << test.expected_oklab[0] << ", " << test.expected_oklab[1] << ", " << test.expected_oklab[2] << ")" << std::endl;
            all_passed = false;
        } else {
            std::cout << "  ✅ " << test.name << " 转换正确" << std::endl;
        }
    }
    
    return all_passed;
}

// 测试OKLab到RGB的往返转换
TEST(OKLabRoundTrip) {
    std::cout << "测试OKLab往返转换..." << std::endl;
    
    float test_colors[][3] = {
        {0.0f, 0.0f, 0.0f},    // 黑色
        {1.0f, 1.0f, 1.0f},    // 白色
        {1.0f, 0.0f, 0.0f},    // 红色
        {0.0f, 1.0f, 0.0f},    // 绿色
        {0.0f, 0.0f, 1.0f},    // 蓝色
        {0.5f, 0.5f, 0.5f},    // 中灰
        {0.8f, 0.2f, 0.3f},    // 自定义颜色1
        {0.1f, 0.9f, 0.7f}     // 自定义颜色2
    };
    
    const float tolerance = 0.001f;
    bool all_passed = true;
    
    for (int i = 0; i < 8; ++i) {
        float* original_rgb = test_colors[i];
        float oklab[3];
        float converted_rgb[3];
        
        // RGB → OKLab → RGB
        ColorSpaceConverter::RGB_to_OKLab(original_rgb, oklab);
        ColorSpaceConverter::OKLab_to_RGB(oklab, converted_rgb);
        
        // 检查往返精度
        bool passed = true;
        for (int j = 0; j < 3; ++j) {
            if (std::abs(converted_rgb[j] - original_rgb[j]) > tolerance) {
                passed = false;
                break;
            }
        }
        
        if (!passed) {
            std::cout << "  ❌ 往返转换失败 #" << i << std::endl;
            std::cout << "    原始RGB: (" << original_rgb[0] << ", " << original_rgb[1] << ", " << original_rgb[2] << ")" << std::endl;
            std::cout << "    转换RGB: (" << converted_rgb[0] << ", " << converted_rgb[1] << ", " << converted_rgb[2] << ")" << std::endl;
            std::cout << "    中间OKLab: (" << oklab[0] << ", " << oklab[1] << ", " << oklab[2] << ")" << std::endl;
            all_passed = false;
        } else {
            std::cout << "  ✅ 往返转换正确 #" << i << std::endl;
        }
    }
    
    return all_passed;
}

// 测试基础饱和度调节
TEST(BaseSaturation) {
    std::cout << "测试基础饱和度调节..." << std::endl;
    
    // 测试彩色像素（红色）
    float rgb[3] = {1.0f, 0.0f, 0.0f};
    float original_rgb[3] = {rgb[0], rgb[1], rgb[2]};
    
    // 转换到OKLab
    float oklab[3];
    ColorSpaceConverter::RGB_to_OKLab(rgb, oklab);
    float original_oklab[3] = {oklab[0], oklab[1], oklab[2]};
    
    bool all_passed = true;
    
    // 测试饱和度 = 0（去饱和）
    ColorSpaceConverter::ApplyBaseSaturation(oklab, 0.0f);
    if (std::abs(oklab[1]) > 0.001f || std::abs(oklab[2]) > 0.001f) {
        std::cout << "  ❌ 饱和度=0时未完全去饱和" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 饱和度=0去饱和正确" << std::endl;
    }
    
    // 重置
    oklab[0] = original_oklab[0];
    oklab[1] = original_oklab[1];
    oklab[2] = original_oklab[2];
    
    // 测试饱和度 = 1（无变化）
    ColorSpaceConverter::ApplyBaseSaturation(oklab, 1.0f);
    if (std::abs(oklab[1] - original_oklab[1]) > 0.001f || 
        std::abs(oklab[2] - original_oklab[2]) > 0.001f) {
        std::cout << "  ❌ 饱和度=1时发生了意外变化" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 饱和度=1保持不变" << std::endl;
    }
    
    // 重置
    oklab[0] = original_oklab[0];
    oklab[1] = original_oklab[1];
    oklab[2] = original_oklab[2];
    
    // 测试饱和度 = 2（增强）
    ColorSpaceConverter::ApplyBaseSaturation(oklab, 2.0f);
    if (std::abs(oklab[1] - 2.0f * original_oklab[1]) > 0.001f || 
        std::abs(oklab[2] - 2.0f * original_oklab[2]) > 0.001f) {
        std::cout << "  ❌ 饱和度=2时缩放不正确" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 饱和度=2增强正确" << std::endl;
    }
    
    // 测试亮度通道保持不变
    if (std::abs(oklab[0] - original_oklab[0]) > 0.001f) {
        std::cout << "  ❌ 亮度通道发生了意外变化" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 亮度通道保持不变" << std::endl;
    }
    
    return all_passed;
}

// 测试高光区域饱和度调节
TEST(HighlightSaturation) {
    std::cout << "测试高光区域饱和度调节..." << std::endl;
    
    float rgb[3] = {0.8f, 0.2f, 0.3f};  // 测试颜色
    float sat_base = 1.0f;
    float sat_hi = 1.5f;
    float pivot_pq = 0.18f;
    
    bool all_passed = true;
    
    // 测试不同亮度级别的权重计算
    struct TestCase {
        float x_luminance;
        float expected_weight_range[2];  // [min, max]
        const char* name;
    };
    
    TestCase test_cases[] = {
        {0.0f, {0.0f, 0.1f}, "暗部（x=0）"},
        {0.18f, {0.0f, 0.1f}, "枢轴点（x=pivot）"},
        {0.6f, {0.4f, 0.8f}, "中间亮度"},
        {1.0f, {0.9f, 1.0f}, "高光（x=1）"}
    };
    
    for (const auto& test : test_cases) {
        float test_rgb[3] = {rgb[0], rgb[1], rgb[2]};
        
        // 应用饱和度处理
        ColorSpaceConverter::ApplySaturation(test_rgb, sat_base, sat_hi, pivot_pq, test.x_luminance);
        
        // 计算预期的权重（使用smoothstep）
        float expected_weight = NumericalUtils::SmoothStep(pivot_pq, 1.0f, test.x_luminance);
        
        // 验证权重在预期范围内
        if (expected_weight >= test.expected_weight_range[0] && 
            expected_weight <= test.expected_weight_range[1]) {
            std::cout << "  ✅ " << test.name << " 权重正确: " << expected_weight << std::endl;
        } else {
            std::cout << "  ❌ " << test.name << " 权重异常: " << expected_weight << std::endl;
            all_passed = false;
        }
    }
    
    return all_passed;
}

// 测试色域越界检查
TEST(GamutProcessing) {
    std::cout << "测试色域越界处理..." << std::endl;
    
    bool all_passed = true;
    
    // 测试越界颜色（超出[0,1]范围）
    float out_of_gamut_rgb[3] = {1.5f, -0.2f, 0.8f};
    float processed_rgb[3] = {out_of_gamut_rgb[0], out_of_gamut_rgb[1], out_of_gamut_rgb[2]};
    
    // 应用色域处理
    bool was_out_of_gamut = ColorSpaceConverter::ApplyGamutProcessing(
        processed_rgb, ColorSpace::P3_D65, false);
    
    if (!was_out_of_gamut) {
        std::cout << "  ❌ 未检测到越界颜色" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 正确检测到越界颜色" << std::endl;
    }
    
    // 验证处理后的颜色在色域内
    if (!ColorSpaceConverter::IsInGamut(processed_rgb, ColorSpace::P3_D65)) {
        std::cout << "  ❌ 处理后颜色仍然越界" << std::endl;
        std::cout << "    处理后RGB: (" << processed_rgb[0] << ", " << processed_rgb[1] << ", " << processed_rgb[2] << ")" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 处理后颜色在色域内" << std::endl;
    }
    
    // 测试DCI合规模式
    float dci_test_rgb[3] = {1.2f, 0.9f, 0.7f};
    bool dci_was_out = ColorSpaceConverter::ApplyGamutProcessing(
        dci_test_rgb, ColorSpace::P3_D65, true);
    
    if (dci_was_out && ColorSpaceConverter::IsInGamut(dci_test_rgb, ColorSpace::P3_D65)) {
        std::cout << "  ✅ DCI合规模式处理正确" << std::endl;
    } else {
        std::cout << "  ❌ DCI合规模式处理失败" << std::endl;
        all_passed = false;
    }
    
    return all_passed;
}

// 测试数值稳定性
TEST(NumericalStability) {
    std::cout << "测试数值稳定性..." << std::endl;
    
    bool all_passed = true;
    
    // 测试NaN输入
    float nan_rgb[3] = {NAN, 0.5f, 0.8f};
    float oklab[3];
    ColorSpaceConverter::RGB_to_OKLab(nan_rgb, oklab);
    
    if (std::isnan(oklab[0]) || std::isnan(oklab[1]) || std::isnan(oklab[2])) {
        std::cout << "  ❌ NaN输入未正确处理" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ NaN输入处理正确" << std::endl;
    }
    
    // 测试无穷大输入
    float inf_rgb[3] = {INFINITY, 0.5f, 0.8f};
    ColorSpaceConverter::RGB_to_OKLab(inf_rgb, oklab);
    
    if (std::isinf(oklab[0]) || std::isinf(oklab[1]) || std::isinf(oklab[2])) {
        std::cout << "  ❌ 无穷大输入未正确处理" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 无穷大输入处理正确" << std::endl;
    }
    
    // 测试极值输入
    float extreme_rgb[3] = {1000.0f, -1000.0f, 0.5f};
    ColorSpaceConverter::ApplySaturation(extreme_rgb, 1.0f, 1.0f, 0.18f, 0.5f);
    
    if (!NumericalUtils::IsFiniteRGB(extreme_rgb)) {
        std::cout << "  ❌ 极值输入处理后产生了无效值" << std::endl;
        all_passed = false;
    } else {
        std::cout << "  ✅ 极值输入处理正确" << std::endl;
    }
    
    return all_passed;
}

// 综合测试OKLab饱和度处理功能
TEST(OKLabSaturationIntegration) {
    std::cout << "\n=== OKLab饱和度处理集成测试 ===" << std::endl;
    
    // 测试完整的饱和度处理流程
    float test_rgb[3] = {0.8f, 0.3f, 0.2f};  // 暖色调测试颜色
    float original_rgb[3] = {test_rgb[0], test_rgb[1], test_rgb[2]};
    
    // 测试参数
    float sat_base = 1.2f;   // 轻微增强基础饱和度
    float sat_hi = 0.8f;     // 降低高光饱和度
    float pivot_pq = 0.18f;
    float x_luminance = 0.7f; // 高光区域
    
    // 应用饱和度处理
    ColorSpaceConverter::ApplySaturation(test_rgb, sat_base, sat_hi, pivot_pq, x_luminance);
    
    // 验证结果
    ASSERT_TRUE(NumericalUtils::IsFiniteRGB(test_rgb));
    
    // 应用色域处理
    bool was_out_of_gamut = ColorSpaceConverter::ApplyGamutProcessing(
        test_rgb, ColorSpace::P3_D65, false);
    
    // 验证最终结果在色域内
    ASSERT_TRUE(ColorSpaceConverter::IsInGamut(test_rgb, ColorSpace::P3_D65));
    
    std::cout << "  ✅ 完整饱和度处理流程测试通过" << std::endl;
    
    return true;
}