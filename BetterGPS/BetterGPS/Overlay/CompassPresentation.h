#pragma once
#include <Windows.h>
#include <vector>
#include <cmath>
#include <cstring>
namespace bettergps_overlay {
inline void compass_module(){}
inline double compass_turn(double heading){return heading-std::floor(heading);}
inline void render_compass_at(std::vector<uint8_t>& pixels,unsigned width,unsigned height,int cx,int cy,double heading){
    if(!std::isfinite(heading)||cx<18||cy<18||cx+18>int(width)||cy+18>int(height)||pixels.size()!=size_t(width)*height*4)return;
    static const auto needle=[](){std::vector<uint8_t> data;HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&compass_module),&module);
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(254),RT_RCDATA);
        if(resource){const auto loaded=LoadResource(module,resource);const auto size=SizeofResource(module,resource);if(loaded&&size){const auto* p=static_cast<const uint8_t*>(LockResource(loaded));data.assign(p,p+size);}}
        return data;}();
    if(needle.size()!=8+36*36*4)return;
    // SDK heading grows counterclockwise: west=.25, east=.75. In a
    // heading-up display north rotates clockwise by that same turn value.
    // Correct the observed in-game needle offset clockwise (east).
    constexpr double calibrationDegreesEast=15.0;
    const double angle=(compass_turn(heading)+calibrationDegreesEast/360.0)*6.2831853071795864769,c=std::cos(angle),s=std::sin(angle);
    for(int y=cy-18;y<cy+18;++y)for(int x=cx-18;x<cx+18;++x){
        const double dx=x+.5-cx,dy=y+.5-cy;
        const double u=c*dx+s*dy+17.5,v=-s*dx+c*dy+17.5;
        const int ix=static_cast<int>(std::floor(u)),iy=static_cast<int>(std::floor(v));const double fx=u-ix,fy=v-iy;
        double a=0,color[3]{};
        for(int j=0;j<2;++j)for(int i=0;i<2;++i){const int sx=ix+i,sy=iy+j;if(sx<0||sx>=36||sy<0||sy>=36)continue;
            const auto* p=needle.data()+8+(sy*36+sx)*4;const double weight=(i?fx:1-fx)*(j?fy:1-fy),alpha=p[3]/255.;a+=weight*alpha;for(int k=0;k<3;++k)color[k]+=weight*alpha*p[k];}
        if(a<=0)continue;auto* dst=pixels.data()+(y*width+x)*4;const double oldA=dst[3]/255.,outA=a+oldA*(1-a);
        for(int k=0;k<3;++k)dst[k]=static_cast<uint8_t>(std::lround((color[k]+dst[k]*oldA*(1-a))/outA));dst[3]=static_cast<uint8_t>(std::lround(outA*255));
    }
}
inline void render_compass(std::vector<uint8_t>& pixels,unsigned width,unsigned height,bool trip,double heading){
    if(width!=360||height!=740)return;
    render_compass_at(pixels,width,height,trip?320:324,trip?407:592,heading);
}
inline void render_compass_needle(std::vector<uint8_t>& pixels,double heading){
    pixels.assign(36*36*4,0);render_compass_at(pixels,36,36,18,18,heading);
}

}
