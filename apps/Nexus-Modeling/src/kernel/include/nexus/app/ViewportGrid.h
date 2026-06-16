#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Viewport Grid
//
//  Draws a reference grid on the ground plane with configurable extent
//  and spacing.  Optional origin axes (RGB).
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>

namespace nexus::app {

// Working plane: 0=XZ, 1=XY, 2=YZ
inline constexpr int kWorkPlaneXZ = 0;
inline constexpr int kWorkPlaneXY = 1;
inline constexpr int kWorkPlaneYZ = 2;

struct GridOptions {
    float extent  = 10.f;   // half-extent of grid
    float spacing = 1.f;    // spacing between grid lines
    float axisLength = 2.f; // length of origin axis arrows
    bool  drawGrid  = true;
    bool  drawAxes  = true;
    int   workPlane = kWorkPlaneXZ;
};

class ViewportGrid {
public:
    void render(const GridOptions& opts = {}) const noexcept;
private:
    void renderXZGrid(float extent, float spacing) const noexcept;
    void renderXYGrid(float extent, float spacing) const noexcept;
    void renderYZGrid(float extent, float spacing) const noexcept;
};

} // namespace nexus::app
