#include "cinema_pro_hdr/color_space.h"
#include <cmath>
#include <algorithm>

namespace CinemaProHDR {

// Additional PQ utility functions
class PQUtils {
public:
    // Convert nits to PQ normalized value
    static float NitsToPQ(float nits) {
        return ColorSpaceConverter::PQ_OETF(nits);
    }
    
    // Convert PQ normalized value to nits
    static float PQToNits(float pq_value) {
        return ColorSpaceConverter::PQ_EOTF(pq_value);
    }
    
    // Get MaxRGB in PQ domain
    static float GetMaxRGB_PQ(const float* pq_rgb) {
        return std::max({pq_rgb[0], pq_rgb[1], pq_rgb[2]});
    }
    
    // Validate PQ range
    static bool IsValidPQ(float pq_value) {
        return pq_value >= 0.0f && pq_value <= 1.0f && std::isfinite(pq_value);
    }
    
    // Validate PQ RGB
    static bool IsValidPQ_RGB(const float* pq_rgb) {
        return IsValidPQ(pq_rgb[0]) && IsValidPQ(pq_rgb[1]) && IsValidPQ(pq_rgb[2]);
    }
    
    // Clamp to PQ range
    static void ClampToPQ(float* pq_rgb) {
        pq_rgb[0] = std::clamp(pq_rgb[0], 0.0f, 1.0f);
        pq_rgb[1] = std::clamp(pq_rgb[1], 0.0f, 1.0f);
        pq_rgb[2] = std::clamp(pq_rgb[2], 0.0f, 1.0f);
    }
};

} // namespace CinemaProHDR