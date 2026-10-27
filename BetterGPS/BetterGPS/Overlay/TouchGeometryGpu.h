#pragma once
#include "TouchGeometryView.h"
#include "TouchComposition.h"
#include <map>
namespace bettergps_overlay {
// Debug wireframe uses the established menu compositor, never a native draw.
struct TouchGeometryGpu {
 ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;unsigned width=0,height=0;uint64_t last=0;std::vector<uint8_t> pixels;
 void stop(){SettingsMenuRenderer::drop(view);SettingsMenuRenderer::drop(texture);width=height=0;last=0;pixels.clear();}
 static bool visible(const TouchGeometryMesh& mesh,uint64_t now){return mesh.session==touchStatus.sourceGeneration.load()&&now>=mesh.at&&now-mesh.at<=150;}
 static bool content_point(const TouchGeometryMesh& mesh,float u,float v,unsigned width,unsigned height,touch_geometry::Point& result){
  using namespace touch_geometry;if(u<0||u>1||v<0||v>1)return false;u=mesh.left+u*mesh.width;v=mesh.top+v*mesh.height;
  unsigned axes=0;if(touchComposition.mapping(mesh.proof,GetTickCount64(),axes)!=1)return false;TouchComposition::transform(axes,u,v);
  for(const auto& triangle:mesh.triangles){float minU=triangle.vertices[0].u,minV=triangle.vertices[0].v;for(const auto& p:triangle.vertices){minU=std::min(minU,p.u);minV=std::min(minV,p.v);}const int beginU=mesh.addressU==D3D11_TEXTURE_ADDRESS_WRAP?int(std::floor(minU)):0,beginV=mesh.addressV==D3D11_TEXTURE_ADDRESS_WRAP?int(std::floor(minV)):0;
   if(mesh.addressU!=D3D11_TEXTURE_ADDRESS_WRAP&&mesh.addressU!=D3D11_TEXTURE_ADDRESS_CLAMP||mesh.addressV!=D3D11_TEXTURE_ADDRESS_WRAP&&mesh.addressV!=D3D11_TEXTURE_ADDRESS_CLAMP)return false;
   for(int i=0;i<(mesh.addressU==D3D11_TEXTURE_ADDRESS_WRAP?2:1);++i)for(int j=0;j<(mesh.addressV==D3D11_TEXTURE_ADDRESS_WRAP?2:1);++j)if(project_uv(triangle,mesh.viewport,float(width),float(height),u+beginU+i,v+beginV+j,result))return true;
  }return false;
 }
 static void raster(std::vector<uint8_t>& pixels,unsigned width,unsigned height,const TouchGeometryScene& scene,POINT cursor,uint64_t now){
  using namespace touch_geometry;pixels.assign(size_t(width)*height*4,0);
  auto dot=[&](int x,int y,uint32_t color){if(x>=0&&y>=0&&unsigned(x)<width&&unsigned(y)<height)memcpy(pixels.data()+(size_t(y)*width+x)*4,&color,4);};
  auto line=[&](Point a,Point b,uint32_t color){
   float t0=0,t1=1,dx=b.x-a.x,dy=b.y-a.y;const float p[4]={-dx,dx,-dy,dy},q[4]={a.x,float(width-1)-a.x,a.y,float(height-1)-a.y};
   for(unsigned i=0;i<4;++i){if(std::abs(p[i])<1e-8f){if(q[i]<0)return;}else {const float t=q[i]/p[i];if(p[i]<0)t0=std::max(t0,t);else t1=std::min(t1,t);if(t0>t1)return;}}
   int x=int(a.x+t0*dx),y=int(a.y+t0*dy),endX=int(a.x+t1*dx),endY=int(a.y+t1*dy);int sx=x<endX?1:-1,sy=y<endY?1:-1,ax=std::abs(endX-x),ay=-std::abs(endY-y),error=ax+ay;
   for(unsigned n=0;n<width+height+4;++n){dot(x,y,color);if(x==endX&&y==endY)break;int e=2*error;if(e>=ay){error+=ay;x+=sx;}if(e<=ax){error+=ax;y+=sy;}}
  };
  for(const auto& mesh:scene.meshes)if(visible(mesh,now)){std::vector<Point> silhouette;std::map<std::array<int,4>,std::pair<unsigned,std::array<Point,2>>> edges;for(const auto& triangle:mesh.triangles){Point p[3];bool good=true;for(unsigned i=0;i<3;++i)good=project(triangle.vertices[i],mesh.viewport,float(width),float(height),p[i])&&good;if(!good)continue;for(auto point:p)silhouette.push_back(point);
   // Orange = candidate, not occlusion/final-composition proof.
   for(unsigned i=0;i<3;++i){auto a=p[i],b=p[(i+1)%3];if(a.x>b.x||(a.x==b.x&&a.y>b.y))std::swap(a,b);auto& edge=edges[{int(std::lround(a.x*16)),int(std::lround(a.y*16)),int(std::lround(b.x*16)),int(std::lround(b.y*16))}];++edge.first;edge.second={a,b};}
   float u,v,z;if(intersect(triangle,mesh.viewport,float(width),float(height),{float(cursor.x),float(cursor.y)},u,v,z)){line({float(cursor.x-7),float(cursor.y)},{float(cursor.x+7),float(cursor.y)},0xffff40e0);line({float(cursor.x),float(cursor.y-7)},{float(cursor.x),float(cursor.y+7)},0xffff40e0);}
  }
   const bool phone=mesh.slot<managedDeviceCount&&managed_page_kind(managed_snapshot(mesh.slot))==PageDevice::Phone;
   if(phone&&silhouette.size()>=3){std::sort(silhouette.begin(),silhouette.end(),[](Point a,Point b){return a.x<b.x||a.x==b.x&&a.y<b.y;});silhouette.erase(std::unique(silhouette.begin(),silhouette.end(),[](Point a,Point b){return a.x==b.x&&a.y==b.y;}),silhouette.end());std::vector<Point> hull;auto cross=[](Point a,Point b,Point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};for(auto p:silhouette){while(hull.size()>1&&cross(hull[hull.size()-2],hull.back(),p)<=.001f)hull.pop_back();hull.push_back(p);}const auto lower=hull.size();for(auto it=silhouette.rbegin()+1;it!=silhouette.rend();++it){while(hull.size()>lower&&cross(hull[hull.size()-2],hull.back(),*it)<=.001f)hull.pop_back();hull.push_back(*it);}if(hull.size()>1)for(size_t i=1;i<hull.size();++i)line(hull[i-1],hull[i],0xffffb828);}
   else for(const auto& edge:edges)if(edge.second.first==1)line(edge.second.second[0],edge.second.second[1],0xffffb828);
   if(mesh.slot<managedDeviceCount){const auto d=managed_snapshot(mesh.slot);if(managed_page_kind(d)==PageDevice::Phone&&managed_page_visual(mesh.slot).page==DevicePage::Home)for(bool waze:{false,true}){const auto r=phone_home_target(d,waze);Point p[4];if(content_point(mesh,r.left,r.top,width,height,p[0])&&content_point(mesh,r.right,r.top,width,height,p[1])&&content_point(mesh,r.right,r.bottom,width,height,p[2])&&content_point(mesh,r.left,r.bottom,width,height,p[3]))for(unsigned i=0;i<4;++i)line(p[i],p[(i+1)%4],0xff60d0ff);}}
   if(!mesh.triangles.empty()){Point p;if(project(mesh.triangles.front().vertices[0],mesh.viewport,float(width),float(height),p)&&p.x>=0&&p.y>=0&&p.x<float(width)&&p.y<float(height)){
    const char* digits[]={"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001","111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};const auto label=std::to_string(mesh.slot);int start=int(p.x)+5;
    for(char ch:label){const char* glyph=digits[ch-'0'];for(unsigned y=0;y<5;++y)for(unsigned x=0;x<3;++x)if(glyph[y*3+x]=='1')for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx)dot(start+int(x*2+xx),int(p.y)+5+int(y*2+yy),0xffffff00);start+=8;}
   }}
  }
  const auto sample=touchStatus.snapshot();if(sample.candidateAt&&now>=sample.candidateAt&&now-sample.candidateAt<=150){line({float(sample.candidateCursor.x-8),float(sample.candidateCursor.y)},{float(sample.candidateCursor.x+8),float(sample.candidateCursor.y)},0xffff40e0);line({float(sample.candidateCursor.x),float(sample.candidateCursor.y-8)},{float(sample.candidateCursor.x),float(sample.candidateCursor.y+8)},0xffff40e0);}if(sample.valid&&sample.sampleAt&&now>=sample.sampleAt&&now-sample.sampleAt<=150){line({float(sample.sampleCursor.x-5),float(sample.sampleCursor.y)},{float(sample.sampleCursor.x+5),float(sample.sampleCursor.y)},0xff40ff60);line({float(sample.sampleCursor.x),float(sample.sampleCursor.y-5)},{float(sample.sampleCursor.x),float(sample.sampleCursor.y+5)},0xff40ff60);}
 }
 void draw(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Texture2D* target,SettingsMenuRenderer& renderer,void(STDMETHODCALLTYPE* bind)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView* const*),float ratioX,float ratioY){
  if(!touchStatus.visible)return;auto scene=touchGeometryView.read();if(!scene)return;const auto now=GetTickCount64();if(std::none_of(scene->meshes.begin(),scene->meshes.end(),[&](const auto& m){return visible(m,now);})){last=0;return;}
  D3D11_TEXTURE2D_DESC td{};target->GetDesc(&td);const unsigned w=unsigned(std::lround(td.Width/ratioX)),h=unsigned(std::lround(td.Height/ratioY));if(!w||!h||w>3840||h>2160)return;
  PhoneInternalScope own;if(w!=width||h!=height){stop();width=w;height=h;D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device->CreateTexture2D(&d,nullptr,&texture))||FAILED(device->CreateShaderResourceView(texture,nullptr,&view))){stop();return;}}
  if(!last||now-last>=33){raster(pixels,w,h,*scene,{menuCursorX.load(),menuCursorY.load()},now);D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;for(unsigned y=0;y<h;++y)memcpy((uint8_t*)mapped.pData+y*mapped.RowPitch,pixels.data()+size_t(y)*w*4,w*4);context->Unmap(texture,0);last=now;}
  renderer.render(context,view,target,0,0,float(td.Width),float(td.Height),bind);
 }
};
}
