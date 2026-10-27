#pragma once
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>
namespace bettergps_camera {
struct alignas(16) PackedPose {float position[3]{};int16_t sectors[2]{};float rotation[4]{1,0,0,0};};
static_assert(sizeof(PackedPose)==32,"SCS packed pose layout");
inline void quaternion_multiply(const float* a,const float* b,float* out){const float q[]={a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};for(unsigned i=0;i<4;++i)out[i]=q[i];}
inline void relative_pose(const PackedPose& world,const PackedPose& frame,PackedPose& out){
 const double p[]={double(world.position[0])-frame.position[0]+512.*(int(world.sectors[0])-int(frame.sectors[0])),double(world.position[1])-frame.position[1],double(world.position[2])-frame.position[2]+512.*(int(world.sectors[1])-int(frame.sectors[1]))};
 const double w=frame.rotation[0],x=-frame.rotation[1],y=-frame.rotation[2],z=-frame.rotation[3];
 const double tx=2*(y*p[2]-z*p[1]),ty=2*(z*p[0]-x*p[2]),tz=2*(x*p[1]-y*p[0]);
 out={};out.position[0]=float(p[0]+w*tx+y*tz-z*ty);out.position[1]=float(p[1]+w*ty+z*tx-x*tz);out.position[2]=float(p[2]+w*tz+x*ty-y*tx);
 const float inverse[]={float(w),float(x),float(y),float(z)};quaternion_multiply(inverse,world.rotation,out.rotation);
}
inline void camera_rotation(float yaw,float down,float* out){const auto a=yaw*3.14159265358979323846f/360,b=-down*3.14159265358979323846f/360;const float y[]={std::cos(a),0,std::sin(a),0},p[]={std::cos(b),std::sin(b),0,0};quaternion_multiply(y,p,out);}
// Camera material reverses both U and V. A camera-local half turn maps
// renderer axes to the displayed image axes (and its inverse is identical).
inline PackedPose camera_image_basis(const PackedPose& pose){PackedPose mapped=pose;const float halfTurn[]={0,0,0,1};quaternion_multiply(pose.rotation,halfTurn,mapped.rotation);return mapped;}
struct ViewMetrics {std::array<double,3> position{},forward{},up{};double yaw=0,down=0,roll=0;};
inline void rotate_vector(const float* rotation,const double* v,double* out){
 double q[4]={rotation[0],rotation[1],rotation[2],rotation[3]};double norm=0;for(auto n:q)norm+=n*n;if(norm<=1e-12)return;norm=std::sqrt(norm);for(auto& n:q)n/=norm;
 const double tx=2*(q[2]*v[2]-q[3]*v[1]),ty=2*(q[3]*v[0]-q[1]*v[2]),tz=2*(q[1]*v[1]-q[2]*v[0]);
 out[0]=v[0]+q[0]*tx+q[2]*tz-q[3]*ty;out[1]=v[1]+q[0]*ty+q[3]*tx-q[1]*tz;out[2]=v[2]+q[0]*tz+q[1]*ty-q[2]*tx;
}
inline ViewMetrics view_metrics(const PackedPose& pose){
 ViewMetrics m;m.position={double(pose.position[0])+512.*pose.sectors[0],pose.position[1],double(pose.position[2])+512.*pose.sectors[1]};
 const double forward[]={0,0,-1},up[]={0,1,0};rotate_vector(pose.rotation,forward,m.forward.data());rotate_vector(pose.rotation,up,m.up.data());
 constexpr double rad=180./3.14159265358979323846;
 m.yaw=std::atan2(-m.forward[0],-m.forward[2])*rad;if(m.yaw<0)m.yaw+=360;
 m.down=-std::asin(std::clamp(m.forward[1],-1.,1.))*rad;
 const double length=std::hypot(m.forward[0],m.forward[2]);if(length>1e-8){
  const double right[]={-m.forward[2]/length,0,m.forward[0]/length};
  const double flatUp[]={-right[2]*m.forward[1],right[2]*m.forward[0]-right[0]*m.forward[2],right[0]*m.forward[1]};
  double x=0,y=0;for(unsigned i=0;i<3;++i){x+=m.up[i]*right[i];y+=m.up[i]*flatUp[i];}m.roll=std::atan2(x,y)*rad;
 }return m;
}
inline double direction_error(const ViewMetrics& a,const ViewMetrics& b){double dot=0;for(unsigned i=0;i<3;++i)dot+=a.forward[i]*b.forward[i];return std::acos(std::clamp(dot,-1.,1.))*180./3.14159265358979323846;}

}
