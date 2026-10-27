#pragma once
// Local Steam screenshots only. No scan/thread/decode runs until Gallery opens.
static std::vector<std::filesystem::path> gallery_steam_folders(){
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
 std::vector<std::filesystem::path> roots;wchar_t path[32768]{};DWORD bytes=sizeof(path),active=0,size=sizeof(active);
 if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,path,&bytes)==ERROR_SUCCESS&&path[0])roots.emplace_back(path);
 RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam\\ActiveProcess",L"ActiveUser",RRF_RT_REG_DWORD,nullptr,&active,&size);
 if(roots.empty()){if(auto* program=_wgetenv(L"ProgramFiles(x86)"))roots.emplace_back(std::filesystem::path(program)/L"Steam");}
 std::vector<std::filesystem::path> result;std::error_code ec;
 for(const auto& root:roots){const auto users=root/L"userdata";if(active){result.push_back(users/std::to_wstring(active)/L"760/remote/270880/screenshots");continue;}
  for(std::filesystem::directory_iterator it(users,std::filesystem::directory_options::skip_permission_denied,ec),end;!ec&&it!=end;it.increment(ec)){auto name=it->path().filename().wstring();if(name.empty()||!std::all_of(name.begin(),name.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))continue;result.push_back(it->path()/L"760/remote/270880/screenshots");}ec.clear();}
 return result;
}
static bettergps_gallery::Picture gallery_decode(const std::filesystem::path& file,unsigned limit){
 auto source=file;std::error_code ec;if(limit==256){auto thumb=file.parent_path()/L"thumbnails"/file.filename();if(std::filesystem::is_regular_file(thumb,ec))source=thumb;}
 auto decoded=import_decode(source,limit);if(!decoded&&source!=file)decoded=import_decode(file,limit);if(!decoded||decoded->pixels.empty())return {};
 auto image=std::make_shared<bettergps_gallery::Image>();image->width=decoded->width;image->height=decoded->height;image->originalWidth=decoded->originalWidth;image->originalHeight=decoded->originalHeight;image->pixels=std::move(decoded->pixels);return image;
}
static void gallery_prepare(uint64_t now){bettergps_gallery::gallery.configure(&gallery_steam_folders,&gallery_decode);bettergps_gallery::gallery.activate(now);}
