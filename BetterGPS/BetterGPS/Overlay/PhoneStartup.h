#pragma once
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <string>
#include <cstdio>
#include "LockscreenStyle.h"
namespace bettergps_overlay {
// Unknown telemetry leaves the normal GPS available. A rising electrical edge
// arms startup; start its clock only when a phone is actually rendered.
inline std::atomic<uint64_t> phonePower{0}; // unknown, off, armed, start tick + 3
// A model may be sampled in loading previews before the player's vehicle is ready.
inline std::atomic<bool> phoneStartupReady{true};
inline constexpr int startupBlackMs=1000;
inline std::atomic<int> phoneFirstDelaySeconds{5};
inline std::atomic<bool> phoneFirstWaitPending{false};
inline std::atomic<uint64_t> phoneFirstWaitStart{0};
inline std::atomic<uint64_t> phoneSessionSerial{1};
inline void phone_startup_session(){++phoneSessionSerial;phonePower=0;phoneFirstWaitStart=0;phoneFirstWaitPending=true;}
inline void phone_startup_resume_wait(uint64_t pausedAt,uint64_t now){
 auto start=phoneFirstWaitStart.load();if(!phoneFirstWaitPending||!start)return;
 const auto frozenFrom=std::max(pausedAt,start-1);if(now>frozenFrom)phoneFirstWaitStart.compare_exchange_strong(start,start+now-frozenFrom);
}
inline constexpr int startupTotalMs=startupBlackMs+4200;
inline std::atomic<int> phoneStartupHoldMs{3000};
inline thread_local int startupHoldOverride=-1,startupFontOverride=-1;
inline thread_local bool startupLayersScoped=false;
inline thread_local std::shared_ptr<const HomeLayers> startupLayersOverride;
inline int startup_hold_ms(){return startupHoldOverride>=0?startupHoldOverride:phoneStartupHoldMs.load();}
inline int startup_font(){return startupFontOverride>=0?startupFontOverride:homeClockFont.load();}
struct StartupVisualScope {int hold,font;bool scoped;std::shared_ptr<const HomeLayers> layers;StartupVisualScope(int duration,int face,std::shared_ptr<const HomeLayers> style={},bool replace=false):hold(startupHoldOverride),font(startupFontOverride),scoped(startupLayersScoped),layers(startupLayersOverride){startupHoldOverride=duration*1000;startupFontOverride=face;if(replace){startupLayersScoped=true;startupLayersOverride=std::move(style);}}~StartupVisualScope(){startupHoldOverride=hold;startupFontOverride=font;startupLayersScoped=scoped;startupLayersOverride=std::move(layers);}};
inline int startup_total_ms(){return startupBlackMs+400+startup_hold_ms()+800;}
inline constexpr uint64_t phoneShutdownBit=uint64_t(1)<<63;
inline constexpr int shutdownHoldMs=1000,shutdownFadeMs=400;
inline std::atomic<int> phoneShutdownVisual{startupTotalMs};
inline void phone_power(bool on,uint64_t now=GetTickCount64()) {
    auto old=phonePower.load();
    if(on){while(old<2||(old&phoneShutdownBit)){phoneStartupHoldMs=homeDurationSeconds.load()*1000;if(phonePower.compare_exchange_weak(old,2))return;}return;}
    while(!(old&phoneShutdownBit)&&old!=1){
        if(!old){if(phonePower.compare_exchange_weak(old,1))return;continue;}
        // Preserve a partially completed startup if power is cut mid-animation.
        phoneShutdownVisual=old>=3?int(std::min<uint64_t>(now>=old-3?now-(old-3):0,startup_total_ms())):0;
        if(phonePower.compare_exchange_weak(old,phoneShutdownBit|now))return;
    }
}
inline int startup_elapsed(uint64_t now) {
    auto state=phonePower.load();
    if(state==2){if(!phoneStartupReady.load())return 0;
        if(phoneFirstWaitPending.load()){
            auto start=phoneFirstWaitStart.load();if(!start){phoneFirstWaitStart.compare_exchange_strong(start,now+1);start=phoneFirstWaitStart.load();}
            if(now<start-1||now-(start-1)<uint64_t(phoneFirstDelaySeconds.load())*1000)return 0;
            phoneFirstWaitPending=false;
        }
        if(phonePower.compare_exchange_strong(state,now+3))++homePowerOnSerial;state=phonePower.load();}
    if(state&phoneShutdownBit){auto start=state&~phoneShutdownBit;auto elapsed=now>=start?now-start:0;
        return elapsed>=shutdownHoldMs+shutdownFadeMs?-2:-3-int(elapsed);}
    if(state<3)return state==1?-2:-1;
    return static_cast<int>(std::min<uint64_t>(now>=state-3?now-(state-3):0,startup_total_ms()));
}
inline int startup_frame(int elapsed) {
    if(elapsed<=-3){const int off=-elapsed-3;return off<shutdownHoldMs?-3:-4-(off-shutdownHoldMs)*30/1000;}
    if(elapsed<0)return elapsed;if(elapsed>=startup_total_ms())return 1000;if(elapsed<startupBlackMs)return 0;
    const int active=elapsed-startupBlackMs,swipe=400+startup_hold_ms();
    return active<swipe?1+std::min(active,1200)*30/1000:100+(active-swipe)*30/1000;
}
struct StartupPhase {int elapsed;uint64_t state;};
inline StartupPhase startup_sample(uint64_t now){for(;;){const auto before=phonePower.load();const int elapsed=startup_elapsed(now);const auto after=phonePower.load();if(before==after||(before==2&&after>=3&&!(after&phoneShutdownBit)))return {elapsed,after};}}
struct StartupUnlock {uint32_t width=0,height=0,count=0;const uint8_t* pixels=nullptr;};
inline const StartupUnlock& startup_unlock(){
    static const auto frames=[](){StartupUnlock out;HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&startup_unlock),&module);
        auto r=FindResourceW(module,MAKEINTRESOURCEW(302),RT_RCDATA);if(!r)return out;
        auto size=SizeofResource(module,r);auto loaded=LoadResource(module,r);auto bytes=static_cast<const uint8_t*>(LockResource(loaded));
        if(!bytes||size<12)return out;uint32_t w,h,n;memcpy(&w,bytes,4);memcpy(&h,bytes+4,4);memcpy(&n,bytes+8,4);
        if(!w||w>128||!h||h>128||!n||n>300||size!=12+size_t(w)*h*n*4)return out;
        out={w,h,n,bytes+12};return out;}();return frames;
}
inline unsigned startup_unlock_frame(int elapsed,unsigned count){
    if(!count||elapsed<500)return 0;
    return std::min(count-1,1u+unsigned(elapsed-500)*30/1000);
}
struct StartupClock {std::wstring time,period,date;};
inline StartupClock startup_clock(int64_t minutes){
    if(minutes<0)return {L"--:--",L"",L""};
    const auto day=minutes/1440;const int hour=int(minutes%1440)/60;wchar_t time[32],date[64];
    swprintf_s(time,L"%d:%02d",hour%12?hour%12:12,int(minutes%60));
    const wchar_t* days[]={L"Mon",L"Tue",L"Wed",L"Thu",L"Fri",L"Sat",L"Sun"};
    const auto week=day/7+1;
    swprintf_s(date,L"%s \x00b7 Week %lld%s",days[day%7],std::min<int64_t>(week,9000),week>9000?L"+":L"");
    return {time,hour<12?L"AM":L"PM",date};
}
// Grayscale coverage at 2x resolution; the clock uses a condensed
// display treatment while the small date keeps the font's native proportions.
inline void startup_text(std::vector<uint8_t>& mask,const std::wstring& text,const wchar_t* face,RECT box,double horizontalScale=1.0,int weight=FW_NORMAL,double sizeScale=1.0){
    if(text.empty())return;
    const int w=horizontalScale<0.9?4096:720;constexpr int h=1480;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    auto dc=CreateCompatibleDC(nullptr);if(!dc)return;void* bits=nullptr;auto bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bmp){DeleteDC(dc);return;}auto oldBmp=SelectObject(dc,bmp);SetTextColor(dc,RGB(255,255,255));SetBkColor(dc,RGB(0,0,0));SetBkMode(dc,TRANSPARENT);
    RECT ink{};int fontHeight=std::min(256,int(box.bottom-box.top)*3);HFONT font=nullptr;HGDIOBJ oldFont=nullptr;
    for(int attempt=0;attempt<2;++attempt){
        memset(bits,0,size_t(w)*h*4);font=CreateFontW(-fontHeight,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
        oldFont=SelectObject(dc,font);TextOutW(dc,16,16,text.data(),int(text.size()));GdiFlush();
        ink={w,h,0,0};auto* data=static_cast<uint8_t*>(bits);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(data[(size_t(y)*w+x)*4]){ink.left=std::min<LONG>(ink.left,x);ink.right=std::max<LONG>(ink.right,x+1);ink.top=std::min<LONG>(ink.top,y);ink.bottom=std::max<LONG>(ink.bottom,y+1);}
        SelectObject(dc,oldFont);DeleteObject(font);
        if(ink.right<=ink.left)break;
        if(!attempt){double fit=std::min(double((box.right-box.left)*2)/((ink.right-ink.left)*horizontalScale),double((box.bottom-box.top)*2)/(ink.bottom-ink.top));fontHeight=std::max(1,int(fontHeight*fit*sizeScale));}
    }
    if(ink.right>ink.left){
        const int left=(box.left+box.right)*1-int((ink.right-ink.left)*horizontalScale)/2,top=(box.top+box.bottom)*1-(ink.bottom-ink.top)/2;
        auto* data=static_cast<uint8_t*>(bits);
        for(int y=std::max<LONG>(0,box.top);y<std::min(740,int(box.bottom));++y)for(int x=std::max<LONG>(0,box.left);x<std::min(360,int(box.right));++x){unsigned coverage=0;
            for(int sy=0;sy<2;++sy)for(int sx=0;sx<2;++sx){int px=int(std::floor((x*2+sx-left)/horizontalScale))+ink.left,py=y*2+sy-top+ink.top;if(px>=ink.left&&px<ink.right&&py>=ink.top&&py<ink.bottom)coverage+=data[(size_t(py)*w+px)*4];}
            mask[size_t(y)*360+x]=std::max(mask[size_t(y)*360+x],static_cast<uint8_t>((coverage+2)/4));
        }
    }
    SelectObject(dc,oldBmp);DeleteObject(bmp);DeleteDC(dc);
}
struct StartupTypography {const wchar_t* timeFace;const wchar_t* dateFace;RECT timeBox,dateBox;double horizontalScale,dateSize;int dateWeight;};
inline StartupTypography startup_typography(int style,int variant=1){
 if(!style)return {L"Chonburi",L"Google Sans",{48,114,312,427},{60,82,300,99},.2084,.8,FW_NORMAL};
 variant=std::clamp(variant,0,2);const int margins[]={92,84,76},bottoms[]={400,418,444};
 return {L"Zen Tokyo Zoo",L"Roboto Light",{margins[variant],128,360-margins[variant],bottoms[variant]},{60,82,300,99},1.0,.8,FW_LIGHT};
}
inline std::array<std::wstring,2> startup_style2_clock(int64_t minutes){
 if(minutes<0)return {L"--",L"--"};
 const int hour=int(minutes%1440)/60;wchar_t hours[3],minute[3];
 swprintf_s(hours,L"%02d",hour%12?hour%12:12);swprintf_s(minute,L"%02d",int(minutes%60));
 return {hours,minute};
}
inline void startup_clock_mask(std::vector<uint8_t>& mask,int64_t minutes,int style,int variant=1){
 const auto typography=startup_typography(style,variant);
 if(style){
  const auto rows=startup_style2_clock(minutes);auto upper=typography.timeBox,lower=typography.timeBox;
  const int middle=(upper.top+upper.bottom)/2;upper.bottom=middle-5;lower.top=middle+5;
  startup_text(mask,rows[0],typography.timeFace,upper,typography.horizontalScale);
  startup_text(mask,rows[1],typography.timeFace,lower,typography.horizontalScale);
 }else startup_text(mask,startup_clock(minutes).time,typography.timeFace,typography.timeBox,typography.horizontalScale);
}

inline unsigned startup_overlay(unsigned background,unsigned foreground){
    return background<128?(2*background*foreground+127)/255:255-(2*(255-background)*(255-foreground)+127)/255;
}
inline const std::array<std::vector<uint8_t>,2>& startup_images(){
    static const auto images=[](){std::array<std::vector<uint8_t>,2> result;HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&startup_images),&module);
        for(int i=0;i<2;++i){auto r=FindResourceW(module,MAKEINTRESOURCEW(300+i),RT_RCDATA);if(!r)continue;
            auto size=SizeofResource(module,r);auto loaded=LoadResource(module,r);auto p=static_cast<const uint8_t*>(LockResource(loaded));
            uint32_t w=0,h=0;if(p&&size>=8){memcpy(&w,p,4);memcpy(&h,p+4,4);}if(w==360&&h==740&&size==8+360*740*4)result[i].assign(p+8,p+size);
        }return result;}();return images;
}
inline void render_phone_startup(std::vector<uint8_t>& pixels,int elapsed,int64_t gameMinutes=-1,bool dark=false,int fontVariant=1){
    if(elapsed==-1||elapsed>=startup_total_ms()||pixels.size()!=360*740*4)return;
    if(elapsed<=-3){
        render_phone_startup(pixels,phoneShutdownVisual.load(),gameMinutes,dark,fontVariant);
        const int off=-elapsed-3;if(off<shutdownHoldMs)return;
        const unsigned a=unsigned(std::clamp(off-shutdownHoldMs,0,shutdownFadeMs))*255/shutdownFadeMs;
        // Source-over black increases opacity as well as darkening the skin, so
        // the native map underneath disappears too; the final screen is opaque.
        for(size_t i=0;i<pixels.size();i+=4){const unsigned old=pixels[i+3],combined=a+(old*(255-a)+127)/255;
            for(int c=0;c<3;++c)pixels[i+c]=combined?uint8_t((pixels[i+c]*old*(255-a)/255+combined/2)/combined):0;
            pixels[i+3]=uint8_t(combined);}
        return;
    }
    if(elapsed==-2||elapsed<startupBlackMs){for(size_t i=0;i<pixels.size();i+=4){pixels[i]=pixels[i+1]=pixels[i+2]=0;pixels[i+3]=255;}return;}
    // All animation timings share this delayed origin, including the lock.
    elapsed-=startupBlackMs;
    const auto& images=startup_images();if(images[0].empty()||images[1].empty())return;
    const auto style=startupLayersScoped?startupLayersOverride:std::atomic_load(&homeLayers);const auto styleRevision=style?style->revision:0;
    const auto& background=style&&style->pixels[2]?*style->pixels[2]:images[0];
    const auto& toplayer=style&&style->pixels[3]?*style->pixels[3]:images[1];
    const int fontKey=startup_font()*4+std::clamp(fontVariant,0,2);const auto typography=startup_typography(startup_font(),fontVariant);
    static int cachedFont=-1,maskFont=-1;static int64_t cachedMinutes=INT64_MIN;static bool cachedDark=false;static uint64_t cachedStyle=~uint64_t(0);static std::vector<uint8_t> home,decoratedHome,mask;static int64_t maskMinutes=INT64_MIN;
    if(home.empty()||cachedMinutes!=gameMinutes||cachedDark!=dark||cachedStyle!=styleRevision||cachedFont!=fontKey){
        if(mask.empty()||maskMinutes!=gameMinutes||maskFont!=fontKey){
            const auto clock=startup_clock(gameMinutes);mask.assign(360*740,0);
            startup_clock_mask(mask,gameMinutes,startup_font(),fontVariant);
            // Font coverage depends on the visible time/date, not wallpaper/night.
            startup_text(mask,clock.date,typography.dateFace,typography.dateBox,1.0,typography.dateWeight,typography.dateSize);maskMinutes=gameMinutes;maskFont=fontKey;
        }
        home=background;
        // Neutral white in Overlay mode lights the wallpaper itself instead of
        // painting the glyphs cyan. Partial opacity retains its cloud detail.
        for(size_t i=0;i<mask.size();++i){const auto a=toplayer[i*4+3];
            for(int c=0;c<3;++c){const unsigned original=home[i*4+c],bg=dark?original*2/3:original,foreground=dark?toplayer[i*4+c]*2/3:toplayer[i*4+c],coverage=(mask[i]*210+127)/255;const unsigned text=(bg*(255-coverage)+startup_overlay(original,255)*coverage+127)/255;
                home[i*4+c]=static_cast<uint8_t>((foreground*a+text*(255-a)+127)/255);}
        }
        // Icons are static for this style. Precompose them once, except where
        // the animated lock must be placed underneath them on the current frame.
        decoratedHome=home;
        for(size_t src=0;src<home.size();src+=4)for(int c=0;c<3;++c){unsigned color=home[src+c];
            if(style&&style->pixels[1]){const auto* icon=style->pixels[1]->data()+src;const unsigned a=icon[3];color=(startup_overlay(color,icon[c])*a+color*(255-a)+127)/255;}
            if(style&&style->pixels[0]){const auto* icon=style->pixels[0]->data()+src;const unsigned a=icon[3];color=(icon[c]*a+color*(255-a)+127)/255;}
            decoratedHome[src+c]=uint8_t(color);
        }
        cachedMinutes=gameMinutes;cachedDark=dark;cachedStyle=styleRevision;cachedFont=fontKey;
    }
    const double fade=std::min(1.0,elapsed/400.0);
    const double swipe=std::clamp((elapsed-400-startup_hold_ms())/800.0,0.0,1.0);
    const double eased=swipe*swipe*(3-2*swipe);
    const int offset=static_cast<int>(std::lround(eased*740));
    const double opacity=1-swipe*swipe;
    const auto& unlock=startup_unlock();
    const auto* lockPixels=unlock.pixels?unlock.pixels+size_t(startup_unlock_frame(elapsed,unlock.count))*unlock.width*unlock.height*4:nullptr;
    const unsigned alpha=static_cast<unsigned>(std::lround(255*opacity));if(!alpha)return;
    for(int y=0;y<740-offset;++y)for(int x=0;x<360;++x){
        const size_t src=(size_t(y+offset)*360+x)*4,dst=(size_t(y)*360+x)*4;
        // Source coordinates make the lock travel and fade with the home screen,
        // including its held final frame. The closed shackle is centered at x180.
        const int lx=x-170,ly=y+offset-45;
        const auto* lockPixel=lockPixels&&lx>=0&&ly>=0&&lx<int(unlock.width)&&ly<int(unlock.height)?lockPixels+(size_t(ly)*unlock.width+lx)*4:nullptr;
        const unsigned old=pixels[dst+3],combined=alpha+(old*(255-alpha)+127)/255;
        for(int c=0;c<3;++c){unsigned colorAtHome=decoratedHome[src+c];
            if(lockPixel&&lockPixel[3]){colorAtHome=(lockPixel[c]*lockPixel[3]+home[src+c]*(255-lockPixel[3])+127)/255;
            // Icons are above the wallpaper, text, foreground and animated lock.
            if(style&&style->pixels[1]){const auto* icon=style->pixels[1]->data()+src;const unsigned a=icon[3];colorAtHome=(startup_overlay(colorAtHome,icon[c])*a+colorAtHome*(255-a)+127)/255;}
            if(style&&style->pixels[0]){const auto* icon=style->pixels[0]->data()+src;const unsigned a=icon[3];colorAtHome=(icon[c]*a+colorAtHome*(255-a)+127)/255;}}
            unsigned color=static_cast<unsigned>(colorAtHome*fade);
            pixels[dst+c]=static_cast<uint8_t>((color*alpha+(pixels[dst+c]*old*(255-alpha)+127)/255+combined/2)/combined);}
        pixels[dst+3]=static_cast<uint8_t>(combined);
    }
}
}
