#pragma once
#include "TouchGeometryMath.h"
#include <mutex>
#include <memory>
#include <string>
#include <atomic>
#include <d3d11.h>
#include <wrl/client.h>
namespace bettergps_overlay {
inline std::atomic<size_t> touchDepthBytes{0};
inline std::atomic<unsigned> touchMainTargetWidth{0},touchMainTargetHeight{0};
struct TouchGeometryDepth {
 Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
 float min=0,max=1;bool usable=false,scissored=false;RECT scissor{};
 // Render-thread snapshots become immutable before publication to the worker.
 Microsoft::WRL::ComPtr<ID3D11Texture2D> frozen,pixel;
 Microsoft::WRL::ComPtr<ID3D11Query> completed;
 D3D11_DEPTH_STENCIL_DESC state{};D3D11_TEXTURE2D_DESC desc{};DXGI_FORMAT viewFormat=DXGI_FORMAT_UNKNOWN;
 POINT cursor{};unsigned clientW=0,clientH=0,x=0,y=0;uint64_t trace=0,at=0,frame=0,id=0,continuity=0;
 bool sealed=false,ready=false;float actual=-1;std::array<uint8_t,8> raw{};
 std::string boundary;
 std::string reason="No native depth evidence";
 size_t reserved=0;
 ~TouchGeometryDepth(){if(reserved)touchDepthBytes.fetch_sub(reserved);}
};
struct TouchGeometryMesh {
 unsigned slot=~0u,addressU=0,addressV=0;uint64_t at=0,epoch=0,power=0,page=0,frame=0,session=0;
 float left=0,top=0,width=1,height=1;
 touch_geometry::Viewport viewport;std::vector<touch_geometry::Triangle> triangles;
 std::shared_ptr<const TouchGeometryDepth> depth;uint64_t proof=0;
};
struct TouchGeometryScene {std::vector<TouchGeometryMesh> meshes;std::string reason="Awaiting native screen draw";unsigned observed=0,completed=0,dropped=0;};
struct TouchGeometryView {
 std::mutex mutex;std::shared_ptr<const TouchGeometryScene> scene;
 void publish(TouchGeometryScene value){auto next=std::make_shared<const TouchGeometryScene>(std::move(value));std::lock_guard<std::mutex> lock(mutex);scene=std::move(next);}
 std::shared_ptr<const TouchGeometryScene> read(){std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);return lock?scene:nullptr;}
};
inline TouchGeometryView touchGeometryView;
}
