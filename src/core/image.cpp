#include "cinema_pro_hdr/core.h"
#include <algorithm>
#include <cmath>

namespace CinemaProHDR {

Image::Image(int w, int h, int c) 
    : width(w), height(h), channels(c) {
    data.resize(width * height * channels, 0.0f);
}

float* Image::GetPixel(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return nullptr;
    }
    return &data[(y * width + x) * channels];
}

const float* Image::GetPixel(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return nullptr;
    }
    return &data[(y * width + x) * channels];
}

bool Image::IsValid() const {
    if (width <= 0 || height <= 0 || channels <= 0) {
        return false;
    }
    
    if (data.size() != static_cast<size_t>(width * height * channels)) {
        return false;
    }
    
    // Check for NaN/Inf values
    for (float value : data) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    
    return true;
}

void Image::Clear() {
    std::fill(data.begin(), data.end(), 0.0f);
}

} // namespace CinemaProHDR