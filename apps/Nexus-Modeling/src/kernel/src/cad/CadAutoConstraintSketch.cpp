#include <nexus/cad/CadAutoConstraintSketch.h>
#include <algorithm>
#include <cmath>

namespace nexus::cad {

CadAutoConstraintSketch::CadAutoConstraintSketch(CadDocument& doc) : m_doc(doc) {}

void CadAutoConstraintSketch::beginSketch() { m_profile = parametric::ParametricSketchFactory::createSketch(); }

void CadAutoConstraintSketch::endSketch() {
    if(m_profile.points.size()>=2) m_sketchFeatureId = m_doc.addSketch(m_profile);
}

parametric::ParametricEntityId CadAutoConstraintSketch::addPoint(double x, double y) {
    if(m_opts.inferCoincident) {
        for(auto eid : m_profile.points) {
            const auto* ep = m_profile.graph.point(eid);
            if(!ep) continue;
            double dx=x-ep->x, dy=y-ep->y;
            if(std::sqrt(dx*dx+dy*dy) < m_opts.snapRadius) return eid;
        }
    }
    auto id = parametric::ParametricSketchFactory::addSketchPoint(m_profile, x, y);
    inferConstraints(id);
    return id;
}

parametric::ParametricConstraintId CadAutoConstraintSketch::addLine(parametric::ParametricEntityId p1, parametric::ParametricEntityId p2) {
    return parametric::ParametricSketchFactory::addLineSegment(m_profile, p1, p2);
}

std::array<parametric::ParametricEntityId,4> CadAutoConstraintSketch::addRectangle(double ox,double oy,double w,double h) {
    auto [origin,xH,yH,corner] = parametric::ParametricSketchFactory::addRectangle(m_profile,ox,oy,w,h);
    return {origin,xH,yH,corner};
}

parametric::ParametricEntityId CadAutoConstraintSketch::addCircle(double cx,double cy,double r) {
    auto pts = parametric::ParametricSketchFactory::addCircle(m_profile,cx,cy,r);
    return pts.empty() ? parametric::kInvalidEntityId : pts.front();
}

void CadAutoConstraintSketch::inferConstraints(parametric::ParametricEntityId newPoint) {
    const auto* np = m_profile.graph.point(newPoint);
    if(!np) return;
    for(auto eid : m_profile.points) {
        if(eid == newPoint) continue;
        const auto* ep = m_profile.graph.point(eid);
        if(!ep) continue;
        double dx=np->x-ep->x, dy=np->y-ep->y;
        double dist=std::sqrt(dx*dx+dy*dy);
        if(m_opts.inferHorizontal && dist>m_opts.snapRadius && std::abs(dy)<m_opts.snapRadius)
            (void)m_profile.graph.addAxisAlignedDistanceConstraint(newPoint,eid,parametric::Axis::Y,0.0);
        if(m_opts.inferVertical && dist>m_opts.snapRadius && std::abs(dx)<m_opts.snapRadius)
            (void)m_profile.graph.addAxisAlignedDistanceConstraint(newPoint,eid,parametric::Axis::X,0.0);
    }
}

}
