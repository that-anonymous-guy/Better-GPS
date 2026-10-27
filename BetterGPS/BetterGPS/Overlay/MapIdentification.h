// RETIRED: unreferenced legacy map postprocessing. NativeMapTheme.h is used instead.
#pragma once
#include "MapPalette.h"
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace bettergps_overlay {
// Recognize generated map images that never load bare_map.dds. Dimensions and
// a red pixel alone are insufficient: require several source map colors and
// road pixels, spread across the image, with very little unrelated artwork.
inline bool map_pixels(const uint8_t* pixels,unsigned width,unsigned height,unsigned pitch,bool bgra,const MapPalette& palette){
    unsigned visible=0,known=0,roads=0,route=0,mask=0,total=0;
    float decode[256];for(unsigned i=0;i<256;++i){const float c=i/255.f;decode[i]=c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
    for(unsigned y=0;y<height;y+=std::max(1u,height/64)){
        for(unsigned x=0;x<width;x+=std::max(1u,width/64)){
            ++total;const auto* p=pixels+y*pitch+x*4;if(p[3]<200)continue;++visible;
            float rgb[3]={p[bgra?2:0]/255.f,p[1]/255.f,p[bgra?0:2]/255.f};
            float linear[3]={decode[p[bgra?2:0]],decode[p[1]],decode[p[bgra?0:2]]};
            float best=.0002f;int index=-1;
            for(unsigned i=0;i<mapPaletteCount;++i)for(const auto* source:{palette.source[i],builtinMapPalette.source[i]}){
                if(source[3]==0)continue;float d=0,e=0;for(int j=0;j<3;++j){d+=(rgb[j]-source[j])*(rgb[j]-source[j]);e+=(linear[j]-source[j])*(linear[j]-source[j]);}
                if(std::min(d,e)<best){best=std::min(d,e);index=static_cast<int>(i);}
            }
            if(index>=0){++known;mask|=1u<<index;if(index<4||index==12||index==13||index==26||index==27)++roads;if((index>=9&&index<=11)||(index>=20&&index<=25))++route;}
        }
    }
    unsigned colors=0;for(;mask;mask&=mask-1)++colors;
    const bool sparseRoute=route>=8&&colors>=2&&visible*100<total*75;
    return visible>=48&&known*100>=visible*85&&roads>=16&&((colors>=3&&route>=2)||colors>=4||sparseRoute);
}
}
