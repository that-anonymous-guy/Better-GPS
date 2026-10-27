#include "../TouchVertexCpu.h"
#include <iostream>
#include <stdexcept>
using namespace bettergps_overlay::touch_geometry;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 Triangle triangle;triangle.vertices={Vertex{{{-1,-1,.5f,1}},0,0},Vertex{{{2,-2,1,2}},1,0},Vertex{{{0,4,2,4}},0,1}};Viewport viewport{0,0,100,100,100,100};float u,v,z;
 check(intersect(triangle,viewport,100,100,{50,50},u,v,z)&&std::abs(u-.25f)<1e-5&&std::abs(v-.25f)<1e-5,"Perspective-correct UV interpolation failed");
 Point reverse;check(project_uv(triangle,viewport,100,100,u,v,reverse)&&std::abs(reverse.x-50)<1e-5&&std::abs(reverse.y-50)<1e-5,"Projected button border does not invert perspective UV hit test");
 check(!intersect(triangle,viewport,100,100,{0,0},u,v,z),"Outside triangle accepted");Point point;viewport={20,10,100,100,200,120};check(project(triangle.vertices[0],viewport,400,240,point)&&point.x==40&&point.y==220,"Viewport/client scaling incorrect");
 triangle.vertices[0].clip[3]=-1;check(!project(triangle.vertices[0],viewport,400,240,point),"Behind-camera vertex projected");
 check(address(3.25f,1,u)&&u==.25f&&address(-.25f,1,u)&&u==.75f,"Wrap mapping incorrect");check(address(1.25f,2,u)&&u==.75f&&address(-2,3,u)&&u==0&&!address(.2f,4,u),"Addressing mode guessed");
 Program program;check(program.parse("vs_5_0\ndcl_input v0.xyz\ndcl_output_siv o1.xyzw, position\nmul r0.xyz, v0.xyzx, cb3[0].xyzx\nadd o1.xyz, r0.xyzx, cb3[1].xyzx\nmov o1.w, l(1.0)\nmov o2.xy, v1.xyxx\nret\n"),"Straight-line shader rejected");
 std::array<Register,32> inputs{},outputs{};std::array<std::vector<V4>,14> constants;inputs[0]={{{1,2,3,1}},15};inputs[1]={{{.3f,.7f,0,1}},15};constants[3]={{{2,3,4,0}},{{10,20,30,0}}};std::string why;
 check(program.run(inputs,constants,outputs,why)&&outputs[1].valid==15&&outputs[1].value[0]==12&&outputs[1].value[1]==26&&outputs[1].value[2]==42&&outputs[2].value[1]==.7f,"Actual constant register/swizzle evaluation failed");
 constants[3].clear();check(program.run(inputs,constants,outputs,why)&&outputs[1].valid!=15,"Missing constants silently became zero");
 check(!program.parse("vs_5_0\nif_nz v0.x\nmov o0, v0\nendif\nret\n"),"Unsupported control flow accepted");
 check(program.parse("vs_5_0\nunknown o0, v0\nret\n")&&program.run(inputs,constants,outputs,why)&&outputs[0].valid==0,"Unknown operation fabricated output");
 std::cout<<"PASS: perspective UVs, viewport scaling, clipping, sampler addressing, actual constant slots/swizzles, missing dependencies and unsupported shader fail-closed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
