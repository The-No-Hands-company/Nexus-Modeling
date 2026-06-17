#include <nexus/app/SelectionHighlight.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

void SelectionHighlight::render(const nexus::cad::CadDocument& doc,
                                 nexus::parametric::FeatureId selectedId) const noexcept {
    if(selectedId == nexus::parametric::kInvalidFeatureId) return;

    auto* node = doc.history().node(selectedId);
    if(!node || !node->mesh) return;

    const auto& pos = node->mesh->attributes().positions();
    const auto& topo = node->mesh->topology();
    const size_t psz = pos.size();

#ifndef NEXUS_HEADLESS
    glLineWidth(3.f);
    glColor3f(1.f, 0.6f, 0.1f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_TRIANGLES);
    for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if(face.vertexCount()<3) continue;
        for(size_t j=0; j+2<face.vertexCount(); ++j) {
            uint32_t i0=face.indices[0], i1=face.indices[j+1], i2=face.indices[j+2];
            if(i0>=psz||i1>=psz||i2>=psz) continue;
            auto& v0 = pos[i0], &v1 = pos[i1], &v2 = pos[i2];
            glVertex3f(v0.x, v0.y, v0.z);
            glVertex3f(v1.x, v1.y, v1.z);
            glVertex3f(v2.x, v2.y, v2.z);
        }
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.f);

    Vec3 center{0,0,0};
    size_t vc = pos.size();
    for(size_t i=0; i<vc; ++i) {
        center.x += pos[i].x;
        center.y += pos[i].y;
        center.z += pos[i].z;
    }
    if(vc > 0) { center.x /= static_cast<float>(vc); center.y /= static_cast<float>(vc); center.z /= static_cast<float>(vc); }

    float s = 0.5f;
    glBegin(GL_LINES);
    glColor3f(1,0,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x+s,center.y,center.z);
    glColor3f(0,1,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y+s,center.z);
    glColor3f(0,0,1); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y,center.z+s);
    glEnd();
#endif
}

} // namespace nexus::app
