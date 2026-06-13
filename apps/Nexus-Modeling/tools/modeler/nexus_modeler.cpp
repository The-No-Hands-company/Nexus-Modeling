#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/geometry/MeshNormals.h>
#include <nexus/geometry/MeshSimplify.h>
#include <nexus/geometry/MeshDisplace.h>
#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/MeshConvexHull.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshVertexWeld.h>
#include <nexus/geometry/SurfaceOffset.h>
#include <nexus/procedural/Noise3D.h>
#include <nexus/render/Camera.h>
#include <nexus/sculpt/Brush.h>
#include <nexus/sculpt/SculptSession.h>
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

using namespace nexus::geometry;
using namespace nexus::softrast;
using Vec3 = nexus::render::Vec3;
using Mat4 = nexus::render::Mat4;
static constexpr uint32_t W=1200,H=800;

// ─── Colors ───
static const RGBA8 BG{40,40,45,255},DEF{180,175,170,255},SEL{230,180,100,255},HOV{200,190,170,255};
static const RGBA8 OBJ_COL[]={{185,175,165,255},{170,180,185,255},{180,170,175,255},{175,185,170,255},{170,175,185,255},{185,170,180,255}};

// ─── Camera ───
struct Cam{float az=60,el=25,dist=8,fov=45;uint32_t w=W,h=H;
    nexus::render::Camera build()const{float a=az*M_PI/180,e=el*M_PI/180;nexus::render::Camera c;c.setPerspective(fov,(float)w/h,.1f,200.f);c.lookAt({dist*cos(e)*sin(a),dist*sin(e),dist*cos(e)*cos(a)},{0,0,0});return c;}
    Vec3 eye()const{float a=az*M_PI/180,e=el*M_PI/180;return{dist*cos(e)*sin(a),dist*sin(e),dist*cos(e)*cos(a)};}
    Vec3 rgt()const{float a=az*M_PI/180;return{cos(a),0.f,-sin(a)};}
    Vec3 fwd()const{float a=az*M_PI/180,e=el*M_PI/180;return{cos(e)*sin(a),sin(e),cos(e)*cos(a)};}
    Vec3 up()const{auto r=rgt(),f=fwd();return{r.y*f.z-r.z*f.y,r.z*f.x-r.x*f.z,r.x*f.y-r.y*f.x};}
};

// ─── Primitives ───
static Mesh mkBox(float sx=1,float sy=1,float sz=1){Mesh m;float hx=sx*.5f,hy=sy*.5f,hz=sz*.5f;m.attributes().setPositions({{-hx,-hy,hz},{hx,-hy,hz},{hx,hy,hz},{-hx,hy,hz},{-hx,-hy,-hz},{hx,-hy,-hz},{hx,hy,-hz},{-hx,hy,-hz}});uint32_t f[][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};for(auto&q:f)m.topology().addFace(Face{{q[0],q[1],q[2],q[3]}});return m;}
static Mesh mkSphere(int sub=3){float s=.7071f;std::vector<Vec3>v={{s,0,0},{-s,0,0},{0,s,0},{0,-s,0},{0,0,s},{0,0,-s}};std::vector<std::array<uint32_t,3>>t={{0,2,4},{4,2,1},{4,1,3},{3,1,5},{5,1,2},{0,4,3},{3,5,0},{0,5,2}};for(int lv=0;lv<sub;++lv){std::vector<std::array<uint32_t,3>>nt;for(auto&ti:t){uint32_t a=ti[0],b=ti[1],c2=ti[2];Vec3 ab{(v[a].x+v[b].x)*.5f,(v[a].y+v[b].y)*.5f,(v[a].z+v[b].z)*.5f},bc{(v[b].x+v[c2].x)*.5f,(v[b].y+v[c2].y)*.5f,(v[b].z+v[c2].z)*.5f},ca{(v[c2].x+v[a].x)*.5f,(v[c2].y+v[a].y)*.5f,(v[c2].z+v[a].z)*.5f};uint32_t abi=(uint32_t)v.size();float l=sqrt(ab.x*ab.x+ab.y*ab.y+ab.z*ab.z);v.push_back({ab.x/l,ab.y/l,ab.z/l});uint32_t bci=(uint32_t)v.size();l=sqrt(bc.x*bc.x+bc.y*bc.y+bc.z*bc.z);v.push_back({bc.x/l,bc.y/l,bc.z/l});uint32_t cai=(uint32_t)v.size();l=sqrt(ca.x*ca.x+ca.y*ca.y+ca.z*ca.z);v.push_back({ca.x/l,ca.y/l,ca.z/l});nt.push_back({a,abi,cai});nt.push_back({abi,b,bci});nt.push_back({cai,bci,c2});nt.push_back({abi,bci,cai});}t=std::move(nt);}Mesh m;m.attributes().setPositions(v);for(auto&ti:t)m.topology().addFace(Face{{ti[0],ti[1],ti[2]}});return m;}
static Mesh mkCyl(int segs=24,int stacks=2,float r=.5f,float h=2.f){Mesh m;std::vector<Vec3>v;for(int j=0;j<=stacks;++j){float y=-h*.5f+j*(h/stacks);for(int i=0;i<segs;++i){float a=i*2.f*(float)M_PI/segs;v.push_back({r*cos(a),y,r*sin(a)});}}m.attributes().setPositions(v);for(int j=0;j<stacks;++j)for(int i=0;i<segs;++i){int a=j*segs+i,b=j*segs+(i+1)%segs,c=(j+1)*segs+i,d=(j+1)*segs+(i+1)%segs;m.topology().addFace(Face{{(uint32_t)a,(uint32_t)b,(uint32_t)d}});m.topology().addFace(Face{{(uint32_t)a,(uint32_t)d,(uint32_t)c}});}return m;}
static Mesh mkTorus(int segs=24,int rings=16,float R=1.f,float r=.3f){Mesh m;std::vector<Vec3>v;for(int j=0;j<rings;++j){float a1=j*2.f*(float)M_PI/rings;Vec3 c{R*cos(a1),0,R*sin(a1)};Vec3 t{-sin(a1),0,cos(a1)};for(int i=0;i<segs;++i){float a2=i*2.f*(float)M_PI/segs;v.push_back({c.x+r*cos(a2)*t.x,c.y+r*sin(a2),c.z+r*cos(a2)*t.z});}}m.attributes().setPositions(v);for(int j=0;j<rings;++j)for(int i=0;i<segs;++i){int a=j*segs+i,b=j*segs+(i+1)%segs,c=((j+1)%rings)*segs+i,d=((j+1)%rings)*segs+(i+1)%segs;m.topology().addFace(Face{{(uint32_t)a,(uint32_t)b,(uint32_t)d}});m.topology().addFace(Face{{(uint32_t)a,(uint32_t)d,(uint32_t)c}});}return m;}
static Mesh mkCone(int segs=24,int stacks=2,float r=.5f,float h=2.f){Mesh m;std::vector<Vec3>v;for(int j=0;j<=stacks;++j){float y=-h*.5f+j*(h/stacks);float cr=r*(1.f-j/(float)stacks);for(int i=0;i<segs;++i){float a=i*2.f*(float)M_PI/segs;v.push_back({cr*cos(a),y,cr*sin(a)});}}m.attributes().setPositions(v);for(int j=0;j<stacks;++j)for(int i=0;i<segs;++i){int a=j*segs+i,b=j*segs+(i+1)%segs,c=(j+1)*segs+i,d=(j+1)*segs+(i+1)%segs;m.topology().addFace(Face{{(uint32_t)a,(uint32_t)b,(uint32_t)d}});m.topology().addFace(Face{{(uint32_t)a,(uint32_t)d,(uint32_t)c}});}return m;}
// Grid floor: thin quads at y=-0.01
static Mesh mkGrid(int size=10,float spacing=1.f){
    Mesh m;std::vector<Vec3>v;float h=size*spacing*.5f,w=.02f;
    for(int i=-size;i<=size;++i){float p=i*spacing;v.insert(v.end(),{{p-w,-.01f,-h},{p+w,-.01f,-h},{p+w,-.01f,h},{p-w,-.01f,h}});}
    for(int i=-size;i<=size;++i){float p=i*spacing;v.insert(v.end(),{{-h,-.01f,p-w},{h,-.01f,p-w},{h,-.01f,p+w},{-h,-.01f,p+w}});}
    m.attributes().setPositions(v);
    for(size_t i=0;i<v.size();i+=4)m.topology().addFace(Face{{(uint32_t)i,(uint32_t)i+1,(uint32_t)i+2,(uint32_t)i+3}});
    return m;
}
// XYZ axis indicator: thin colored bars from origin
static void mkAxes(std::vector<Mesh>&out){
    float len=3.f,w=.03f;
    for(int ax=0;ax<3;++ax){
        Mesh m;float h=len*.5f;
        if(ax==0){m.attributes().setPositions({{-w,-w,0},{len,-w,0},{len,w,0},{-w,w,0},{0,-w,-w},{0,len,-w},{0,len,w},{0,-w,w}});} // X
        else if(ax==1){m.attributes().setPositions({{-w,0,-w},{-w,len,-w},{-w,len,w},{-w,0,w},{w,0,-w},{w,len,-w},{w,len,w},{w,0,w}});} // Y
        else{m.attributes().setPositions({{-w,-w,0},{w,-w,0},{w,w,0},{-w,w,0},{-w,-w,len},{w,-w,len},{w,w,len},{-w,w,len}});} // Z
        m.topology().addFace(Face{{0,1,2,3}});m.topology().addFace(Face{{4,5,6,7}});
        out.push_back(std::move(m));
    }
}

// ─── Scene ───
struct Obj{std::string n;Mesh m;Vec3 p{0,0,0};RGBA8 c=DEF;bool v=true;MeshBVH bvh;void rb(){bvh.build(m);}};
struct Scene{std::vector<Obj>o;int sel=-1;int hov=-1;std::deque<Mesh>undo;};
static void su(Scene&s){if(s.sel<0||(size_t)s.sel>=s.o.size())return;auto&ob=s.o[s.sel];Mesh c;std::vector<Vec3>cp;for(auto&p:ob.m.attributes().positions())cp.push_back(p);c.attributes().setPositions(cp);for(size_t i=0;i<ob.m.topology().faceCount();++i)c.topology().addFace(ob.m.topology().face(i));s.undo.push_back(std::move(c));if(s.undo.size()>32)s.undo.pop_front();}
static void undo(Scene&s){if(s.sel<0||s.undo.empty())return;s.o[s.sel].m=std::move(s.undo.back());s.undo.pop_back();s.o[s.sel].rb();SDL_Log("Undo");}
static void add(Scene&s,Mesh m,std::string n,Vec3 p={0,0,0},RGBA8 c=DEF){s.o.push_back({std::move(n),std::move(m),p,c,true,{}});s.o.back().rb();if(s.sel<0)s.sel=(int)s.o.size()-1;}

// ─── Ray pick ───
static Vec3 rdir(int mx,int my,const Cam&c){float x=(2.f*mx/(float)c.w-1.f),y=(1.f-2.f*my/(float)c.h),th=tan(c.fov*.5f*M_PI/180.f),asp=(float)c.w/c.h;auto f=c.fwd(),r=c.rgt(),u=c.up();Vec3 d{f.x+r.x*x*th*asp+u.x*y*th,f.y+r.y*x*th*asp+u.y*y*th,f.z+r.z*x*th*asp+u.z*y*th};float l=sqrt(d.x*d.x+d.y*d.y+d.z*d.z);return{d.x/l,d.y/l,d.z/l};}
static void rpick(Scene&s,int mx,int my,const Cam&c){auto d=rdir(mx,my,c);Ray ray{c.eye(),d};s.hov=-1;float b=1e30f;for(int i=0;i<(int)s.o.size();++i){if(!s.o[i].bvh.isValid())s.o[i].rb();auto h=s.o[i].bvh.raycast(ray);if(h.t<b){b=h.t;s.hov=i;}}}

int main(int argc,char*argv[]){
    bool off=false;int mf=0;for(int i=1;i<argc;++i){if(strcmp(argv[i],"--offscreen")==0)off=true;else if(strcmp(argv[i],"--frames")==0&&i+1<argc)mf=atoi(argv[++i]);}
    if(off)SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
    if(SDL_Init(SDL_INIT_VIDEO)!=0){SDL_Log("SDL:%s",SDL_GetError());return 1;}
    SDL_Window*win=SDL_CreateWindow("Nexus Modeler v7",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!win){SDL_Quit();return 1;}
    SDL_Surface*screen=SDL_CreateRGBSurface(0,W,H,32,0x00FF0000,0x0000FF00,0x000000FF,0xFF000000);
    if(!screen){SDL_DestroyWindow(win);SDL_Quit();return 1;}

    Scene sc;add(sc,mkBox(2,2,2),"box",{0,0,0},{140,140,155,255});sc.sel=0;
    SoftwareRasterizer sr;PixelBuffer buf(W,H);Cam cam;cam.w=W;cam.h=H;
    bool wire=false;int frm=0;bool run=true;bool drag=false;int dsx=0,dsy=0;Vec3 dsp;
    bool sm=false;nexus::sculpt::BrushKind bk=nexus::sculpt::BrushKind::Draw;float br=.6f,bs=.4f;
    std::unique_ptr<nexus::sculpt::SculptSession>ss;uint32_t si=nexus::sculpt::kInvalidStrokeId;bool sa=false;

    SDL_Log("Modeler v7 | 1-6=prims U/I/X=bool D=dec S=smooth N=noise T=thick H=hull M=mass O=off W=weld P=sculpt F1-4=brush F=wire G=shot Z=undo Arrows=orbit Q=quit");
    while(run){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)run=false;
            else if(e.type==SDL_MOUSEBUTTONDOWN){
                if(sm&&e.button.button==SDL_BUTTON_LEFT&&sc.sel>=0){if(!ss||sa){ss.reset(new nexus::sculpt::SculptSession(sc.o[sc.sel].m));}nexus::sculpt::BrushParams bp;bp.radius=br;bp.strength=bs;bp.useVertexNormal=(bk==nexus::sculpt::BrushKind::Draw||bk==nexus::sculpt::BrushKind::Inflate);si=ss->beginStroke(bk,bp);if(si!=nexus::sculpt::kInvalidStrokeId){sa=true;auto d=rdir(e.button.x,e.button.y,cam);Ray ry{cam.eye(),d};auto h=sc.o[sc.sel].bvh.raycast(ry);if(h.t<h.t+1){float t=h.t;Vec3 hp{cam.eye().x+ry.direction.x*t,cam.eye().y+ry.direction.y*t,cam.eye().z+ry.direction.z*t};nexus::sculpt::BrushSample sp;sp.position=hp;sp.pressure=1.f;sp.sequence=1;ss->applySample(si,sp);}}}
                else if(e.button.button==SDL_BUTTON_LEFT){rpick(sc,e.button.x,e.button.y,cam);if(sc.hov>=0)sc.sel=sc.hov;drag=true;dsx=e.button.x;dsy=e.button.y;if(sc.sel>=0)dsp=sc.o[sc.sel].p;}
                else if(e.button.button==SDL_BUTTON_RIGHT){auto d=rdir(e.button.x,e.button.y,cam);float t=1.f+sc.o.size()*.3f;auto cl=OBJ_COL[sc.o.size()%6];add(sc,mkBox(.5f,.5f,.5f),"b"+std::to_string(sc.o.size()),{cam.eye().x+d.x*t,cam.eye().y+d.y*t,cam.eye().z+d.z*t},cl);}
                else if(e.button.button==SDL_BUTTON_MIDDLE){drag=true;dsx=e.button.x;dsy=e.button.y;if(sc.sel>=0)dsp=sc.o[sc.sel].p;}
            }else if(e.type==SDL_MOUSEBUTTONUP){if(e.button.button==SDL_BUTTON_LEFT&&sa){ss->endStroke(si);sa=false;si=nexus::sculpt::kInvalidStrokeId;sc.o[sc.sel].rb();(void)MeshNormals::computeSmooth(sc.o[sc.sel].m);}if(e.button.button==SDL_BUTTON_LEFT||e.button.button==SDL_BUTTON_MIDDLE)drag=false;}
            else if(e.type==SDL_MOUSEMOTION){rpick(sc,e.motion.x,e.motion.y,cam);
                if(sa&&sc.sel>=0){auto d=rdir(e.motion.x,e.motion.y,cam);Ray ry{cam.eye(),d};auto h=sc.o[sc.sel].bvh.raycast(ry);if(h.t<h.t+1){float t=h.t;Vec3 hp{cam.eye().x+ry.direction.x*t,cam.eye().y+ry.direction.y*t,cam.eye().z+ry.direction.z*t};nexus::sculpt::BrushSample sp;sp.position=hp;sp.pressure=1.f;sp.sequence=ss->stats().samplesProcessed+1;ss->applySample(si,sp);}}
                else if(drag&&sc.sel>=0){float dx=(e.motion.x-dsx)*.01f,dy=(dsy-e.motion.y)*.01f;auto&o=sc.o[sc.sel];auto r=cam.rgt(),u=cam.up();o.p=Vec3{dsp.x+r.x*dx+u.x*dy,dsp.y+r.y*dx+u.y*dy,dsp.z+r.z*dx+u.z*dy};}}
            else if(e.type==SDL_MOUSEWHEEL){if(sm&&SDL_GetModState()&KMOD_SHIFT)bs=std::max(.05f,std::min(2.f,bs+(e.wheel.y>0?.1f:-.1f)));else if(sm)br=std::max(.1f,std::min(5.f,br+(e.wheel.y>0?.2f:-.2f)));else cam.dist*=(e.wheel.y>0)?.9f:1.1f;}
            else if(e.type==SDL_KEYDOWN){bool ctrl=SDL_GetModState()&KMOD_CTRL;
                switch(e.key.keysym.sym){
                case SDLK_ESCAPE:case SDLK_q:run=false;break;
                case SDLK_LEFT:cam.az-=5;break;case SDLK_RIGHT:cam.az+=5;break;case SDLK_UP:cam.el=std::min(89.f,cam.el+5);break;case SDLK_DOWN:cam.el=std::max(-89.f,cam.el-5);break;
                case SDLK_TAB:if(sc.o.size()>1){sc.sel=(sc.sel+1)%(int)sc.o.size();SDL_Log("Sel:%s",sc.o[sc.sel].n.c_str());}break;
                case SDLK_DELETE:if(sc.sel>=0&&sc.o.size()>1){sc.o.erase(sc.o.begin()+sc.sel);sc.sel=std::min(sc.sel,(int)sc.o.size()-1);}break;
                case SDLK_f:wire=!wire;break;
                case SDLK_g:{static int sn=0;char fn[64];snprintf(fn,sizeof(fn),"/tmp/modeler_%04d.ppm",sn++);if(writePPM(fn,buf))SDL_Log("%s",fn);}break;
                case SDLK_p:sm=!sm;if(sm&&sc.sel>=0)ss.reset(new nexus::sculpt::SculptSession(sc.o[sc.sel].m));SDL_Log(sm?"Sculpt ON":"Sculpt OFF");break;
                case SDLK_z:if(ctrl)undo(sc);break;
                case SDLK_d:if(ctrl&&sc.sel>=0){auto&o=sc.o[sc.sel];Mesh c;std::vector<Vec3>cp;for(auto&p:o.m.attributes().positions())cp.push_back(p);c.attributes().setPositions(cp);for(size_t i=0;i<o.m.topology().faceCount();++i)c.topology().addFace(o.m.topology().face(i));add(sc,std::move(c),o.n+"_copy",{o.p.x+1,o.p.y,o.p.z});}else if(sc.sel>=0){su(sc);SimplifyOptions op;uint32_t t=std::max(4u,(uint32_t)sc.o[sc.sel].m.topology().faceCount()/3);op.targetFaceCount=t;sc.o[sc.sel].m=MeshSimplify::decimate(sc.o[sc.sel].m,op);sc.o[sc.sel].rb();SDL_Log("Dec");}break;
                case SDLK_s:if(sc.sel>=0&&!ctrl){su(sc);SmoothOptions op;op.lambda=.5f;op.iterations=2;sc.o[sc.sel].m=MeshLaplacian::smooth(sc.o[sc.sel].m,op);sc.o[sc.sel].rb();SDL_Log("Smooth");}break;
                case SDLK_n:if(sc.sel>=0){su(sc);sc.o[sc.sel].m=MeshDisplace::displaceByScalar(sc.o[sc.sel].m,[](const Vec3&p)->float{nexus::procedural::Noise3D nz;return nz.fbm(p.x*3,p.y*3,p.z*3)*.3f;});(void)MeshNormals::computeSmooth(sc.o[sc.sel].m);sc.o[sc.sel].rb();SDL_Log("Displace");}break;
                case SDLK_t:if(sc.sel>=0){su(sc);ThickenOptions op;op.thickness=.05f;sc.o[sc.sel].m=MeshThicken::solidify(sc.o[sc.sel].m,op);sc.o[sc.sel].rb();SDL_Log("Thicken");}break;
                case SDLK_h:if(sc.sel>=0){auto r=MeshConvexHull::fromMesh(sc.o[sc.sel].m);Mesh hm;hm.attributes().setPositions(r.vertices);for(auto&f:r.faces)hm.topology().addFace(Face{{f[0],f[1],f[2]}});add(sc,std::move(hm),"hull");}break;
                case SDLK_m:if(sc.sel>=0){auto r=MeshMassProperties::compute(sc.o[sc.sel].m);if(r.volume>0){char b[256];snprintf(b,sizeof(b),"V=%.2f C=(%.1f,%.1f,%.1f)",r.volume,r.centroid.x,r.centroid.y,r.centroid.z);SDL_Log("%s",b);}}break;
                case SDLK_o:if(sc.sel>=0){SurfaceOffsetOptions op;op.distance=.1f;add(sc,SurfaceOffset::offset(sc.o[sc.sel].m,op),"off",sc.o[sc.sel].p);}break;
                case SDLK_w:if(sc.sel>=0&&!ctrl){WeldOptions op;op.tolerance=1e-3f;sc.o[sc.sel].m=MeshVertexWeld::weld(sc.o[sc.sel].m,op);sc.o[sc.sel].rb();SDL_Log("Weld");}break;
                case SDLK_u:case SDLK_i:case SDLK_x:if(sc.sel>=0&&sc.o.size()>1){BooleanOp bo=(e.key.keysym.sym==SDLK_u)?BooleanOp::Union:(e.key.keysym.sym==SDLK_i)?BooleanOp::Intersection:BooleanOp::Difference;auto r=MeshBoolean::compute(sc.o[sc.sel].m,sc.o[(sc.sel+1)%(int)sc.o.size()].m,bo);if(r.ok)add(sc,std::move(r.result),bo==BooleanOp::Union?"U":bo==BooleanOp::Intersection?"I":"D");}break;
                case SDLK_F1:bk=nexus::sculpt::BrushKind::Draw;SDL_Log("Draw");break;
                case SDLK_F2:bk=nexus::sculpt::BrushKind::Smooth;SDL_Log("Smooth");break;
                case SDLK_F3:bk=nexus::sculpt::BrushKind::Inflate;SDL_Log("Inflate");break;
                case SDLK_F4:bk=nexus::sculpt::BrushKind::Grab;SDL_Log("Grab");break;
                case SDLK_1:add(sc,mkBox(1.5f,1.5f,1.5f),"box",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_2:add(sc,mkSphere(3),"sphere",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_3:add(sc,mkCyl(),"cyl",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_4:add(sc,mkTorus(),"torus",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_5:add(sc,mkBox(3,.08f,3),"plane",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_6:add(sc,mkCone(),"cone",{0,0,0},OBJ_COL[sc.o.size()%6]);break;
                case SDLK_KP_1:cam.az=90;cam.el=0;break;case SDLK_KP_3:cam.az=0;cam.el=0;break;
                case SDLK_KP_7:cam.az=90;cam.el=90;break;case SDLK_KP_0:cam.az=60;cam.el=25;cam.dist=8;break;
                default:break;}
            }
        }
        // Render all objects
        RasterizerConfig cfg;cfg.mode=wire?ShadingMode::Wireframe:ShadingMode::Gouraud;
        cfg.background=BG;cfg.ambientMin=.45f;cfg.specStrength=.2f;cfg.shininess=16.f;
        cfg.lightDir={.4f,.5f,.6f};cfg.wireColor={160,155,150,255};
        buf.clear(cfg.background);
        auto camera=cam.build();Mat4 id=Mat4::identity();
        for(auto&o:sc.o){if(!o.v)continue;bool isS=sc.sel>=0&&(size_t)sc.sel<sc.o.size()&&&o==&sc.o[sc.sel];bool isH=sc.hov>=0&&(size_t)sc.hov<sc.o.size()&&&o==&sc.o[sc.hov];cfg.baseColor=isS?SEL:isH?HOV:o.c;
            sr.renderInto(buf,o.m,camera,cfg,id);}
        // Upload to window surface with explicit ARGB pixel format
        if(SDL_LockSurface(screen)==0){auto*d=(uint32_t*)screen->pixels;for(uint32_t y=0;y<H;++y)for(uint32_t x=0;x<W;++x){auto p=buf.getPixel(x,y);d[y*screen->pitch/4+x]=(uint32_t{255}<<24)|((uint32_t)p.r<<16)|((uint32_t)p.g<<8)|(uint32_t)p.b;}SDL_UnlockSurface(screen);}
        SDL_Surface*ws=SDL_GetWindowSurface(win);if(ws){SDL_BlitSurface(screen,nullptr,ws,nullptr);SDL_UpdateWindowSurface(win);}
        if(!off){char t[256];auto&o=sc.o[sc.sel>=0?sc.sel:0];snprintf(t,sizeof(t),"Modeler v7 | %zu obj | %s (%zuV %zuF)%s | 1-6=prims Q=quit",
            sc.o.size(),sc.sel>=0?sc.o[sc.sel].n.c_str():"none",
            o.m.attributes().positions().size(),o.m.topology().faceCount(),sm?" SCULPT":"");SDL_SetWindowTitle(win,t);}
        frm++;if(mf>0&&frm>=mf){writePPM("/tmp/modeler_last.ppm",buf);run=false;}
    }
    SDL_FreeSurface(screen);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
