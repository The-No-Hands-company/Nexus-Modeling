#pragma once
#include <nexus/cad/CadDocument.h>
#include <cstdint>
#include <vector>

namespace nexus::cad {
class CadDirectEdit {
public:
    [[nodiscard]] static bool moveFaces(CadDocument& doc, parametric::FeatureId fid, const std::vector<uint32_t>& faces, float d) noexcept;
    [[nodiscard]] static bool offsetFaces(CadDocument& doc, parametric::FeatureId fid, const std::vector<uint32_t>& faces, float d) noexcept;
    [[nodiscard]] static bool deleteFaces(CadDocument& doc, parametric::FeatureId fid, const std::vector<uint32_t>& faces) noexcept;
};
}
