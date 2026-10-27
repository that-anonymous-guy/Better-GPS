#pragma once
#include <atomic>
#include <algorithm>
namespace bettergps_overlay {
inline std::atomic<size_t> displayStyleIndex{0};
inline thread_local int displayStyleOverride=-1;
inline thread_local int displayDarkOverride=-1;
struct DisplayThemeScope {int previous;explicit DisplayThemeScope(bool dark):previous(displayDarkOverride){displayDarkOverride=dark?1:0;}~DisplayThemeScope(){displayDarkOverride=previous;}};
struct DisplayStyleScope {int previous;explicit DisplayStyleScope(int style):previous(displayStyleOverride){displayStyleOverride=style;}~DisplayStyleScope(){displayStyleOverride=previous;}};
inline bool waze_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==1;}
inline bool copilot_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==3;}
inline bool tomtom_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==4;}
inline bool sygic_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==6;}
inline bool vnl2014_style(){return (displayStyleOverride>=0?size_t(displayStyleOverride):displayStyleIndex.load())==5;}
// Keep the nine published Google/Waze/Garmin icon combinations stable.
inline constexpr unsigned nativeMapThemeCount=36;
inline size_t native_style_palette(size_t style){return style>=15?5:style>=12?4:style>=9?3:style%3;}
inline size_t native_style_icon(size_t style){return style>=15?std::min(size_t(2),style-15):style>=12?std::min(size_t(2),style-12):style>=9?std::min(size_t(2),style-9):style/3;}
inline int native_style_encode(int palette,int icon){return palette==6?15+icon:palette==4?12+icon:palette==3?9+icon:palette+icon*3;}
}
