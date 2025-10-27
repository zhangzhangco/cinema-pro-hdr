#pragma once

#include "core.h"
#include <vector>

namespace CinemaProHDR {

/**
 * @brief 色调映射算法实现类
 * 
 * 提供PPR和RLOG两种色调映射曲线的实现，包括：
 * - PPR (Pivoted Power-Rational) 算法：枢轴幂-有理式曲线
 * - RLOG (Rational Logarithmic) 算法：有理对数曲线
 * - 软膝和toe夹持处理
 * - 单调性和C¹连续性验证
 * 
 * 用途：将HDR亮度映射到显示范围，保持视觉自然
 * 不是：通用的颜色校正或艺术滤镜
 * 
 * 适用场景：
 * - 需要数学可证明的单调性
 * - 要求跨平台数值一致性
 * - HDR到SDR的色调映射
 * 
 * 不适用场景：
 * - 艺术风格化效果
 * - 非单调的创意映射
 */
class ToneMapper {
public:
    ToneMapper();
    ~ToneMapper();
    
    /**
     * @brief 初始化色调映射器
     * @param params 色调映射参数
     * @return 初始化是否成功
     */
    bool Initialize(const CphParams& params);
    
    /**
     * @brief 应用色调映射到单个像素的亮度值
     * @param luminance 输入亮度值（PQ归一化域 [0,1]）
     * @return 映射后的亮度值
     */
    float ApplyToneMapping(float luminance) const;
    
    /**
     * @brief 批量应用色调映射
     * @param input_luminance 输入亮度数组
     * @param output_luminance 输出亮度数组
     * @param count 数组长度
     */
    void ApplyToneMappingBatch(const float* input_luminance, 
                               float* output_luminance, 
                               size_t count) const;
    
    /**
     * @brief 验证曲线的单调性
     * @param sample_count 采样点数量（默认4096）
     * @param problem_points 额外的问题点数量（默认256）
     * @return 是否单调
     */
    bool ValidateMonotonicity(int sample_count = 4096, int problem_points = 256) const;
    
    /**
     * @brief 验证曲线的C¹连续性
     * @param epsilon 导数差分步长（默认1e-3）
     * @param threshold 连续性阈值（默认1e-3）
     * @return 是否C¹连续
     */
    bool ValidateC1Continuity(float epsilon = 1e-3f, float threshold = 1e-3f) const;
    
    /**
     * @brief 获取当前参数
     * @return 当前使用的参数
     */
    const CphParams& GetParams() const { return params_; }
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string GetLastError() const { return last_error_; }

private:
    CphParams params_;
    std::string last_error_;
    bool initialized_ = false;
    
    // PPR算法实现
    float ApplyPPR(float x) const;
    float PPRShadowSegment(float x) const;
    float PPRHighlightSegment(float x) const;
    
    // RLOG算法实现
    float ApplyRLOG(float x) const;
    float RLOGDarkSegment(float x) const;
    float RLOGHighlightSegment(float x) const;
    float RLOGBlendFunction(float x) const;
    
    // 软膝和toe处理
    float ApplySoftKnee(float y) const;
    float ApplyToeClamp(float y) const;
    
    // 辅助函数
    float SmoothStep(float edge0, float edge1, float x) const;
    float Mix(float a, float b, float t) const;
    
    // 验证辅助函数
    float ComputeDerivative(float x, float epsilon) const;
    std::vector<float> GenerateSamplePoints(int sample_count, int problem_points) const;
};

/**
 * @brief 色调映射工具函数
 */
namespace ToneMappingUtils {
    
    /**
     * @brief 计算PPR曲线在指定点的值
     * @param x 输入值 [0,1]
     * @param pivot 枢轴点
     * @param gamma_s 阴影伽马
     * @param gamma_h 高光伽马
     * @param shoulder 肩部参数
     * @return 映射后的值
     */
    float EvaluatePPR(float x, float pivot, float gamma_s, float gamma_h, float shoulder);
    
    /**
     * @brief 计算RLOG曲线在指定点的值
     * @param x 输入值 [0,1]
     * @param a 暗部参数
     * @param b 高光增益
     * @param c 高光压缩
     * @param t 拼接阈值
     * @return 映射后的值
     */
    float EvaluateRLOG(float x, float a, float b, float c, float t);
    
    /**
     * @brief 软膝函数
     * @param y 输入值
     * @param knee 膝点位置
     * @param alpha 软化强度
     * @return 软膝处理后的值
     */
    float SoftKnee(float y, float knee, float alpha);
    
    /**
     * @brief Toe夹持函数
     * @param y 输入值
     * @param toe 夹持阈值
     * @return 夹持后的值
     */
    float ToeClamp(float y, float toe);
}

} // namespace CinemaProHDR