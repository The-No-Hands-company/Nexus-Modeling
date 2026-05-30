#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — CPU pixel + depth buffer
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::softrast {

struct RGBA8 {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    bool operator==(const RGBA8&) const = default;
};

class PixelBuffer {
public:
    PixelBuffer(uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_pixels(static_cast<std::size_t>(width) * height, RGBA8{0,0,0,255})
        , m_depth(static_cast<std::size_t>(width) * height, std::numeric_limits<float>::infinity())
    {}

    [[nodiscard]] uint32_t width()  const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] const std::vector<RGBA8>& pixels() const noexcept { return m_pixels; }

    void clear(RGBA8 color = {30, 30, 30, 255}) noexcept {
        for (auto& px : m_pixels) px = color;
        for (auto& d  : m_depth)  d  = std::numeric_limits<float>::infinity();
    }

    void setPixelDepth(uint32_t x, uint32_t y, RGBA8 color, float depth) noexcept {
        if (x >= m_width || y >= m_height) return;
        const std::size_t idx = static_cast<std::size_t>(y) * m_width + x;
        if (depth < m_depth[idx]) {
            m_depth[idx]  = depth;
            m_pixels[idx] = color;
        }
    }

    void setPixel(uint32_t x, uint32_t y, RGBA8 color) noexcept {
        if (x >= m_width || y >= m_height) return;
        m_pixels[static_cast<std::size_t>(y) * m_width + x] = color;
    }

    [[nodiscard]] RGBA8 getPixel(uint32_t x, uint32_t y) const noexcept {
        if (x >= m_width || y >= m_height) return {};
        return m_pixels[static_cast<std::size_t>(y) * m_width + x];
    }

    // FNV-1a 64-bit hash over all pixels — for golden-file comparison.
    [[nodiscard]] uint64_t fnv1aHash() const noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (const auto& px : m_pixels) {
            h ^= px.r; h *= 1099511628211ULL;
            h ^= px.g; h *= 1099511628211ULL;
            h ^= px.b; h *= 1099511628211ULL;
            h ^= px.a; h *= 1099511628211ULL;
        }
        return h;
    }

private:
    uint32_t           m_width;
    uint32_t           m_height;
    std::vector<RGBA8> m_pixels;
    std::vector<float> m_depth;
};

} // namespace nexus::softrast
