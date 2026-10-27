#pragma once
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
namespace bettergps_overlay {
struct SolarSettings {
    double latitude=39,latitudeFactor=-1.7706234e-4,longitudeFactor=1.76689948e-4;
    double parallel1=33,parallel2=45,day=172,summerTime=1,threshold=5,lambert=1;
};
inline SolarSettings solar_settings(const std::wstring& directory) {
    SolarSettings settings;std::ifstream file(std::filesystem::path(directory)/L"solar.bin",std::ios::binary);double data[9]{};
    if(file.read(reinterpret_cast<char*>(data),sizeof(data))){bool valid=true;for(auto value:data)valid=valid&&std::isfinite(value);
        if(valid&&data[0]>-89&&data[0]<89&&data[5]>=1&&data[5]<=366&&std::abs(data[7])<30)settings={data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8]};}
    return settings;
}
inline double latitude_at(double x,double z,const SolarSettings& settings) {
    constexpr double rad=3.14159265358979323846/180;
    double latitude=settings.latitude+z*settings.latitudeFactor;
    if(settings.lambert!=0){const auto p1=settings.parallel1*rad,p2=settings.parallel2*rad,p0=settings.latitude*rad;
        const double n=std::abs(p1-p2)<1e-8?std::sin(p1):std::log(std::cos(p1)/std::cos(p2))/std::log(std::tan(3.14159265358979323846/4+p2/2)/std::tan(3.14159265358979323846/4+p1/2));
        if(std::abs(n)>1e-8){const auto f=std::cos(p1)*std::pow(std::tan(3.14159265358979323846/4+p1/2),n)/n,rho0=f/std::pow(std::tan(3.14159265358979323846/4+p0/2),n);
            auto rho=std::hypot(x*settings.longitudeFactor*rad,rho0-z*settings.latitudeFactor*rad);if(n<0)rho=-rho;
            if(rho!=0&&f/rho>0)latitude=(2*std::atan(std::pow(f/rho,1/n))-3.14159265358979323846/2)/rad;
        }
    }
    return std::clamp(latitude,-85.0,85.0);
}
inline double solar_elevation(int64_t gameMinutes,double x,double z,const SolarSettings& settings) {
    constexpr double pi=3.14159265358979323846,rad=pi/180;
    const double latitude=latitude_at(x,z,settings)*rad;
    const double declination=std::asin(std::sin(23.44*rad)*std::sin(2*pi*(settings.day-80)/365.2422));
    auto minute=gameMinutes%1440;if(minute<0)minute+=1440;
    const double angle=(static_cast<double>(minute)/60-settings.summerTime-12)*15*rad;
    return std::asin(std::clamp(std::sin(latitude)*std::sin(declination)+std::cos(latitude)*std::cos(declination)*std::cos(angle),-1.0,1.0))/rad;
}
inline bool solar_night(bool previous,bool initialized,double elevation,double threshold) {
    if(!initialized)return elevation<threshold;
    return previous?elevation<threshold+.5:elevation<threshold-.5;
}
}
