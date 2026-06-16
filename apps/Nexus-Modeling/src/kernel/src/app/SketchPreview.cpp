#include <nexus/app/SketchPreview.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace nexus::app {

void SketchPreview::render(nexus::cad::CadAutoConstraintSketch& sketcher) const noexcept {
    auto& profile = sketcher.profile();
    if(profile.points.empty()) return;

    glPointSize(6.f);
    glColor3f(1.f, 0.8f, 0.2f);
    glBegin(GL_POINTS);
    for(auto id : profile.points) {
        auto* pt = profile.graph.point(id);
        if(pt) glVertex3d(pt->x, pt->y, pt->z);
    }
    glEnd();
}

} // namespace nexus::app
