#include <nexus/geometry/FaceFillet.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

Mesh faceFillet(const Mesh& solid, uint32_t faceA, uint32_t faceB, const FaceFilletOptions& opts) noexcept {
    if(!solid.isValid()||opts.radius<=0.f) return solid;
    auto heOpt = HalfEdgeMesh::fromMesh(solid);
    if(!heOpt) return solid;
    HalfEdgeMesh he = *heOpt;
    if(faceA>=he.faceCount()||faceB>=he.faceCount()) return solid;

    // Find the common edge between faceA and faceB.
    uint32_t commonEdge = HalfEdgeMesh::kInvalid;
    uint32_t startHe = he.face(faceA).edge;
    if(startHe==HalfEdgeMesh::kInvalid) return solid;
    uint32_t heIdx = startHe;
    do {
        uint32_t twin = he.edge(heIdx).twin;
        if(twin!=HalfEdgeMesh::kInvalid && he.edge(twin).face==faceB) {
            commonEdge = heIdx; break;
        }
        heIdx = he.edge(heIdx).next;
    } while(heIdx!=startHe);

    if(commonEdge==HalfEdgeMesh::kInvalid) return solid;

    // Compute face normals and bisector for fillet direction.
    auto faceNormal = [&](uint32_t f)->Vec3{
        uint32_t fe = he.face(f).edge;
        Vec3 a=he.positions()[he.edge(fe).src];
        Vec3 b=he.positions()[he.edge(he.edge(fe).next).src];
        Vec3 c=he.positions()[he.edge(he.edge(he.edge(fe).next).next).src];
        Vec3 n=(b-a).cross(c-a); float len=n.length();
        return (len>1e-10f)?n*(1.f/len):Vec3{0,0,1};
    };

    Vec3 nA=faceNormal(faceA), nB=faceNormal(faceB);
    Vec3 bisector=nA+nB; float bLen=bisector.length();
    if(bLen<1e-10f) return solid;
    bisector=bisector*(1.f/bLen);

    // Check convex/concave.
    Vec3 pa=he.positions()[he.edge(commonEdge).src];
    uint32_t twin=he.edge(commonEdge).twin;
    Vec3 pb=he.positions()[he.edge(twin).src];
    Vec3 mid=(pa+pb)*0.5f;
    Vec3 offset=mid+bisector*opts.radius;
    if((offset-pa).dot(nA)<0.f||(offset-pb).dot(nB)<0.f) bisector=bisector*-1.f;

    // Split the common edge to create the fillet facet.
    uint32_t oldVc=he.vertexCount();
    if(!he.splitEdge(commonEdge)) return solid;
    if(oldVc>=he.vertexCount()) return solid;
    he.positions()[oldVc]=mid+bisector*opts.radius;

    return he.toMesh();
}

NurbsSurface extendSurface(const NurbsSurface& surf, const SurfaceExtensionOptions& opts) noexcept {
    if(!surf.isValid()) return {};
    int32_t nU=surf.controlPointCountU(), nV=surf.controlPointCountV();
    int32_t extU=opts.extendU?std::max(4,nU/4):0;
    int32_t extV=opts.extendU?0:std::max(4,nV/4);
    int32_t newNU=nU+extU, newNV=nV+extV;
    std::vector<Vec3> cps(static_cast<size_t>(newNU)*static_cast<size_t>(newNV));

    // Copy existing CPs and extrapolate new ones.
    for(int32_t i=0;i<newNU;++i) for(int32_t j=0;j<newNV;++j) {
        int32_t si=std::min(i,nU-1), sj=std::min(j,nV-1);
        Vec3 pt=surf.evaluate(
            surf.domainU().first+(surf.domainU().second-surf.domainU().first)*static_cast<float>(si)/static_cast<float>(nU-1),
            surf.domainV().first+(surf.domainV().second-surf.domainV().first)*static_cast<float>(sj)/static_cast<float>(nV-1));
        if(i>=nU) { Vec3 tan=surf.derivativeU(surf.domainU().second-0.01f,surf.domainV().first); pt=pt+tan*opts.distance*static_cast<float>(i-nU+1)/static_cast<float>(extU); }
        if(j>=nV) { Vec3 tan=surf.derivativeV(surf.domainU().first,surf.domainV().second-0.01f); pt=pt+tan*opts.distance*static_cast<float>(j-nV+1)/static_cast<float>(extV); }
        cps[static_cast<size_t>(i*newNV+j)]=pt;
    }

    int32_t degU=std::min(3,newNU-1),degV=std::min(3,newNV-1); degU=std::max(degU,1);degV=std::max(degV,1);
    std::vector<float> kU(static_cast<size_t>(newNU+degU+1)),kV(static_cast<size_t>(newNV+degV+1));
    for(int32_t j=0;j<=degU;++j){kU[static_cast<size_t>(j)]=0.f;kU[kU.size()-1-static_cast<size_t>(j)]=1.f;}
    for(int32_t j=1;j<newNU-degU;++j) kU[static_cast<size_t>(degU+j)]=static_cast<float>(j)/static_cast<float>(newNU-degU);
    for(int32_t j=0;j<=degV;++j){kV[static_cast<size_t>(j)]=0.f;kV[kV.size()-1-static_cast<size_t>(j)]=1.f;}
    for(int32_t j=1;j<newNV-degV;++j) kV[static_cast<size_t>(degV+j)]=static_cast<float>(j)/static_cast<float>(newNV-degV);
    return NurbsSurface(degU,degV,std::move(kU),std::move(kV),std::move(cps),newNU,newNV);
}

Mesh offsetFaceWithDraft(const HalfEdgeMesh& mesh, uint32_t face, const FaceOffsetOptions& opts) noexcept {
    (void)mesh;(void)face;(void)opts;
    return {};
}

ImprintResult imprintCurveOnMesh(const Mesh& mesh, const NurbsCurve& curve, const Vec3& projDir) noexcept {
    (void)mesh;(void)curve;(void)projDir;
    return {};
}
}
