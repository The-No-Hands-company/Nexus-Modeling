#pragma once
#include "PixelBuffer.h"
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <cstdint>

namespace nexus::softrast {

enum class ShadingMode:uint8_t{Flat,Gouraud,Wireframe};

struct RasterizerConfig{
    ShadingMode mode=ShadingMode::Flat;
    RGBA8 background{30,30,30,255},baseColor{180,180,180,255},wireColor{220,220,220,255},specColor{255,255,255,255};
    nexus::render::Vec3 lightDir{.577f,.577f,.577f};
    float ambientMin=.15f,specStrength=0.f,shininess=32.f;
};

class SoftwareRasterizer{
public:
    void render(PixelBuffer&buf,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,const RasterizerConfig&cfg={})const{buf.clear(cfg.background);renderImpl(buf,mesh,camera,cfg,nexus::render::Mat4::identity());}
    void renderInto(PixelBuffer&buf,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,const RasterizerConfig&cfg,const nexus::render::Mat4&model)const{renderImpl(buf,mesh,camera,cfg,model);}
private:
    void renderImpl(PixelBuffer&buf,const nexus::geometry::Mesh&mesh,const nexus::render::Camera&camera,const RasterizerConfig&cfg,const nexus::render::Mat4&model)const;
};

} // namespace nexus::softrast
