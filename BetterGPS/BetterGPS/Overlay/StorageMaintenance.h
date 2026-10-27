#pragma once
#include <filesystem>
#include <vector>
#include <algorithm>
#include <system_error>
namespace bettergps_navigation {
namespace storage {
namespace fs=std::filesystem;
inline bool plain(const fs::path& path){auto attrs=GetFileAttributesW(path.c_str());return attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_REPARSE_POINT);}
inline bool plain_tree(const fs::path& root){if(!plain(root))return false;for(auto& f:fs::recursive_directory_iterator(root))if(!plain(f.path()))return false;return true;}
inline bool remove_owned(const fs::path& path,const fs::path& root){
    auto p=fs::absolute(path).lexically_normal(),r=fs::absolute(root).lexically_normal();auto rel=p.lexically_relative(r);
    if(rel.empty()||rel==L"."||*rel.begin()==L".."||!plain_tree(p))return false;
    std::error_code ec;fs::remove_all(p,ec);return !ec;
}
inline uint64_t size(const fs::path& root){uint64_t sum=0;if(!plain(root))return 0;for(auto& f:fs::recursive_directory_iterator(root)){if(!plain(f.path())){if(f.is_directory())throw std::runtime_error("Linked BetterGPS storage cannot be maintained");continue;}if(f.is_regular_file())sum+=f.file_size();}return sum;}
inline void trim_folders(const fs::path& root,uint64_t limit){
    if(!plain(root))return;uint64_t total=size(root);std::vector<fs::path> dirs;
    for(auto& p:fs::directory_iterator(root))if(p.is_directory()&&plain(p.path())){auto n=p.path().filename().wstring();bool session=n.size()==24&&n[8]==L'_'&&n[15]==L'_';if(session)for(size_t i=0;i<n.size();++i)if(i!=8&&i!=15&&!(i<15?iswdigit(n[i]):iswxdigit(n[i])))session=false;if(session)dirs.push_back(p.path());}
    std::sort(dirs.begin(),dirs.end(),[](auto& a,auto& b){return fs::last_write_time(a)<fs::last_write_time(b);});
    for(auto& dir:dirs){if(total<=limit)break;auto bytes=size(dir);if(remove_owned(dir,root))total-=bytes;}
}
// Called only before the host starts, or after its owned process has exited.
inline void maintain(const fs::path& cache,const fs::path& logs,bool trimCache=true){
    try{
        if(plain(cache)){
            auto evidence=cache/L"live-route-evidence";if(fs::exists(evidence))remove_owned(evidence,cache);
            if(trimCache){
            auto root=fs::absolute(cache).lexically_normal();std::vector<fs::path> candidates;uint64_t total=size(root);
            for(auto& f:fs::recursive_directory_iterator(root))if(f.is_regular_file()&&plain(f.path())){
                auto rel=f.path().lexically_relative(root);auto name=f.path().filename().wstring();
                bool fingerprint=name.size()>69&&name[64]==L'-'&&std::all_of(name.begin(),name.begin()+64,[](wchar_t c){return iswxdigit(c)!=0;});
                if((fingerprint&&f.path().extension()==L".json")||(*rel.begin()==L"world-v1"&&(f.path().extension()==L".wgc"||name==L"complete.txt")))candidates.push_back(f.path());
            }
            std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){return fs::last_write_time(a)<fs::last_write_time(b);});
            for(auto& file:candidates){if(total<=640ull*1024*1024)break;auto bytes=fs::file_size(file);std::error_code ec;fs::remove(file,ec);if(!ec){total-=bytes;if(file.extension()==L".wgc"){auto marker=file.parent_path()/L"complete.txt";if(fs::exists(marker)){auto count=fs::file_size(marker);fs::remove(marker,ec);if(!ec)total-=count;}}}}
            }
        }
        trim_folders(logs,32ull*1024*1024);
    }catch(const std::exception&){/* Storage must never prevent guidance. */}
}
}
}
