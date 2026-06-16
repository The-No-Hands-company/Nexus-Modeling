#pragma once
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <vector>

namespace nexus::cad {
using Vec3 = nexus::render::Vec3;
enum class ContinuityLevel : uint8_t { G0, G1, G2, G3 };
struct BlendSpec { ContinuityLevel continuity=ContinuityLevel::G2; float radius=5,tangentScale=1,curvatureScale=1; };
struct CurvatureComb { std::vector<Vec3> points,normals; std::vector<float> curvature; };
class CadSurfacing {
public:
    [[nodiscard]] static geometry::NurbsSurface createBlend(const geometry::NurbsSurface& a,float ua,float va,const geometry::NurbsSurface& b,float ub,float vb,const BlendSpec& spec = BlendSpec{}) noexcept;
    [[nodiscard]] static bool validateClassA(const geometry::NurbsSurface&) noexcept;
    [[nodiscard]] static CurvatureComb generateCurvatureComb(const geometry::NurbsSurface&,float,bool isoU) noexcept;
    [[nodiscard]] static geometry::Mesh generateZebraStripes(const geometry::NurbsSurface&,uint32_t=20) noexcept;
};
}
