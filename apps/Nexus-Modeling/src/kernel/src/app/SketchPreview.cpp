#include <nexus/app/SketchPreview.h>
#include <nexus/parametric/ConstraintGraph.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

namespace nexus::app {

void SketchPreview::render(nexus::cad::CadAutoConstraintSketch& sketch) const noexcept {
    const auto& profile = sketch.profile();
    if(profile.points.empty()) return;

#ifndef NEXUS_HEADLESS
    glPointSize(6.f);
    glColor3f(0.2f, 0.8f, 1.0f);
    glBegin(GL_POINTS);
    for(auto id : profile.points) {
        auto* pt = profile.graph.point(id);
        if(pt) glVertex3d(pt->x, pt->y, pt->z);
    }
    glEnd();
    glPointSize(1.f);
#endif
}

} // namespace nexus::app
