#pragma once
#include <filesystem>
#include <fstream>
#include <vector>
#include <Windows.h>
namespace bettergps_overlay {
// ATS loads map_data and map materials at startup. Prepare native definitions
// before VFS mounting. LiveMapTheme updates reflected native palette fields
// and their UI caches later; the flattened map is never transformed.
inline bool native_map_dark(int theme,const std::filesystem::path& state){
    int previous=0;std::ifstream f(state);f>>previous;
    return theme==2||(theme==0&&previous==1);
}
inline bool native_map_save(const std::filesystem::path& state,bool dark){
    if(state.empty())return true;
    auto temp=state;temp+=L".tmp";
    std::ofstream f(temp);f<<(dark?1:0)<<'\n';f.close();
    return f&&MoveFileExW(temp.c_str(),state.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
inline bool native_map_prepare(const std::filesystem::path& assets,bool dark){
    namespace fs=std::filesystem;
    auto profile=assets/"native_themes"/(dark?"dark":"light");
    if(!fs::is_regular_file(profile/"colour_data.sui")||!fs::is_regular_file(profile/"material/ui/map/player.dds"))return false;
    struct File{fs::path from,to;};std::vector<File> files{{profile/"colour_data.sui",assets/"def/map_data/colour_data.sui"}};
    for(auto& p:fs::recursive_directory_iterator(profile/"material"))
        if(p.is_regular_file()&&p.path().extension()==".dds")files.push_back({p.path(),assets/p.path().lexically_relative(profile)});
    // Preflight all sources before making changes, fail rather than mount a
    // missing profile. Each replacement is atomic for readers and installers.
    for(auto& p:files){std::ifstream f(p.from,std::ios::binary);if(!f)return false;}
    for(auto& p:files){
        std::ifstream from(p.from,std::ios::binary),to(p.to,std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(from)),{}),old((std::istreambuf_iterator<char>(to)),{});
        from.close();to.close();
        if(data==old)continue;
        fs::create_directories(p.to.parent_path());auto temp=p.to;temp+=L".native.tmp";
        std::ofstream out(temp,std::ios::binary);out.write(data.data(),data.size());out.close();
        if(!out||!MoveFileExW(temp.c_str(),p.to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
    }
    return true;
}
}
