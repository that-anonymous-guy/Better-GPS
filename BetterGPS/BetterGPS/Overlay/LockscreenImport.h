#pragma once
#include "LockscreenStyle.h"
#include "F150Startup.h"
#include <cwctype>
#include <stdexcept>
#include <set>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")
namespace bettergps_overlay {
enum class ImportPage {Closed,Browse,Loading,Crop,Saving};
enum class ImportJob {None,List,Decode,Save,Filter};
inline constexpr size_t importPageSize=9;
struct ImportImage {unsigned width=0,height=0,originalWidth=0,originalHeight=0;HomePixels pixels;};
struct ImportEntry {std::filesystem::path path;bool directory=false;};
struct ImportState {
 std::mutex mutex;ImportPage page=ImportPage::Closed;ImportJob job=ImportJob::None;
 uint64_t generation=0;size_t layer=2,first=0;int deviceSlot=-1;std::filesystem::path folder,file;
 std::wstring address,search;bool edit=false,searchEdit=false,drag=false;int mouseX=0,mouseY=0;
 std::vector<ImportEntry> entries;std::vector<size_t> filtered;std::shared_ptr<const std::vector<std::wstring>> searchNames;std::shared_ptr<const ImportImage> image;
 double centerX=.5,centerY=.5,zoom=1;std::string error;
};
inline ImportState homeImport;
inline std::atomic<bool> homeImportActive{false};
inline std::atomic<uint64_t> homeImportRevision{0};
inline std::filesystem::path importDeviceRoot;
inline std::atomic<unsigned> importDeviceWidth{1280},importDeviceHeight{800};
inline void (*importDeviceSaved)(int,const std::filesystem::path&)=nullptr;
inline unsigned import_width(size_t layer){return layer==4?importDeviceWidth.load():layer==5?(carIsBronco?1024:1440):layer==6?800:360;}
inline unsigned import_height(size_t layer){return layer==4?importDeviceHeight.load():layer==5?(carIsBronco?620:540):layer==6?500:740;}
inline int import_preview_height(size_t layer){return layer==4?int(240*importDeviceHeight.load()/importDeviceWidth.load()):(layer==5||layer==6)?int(240*import_height(layer)/import_width(layer)):493;}
inline bool import_extension(const std::filesystem::path& p){auto e=home_lower(p.extension().u8string());return e==".png"||e==".dds"||e==".bmp"||e==".jpg"||e==".jpeg"||e==".tif"||e==".tiff";}
inline const char* import_label(size_t i){const char* names[]={"Icons - Normal mix","Icons - Overlay mix","Background","Toplayer","Startup splash","F150 info splash","Frame skin"};return names[std::min(size_t(6),i)];}
inline std::filesystem::path import_user_folder(bool desktop){PWSTR path=nullptr;std::filesystem::path result;
 if(SUCCEEDED(SHGetKnownFolderPath(desktop?FOLDERID_Desktop:FOLDERID_Downloads,KF_FLAG_DONT_VERIFY,nullptr,&path))&&path)result=path;CoTaskMemFree(path);
 if(result.empty()){wchar_t profile[32768]{};if(GetEnvironmentVariableW(L"USERPROFILE",profile,32768))result=std::filesystem::path(profile)/(desktop?L"Desktop":L"Downloads");else result=std::filesystem::current_path();}return result;
}
inline bool import_matches(const std::wstring& name,const std::wstring& query){return query.empty()||FindNLSStringEx(LOCALE_NAME_USER_DEFAULT,FIND_FROMSTART|NORM_IGNORECASE|NORM_IGNORENONSPACE,name.c_str(),int(name.size()),query.c_str(),int(query.size()),nullptr,nullptr,nullptr,0)>=0;}
inline void import_search_locked(){++homeImport.generation;homeImport.first=0;homeImport.job=ImportJob::Filter;++homeImportRevision;}
inline bool import_escape_search(){std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.page!=ImportPage::Browse||!homeImport.searchEdit)return false;homeImport.searchEdit=false;if(!homeImport.search.empty()){homeImport.search.clear();import_search_locked();}else ++homeImportRevision;return true;}
inline void import_close(){std::lock_guard<std::mutex> lock(homeImport.mutex);++homeImport.generation;homeImport.page=ImportPage::Closed;homeImport.job=ImportJob::None;homeImport.image.reset();homeImport.entries.clear();homeImport.filtered.clear();homeImport.searchNames.reset();homeImport.search.clear();homeImport.searchEdit=false;homeImport.drag=homeImport.edit=false;homeImportActive=false;++homeImportRevision;}
inline void import_open(size_t layer){std::lock_guard<std::mutex> lock(homeImport.mutex);++homeImport.generation;homeImport.layer=layer;homeImport.deviceSlot=-1;importDeviceWidth=1280;importDeviceHeight=800;homeImport.page=ImportPage::Loading;homeImport.job=ImportJob::List;homeImport.error.clear();homeImport.first=0;homeImport.edit=homeImport.searchEdit=false;homeImport.search.clear();homeImport.image.reset();homeImport.drag=false;
 if(homeImport.folder.empty())homeImport.folder=import_user_folder(false);
 homeImport.address=homeImport.folder.wstring();homeImportActive=true;++homeImportRevision;
}
struct ImportCrop {double left,top,width,height;};
inline ImportCrop import_crop(const ImportImage& image,double& cx,double& cy,double zoom,unsigned outWidth=360,unsigned outHeight=740){
 const double w=std::min(double(image.width),double(image.height)*outWidth/outHeight)/std::clamp(zoom,1.0,16.0),h=w*outHeight/outWidth;
 cx=std::clamp(cx,w/(2*image.width),1-w/(2*image.width));cy=std::clamp(cy,h/(2*image.height),1-h/(2*image.height));
 return {cx*image.width-w/2,cy*image.height-h/2,w,h};
}
inline HomePixels import_resample(const ImportImage& image,double cx,double cy,double zoom,unsigned outWidth=360,unsigned outHeight=740){
 if(!image.width||!image.height||image.pixels.size()!=size_t(image.width)*image.height*4)return {};
 const auto crop=import_crop(image,cx,cy,zoom,outWidth,outHeight);HomePixels out(size_t(outWidth)*outHeight*4);
 for(unsigned y=0;y<outHeight;++y)for(unsigned x=0;x<outWidth;++x){
  const double sx=std::clamp(crop.left+(x+.5)*crop.width/outWidth-.5,0.0,double(image.width-1)),sy=std::clamp(crop.top+(y+.5)*crop.height/outHeight-.5,0.0,double(image.height-1));
  const unsigned x0=unsigned(sx),y0=unsigned(sy),x1=std::min(x0+1,image.width-1),y1=std::min(y0+1,image.height-1);const double fx=sx-x0,fy=sy-y0;
  const uint8_t* p[]={image.pixels.data()+(size_t(y0)*image.width+x0)*4,image.pixels.data()+(size_t(y0)*image.width+x1)*4,image.pixels.data()+(size_t(y1)*image.width+x0)*4,image.pixels.data()+(size_t(y1)*image.width+x1)*4};
  const double weights[]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};double a=0,c[3]{};
  for(int i=0;i<4;++i){a+=weights[i]*p[i][3];for(int k=0;k<3;++k)c[k]+=weights[i]*p[i][k]*p[i][3];}
  auto* dst=out.data()+(size_t(y)*outWidth+x)*4;dst[3]=uint8_t(std::clamp(std::lround(a),0l,255l));for(int k=0;k<3;++k)dst[k]=a?uint8_t(std::clamp(std::lround(c[k]/a),0l,255l)):0;
 }return out;
}
inline std::shared_ptr<ImportImage> import_decode(const std::filesystem::path& path,unsigned limit=2048,const ImportCrop* region=nullptr,unsigned outWidth=360,unsigned outHeight=740){
 auto image=std::make_shared<ImportImage>();
 if(home_lower(path.extension().u8string())==".dds"){
  HomeDecoder decoder;image->pixels=decoder.dds(path,&image->width,&image->height,limit);std::ifstream file(path,std::ios::binary);uint32_t hdr[5]{};file.read((char*)hdr,sizeof(hdr));image->originalWidth=hdr[4];image->originalHeight=hdr[3];return image->pixels.empty()?nullptr:image;
 }
 const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICBitmapScaler* scaler=nullptr;IWICFormatConverter* converter=nullptr;IWICBitmapFlipRotator* rotate=nullptr;IWICBitmapClipper* clipper=nullptr;
 bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame));
 UINT width=0,height=0;IWICBitmapSource* source=frame;
 if(ok){IWICMetadataQueryReader* metadata=nullptr;PROPVARIANT orientation{};PropVariantInit(&orientation);if(SUCCEEDED(frame->GetMetadataQueryReader(&metadata))){if(FAILED(metadata->GetMetadataByName(L"/app1/ifd/{ushort=274}",&orientation)))metadata->GetMetadataByName(L"/ifd/{ushort=274}",&orientation);metadata->Release();}
  const unsigned v=orientation.vt==VT_UI2?orientation.uiVal:1;PropVariantClear(&orientation);
  const WICBitmapTransformOptions transforms[]={WICBitmapTransformRotate0,WICBitmapTransformRotate0,WICBitmapTransformFlipHorizontal,WICBitmapTransformRotate180,WICBitmapTransformFlipVertical,WICBitmapTransformOptions(WICBitmapTransformRotate90|WICBitmapTransformFlipHorizontal),WICBitmapTransformRotate90,WICBitmapTransformOptions(WICBitmapTransformRotate270|WICBitmapTransformFlipHorizontal),WICBitmapTransformRotate270};
  if(v>=2&&v<=8&&SUCCEEDED(factory->CreateBitmapFlipRotator(&rotate))&&SUCCEEDED(rotate->Initialize(frame,transforms[v])))source=rotate;
  ok=SUCCEEDED(source->GetSize(&width,&height))&&width&&height;
 }
 if(ok){image->originalWidth=width;image->originalHeight=height;
  if(region){const int left=std::clamp(int(std::floor(region->left*width)),0,int(width)-1),top=std::clamp(int(std::floor(region->top*height)),0,int(height)-1);WICRect rect{left,top,std::clamp(int(std::ceil((region->left+region->width)*width))-left,1,int(width)-left),std::clamp(int(std::ceil((region->top+region->height)*height))-top,1,int(height)-top)};ok=SUCCEEDED(factory->CreateBitmapClipper(&clipper))&&SUCCEEDED(clipper->Initialize(source,&rect));if(ok){source=clipper;width=rect.Width;height=rect.Height;}}
  const double scale=std::min(1.0,double(limit)/std::max(width,height));image->width=std::max(1u,unsigned(width*scale));image->height=std::max(1u,unsigned(height*scale));if(region){image->width=outWidth;image->height=outHeight;}ok=ok&&SUCCEEDED(factory->CreateBitmapScaler(&scaler))&&SUCCEEDED(scaler->Initialize(source,image->width,image->height,WICBitmapInterpolationModeFant));}
 if(ok)ok=SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(scaler,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
 if(ok){image->pixels.resize(size_t(image->width)*image->height*4);ok=SUCCEEDED(converter->CopyPixels(nullptr,image->width*4,UINT(image->pixels.size()),image->pixels.data()));}
 SettingsMenuRenderer::drop(converter);SettingsMenuRenderer::drop(scaler);SettingsMenuRenderer::drop(clipper);SettingsMenuRenderer::drop(rotate);SettingsMenuRenderer::drop(frame);SettingsMenuRenderer::drop(decoder);SettingsMenuRenderer::drop(factory);if(SUCCEEDED(com))CoUninitialize();return ok?image:nullptr;
}
inline bool import_png(const std::filesystem::path& path,const HomePixels& pixels,unsigned width=360,unsigned height=740){
 if(pixels.size()!=size_t(width)*height*4)return false;const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;IPropertyBag2* properties=nullptr;
 bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE))&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream,WICBitmapEncoderNoCache))&&SUCCEEDED(encoder->CreateNewFrame(&frame,&properties))&&SUCCEEDED(frame->Initialize(properties))&&SUCCEEDED(frame->SetSize(width,height));
 WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;if(ok)ok=SUCCEEDED(frame->SetPixelFormat(&format))&&IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA);
 if(ok)ok=SUCCEEDED(frame->WritePixels(height,width*4,UINT(pixels.size()),const_cast<BYTE*>(pixels.data())))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit());
 SettingsMenuRenderer::drop(properties);SettingsMenuRenderer::drop(frame);SettingsMenuRenderer::drop(encoder);SettingsMenuRenderer::drop(stream);SettingsMenuRenderer::drop(factory);if(SUCCEEDED(com))CoUninitialize();return ok;
}
inline std::wstring import_stem(const std::filesystem::path& file){auto stem=file.stem().wstring();for(const auto* suffix:homeSuffixes){std::wstring tail(suffix,suffix+strlen(suffix));if(stem.size()>tail.size()){auto ending=stem.substr(stem.size()-tail.size());std::transform(ending.begin(),ending.end(),ending.begin(),towlower);if(ending==tail){stem.resize(stem.size()-tail.size());break;}}}for(auto& c:stem)if(c<32||std::wstring(L"<>:\"/\\|?*").find(c)!=std::wstring::npos)c=L'_';while(!stem.empty()&&(stem.back()==L'.'||stem.back()==L' '))stem.pop_back();if(stem.empty())stem=L"image";return stem.substr(0,140);}
// Called by the existing CPU worker, never from a render hook or WndProc.
inline void home_import_prepare(){
 ImportJob job;uint64_t generation;size_t layer;int deviceSlot;unsigned outputWidth,outputHeight;std::filesystem::path folder,file;std::shared_ptr<const ImportImage> image;std::shared_ptr<const std::vector<std::wstring>> searchNames;std::wstring query;double cx,cy,zoom;
 {std::lock_guard<std::mutex> lock(homeImport.mutex);job=homeImport.job;if(job==ImportJob::None)return;homeImport.job=ImportJob::None;generation=homeImport.generation;layer=homeImport.layer;deviceSlot=homeImport.deviceSlot;outputWidth=import_width(layer);outputHeight=import_height(layer);folder=homeImport.folder;file=homeImport.file;image=homeImport.image;cx=homeImport.centerX;cy=homeImport.centerY;zoom=homeImport.zoom;searchNames=homeImport.searchNames;query=homeImport.search;}
 std::string error;std::vector<ImportEntry> entries;std::vector<size_t> filtered;std::shared_ptr<std::vector<std::wstring>> names;std::shared_ptr<const ImportImage> decoded;std::filesystem::path imported;
 try{
  if(job==ImportJob::Filter){if(searchNames)for(size_t i=0;i<searchNames->size();++i)if(import_matches((*searchNames)[i],query))filtered.push_back(i);}
  else if(job==ImportJob::List){std::error_code ec;std::filesystem::directory_iterator iterator(folder,std::filesystem::directory_options::skip_permission_denied,ec);if(ec)throw std::runtime_error("Cannot open this folder.");
   for(const auto& entry:iterator){if(entries.size()>=10000)break;const auto attributes=GetFileAttributesW(entry.path().c_str());if(attributes==INVALID_FILE_ATTRIBUTES)continue;const bool dir=entry.is_directory(ec);if(dir||(entry.is_regular_file(ec)&&import_extension(entry.path())))entries.push_back({entry.path(),dir});}
   std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){if(a.directory!=b.directory)return a.directory;return home_lower(a.path.filename().u8string())<home_lower(b.path.filename().u8string());});
   names=std::make_shared<std::vector<std::wstring>>();names->reserve(entries.size());for(size_t i=0;i<entries.size();++i){names->push_back(entries[i].path.filename().wstring());filtered.push_back(i);}
  }else if(job==ImportJob::Decode){decoded=import_decode(file);if(!decoded)throw std::runtime_error("Cannot decode this image. Try PNG, JPEG, BMP, TIFF or a supported DDS.");
  }else if(job==ImportJob::Save){if(!image)throw std::runtime_error("Choose an image first.");std::filesystem::path root;{std::lock_guard<std::mutex> lock(homeMutex);root=homeRoot;}
   if(layer>=4)root=deviceSlot>=0?importDeviceRoot:layer==5?carInfoRoot:carRoot;
   if(root.empty())throw std::runtime_error("Lockscreen folder is unavailable.");auto target=layer>=4?root:root/homeFolders[layer];std::filesystem::create_directories(target);if(GetFileAttributesW(target.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Import destination must be a regular folder.");
   const auto stem=import_stem(file);const std::wstring suffix=layer>=4?L"":std::wstring(homeSuffixes[layer],homeSuffixes[layer]+strlen(homeSuffixes[layer]));unsigned ow=outputWidth,oh=outputHeight;HomePixels pixels;
   if(home_lower(file.extension().u8string())==".dds"){auto original=import_decode(file,16384);if(original)pixels=import_resample(*original,cx,cy,zoom,ow,oh);}
   else{auto crop=import_crop(*image,cx,cy,zoom,ow,oh);ImportCrop normalized{crop.left/image->width,crop.top/image->height,crop.width/image->width,crop.height/image->height};auto original=import_decode(file,2048,&normalized,ow,oh);if(original)pixels=std::move(original->pixels);}
   if(pixels.empty())throw std::runtime_error("Cannot reread the original image. It may have moved.");
   if(layer==5&&ow==1024&&oh==620){HomePixels normalized(size_t(1440)*540*4);for(unsigned y=0;y<540;++y)for(unsigned x=0;x<1440;++x)memcpy(normalized.data()+(size_t(y)*1440+x)*4,pixels.data()+(size_t(y)*620/540*1024+x*1024/1440)*4,4);pixels=std::move(normalized);ow=1440;oh=540;}
   auto temporary=target/(L".bettergps-import-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp");
   const auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,nullptr);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create a temporary import file.");CloseHandle(handle);
   struct TemporaryFile {std::filesystem::path path;~TemporaryFile(){DeleteFileW(path.c_str());}} cleanup{temporary};
   if(!((layer>=4&&layer!=6)?car_write_still(temporary,pixels,ow,oh):import_png(temporary,pixels,ow,oh))){DeleteFileW(temporary.c_str());throw std::runtime_error("Cannot save the imported image.");}
   {std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.generation!=generation){DeleteFileW(temporary.c_str());return;}}
   std::set<std::string> existing;for(const auto& entry:std::filesystem::directory_iterator(target))if(import_extension(entry.path())||entry.path().extension()==".bga")existing.insert(home_lower(entry.path().stem().u8string()));
   for(unsigned attempt=0;attempt<10000;++attempt){auto name=stem+(attempt?L"_"+std::to_wstring(attempt+1):L"")+suffix+(layer>=4&&layer!=6?L".bga":L".png");auto destination=target/name;if(existing.count(home_lower(destination.stem().u8string())))continue;if(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){imported=destination;break;}const auto code=GetLastError();if(code!=ERROR_ALREADY_EXISTS&&code!=ERROR_FILE_EXISTS)break;}
   DeleteFileW(temporary.c_str());if(imported.empty())throw std::runtime_error("Cannot import this file; check folder permissions.");
   if(deviceSlot>=0&&importDeviceSaved){importDeviceSaved(deviceSlot,imported);}else if(layer>=4){car_scan();std::lock_guard<std::mutex> lock(carMutex);(layer==5?carInfoSelected:carSplashSelected)=imported.filename().u8string();(layer==5?carInfoEnabled:carSplashEnabled)=true;carDirty=true;++carRevision;}else {home_scan();{std::lock_guard<std::mutex> lock(homeMutex);auto name=imported.stem().u8string();name.resize(name.size()-strlen(homeSuffixes[layer]));homeSelected[layer]=name;if(layer==2)homeSelected[3]="__none__";if(layer>=2)homeShuffleMode=0;homeShufflePending=false;++homeRevision;}}
  }
 }catch(const std::exception& e){error=e.what();}
 {std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.generation!=generation)return;homeImport.error=error;
  if(job==ImportJob::Filter){homeImport.filtered=std::move(filtered);homeImport.first=0;}
  if(job==ImportJob::List){homeImport.searchNames=names;homeImport.filtered=std::move(filtered);homeImport.entries=std::move(entries);homeImport.first=0;homeImport.page=ImportPage::Browse;homeImport.address=folder.wstring();}
  if(job==ImportJob::Decode){homeImport.image=decoded;homeImport.page=decoded?ImportPage::Crop:ImportPage::Browse;homeImport.centerX=homeImport.centerY=.5;homeImport.zoom=1;}
  if(job==ImportJob::Save){homeImport.page=imported.empty()?ImportPage::Crop:ImportPage::Closed;if(!imported.empty()){homeImport.image.reset();homeImport.entries.clear();homeImport.filtered.clear();homeImport.searchNames.reset();homeImportActive=false;}}
  ++homeImportRevision;
 }
}
inline void import_request_list_locked(const std::filesystem::path& path){++homeImport.generation;homeImport.folder=path;homeImport.address=path.wstring();homeImport.job=ImportJob::List;homeImport.page=ImportPage::Loading;homeImport.error.clear();homeImport.edit=homeImport.searchEdit=false;homeImport.search.clear();++homeImportRevision;}
inline void import_address(){std::lock_guard<std::mutex> lock(homeImport.mutex);auto address=homeImport.address;while(!address.empty()&&iswspace(address.front()))address.erase(address.begin());while(!address.empty()&&iswspace(address.back()))address.pop_back();if(address.size()>=2&&address.front()==L'"'&&address.back()==L'"')address=address.substr(1,address.size()-2);auto path=std::filesystem::path(address);if(path.is_relative())path=homeImport.folder/path;if(import_extension(path)){++homeImport.generation;homeImport.file=path;homeImport.job=ImportJob::Decode;homeImport.page=ImportPage::Loading;homeImport.edit=false;++homeImportRevision;}else import_request_list_locked(path);}
inline void import_pointer(int x,int y,bool left){if(!homeImportActive)return;std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.page==ImportPage::Crop&&homeImport.drag&&left&&homeImport.image){const auto layer=homeImport.layer;auto crop=import_crop(*homeImport.image,homeImport.centerX,homeImport.centerY,homeImport.zoom,import_width(layer),import_height(layer));homeImport.centerX-=(x-homeImport.mouseX)*crop.width/(240*homeImport.image->width);homeImport.centerY-=(y-homeImport.mouseY)*crop.height/(double(import_preview_height(layer))*homeImport.image->height);import_crop(*homeImport.image,homeImport.centerX,homeImport.centerY,homeImport.zoom,import_width(layer),import_height(layer));++homeImportRevision;}if(!left)homeImport.drag=false;homeImport.mouseX=x;homeImport.mouseY=y;}
inline void import_wheel(int delta){std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.page==ImportPage::Browse){auto count=homeImport.filtered.size();const int last=count?int((count-1)/importPageSize)*int(importPageSize):0;homeImport.first=size_t(std::clamp(int(homeImport.first)-(delta>0?int(importPageSize):-int(importPageSize)),0,last));}if(homeImport.page==ImportPage::Crop)homeImport.zoom=std::clamp(homeImport.zoom*(delta>0?1.1:1/1.1),1.0,16.0);++homeImportRevision;}
inline void import_hit(int x,int y){
 if(x>=850&&y>=20&&y<65){import_close();return;}
 bool go=false;{std::lock_guard<std::mutex> lock(homeImport.mutex);auto& s=homeImport;
  if(s.page==ImportPage::Browse){if(y>=130&&y<168){if(x>=60&&x<790){s.edit=true;s.searchEdit=false;++homeImportRevision;return;}if(x>=795&&x<900)go=true;}
   else if(y>=180&&y<215){if(x>=60&&x<150)import_request_list_locked(s.folder.parent_path().empty()?s.folder:s.folder.parent_path());if(x>=160&&x<300){import_request_list_locked(import_user_folder(false));}if(x>=310&&x<450){import_request_list_locked(import_user_folder(true));}if(x>=460&&x<900){const auto mask=GetLogicalDrives();unsigned number=0;for(unsigned d=0;d<26;++d)if(mask&(1u<<d)){wchar_t path[]={wchar_t(L'A'+d),L':',L'\\',0};auto type=GetDriveTypeW(path);if(type!=DRIVE_FIXED&&type!=DRIVE_REMOVABLE)continue;if(x>=460+int(number)*45&&x<500+int(number)*45){import_request_list_locked(path);break;}++number;}}}
   else if(y>=76&&y<118&&x>=60&&x<900){s.edit=false;s.searchEdit=true;if(x>=850){s.search.clear();import_search_locked();}else ++homeImportRevision;}
   else if(x>=60&&x<900&&y>=235&&y<550){const auto visible=s.first+size_t((y-235)/35);if(visible<s.filtered.size()){auto entry=s.entries[s.filtered[visible]];if(entry.directory)import_request_list_locked(entry.path);else{++s.generation;s.file=entry.path;s.job=ImportJob::Decode;s.page=ImportPage::Loading;s.edit=false;++homeImportRevision;}}}
   else if(y>=580&&y<620){if(x>=60&&x<200){s.first=s.first>=importPageSize?s.first-importPageSize:0;++homeImportRevision;}if(x>=210&&x<350){if(s.first+importPageSize<s.filtered.size())s.first+=importPageSize;++homeImportRevision;}}
  }else if(s.page==ImportPage::Crop){if(x>=580&&x<=820&&y>=125&&y<(125+import_preview_height(s.layer))){s.drag=true;s.mouseX=x;s.mouseY=y;}
   if(y>=300&&y<345){if(x>=60&&x<140)s.zoom=std::max(1.0,s.zoom/1.1);if(x>=150&&x<230)s.zoom=std::min(16.0,s.zoom*1.1);if(x>=240&&x<390){s.zoom=1;s.centerX=s.centerY=.5;}++homeImportRevision;}
   if(y>=550&&y<600){if(x>=60&&x<270)import_request_list_locked(s.folder);if(x>=285&&x<510){s.page=ImportPage::Saving;s.job=ImportJob::Save;++homeImportRevision;}}
  }
 }if(go)import_address();
}
inline bool import_key(UINT message,WPARAM key){if(!homeImportActive)return false;
 if(message==WM_MOUSEWHEEL){import_wheel(GET_WHEEL_DELTA_WPARAM(key));return true;}
 if(message!=WM_CHAR&&message!=WM_KEYDOWN)return false;
 if(message==WM_KEYDOWN&&key=='F'&&(GetAsyncKeyState(VK_CONTROL)&0x8000)){std::lock_guard<std::mutex> lock(homeImport.mutex);if(homeImport.page==ImportPage::Browse){homeImport.edit=false;homeImport.searchEdit=true;++homeImportRevision;}return true;}
 if(message==WM_KEYDOWN&&key==VK_RETURN){bool edit=false;{std::lock_guard<std::mutex> lock(homeImport.mutex);edit=homeImport.edit&&homeImport.page==ImportPage::Browse;}if(edit)import_address();return true;}
 std::lock_guard<std::mutex> lock(homeImport.mutex);if((!homeImport.edit&&!homeImport.searchEdit)||homeImport.page!=ImportPage::Browse)return false;auto& field=homeImport.searchEdit?homeImport.search:homeImport.address;auto changed=[&]{if(homeImport.searchEdit)import_search_locked();else ++homeImportRevision;};
 if(message==WM_KEYDOWN&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&key=='A'){field.clear();changed();return true;}
 if(message==WM_KEYDOWN&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&key=='V'){if(OpenClipboard(nullptr)){auto h=GetClipboardData(CF_UNICODETEXT);if(h){auto* text=static_cast<const wchar_t*>(GlobalLock(h));if(text){field=std::wstring(text).substr(0,homeImport.searchEdit?256:32760);GlobalUnlock(h);}}CloseClipboard();}changed();return true;}
 if(message==WM_CHAR){if(key==8){if(!field.empty())field.pop_back();}else if(key>=32&&key!=127&&field.size()<(homeImport.searchEdit?256u:32760u))field.push_back(wchar_t(key));changed();return true;}return false;
}
template<class Fill,class Text,class Button>inline bool import_render(HDC dc,Fill fill,Text text,Button button){
 ImportPage page;size_t layer,first,total,allCount;std::wstring address,query;bool searchEdit;std::string filename,error;bool edit;double cx,cy,zoom;std::vector<ImportEntry> rows;std::shared_ptr<const ImportImage> image;
 {std::lock_guard<std::mutex> lock(homeImport.mutex);auto& s=homeImport;page=s.page;layer=s.layer;first=s.first;total=s.filtered.size();allCount=s.entries.size();query=s.search;searchEdit=s.searchEdit;address=s.address;filename=s.file.filename().u8string();error=s.error;edit=s.edit;cx=s.centerX;cy=s.centerY;zoom=s.zoom;image=s.image;for(size_t i=first;i<std::min(first+importPageSize,total);++i)rows.push_back(s.entries[s.filtered[i]]);}
 fill({20,12,940,704},RGB(28,28,29));text(std::string("Import image - ")+import_label(layer),{50,25,830,65},27,RGB(220,135,51),false);button("X",{850,25,905,65},false);
 if(page==ImportPage::Browse){
  fill({60,130,790,168},edit?RGB(77,67,54):RGB(55,57,60));text(std::filesystem::path(address).u8string()+(edit?" |":""),{70,130,780,168},19,RGB(235,235,235),false);button("Go",{795,130,900,168},false);
  button("Up",{60,180,150,215},false);button("Downloads",{160,180,300,215},false);button("Desktop",{310,180,450,215},false);
  auto mask=GetLogicalDrives();unsigned number=0;for(unsigned d=0;d<26&&number<9;++d)if(mask&(1u<<d)){wchar_t path[]={wchar_t(L'A'+d),L':',L'\\',0};auto type=GetDriveTypeW(path);if(type==DRIVE_FIXED||type==DRIVE_REMOVABLE){char label[]={char('A'+d),':',0};const int x=460+int(number++)*45;button(label,{x,180,x+40,215},false);}}
  fill({60,76,840,118},searchEdit?RGB(77,67,54):RGB(55,57,60));text(query.empty()&&!searchEdit?"Search files and folders... (Ctrl+F)":std::filesystem::path(query).u8string()+(searchEdit?" |":""),{72,76,830,118},20,query.empty()&&!searchEdit?RGB(170,170,170):RGB(235,235,235),false);button("X",{850,76,900,118},false);
  for(size_t i=0;i<rows.size();++i){int y=235+int(i)*35;fill({60,y,900,y+33},i%2?RGB(42,44,47):RGB(48,50,53));text((rows[i].directory?"[Folder]  ":"[Image]  ")+rows[i].path.filename().u8string(),{75,y,885,y+33},20,rows[i].directory?RGB(225,177,110):RGB(235,235,235),false);}
  if(rows.empty())text(query.empty()?"No supported images in this folder.":"No files or folders match this search.",{75,270,880,320},23,RGB(185,185,185),true);
  button("Previous",{60,580,200,620},false);button("Next",{210,580,350,620},false);text(std::to_string(total?first+1:0)+" - "+std::to_string(std::min(first+importPageSize,total))+" / "+std::to_string(total)+(query.empty()?" items":" matches / "+std::to_string(allCount)+" items"),{380,580,900,620},20,RGB(195,195,195),false);
 }else if(page==ImportPage::Crop&&image){text(filename,{60,90,510,130},24,RGB(235,235,235),false);text(std::to_string(image->originalWidth)+" x "+std::to_string(image->originalHeight)+" pixels",{60,140,510,175},21,RGB(180,180,180),false);
  text("Drag the preview to move the image.",{60,200,535,235},22,RGB(235,235,235),false);text("Mouse wheel or +/- to zoom.",{60,240,535,275},22,RGB(235,235,235),false);
  button("-",{60,300,140,345},false);button("+",{150,300,230,345},false);button("Reset crop",{240,300,390,345},false);char label[40];std::snprintf(label,sizeof(label),"%.2fx",zoom);text(label,{400,300,535,345},22,RGB(235,235,235),true);
  text(layer==5?(carIsBronco?"Output: 1024 x 620 splash":"Output: 1440 x 540 splash"):layer==4?(importDeviceWidth==360?"Output: 360 x 740 splash":"Output: 1280 x 800 splash"):"Output: 360 x 740 PNG",{60,370,530,410},24,RGB(220,135,51),false);text(layer>=4?"Transparency uses the dark screen color.":"Transparency is preserved.",{60,415,530,450},21,RGB(195,195,195),false);
  if(layer==2)text("Toplayer will be set to None.",{60,465,540,505},21,RGB(195,195,195),false);
  if(layer>=2&&layer<4)text("Manual selection turns shuffle off.",{60,505,540,535},18,RGB(180,180,180),false);
  button("Choose another",{60,550,270,600},false);button("Import image",{285,550,510,600},true);
  unsigned pw=import_width(layer),ph=import_height(layer);auto preview=import_resample(*image,cx,cy,zoom,pw,ph);for(unsigned y=0;y<ph;++y)for(unsigned x=0;x<pw;++x){auto* p=preview.data()+(size_t(y)*pw+x)*4;unsigned bg=layer>=4?5:((x/20+y/20)&1)?62:86;for(int c=0;c<3;++c)p[c]=uint8_t((unsigned(p[c])*p[3]+bg*(255-p[3])+127)/255);p[3]=255;}
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=pw;info.bmiHeader.biHeight=-LONG(ph);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);StretchDIBits(dc,580,125,240,import_preview_height(layer),0,0,pw,ph,preview.data(),&info,DIB_RGB_COLORS,SRCCOPY);
 }else{text(page==ImportPage::Saving?"Importing image...":"Loading...",{60,240,900,340},30,RGB(235,235,235),true);text("The original file will remain unchanged.",{60,355,900,395},21,RGB(185,185,185),true);}
 text(error,{60,625,915,651},18,RGB(240,130,100),false);text("Accepted: PNG, DDS (BC1/2/3/7, RGBA), JPG/JPEG, BMP, TIF/TIFF",{60,655,915,677},17,RGB(195,195,195),false);text("Esc: back  |  Images are cropped to the display aspect ratio, never stretched.",{60,680,915,700},15,RGB(170,170,170),false);return page==ImportPage::Crop&&bool(image);
}
}
