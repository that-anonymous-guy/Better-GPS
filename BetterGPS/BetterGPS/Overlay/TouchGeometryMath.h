#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace bettergps_overlay::touch_geometry {
struct V4 { float x[4]{}; float& operator[](size_t i){return x[i];}float operator[](size_t i)const{return x[i];} };
struct Vertex {V4 clip;float u=0,v=0;};
struct Viewport {float x=0,y=0,w=0,h=0,targetW=0,targetH=0;};
struct Triangle {std::array<Vertex,3> vertices;};
struct Point {float x=0,y=0;};
inline bool project(const Vertex& v,const Viewport& p,float width,float height,Point& out){
 if(!(v.clip[3]>1e-5f&&p.targetW>0&&p.targetH>0&&width>0&&height>0))return false;
 // Reject near-plane crossings rather than inventing clipped endpoints.
 for(float n:v.clip.x)if(!std::isfinite(n))return false;
 if(v.clip[2]<0||v.clip[2]>v.clip[3])return false;
 out={(p.x+(v.clip[0]/v.clip[3]+1)*p.w*.5f)*width/p.targetW,(p.y+(1-v.clip[1]/v.clip[3])*p.h*.5f)*height/p.targetH};
 return std::isfinite(out.x)&&std::isfinite(out.y);
}
inline bool intersect(const Triangle& triangle,const Viewport& vp,float width,float height,Point mouse,float& u,float& v,float& depth){
 Point p[3];for(unsigned i=0;i<3;++i)if(!project(triangle.vertices[i],vp,width,height,p[i]))return false;
 const float d=(p[1].y-p[2].y)*(p[0].x-p[2].x)+(p[2].x-p[1].x)*(p[0].y-p[2].y);if(std::abs(d)<1e-6f)return false;
 float b[3];b[0]=((p[1].y-p[2].y)*(mouse.x-p[2].x)+(p[2].x-p[1].x)*(mouse.y-p[2].y))/d;
 b[1]=((p[2].y-p[0].y)*(mouse.x-p[2].x)+(p[0].x-p[2].x)*(mouse.y-p[2].y))/d;b[2]=1-b[0]-b[1];
 if(b[0]<-1e-5f||b[1]<-1e-5f||b[2]<-1e-5f)return false;
 float total=0;u=v=depth=0;for(unsigned i=0;i<3;++i){const auto& a=triangle.vertices[i];const float q=b[i]/a.clip[3];total+=q;u+=q*a.u;v+=q*a.v;depth+=b[i]*a.clip[2]/a.clip[3];}
 if(!(total>0))return false;u/=total;v/=total;return std::isfinite(u)&&std::isfinite(v);
}
inline bool address(float raw,unsigned mode,float& value){
 if(!std::isfinite(raw))return false;
 if(mode==1)value=raw-std::floor(raw); // D3D11_TEXTURE_ADDRESS_WRAP
 else if(mode==2){float n=raw-2*std::floor(raw/2);value=n>1?2-n:n;}
 else if(mode==3)value=std::clamp(raw,0.f,1.f);
 else return false; // Border and mirror-once need a distinct visibility contract.
 return true;
}
inline bool project_uv(const Triangle& triangle,const Viewport& viewport,float width,float height,float u,float v,Point& result){
 const auto& a=triangle.vertices[0];const auto& b=triangle.vertices[1];const auto& c=triangle.vertices[2];const float det=(b.v-c.v)*(a.u-c.u)+(c.u-b.u)*(a.v-c.v);if(std::abs(det)<1e-9f)return false;
 const float x=((b.v-c.v)*(u-c.u)+(c.u-b.u)*(v-c.v))/det,y=((c.v-a.v)*(u-c.u)+(a.u-c.u)*(v-c.v))/det,z=1-x-y;if(x<-.0001f||y<-.0001f||z<-.0001f)return false;
 Vertex point{};for(unsigned i=0;i<4;++i)point.clip[i]=x*a.clip[i]+y*b.clip[i]+z*c.clip[i];return project(point,viewport,width,height,result);
}
}
