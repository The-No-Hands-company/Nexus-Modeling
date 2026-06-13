#pragma once
#include <cstdint>
#include <vector>

namespace nexus::softrast {

struct RGBA8 { uint8_t r=0,g=0,b=0,a=255; };
struct PixelBuffer {
    PixelBuffer()=default;
    PixelBuffer(uint32_t w,uint32_t h):m_w(w),m_h(h){m_px.resize(w*h);}
    uint32_t width()const{return m_w;} uint32_t height()const{return m_h;}
    void clear(RGBA8 c={30,30,30,255}){for(auto&p:m_px)p=c;}
    void setPixel(uint32_t x,uint32_t y,RGBA8 c){if(x<m_w&&y<m_h)m_px[y*m_w+x]=c;}
    RGBA8 getPixel(uint32_t x,uint32_t y)const{if(x<m_w&&y<m_h)return m_px[y*m_w+x];return{};}
    const std::vector<RGBA8>& pixels()const{return m_px;}
private: uint32_t m_w=0,m_h=0; std::vector<RGBA8> m_px;
};

} // namespace nexus::softrast
