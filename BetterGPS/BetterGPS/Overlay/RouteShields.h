#pragma once
#include <regex>
#include <map>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace bettergps_overlay {
struct RouteShield { std::string identity, number, beside; };
inline std::vector<RouteShield> route_shields(const std::string& text) {
    static const std::regex pattern(R"(^\s*([A-Z.][A-Z. ]*?)[ -]*([1-9][0-9]{0,3}[A-Z]?)(?:\s+(NORTHBOUND|SOUTHBOUND|EASTBOUND|WESTBOUND|NORTH|SOUTH|EAST|WEST|NB|SB|EB|WB|N|S|E|W))?(\s*\[[^\]]+\])?\s*$)",std::regex::icase);
    static const std::map<std::string,std::string> states{
        {"ALABAMA","AL"},{"ALASKA","AK"},{"ARIZONA","AZ"},{"ARKANSAS","AR"},{"CALIFORNIA","CA"},{"COLORADO","CO"},{"CONNECTICUT","CT"},{"DELAWARE","DE"},{"FLORIDA","FL"},{"GEORGIA","GA"},{"HAWAII","HI"},{"IDAHO","ID"},{"ILLINOIS","IL"},{"INDIANA","IN"},{"IOWA","IA"},{"KANSAS","KS"},{"KENTUCKY","KY"},{"LOUISIANA","LA"},{"MAINE","ME"},{"MARYLAND","MD"},{"MASSACHUSETTS","MA"},{"MICHIGAN","MI"},{"MINNESOTA","MN"},{"MISSISSIPPI","MS"},{"MISSOURI","MO"},{"MONTANA","MT"},{"NEBRASKA","NE"},{"NEVADA","NV"},{"NEW HAMPSHIRE","NH"},{"NEW JERSEY","NJ"},{"NEW MEXICO","NM"},{"NEW YORK","NY"},{"NORTH CAROLINA","NC"},{"NORTH DAKOTA","ND"},{"OHIO","OH"},{"OKLAHOMA","OK"},{"OREGON","OR"},{"PENNSYLVANIA","PA"},{"RHODE ISLAND","RI"},{"SOUTH CAROLINA","SC"},{"SOUTH DAKOTA","SD"},{"TENNESSEE","TN"},{"TEXAS","TX"},{"UTAH","UT"},{"VERMONT","VT"},{"VIRGINIA","VA"},{"WASHINGTON","WA"},{"WEST VIRGINIA","WV"},{"WISCONSIN","WI"},{"WYOMING","WY"}};
    auto normalized=text;for(const auto* dash:{"\xE2\x80\x91","\xE2\x80\x93","\xE2\x80\x94","\xE2\x80\x90"}){size_t p;while((p=normalized.find(dash))!=std::string::npos)normalized.replace(p,3,"-");}
    std::vector<RouteShield> result;size_t start=0;
    do {const auto end=normalized.find('/',start);std::smatch match;const auto part=normalized.substr(start,end==std::string::npos?end:end-start);
        if(!std::regex_match(part,match,pattern))return {};
        auto family=match[1].str(),number=match[2].str(),direction=match[3].str();
        for(auto* s:{&family,&number,&direction})for(auto& c:*s)c=static_cast<char>(toupper(static_cast<unsigned char>(c)));
        while(!family.empty()&&family.back()==' ')family.pop_back();
        static const std::regex designation(R"(\s+(?:STATE\s+)?(?:ROUTE|HIGHWAY)$)");family=std::regex_replace(family,designation,"");
        const auto state=states.find(family);if(state!=states.end())family=state->second;
        if(family=="INTERSTATE")family="I";if(family=="U.S."||family=="U.S"||family=="US ROUTE"||family=="US HIGHWAY")family="US";
        if(family.size()>3)return {};
        if(direction=="NB"||direction=="NORTHBOUND")direction="NORTH";if(direction=="SB"||direction=="SOUTHBOUND")direction="SOUTH";if(direction=="EB"||direction=="EASTBOUND")direction="EAST";if(direction=="WB"||direction=="WESTBOUND")direction="WEST";
        if(direction=="N")direction="NORTH";if(direction=="S")direction="SOUTH";if(direction=="E")direction="EAST";if(direction=="W")direction="WEST";
        result.push_back({family+"-"+number,number,direction+match[4].str()});
        if(end==std::string::npos)break;start=end+1;
    }while(result.size()<3);
    return result.size()>2?std::vector<RouteShield>{}:result;
}
struct RoadLabelPart {std::string text;RouteShield shield;bool numbered{};};
inline std::vector<RoadLabelPart> road_label_parts(const std::string& text){
    std::vector<RoadLabelPart> result;size_t start=0;
    do{const auto end=text.find('/',start);auto part=text.substr(start,end==std::string::npos?end:end-start);while(!part.empty()&&part.front()==' ')part.erase(0,1);while(!part.empty()&&part.back()==' ')part.pop_back();
        if(!part.empty()){const auto shield=route_shields(part);result.push_back({part,shield.empty()?RouteShield{}:shield[0],!shield.empty()});}
        if(end==std::string::npos)break;start=end+1;
    }while(start<text.size());return result;
}
struct ShieldBitmap {unsigned width{},height{};std::vector<uint8_t> bgra;};
inline uint64_t shield_revision(const std::wstring& directory) {
    WIN32_FILE_ATTRIBUTE_DATA data{};if(!GetFileAttributesExW((std::filesystem::path(directory)/L"manifest.json").c_str(),GetFileExInfoStandard,&data))return 0;
    return (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime)<<32)|data.ftLastWriteTime.dwLowDateTime;
}
inline const ShieldBitmap& load_route_shield(const std::string& identity,const std::wstring& directory) {
    static std::map<std::string,ShieldBitmap> cache;static uint64_t generation=0;static std::wstring previousDirectory;
    const auto revision=shield_revision(directory);if(generation!=revision||directory!=previousDirectory){cache.clear();generation=revision;previousDirectory=directory;}
    const auto found=cache.find(identity);if(found!=cache.end())return found->second;ShieldBitmap bitmap;
    // Only use committed catalog entries, never leftovers from a disabled mod.
    if(revision){std::ifstream manifest(std::filesystem::path(directory)/L"manifest.json");std::string catalog((std::istreambuf_iterator<char>(manifest)),{});
        if(catalog.find("\"identity\":\""+identity+"\"")!=std::string::npos){
            std::ifstream file(std::filesystem::path(directory)/(identity+".dds"),std::ios::binary);uint32_t header[37]{};file.read(reinterpret_cast<char*>(header),sizeof(header));
            // DDS DX10 uncompressed BGRA/RGBA, sRGB and opaque BGRX. Unsupported
            // mod formats retain the complete road text instead of a false badge.
            if(file&&header[0]==0x20534444&&header[1]==124&&header[21]==0x30315844&&header[33]==3&&header[35]==1&&header[3]>0&&header[3]<=1024&&header[4]>0&&header[4]<=1024&&
               (header[32]==87||header[32]==88||header[32]==91||header[32]==93||header[32]==28||header[32]==29)){
                bitmap.width=header[4];bitmap.height=header[3];bitmap.bgra.resize(static_cast<size_t>(bitmap.width)*bitmap.height*4);file.read(reinterpret_cast<char*>(bitmap.bgra.data()),bitmap.bgra.size());
                if(!file)bitmap={};else for(size_t i=0;i<bitmap.bgra.size();i+=4){if(header[32]==28||header[32]==29)std::swap(bitmap.bgra[i],bitmap.bgra[i+2]);if(header[32]==88||header[32]==93)bitmap.bgra[i+3]=255;}
            }
        }
    }
    return cache.emplace(identity,std::move(bitmap)).first->second;
}
inline void paint_route_shield(std::vector<uint8_t>& pixels,UINT canvasWidth,UINT canvasHeight,const ShieldBitmap& image,int left,int top,int size) {
    if(image.bgra.empty())return;
    const int width=static_cast<int>(size*image.width/std::max(image.width,image.height)),height=static_cast<int>(size*image.height/std::max(image.width,image.height));
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){int px=left+x,py=top+y;if(px<0||py<0||px>=static_cast<int>(canvasWidth)||py>=static_cast<int>(canvasHeight))continue;
        const size_t s=(static_cast<size_t>(y*image.height/height)*image.width+x*image.width/width)*4,d=(static_cast<size_t>(py)*canvasWidth+px)*4;
        const unsigned alpha=image.bgra[s+3];if(!alpha)continue;const unsigned old=pixels[d+3],combined=alpha+old*(255-alpha)/255;
        for(int c=0;c<3;++c)pixels[d+c]=static_cast<uint8_t>((image.bgra[s+c]*alpha+pixels[d+c]*old*(255-alpha)/255)/combined);pixels[d+3]=static_cast<uint8_t>(combined);
    }
}
}
