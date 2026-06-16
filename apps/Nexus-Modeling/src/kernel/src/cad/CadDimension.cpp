#include <nexus/cad/CadDimension.h>

#include <cmath>

namespace nexus::cad {

uint32_t CadDimensionManager::addLinear(const Vec3& p1, const Vec3& p2,
                                          const Vec3& textPos)
{
    CadDimension dim;
    dim.type = DimensionType::Linear;
    dim.point1 = p1;
    dim.point2 = p2;
    dim.textPosition = textPos;
    dim.value = (p2 - p1).length();
    m_dims.push_back(dim);
    return static_cast<uint32_t>(m_dims.size()) - 1;
}

uint32_t CadDimensionManager::addRadial(const Vec3& center, double radius,
                                          const Vec3& textPos)
{
    CadDimension dim;
    dim.type = DimensionType::Radial;
    dim.point1 = center;
    dim.textPosition = textPos;
    dim.value = radius;
    m_dims.push_back(dim);
    return static_cast<uint32_t>(m_dims.size()) - 1;
}

uint32_t CadDimensionManager::addAngular(const Vec3& vertex,
                                           const Vec3& dir1, const Vec3& dir2,
                                           const Vec3& textPos)
{
    CadDimension dim;
    dim.type = DimensionType::Angular;
    dim.point1 = vertex;
    dim.point2 = vertex + dir1;
    dim.textPosition = textPos;
    float dot = dir1.normalize().dot(dir2.normalize());
    dim.value = std::acos(std::clamp(dot, -1.f, 1.f)) * 180.0 / 3.1415926535;
    m_dims.push_back(dim);
    return static_cast<uint32_t>(m_dims.size()) - 1;
}

const std::vector<CadDimension>& CadDimensionManager::dimensions() const noexcept
{ return m_dims; }

void CadDimensionManager::clear() { m_dims.clear(); }

} // namespace nexus::cad
