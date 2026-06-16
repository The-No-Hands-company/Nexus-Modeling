#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Sketch Preview Overlay
//
//  Renders sketch points and constraint visualization during sketch mode.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadAutoConstraintSketch.h>

namespace nexus::app {

class SketchPreview {
public:
    void render(nexus::cad::CadAutoConstraintSketch& sketcher) const noexcept;
};

} // namespace nexus::app
