#pragma once
#include "PixelBuffer.h"
#include <cstdio>
#include <string>
#include <string_view>

namespace nexus::softrast {

inline bool writePPM(std::string_view path, const PixelBuffer& buf) {
    FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", buf.width(), buf.height());
    for (const auto& px : buf.pixels()) {
        uint8_t rgb[3] = {px.r, px.g, px.b};
        if (std::fwrite(rgb, 1, 3, f) != 3) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

} // namespace nexus::softrast
