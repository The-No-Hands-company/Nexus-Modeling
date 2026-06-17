#include "SoftwareRasterizer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace nexus::softrast {

using Vec3 = nexus::render::Vec3;
using Vec4 = nexus::render::Vec4;
using Mat4 = nexus::render::Mat4;

void SoftwareRasterizer::renderImpl(PixelBuffer& buf, const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera, const RasterizerConfig& cfg, const Mat4& model) const
{
    if (!mesh.attributes().hasPositions()) return;
    const auto& pos = mesh.attributes().positions();
    const size_t nVerts = pos.size();
    const auto& topo = mesh.topology();
    uint32_t W = buf.width(), H = buf.height();
    std::vector<float> depth(W*H, std::numeric_limits<float>::infinity());

    // Compute normals if absent
    std::vector<Vec3> norms(pos.size(), {0,0,1});
    if (mesh.attributes().hasNormals()) {
        norms = mesh.attributes().normals();
    } else {
        for (size_t fi=0;fi<topo.faceCount();++fi) {
            auto& f = topo.face(fi); if (!f.indicesInBounds(nVerts)) continue; if (f.indices.size()<3) continue;
            Vec3 ab{pos[f.indices[1]].x-pos[f.indices[0]].x,pos[f.indices[1]].y-pos[f.indices[0]].y,pos[f.indices[1]].z-pos[f.indices[0]].z};
            Vec3 ac{pos[f.indices[2]].x-pos[f.indices[0]].x,pos[f.indices[2]].y-pos[f.indices[0]].y,pos[f.indices[2]].z-pos[f.indices[0]].z};
            Vec3 n{ab.y*ac.z-ab.z*ac.y,ab.z*ac.x-ab.x*ac.z,ab.x*ac.y-ab.y*ac.x};
            for (auto idx : f.indices) { norms[idx].x+=n.x; norms[idx].y+=n.y; norms[idx].z+=n.z; }
        }
        for (auto& n : norms) { float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z); n=l>1e-10f?Vec3{n.x/l,n.y/l,n.z/l}:Vec3{0,0,1}; }
    }

    // Build model-view-projection matrix
    Mat4 vp = camera.ubo().viewProj;
    auto mul = [](const Mat4& m, const Vec3& v) -> Vec4 {
        return {m.m[0][0]*v.x+m.m[0][1]*v.y+m.m[0][2]*v.z+m.m[0][3],
                m.m[1][0]*v.x+m.m[1][1]*v.y+m.m[1][2]*v.z+m.m[1][3],
                m.m[2][0]*v.x+m.m[2][1]*v.y+m.m[2][2]*v.z+m.m[2][3],
                m.m[3][0]*v.x+m.m[3][1]*v.y+m.m[3][2]*v.z+m.m[3][3]};
    };
    Mat4 mvp; mvp = vp * model;
    std::vector<Vec4> clip(pos.size());
    for (size_t i=0;i<pos.size();++i) clip[i]=mul(mvp,pos[i]);

    float hw=W*.5f, hh=H*.5f;

    for (size_t fi=0;fi<topo.faceCount();++fi) {
        auto& f=topo.face(fi); if(!f.indicesInBounds(nVerts)) continue; if(f.indices.size()<3) continue;
        for (size_t k=1;k+1<f.indices.size();++k) {
            uint32_t i0=f.indices[0], i1=f.indices[k], i2=f.indices[k+1];
            Vec4 v0=clip[i0], v1=clip[i1], v2=clip[i2];
            if (v0.w<.001f||v1.w<.001f||v2.w<.001f) continue;
            float iw0=1.f/v0.w, iw1=1.f/v1.w, iw2=1.f/v2.w;
            // NDC
            float nx0=v0.x*iw0, ny0=v0.y*iw0, nz0=v0.z*iw0;
            float nx1=v1.x*iw1, ny1=v1.y*iw1, nz1=v1.z*iw1;
            float nx2=v2.x*iw2, ny2=v2.y*iw2, nz2=v2.z*iw2;
            // Viewport
            float sx0=(nx0+1.f)*hw, sy0=(1.f-ny0)*hh;
            float sx1=(nx1+1.f)*hw, sy1=(1.f-ny1)*hh;
            float sx2=(nx2+1.f)*hw, sy2=(1.f-ny2)*hh;

            int minX=std::max(0,(int)std::floor(std::min({sx0,sx1,sx2})));
            int maxX=std::min((int)W-1,(int)std::ceil(std::max({sx0,sx1,sx2})));
            int minY=std::max(0,(int)std::floor(std::min({sy0,sy1,sy2})));
            int maxY=std::min((int)H-1,(int)std::ceil(std::max({sy0,sy1,sy2})));
            if (minX>maxX||minY>maxY) continue;

            float area=(sx1-sx0)*(sy2-sy0)-(sx2-sx0)*(sy1-sy0);
            //if (area<=0.f) { fprintf(stderr,"Culled: area=%.2f\n",area); continue; } // skip backface cull for debug

            for (int py=minY;py<=maxY;++py) {
                for (int px=minX;px<=maxX;++px) {
                    float cx=px+.5f, cy=py+.5f;
                    float d0=(sx1-sx2)*(cy-sy2)+(sx2-cx)*(sy1-sy2);
                    float d1=(sx2-sx0)*(cy-sy0)+(sx0-cx)*(sy2-sy0);
                    float d2=(sx0-sx1)*(cy-sy1)+(sx1-cx)*(sy0-sy1);
                    if (d0<0.f||d1<0.f||d2<0.f) continue;
                    if (d0<0.f||d1<0.f||d2<0.f) continue;
                    float invA = 1.f / area;
                    float w0=d0*invA, w1=d1*invA, w2=d2*invA;
                    float z=w0*nz0+w1*nz1+w2*nz2;
                    size_t idx=py*W+px;
                    if (z>=depth[idx]) continue;
                    depth[idx]=z;

                    RGBA8 color=cfg.baseColor;
                    if (cfg.mode==ShadingMode::Wireframe) {
                        if (px>minX&&px<maxX&&py>minY&&py<maxY) continue;
                        color=cfg.wireColor;
                    } else {
                        Vec3 N{w0*norms[i0].x+w1*norms[i1].x+w2*norms[i2].x,
                               w0*norms[i0].y+w1*norms[i1].y+w2*norms[i2].y,
                               w0*norms[i0].z+w1*norms[i1].z+w2*norms[i2].z};
                        float nl=std::sqrt(N.x*N.x+N.y*N.y+N.z*N.z);
                        if (nl>1e-10f) { N.x/=nl; N.y/=nl; N.z/=nl; }
                        float diff=std::max(0.f, N.x*cfg.lightDir.x+N.y*cfg.lightDir.y+N.z*cfg.lightDir.z);
                        float l=cfg.ambientMin+(1.f-cfg.ambientMin)*diff;
                        color={(uint8_t)std::min(255.f,l*color.r+.5f),
                               (uint8_t)std::min(255.f,l*color.g+.5f),
                               (uint8_t)std::min(255.f,l*color.b+.5f),255};
                    }
                    buf.setPixel(px,py,color);
                }
            }
        }
    }
}

} // namespace nexus::softrast
