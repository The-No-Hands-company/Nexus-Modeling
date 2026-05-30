#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — PPM / TGA image I/O (no external dependencies)
// ─────────────────────────────────────────────────────────────────────────────
#include "PixelBuffer.h"

#include <cstdio>
#include <cstring>
#include <string_view>

namespace nexus::softrast {

// ── PPM P6 (binary) write ─────────────────────────────────────────────────────
[[nodiscard]] inline bool writePPM(std::string_view path, const PixelBuffer& buf) {
    std::FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return false;

    // Header: "P6\n<width> <height>\n255\n"
    std::fprintf(f, "P6\n%u %u\n255\n", buf.width(), buf.height());

    // Pixel data: packed RGB bytes (no alpha in PPM)
    for (const auto& px : buf.pixels()) {
        const uint8_t rgb[3] = {px.r, px.g, px.b};
        if (std::fwrite(rgb, 1, 3, f) != 3) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

// ── PPM P6 read (for golden-file comparison) ──────────────────────────────────
[[nodiscard]] inline bool readPPM(std::string_view path, PixelBuffer& out) {
    std::FILE* f = std::fopen(std::string(path).c_str(), "rb");
    if (!f) return false;

    char magic[3] = {};
    if (std::fscanf(f, "%2s", magic) != 1 || std::strcmp(magic, "P6") != 0) {
        std::fclose(f); return false;
    }

    uint32_t w = 0, h = 0, maxval = 0;
    if (std::fscanf(f, " %u %u %u", &w, &h, &maxval) != 3 || maxval != 255 || w == 0 || h == 0) {
        std::fclose(f); return false;
    }

    // Skip exactly one whitespace byte after maxval
    std::fgetc(f);

    out = PixelBuffer(w, h);
    out.clear({0, 0, 0, 255});

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t rgb[3] = {};
            if (std::fread(rgb, 1, 3, f) != 3) { std::fclose(f); return false; }
            out.setPixel(x, y, {rgb[0], rgb[1], rgb[2], 255});
        }
    }
    std::fclose(f);
    return true;
}

// ── TGA (uncompressed 24-bit BGR) write ───────────────────────────────────────
[[nodiscard]] inline bool writeTGA(std::string_view path, const PixelBuffer& buf) {
    std::FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return false;

    const uint32_t w = buf.width(), h = buf.height();
    // 18-byte TGA header
    const uint8_t hdr[18] = {
        0, 0, 2,           // id length=0, no colormap, uncompressed truecolor
        0, 0, 0, 0, 0,     // colormap fields (unused)
        0, 0, 0, 0,        // x-origin, y-origin (0,0)
        static_cast<uint8_t>(w & 0xFF), static_cast<uint8_t>((w >> 8) & 0xFF),
        static_cast<uint8_t>(h & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
        24, 0x20,          // 24 bpp, top-left origin bit set
    };
    if (std::fwrite(hdr, 1, 18, f) != 18) { std::fclose(f); return false; }

    // TGA stores BGR; with 0x20 origin bit this is top-to-bottom.
    for (const auto& px : buf.pixels()) {
        const uint8_t bgr[3] = {px.b, px.g, px.r};
        if (std::fwrite(bgr, 1, 3, f) != 3) { std::fclose(f); return false; }
    }
    std::fclose(f);
    return true;
}

} // namespace nexus::softrast
