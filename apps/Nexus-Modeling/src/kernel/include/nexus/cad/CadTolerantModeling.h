#pragma once
#include <nexus/geometry/Mesh.h>
#include <cstdint>

namespace nexus::cad {
struct TolerantBooleanOptions { float gapTolerance=0.01f,sliverThreshold=0.001f; bool autoHeal=true,removeSlivers=true,closeGaps=true; };
class CadTolerantModeling {
public:
    [[nodiscard]] static geometry::Mesh fuzzyUnion(const geometry::Mesh& a,const geometry::Mesh& b,const TolerantBooleanOptions& opts = TolerantBooleanOptions{}) noexcept;
    [[nodiscard]] static geometry::Mesh healGaps(const geometry::Mesh&,float=0.1f) noexcept;
    [[nodiscard]] static geometry::Mesh removeSlivers(const geometry::Mesh&,float=0.001f) noexcept;
    [[nodiscard]] static geometry::Mesh repairImport(const geometry::Mesh&) noexcept;
};
}
