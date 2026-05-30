#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — CPU 2D texture
//
//  A simple RGBA8 pixel grid with nearest-neighbour and bilinear sampling.
//  Coordinates are in [0,1] UV space and wrap with repeat semantics.
// ─────────────────────────────────────────────────────────────────────────────
#include "PixelBuffer.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::softrast {

enum class TextureFilter : uint8_t {
    Nearest,
    Bilinear,
};

class Texture2D {
public:
    Texture2D() = default;
    Texture2D(uint32_t width, uint32_t height)
        : m_width(width), m_height(height)
        , m_pixels(static_cast<std::size_t>(width) * height, RGBA8{255,255,255,255})
    {}

    [[nodiscard]] uint32_t width()  const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] bool     empty()  const noexcept { return m_pixels.empty(); }

    void setPixel(uint32_t x, uint32_t y, RGBA8 color) noexcept {
        if (x >= m_width || y >= m_height) return;
        m_pixels[static_cast<std::size_t>(y) * m_width + x] = color;
    }

    [[nodiscard]] RGBA8 getPixel(uint32_t x, uint32_t y) const noexcept {
        if (m_pixels.empty()) return {255, 255, 255, 255};
        const uint32_t xi = x % m_width;
        const uint32_t yi = y % m_height;
        return m_pixels[static_cast<std::size_t>(yi) * m_width + xi];
    }

    // Sample with UV repeat wrapping.
    [[nodiscard]] RGBA8 sample(float u, float v,
                               TextureFilter filter = TextureFilter::Bilinear) const noexcept
    {
        if (m_pixels.empty()) return {255, 255, 255, 255};

        // Wrap to [0,1)
        u = u - std::floor(u);
        v = v - std::floor(v);

        const float fu = u * static_cast<float>(m_width);
        const float fv = v * static_cast<float>(m_height);

        if (filter == TextureFilter::Nearest) {
            return getPixel(static_cast<uint32_t>(fu) % m_width,
                            static_cast<uint32_t>(fv) % m_height);
        }

        // Bilinear
        const int x0 = static_cast<int>(fu);
        const int y0 = static_cast<int>(fv);
        const float tx = fu - static_cast<float>(x0);
        const float ty = fv - static_cast<float>(y0);

        const RGBA8 c00 = getPixel(static_cast<uint32_t>(x0)     % m_width,
                                   static_cast<uint32_t>(y0)     % m_height);
        const RGBA8 c10 = getPixel(static_cast<uint32_t>(x0 + 1) % m_width,
                                   static_cast<uint32_t>(y0)     % m_height);
        const RGBA8 c01 = getPixel(static_cast<uint32_t>(x0)     % m_width,
                                   static_cast<uint32_t>(y0 + 1) % m_height);
        const RGBA8 c11 = getPixel(static_cast<uint32_t>(x0 + 1) % m_width,
                                   static_cast<uint32_t>(y0 + 1) % m_height);

        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        return RGBA8{
            static_cast<uint8_t>(lerp(lerp(c00.r, c10.r, tx), lerp(c01.r, c11.r, tx), ty)),
            static_cast<uint8_t>(lerp(lerp(c00.g, c10.g, tx), lerp(c01.g, c11.g, tx), ty)),
            static_cast<uint8_t>(lerp(lerp(c00.b, c10.b, tx), lerp(c01.b, c11.b, tx), ty)),
            255
        };
    }

    // ── Factory helpers ───────────────────────────────────────────────────────

    // Classic 2-color checkerboard.
    [[nodiscard]] static Texture2D makeCheckerboard(uint32_t size,
                                                    uint32_t tilePixels = 8,
                                                    RGBA8 colorA = {220, 220, 220, 255},
                                                    RGBA8 colorB = {50,  50,  50,  255})
    {
        Texture2D tex(size, size);
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                const bool evenX = (x / tilePixels) % 2 == 0;
                const bool evenY = (y / tilePixels) % 2 == 0;
                tex.setPixel(x, y, (evenX == evenY) ? colorA : colorB);
            }
        }
        return tex;
    }

    // UV gradient: red=U, green=V, blue=0 — useful for UV debug visualization.
    [[nodiscard]] static Texture2D makeUVGradient(uint32_t size)
    {
        Texture2D tex(size, size);
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                tex.setPixel(x, y, RGBA8{
                    static_cast<uint8_t>(255u * x / (size - 1)),
                    static_cast<uint8_t>(255u * y / (size - 1)),
                    0, 255
                });
            }
        }
        return tex;
    }

private:
    uint32_t           m_width  = 0;
    uint32_t           m_height = 0;
    std::vector<RGBA8> m_pixels;
};

} // namespace nexus::softrast
