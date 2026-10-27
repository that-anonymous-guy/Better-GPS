#pragma once
// SPDX-License-Identifier: Apache-2.0
// Original concepts and resources: Copyright (C) 2012 The Android Open Source Project.
// C++ adaptation of AOSP DeskClock android-4.2.2_r1 (Apache-2.0).
// TimerObj/Stopwatches/Alarm recurrence semantics; Android services are replaced
// by the existing BetterGPS preparation worker. See docs/android-deskclock.md.
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "DeskClockCities.h"
namespace bettergps_deskclock {
inline std::vector<int> default_cities(){std::vector<int> result;for(const char* name:{"Los Angeles","Albuquerque","Dallas","New York"})for(size_t i=0;i<cities.size();++i)if(std::string(cities[i].name)==name){result.push_back(int(i));break;}return result;}
inline int64_t utc_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
enum class Screen {Main,Alarms,AlarmEdit,Cities,Settings,Label,Night};
enum class Action {Tab,Back,Alarms,Cities,Settings,Night,CitySearch,Analog,Format,Timebase,Sound,Volume,SnoozeLength,SilenceLength,City,Scroll,NewTimer,Digit,Erase,CreateTimer,TimerToggle,TimerDelete,TimerMinute,TimerReset,TimerLabel,Stopwatch, LapReset,Share,NewAlarm,EditAlarm,SaveAlarm,AlarmToggle,DeleteAlarm,AlarmPeriod,AlarmDay,AlarmLabel,AlarmSound,LabelChar,LabelSave,Dismiss,Snooze};
enum class TimerState {Stopped,Running,Paused,Expired};
struct Timer {int id=0;int64_t duration=0,left=0;TimerState state=TimerState::Stopped;std::string label;int64_t setup=0;};
struct Alarm {int id=0,minute=360,days=0;bool enabled=true,sound=true;std::string label;int64_t firedDay=-1,snoozed=-1;};
struct Alert {bool timer=false;int id=0;std::string label;int64_t began=0;};
struct View {
 int volume=50;int tab=1,scroll=0,nextId=1,snooze=10,silence=10;Screen screen=Screen::Main,returnScreen=Screen::Main;
 uint64_t now=0,tabAt=0;int previousTab=1;
 bool h24=false,analog=false,gameCounters=false,sound=true,stopwatch=false,settingTimer=false,pm=false,freshDigits=true;
 int64_t elapsed=0,game=-1,utcGame=-1;uint64_t revision=1;std::vector<int64_t> laps;
 std::vector<Timer> timers;std::vector<Alarm> alarms;std::vector<Alert> alerts;std::vector<int> cities;
 Alarm edit;int labelTarget=0;bool labelTimer=false;std::string digits,label,toast,citySearch;uint64_t toastUntil=0;
};
class Clock {
 mutable std::mutex mutex;View s;std::filesystem::path path;bool dirty=false;uint64_t lastReal=0,lastSave=0;int64_t lastGame=-1,lastCounter=-1,lastUtc=0;
 void changed(){++s.revision;dirty=true;}
 Timer* timer(int id){for(auto& t:s.timers)if(t.id==id)return &t;return nullptr;}
 Alarm* alarm(int id){for(auto& a:s.alarms)if(a.id==id)return &a;return nullptr;}
 void notify(bool timer,int id,std::string label){
  if(std::none_of(s.alerts.begin(),s.alerts.end(),[&](const Alert& a){return a.timer==timer&&a.id==id;})){
   s.alerts.push_back({timer,id,label.empty()?(timer?"Timer":"Alarm"):label,lastUtc});changed();
  }
 }
 void dismiss(bool snooze){
  if(s.alerts.empty())return;const auto a=s.alerts.front();s.alerts.erase(s.alerts.begin());
  if(a.timer){if(auto t=timer(a.id)){t->state=TimerState::Stopped;t->duration=t->setup?t->setup:t->duration;t->left=t->duration;}}
  else if(auto p=alarm(a.id)){p->snoozed=snooze&&s.game>=0?s.game+s.snooze:-1;}
  changed();
 }
 static bool valid_label(const std::string& x){return x.size()<=48&&std::none_of(x.begin(),x.end(),[](unsigned char c){return c<32;});}
 static bool due(const Alarm& a,int64_t from,int64_t now,int64_t& day){
  if(now<0||now<=from)return false;const int64_t first=std::max<int64_t>(0,from/1440-1),last=now/1440;
  // Weekly recurrence needs at most eight days even after a long sleep/jump.
  for(int64_t d=std::max(first,last-7);d<=last;++d){const auto at=d*1440+a.minute;
   if(at>from&&at<=now&&d!=a.firedDay&&(!a.days||(a.days&(1<<int(d%7))))){day=d;return true;}}
  return false;
 }
public:
 View view()const{std::lock_guard<std::mutex> lock(mutex);return s;}
 void reset(){std::lock_guard<std::mutex> lock(mutex);s={};path.clear();dirty=false;lastReal=lastSave=0;lastGame=lastCounter=-1;lastUtc=0;}
 void init(const std::filesystem::path& base,int64_t wall=utc_ms()){
  std::lock_guard<std::mutex> lock(mutex);s={};lastReal=lastSave=0;lastGame=lastCounter=-1;lastUtc=wall;path=base/"BetterGPSClock.ini";dirty=false;s.cities=default_cities();bool citiesSpecified=false;
  std::ifstream in(path);std::string line;int64_t saved=wall;bool version=false;size_t count=0;
  while(std::getline(in,line)&&++count<4096){if(line.size()>1024)continue;std::istringstream f(line);std::string key;f>>key;
   if(key=="DeskClock"){int n=0;f>>n;version=n==1;}
   else if(!version)continue;
   else if(key=="settings"){int h=0,a=0,g=0,sound=1,tab=1;f>>h>>a>>g>>sound>>s.snooze>>s.silence>>tab;s.h24=h!=0;s.analog=a!=0;s.gameCounters=g!=0;s.sound=sound!=0;s.tab=std::clamp(tab,0,2);f>>s.volume;s.volume=std::clamp(s.volume,0,100);s.snooze=std::clamp(s.snooze,1,30);s.silence=std::clamp(s.silence,1,60);}
   else if(key=="saved")f>>saved;
   else if(key=="stopwatch"){int run=0;f>>run>>s.elapsed;s.stopwatch=run!=0;s.elapsed=std::clamp<int64_t>(s.elapsed,0,31536000000LL);}
   else if(key=="lap"){int64_t n=0;if(f>>n&&n>=0&&s.laps.size()<500)s.laps.push_back(n);}
   else if(key=="cities_initialized"){int n=0;if(f>>n&&n==1){if(!citiesSpecified)s.cities.clear();citiesSpecified=true;}}
   else if(key=="city"){if(!citiesSpecified){s.cities.clear();citiesSpecified=true;}int n=-1;if(f>>n&&n>=0&&n<1000&&s.cities.size()<280&&std::find(s.cities.begin(),s.cities.end(),n)==s.cities.end())s.cities.push_back(n);}
   else if(key=="timer"){Timer t;int state=0;if(f>>t.id>>t.duration>>t.left>>state>>std::quoted(t.label)&&t.id>0&&t.id<1000000&&t.duration>0&&t.duration<=360000000&&state>=0&&state<=3&&valid_label(t.label)&&s.timers.size()<32&&!timer(t.id)){f>>t.setup;if(t.setup<=0||t.setup>360000000)t.setup=t.duration;t.state=TimerState(state);t.left=std::clamp<int64_t>(t.left,-31536000000LL,360000000LL);s.timers.push_back(t);s.nextId=std::max(s.nextId,t.id+1);}}
   else if(key=="alarm"){Alarm a;int en=0,sound=1;if(f>>a.id>>a.minute>>a.days>>en>>sound>>a.firedDay>>a.snoozed>>std::quoted(a.label)&&a.id>0&&a.id<1000000&&a.minute>=0&&a.minute<1440&&a.days>=0&&a.days<128&&valid_label(a.label)&&s.alarms.size()<64&&!alarm(a.id)){a.enabled=en!=0;a.sound=sound!=0;s.alarms.push_back(a);s.nextId=std::max(s.nextId,a.id+1);}}
  }
  const auto offline=std::clamp<int64_t>(wall-saved,0,31536000000LL);
  if(!s.gameCounters){if(s.stopwatch)s.elapsed+=offline;for(auto& t:s.timers)if(t.state==TimerState::Running)t.left-=offline;}
  for(auto& t:s.timers)if(t.state==TimerState::Expired||(t.state==TimerState::Running&&t.left<=0)){t.state=TimerState::Expired;notify(true,t.id,t.label);}
  ++s.revision;
 }
 bool save(uint64_t now=0,bool force=false){
  std::lock_guard<std::mutex> lock(mutex);if(path.empty()||(!dirty&&!force)||(!force&&lastSave&&now-lastSave<5000))return true;
  std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);auto temp=path;temp+=L".tmp";std::ofstream f(temp,std::ios::trunc);if(!f)return false;
  f<<"DeskClock 1\nsaved "<<lastUtc<<"\nsettings "<<s.h24<<' '<<s.analog<<' '<<s.gameCounters<<' '<<s.sound<<' '<<s.snooze<<' '<<s.silence<<' '<<s.tab<<' '<<s.volume<<"\nstopwatch "<<s.stopwatch<<' '<<s.elapsed<<'\n';
  for(auto n:s.laps)f<<"lap "<<n<<'\n';f<<"cities_initialized 1\n";for(auto n:s.cities)f<<"city "<<n<<'\n';
  for(const auto& t:s.timers)f<<"timer "<<t.id<<' '<<t.duration<<' '<<t.left<<' '<<int(t.state)<<' '<<std::quoted(t.label)<<' '<<t.setup<<'\n';
  for(const auto& a:s.alarms)f<<"alarm "<<a.id<<' '<<a.minute<<' '<<a.days<<' '<<a.enabled<<' '<<a.sound<<' '<<a.firedDay<<' '<<a.snoozed<<' '<<std::quoted(a.label)<<'\n';f.close();if(!f)return false;
#ifdef _WIN32
  if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
#else
  std::filesystem::rename(temp,path,ec);if(ec)return false;
#endif
  dirty=false;lastSave=now;return true;
 }
 void step(uint64_t real,int64_t game,int64_t utcGame,int64_t wall=utc_ms(),int64_t economy=-2){
  std::lock_guard<std::mutex> lock(mutex);const auto delta=lastReal&&real>=lastReal?int64_t(real-lastReal):0;const auto counter=economy==-2?utcGame:economy;const auto gameDelta=lastCounter>=0&&counter>=lastCounter?(counter-lastCounter)*60000:0;
  const auto elapsed=s.gameCounters?gameDelta:delta;lastUtc=wall;
  if(s.stopwatch&&elapsed){s.elapsed=std::min<int64_t>(31536000000LL,s.elapsed+elapsed);dirty=true;}
  for(auto& t:s.timers){if(t.state==TimerState::Running||t.state==TimerState::Expired){t.left=std::max<int64_t>(-31536000000LL,t.left-elapsed);if(elapsed)dirty=true;if(t.state==TimerState::Running&&t.left<=0){t.state=TimerState::Expired;notify(true,t.id,t.label);}}}
  for(auto& a:s.alarms){int64_t day=-1;
   if(a.snoozed>=0&&game>=a.snoozed){a.snoozed=-1;notify(false,a.id,a.label);}
   else if(a.enabled&&lastGame>=0&&due(a,lastGame,game,day)){a.firedDay=day;if(!a.days)a.enabled=false;notify(false,a.id,a.label);}
  }
  while(!s.alerts.empty()&&wall>=s.alerts.front().began&&wall-s.alerts.front().began>=int64_t(s.silence)*60000)dismiss(false);
  if(!s.toast.empty()&&real>s.toastUntil){s.toast.clear();++s.revision;}
  s.game=game;s.utcGame=utcGame;s.now=real;lastReal=real;lastGame=game;lastCounter=counter;
 }
 uint32_t background()const{std::lock_guard<std::mutex> lock(mutex);uint32_t n=s.stopwatch?4:0;if(std::any_of(s.timers.begin(),s.timers.end(),[](const Timer& t){return t.state==TimerState::Running||t.state==TimerState::Expired;}))n|=2;if(std::any_of(s.alarms.begin(),s.alarms.end(),[](const Alarm& a){return a.enabled||a.snoozed>=0;}))n|=8;return n;}
 bool audible()const{std::lock_guard<std::mutex> lock(mutex);if(!s.sound||!s.volume||s.alerts.empty())return false;if(s.alerts.front().timer)return true;for(const auto& a:s.alarms)if(a.id==s.alerts.front().id)return a.sound;return false;}
 std::string key(uint64_t now,bool visible)const{std::lock_guard<std::mutex> lock(mutex);bool moving=visible&&((s.tabAt&&now>=s.tabAt&&now-s.tabAt<220)||(s.screen==Screen::Main&&((s.tab==2&&s.stopwatch)||(s.tab==0&&std::any_of(s.timers.begin(),s.timers.end(),[](const Timer& t){return t.state==TimerState::Running||t.state==TimerState::Expired;})))));return std::to_string(s.revision)+":"+std::to_string(moving?now/(s.tabAt&&now-s.tabAt<220?33:100):0)+":"+std::to_string(!s.alerts.empty());}
 bool back(){std::lock_guard<std::mutex> lock(mutex);if(!s.alerts.empty()){dismiss(false);return true;}if(s.screen==Screen::Label){s.screen=s.returnScreen;changed();return true;}if(s.screen!=Screen::Main){s.screen=s.screen==Screen::AlarmEdit?Screen::Alarms:Screen::Main;s.scroll=0;changed();return true;}if(s.settingTimer){s.settingTimer=false;changed();return true;}return false;}
 void act(Action action,int arg=0){
  std::lock_guard<std::mutex> lock(mutex);
  auto label=[&](bool timer,int id,std::string value){s.returnScreen=s.screen;s.screen=Screen::Label;s.labelTimer=timer;s.labelTarget=id;s.label=value;};
  switch(action){
   case Action::Tab:s.previousTab=s.tab;s.tabAt=s.tab!=std::clamp(arg,0,2)?lastReal:0;s.tab=std::clamp(arg,0,2);s.screen=Screen::Main;s.scroll=0;s.settingTimer=false;break;
   case Action::Alarms:s.screen=Screen::Alarms;s.scroll=0;break;
   case Action::Cities:s.screen=Screen::Cities;s.scroll=0;break;
   case Action::Settings:s.screen=Screen::Settings;break;
   case Action::CitySearch:label(false,-1,s.citySearch);break;
   case Action::Night:s.screen=Screen::Night;break;
   case Action::Analog:s.analog=!s.analog;break;
   case Action::Format:s.h24=!s.h24;break;
   case Action::Timebase:s.gameCounters=!s.gameCounters;break;
   case Action::Sound:s.sound=!s.sound;break;
   case Action::Volume:s.volume=s.volume>=100?0:s.volume+10;break;
   case Action::SnoozeLength:s.snooze=s.snooze>=30?1:s.snooze+1;break;
   case Action::SilenceLength:s.silence=s.silence>=60?1:s.silence+1;break;
   case Action::Scroll:s.scroll=std::max(0,s.scroll+arg);break;
   case Action::City:{auto p=std::find(s.cities.begin(),s.cities.end(),arg);if(p==s.cities.end()&&s.cities.size()<280)s.cities.push_back(arg);else if(p!=s.cities.end())s.cities.erase(p);break;}
   case Action::NewTimer:s.tab=0;s.screen=Screen::Main;s.settingTimer=true;s.digits.clear();break;
   case Action::Digit:if(arg>=0&&arg<=9){if(s.screen==Screen::AlarmEdit&&s.freshDigits){s.digits.clear();s.freshDigits=false;}const size_t max=s.screen==Screen::AlarmEdit?4:6;if(s.digits.size()<max)s.digits+=char('0'+arg);}break;
   case Action::Erase:if(!s.digits.empty())s.digits.pop_back();s.freshDigits=false;break;
   case Action::CreateTimer:{int n=0;for(char c:s.digits)n=n*10+c-'0';const int64_t duration=((n/10000)*3600+((n/100)%100)*60+n%100)*1000LL;if(duration>0&&duration<=360000000&&s.timers.size()<32){s.timers.push_back({s.nextId++,duration,duration,TimerState::Running,{},duration});s.settingTimer=false;s.digits.clear();s.scroll=int(s.timers.size())-1;}break;}
   case Action::TimerToggle:if(auto t=timer(arg)){if(t->state==TimerState::Running)t->state=TimerState::Paused;else if(t->state!=TimerState::Expired)t->state=TimerState::Running;}break;
   case Action::TimerDelete:s.timers.erase(std::remove_if(s.timers.begin(),s.timers.end(),[&](const Timer& t){return t.id==arg;}),s.timers.end());s.alerts.erase(std::remove_if(s.alerts.begin(),s.alerts.end(),[&](const Alert& a){return a.timer&&a.id==arg;}),s.alerts.end());s.scroll=0;break;
   case Action::TimerMinute:if(auto t=timer(arg)){t->left=std::min<int64_t>(360000000,std::max<int64_t>(0,t->left)+60000);if(t->state==TimerState::Expired){t->duration=60000;t->state=TimerState::Running;s.alerts.erase(std::remove_if(s.alerts.begin(),s.alerts.end(),[&](const Alert& a){return a.timer&&a.id==arg;}),s.alerts.end());}else t->duration=std::min<int64_t>(360000000,t->duration+60000);}break;
   case Action::TimerReset:if(auto t=timer(arg)){t->duration=t->setup?t->setup:t->duration;t->left=t->duration;t->state=TimerState::Stopped;s.alerts.erase(std::remove_if(s.alerts.begin(),s.alerts.end(),[&](const Alert& a){return a.timer&&a.id==arg;}),s.alerts.end());}break;
   case Action::TimerLabel:if(auto t=timer(arg))label(true,arg,t->label);break;
   case Action::Stopwatch:s.stopwatch=!s.stopwatch;break;
   case Action::LapReset:if(s.stopwatch){if(s.laps.size()<500)s.laps.push_back(s.elapsed);}else{s.elapsed=0;s.laps.clear();}break;
   case Action::Share:{if(!path.empty()){std::error_code ec;auto out=path.parent_path()/"logs"/"clock";std::filesystem::create_directories(out,ec);std::ofstream f(out/"laps.csv");f<<"lap,lap_ms,total_ms\n";int64_t previous=0;for(size_t i=0;i<s.laps.size();++i){f<<i+1<<','<<s.laps[i]-previous<<','<<s.laps[i]<<'\n';previous=s.laps[i];}s.toast=f?"Laps saved: logs/clock/laps.csv":"Could not save laps";s.toastUntil=lastReal+3500;}break;}
   case Action::NewAlarm:if(s.alarms.size()<64){s.edit={};s.edit.id=s.nextId++;s.digits="0600";s.pm=false;s.freshDigits=true;s.screen=Screen::AlarmEdit;}break;
   case Action::EditAlarm:if(auto a=alarm(arg)){s.edit=*a;const int h=a->minute/60;std::ostringstream f;f<<std::setfill('0')<<std::setw(2)<<(s.h24?h:(h%12?h%12:12))<<std::setw(2)<<a->minute%60;s.digits=f.str();s.pm=h>=12;s.freshDigits=true;s.screen=Screen::AlarmEdit;}break;
   case Action::SaveAlarm:{int n=0;for(char c:s.digits)n=n*10+c-'0';int h=n/100,m=n%100;if(!s.digits.empty()&&m<60&&((s.h24&&h<24)||(!s.h24&&h>=1&&h<=12))){if(!s.h24)h=h%12+(s.pm?12:0);s.edit.minute=h*60+m;s.edit.enabled=true;s.edit.snoozed=-1;s.edit.firedDay=-1;if(auto a=alarm(s.edit.id))*a=s.edit;else if(s.alarms.size()<64)s.alarms.push_back(s.edit);s.screen=Screen::Alarms;s.scroll=0;}else{s.toast="Enter a valid time";s.toastUntil=lastReal+2500;}break;}
   case Action::AlarmToggle:if(auto a=alarm(arg)){a->enabled=!a->enabled;a->snoozed=-1;}break;
   case Action::DeleteAlarm:s.alarms.erase(std::remove_if(s.alarms.begin(),s.alarms.end(),[&](const Alarm& a){return a.id==arg;}),s.alarms.end());s.alerts.erase(std::remove_if(s.alerts.begin(),s.alerts.end(),[&](const Alert& a){return !a.timer&&a.id==arg;}),s.alerts.end());s.screen=Screen::Alarms;s.scroll=0;break;
   case Action::AlarmPeriod:s.pm=!s.pm;break;
   case Action::AlarmDay:if(arg>=0&&arg<7)s.edit.days^=1<<arg;break;
   case Action::AlarmLabel:label(false,s.edit.id,s.edit.label);break;
   case Action::AlarmSound:s.edit.sound=!s.edit.sound;break;
   case Action::LabelChar:if(arg==0)s.label.clear();else if(arg==8){if(!s.label.empty())s.label.pop_back();}else if(arg>=32&&arg<127&&s.label.size()<48)s.label+=char(arg);break;
   case Action::LabelSave:if(s.labelTarget==-1){s.citySearch=s.label;s.scroll=0;}else if(s.labelTimer){if(auto t=timer(s.labelTarget))t->label=s.label;}else s.edit.label=s.label;s.screen=s.returnScreen;break;
   case Action::Dismiss:dismiss(false);break;
   case Action::Snooze:dismiss(true);break;
   case Action::Back:if(s.screen==Screen::Label)s.screen=s.returnScreen;else{s.screen=s.screen==Screen::AlarmEdit?Screen::Alarms:Screen::Main;s.settingTimer=false;s.scroll=0;}break;
  }
  changed();
 }
};
inline Clock clock;
} // namespace bettergps_deskclock
