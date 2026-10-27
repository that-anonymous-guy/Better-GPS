#pragma once
namespace t680_pan_fixture {
using namespace bettergps_overlay;
inline void run(const std::filesystem::path& out){
 MapPanRuntime state;const uint64_t stamp=12,epoch=9,power=7;std::array<float,3> target{100,2,300};
 for(double heading:{0.,.7,1.57,3.14})for(double back:{0.,100.}){
  state.context(2,false,stamp,epoch,power);state.context(2,true,stamp,epoch,power);const double rx=std::cos(heading),rz=std::sin(heading),bx=-rz,bz=rx;
  float camera[]={float(100+back*bx),202,float(300+back*bz)};state.frame(2,stamp,100,target,camera,rx,rz,bx,bz,back);
  Check(state.drag(2,stamp,epoch,power,110,.4,.4,.5,.4,544./900),"Pan rejected fresh horizontal drag");
  Check(state.active(2,stamp,epoch,power)&&!state.active(3,stamp,epoch,power),"Pan state leaked between GPS devices");
  state.frame(2,stamp,120,target,camera,rx,rz,bx,bz,back);const double x=camera[0],z=camera[2];Check((camera[0]-100-back*bx)*rx+(camera[2]-300-back*bz)*rz<0,"Horizontal pan moved against drag");
  auto moved=target;moved[0]+=40;moved[2]-=25;camera[0]=float(moved[0]+back*bx);camera[2]=float(moved[2]+back*bz);state.frame(2,stamp,130,moved,camera,rx,rz,bx,bz,back);Check(std::abs(camera[0]-x)<.001&&std::abs(camera[2]-z)<.001,"Exploration kept following the moving truck");
  Check(!state.drag(2,stamp,epoch,power,700,.4,.4,.5,.4,544./900)&&!state.drag(2,stamp,epoch+1,power,140,.4,.4,.5,.4,544./900),"Pan accepted stale pose/vehicle");
  Check(state.recenter(2)&&!state.active(2,stamp,epoch,power),"Re-center did not release exploration");camera[0]=moved[0];camera[2]=moved[2];Check(!state.frame(2,stamp,140,moved,camera,rx,rz,bx,bz,back)&&camera[0]==moved[0],"Re-center retained camera displacement");
  Check(state.drag(2,stamp,epoch,power,150,.4,.4,.4,.5,544./900),"Vertical pan rejected");state.context(2,true,stamp+4,epoch,power);Check(!state.active(2,stamp+4,epoch,power),"Native view change retained manual pan");
 }
 const auto d=managed_snapshot(0);mapPan.reset_all();touchInput.cancel();settingsOpen=true;touchFixtureForeground=true;g_tripPresent=true;managed_page_destination(0,false);managed_page_sample(0,d,2000000,true,true);managed_page_sample(0,d,2005000,true,true);touch_actions_worker();
 const auto stampNow=mapViewSourceStamp.load(),epochNow=vehicleStart.epoch.load(),powerNow=managedPower[0].serial.load();
 auto pose=[&]{mapPan.context(2,true,stampNow,epochNow,powerNow);float camera[]={100,202,400};mapPan.frame(2,stampNow,GetTickCount64(),target,camera,1,0,0,1,100);};
 auto hit=[&](float u,float v,POINT cursor){TouchHit h;h.valid=true;h.u=u;h.v=v;h.cursor=cursor;h.time=GetTickCount64();h.trace=touchInput.trace;h.surface={0,0,0,1,1,epochNow,powerNow,managed_page_visual(0).serial};touchInput.hit(h);};
 for(bool trip:{false,true}){
  g_tripPresent=trip;managed_page_sample(0,d,2003000+(trip?3000:0),trip,true);pose();const auto now=GetTickCount64();touch_pointer({100,100},true,now);hit(.4f,.4f,{100,100});touch_pointer({140,110},true,now);hit(.5f,.43f,{140,110});touch_actions_worker();Check(mapPan.active(2,stampNow,epochNow,powerNow),"Held map drag did not pan before release");
  // Crossing Apps after a map press must stay a drag, never a Home action.
  touch_pointer({160,170},false,now);hit(.05f,.8f,{160,170});touch_actions_worker();Check(devicePages[0].destination!=1,"Map drag into Apps activated Home");
  std::vector<uint8_t> plain,button;{DisplayStyleScope style(3);DevicePageScope page(trip?DevicePage::Nav:DevicePage::Notrip);render_copilot(plain,d.overlay,bettergps_navigation::Current(),false);render_copilot(button,d.overlay,bettergps_navigation::Current(),true);}
  const size_t pixel=(size_t(d.overlay.items[13].y+5)*900+d.overlay.items[13].x+5)*4;Check(plain[pixel+3]==0&&button[pixel+3]==128&&button[pixel]==255,"Re-center background is not white at 50 percent opacity");Save(out/(trip?"nav-recenter.bmp":"notrip-recenter.bmp"),button,900,544);
  touchInput.cancel();const auto p=d.overlay.items[13];const float u=(p.x+p.w/2.f)/900,v=(p.y+p.h/2.f)/544;touch_pointer({200,200},true,now);hit(u,v,{200,200});touch_pointer({200,200},false,now);hit(u,v,{200,200});touch_actions_worker();Check(!mapPan.active(2,stampNow,epochNow,powerNow),"Re-center touch did not restore follow");
  touchInput.cancel();pose();const auto zoom=d.overlay.items[4];touch_pointer({300,300},true,now);hit((zoom.x+zoom.w*.98f)/900,(zoom.y+zoom.h*.5f)/544,{300,300});touch_pointer({340,330},false,now);hit(.5f,.4f,{340,330});touch_actions_worker();Check(!mapPan.active(2,stampNow,epochNow,powerNow),"Drag starting on zoom activated map pan");
 }
 pose();mapPan.drag(2,stampNow,epochNow,powerNow,GetTickCount64(),.4,.4,.5,.4,544./900);mapPan.context(2,true,stampNow,epochNow,powerNow+1);Check(!mapPan.active(2,stampNow,epochNow,powerNow+1),"Reignition retained exploration");mapPan.reset_all();touchInput.cancel();
 std::cout<<"PASS: Nav/Notrip held drag, Apps/zoom priority, re-center touch and white alpha128, world anchor, rotated/2D cameras, stale pose and power reset.\n";
}
}
