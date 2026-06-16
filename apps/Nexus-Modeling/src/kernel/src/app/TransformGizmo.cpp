#include <nexus/app/TransformGizmo.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cmath>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

void TransformGizmo::render(const Vec3& center) const noexcept {
    float s = m_handleLength;
    glLineWidth(3.f);
    glBegin(GL_LINES);
    // X axis — red.
    glColor3f(1,0,0); glVertex3f(center.x,center.y,center.z);
    glVertex3f(center.x+s,center.y,center.z);
    // Y axis — green.
    glColor3f(0,1,0); glVertex3f(center.x,center.y,center.z);
    glVertex3f(center.x,center.y+s,center.z);
    // Z axis — blue.
    glColor3f(0,0,1); glVertex3f(center.x,center.y,center.z);
    glVertex3f(center.x,center.y,center.z+s);
    glEnd();

    // Arrow heads.
    float h = 0.3f, w = 0.15f;
    glBegin(GL_TRIANGLES);
    glColor3f(1,0,0);
    glVertex3f(center.x+s,center.y,center.z);
    glVertex3f(center.x+s-h,center.y+w,center.z);
    glVertex3f(center.x+s-h,center.y-w,center.z);
    glColor3f(0,1,0);
    glVertex3f(center.x,center.y+s,center.z);
    glVertex3f(center.x+w,center.y+s-h,center.z);
    glVertex3f(center.x-w,center.y+s-h,center.z);
    glColor3f(0,0,1);
    glVertex3f(center.x,center.y,center.z+s);
    glVertex3f(center.x+w,center.y,center.z+s-h);
    glVertex3f(center.x-w,center.y,center.z+s-h);
    glEnd();
    glLineWidth(1.f);
}

TransformGizmo::Axis TransformGizmo::pickAxis(
    const Vec3& center, const Vec3& rayOrigin,
    const Vec3& rayDirection) const noexcept {
    // Simple: check distance from ray to each axis line.
    auto distToAxis = [](const Vec3& /*p*/, const Vec3& axisDir, const Vec3& axisOrigin,
                          const Vec3& rayOrigin, const Vec3& rayDir) -> float {
        // Approximate: distance from ray midpoint to axis point.
        Vec3 tip = axisOrigin + axisDir * 1.5f;
        Vec3 midRay = rayOrigin + rayDir * 5.f;
        Vec3 toTip = tip - midRay;
        return toTip.length();
    };

    float dX = distToAxis(center, {1,0,0}, center, rayOrigin, rayDirection);
    float dY = distToAxis(center, {0,1,0}, center, rayOrigin, rayDirection);
    float dZ = distToAxis(center, {0,0,1}, center, rayOrigin, rayDirection);

    if(dX < dY && dX < dZ && dX < 2.f) return Axis::X;
    if(dY < dX && dY < dZ && dY < 2.f) return Axis::Y;
    if(dZ < dX && dZ < dY && dZ < 2.f) return Axis::Z;
    return Axis::None;
}

void TransformGizmo::translate(const Vec3& /*center*/, Axis axis, float amount,
                                 nexus::cad::CadDocument& doc,
                                 nexus::parametric::FeatureId featureId) const noexcept {
    auto* node = doc.history().node(featureId);
    if(!node || !node->mesh) return;

    Vec3 offset{};
    if(axis == Axis::X) offset.x = amount;
    else if(axis == Axis::Y) offset.y = amount;
    else if(axis == Axis::Z) offset.z = amount;
    else return;

    auto pos = node->mesh->attributes().positions(); // copy
    for(auto& v : pos) { v.x += offset.x; v.y += offset.y; v.z += offset.z; }
    node->mesh->attributes().setPositions(std::move(pos));
}

void TransformGizmo::scale(const Vec3& center, Axis axis, float factor,
                             nexus::cad::CadDocument& doc,
                             nexus::parametric::FeatureId featureId) const noexcept {
    auto* node = doc.history().node(featureId);
    if(!node || !node->mesh) return;

    auto pos = node->mesh->attributes().positions(); // copy
    Vec3 scaleVec{1,1,1};
    if(axis == Axis::X) scaleVec.x = factor;
    else if(axis == Axis::Y) scaleVec.y = factor;
    else if(axis == Axis::Z) scaleVec.z = factor;
    else { scaleVec.x = factor; scaleVec.y = factor; scaleVec.z = factor; }
    for(auto& v : pos) {
        v.x = center.x + (v.x - center.x) * scaleVec.x;
        v.y = center.y + (v.y - center.y) * scaleVec.y;
        v.z = center.z + (v.z - center.z) * scaleVec.z;
    }
    node->mesh->attributes().setPositions(std::move(pos));
}

void TransformGizmo::rotate(const Vec3& center, Axis axis, float angleRad,
                              nexus::cad::CadDocument& doc,
                              nexus::parametric::FeatureId featureId) const noexcept {
    auto* node = doc.history().node(featureId);
    if(!node || !node->mesh) return;

    float c = std::cos(angleRad), s = std::sin(angleRad);
    auto pos = node->mesh->attributes().positions(); // copy
    for(auto& v : pos) {
        float rx = v.x - center.x;
        float ry = v.y - center.y;
        float rz = v.z - center.z;
        if(axis == Axis::X) { float ny = ry*c - rz*s; float nz = ry*s + rz*c; v.y = center.y+ny; v.z = center.z+nz; }
        else if(axis == Axis::Y) { float nx = rx*c + rz*s; float nz = -rx*s + rz*c; v.x = center.x+nx; v.z = center.z+nz; }
        else if(axis == Axis::Z) { float nx = rx*c - ry*s; float ny = rx*s + ry*c; v.x = center.x+nx; v.y = center.y+ny; }
    }
    node->mesh->attributes().setPositions(std::move(pos));
}

} // namespace nexus::app
