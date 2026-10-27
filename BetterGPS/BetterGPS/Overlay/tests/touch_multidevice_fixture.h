#pragma once
// Included by the production CPU integration test, not the experimental replay
// fixture. These depth tickets deliberately arrive in the wrong device order.
static void test_multidevice_picker(){
 auto old=managed_snapshot(1);managedDevices[1]=managed_snapshot(0);managedPower[1].on=true;managedPower[1].serial=1;managedPower[1].phase=startup_total_ms();
 managed_page_sample(1,managed_snapshot(1),100000,true,true);
 uint64_t sequence=0;auto scene=[&](POINT p,unsigned newest){
  const auto now=GetTickCount64();++sequence;TouchInput::Request request;check(touchInput.request(request),"Missing multi-device request");TouchGeometryScene result;
  for(unsigned slot=0;slot<2;++slot){
   TouchGeometryMesh m;m.slot=slot;m.at=now;m.epoch=vehicleStart.epoch;m.session=touchStatus.sourceGeneration;m.power=managedPower[slot].serial;m.page=managed_page_visual(slot).serial;m.addressU=m.addressV=D3D11_TEXTURE_ADDRESS_CLAMP;m.viewport={0,0,800,600,800,600};
   const float x=slot?.5f:-.5f;using touch_geometry::Vertex;const Vertex a{{{x-.4f,-.8f,.5f,1}},0,1},b{{{x+.4f,-.8f,.5f,1}},1,1},c{{{x+.4f,.8f,.5f,1}},1,0},e{{{x-.4f,.8f,.5f,1}},0,0};m.triangles={{{a,b,c}},{{a,c,e}}};
   auto d=std::make_shared<TouchGeometryDepth>();d->trace=request.trace;d->at=now;d->id=sequence*100+(slot==newest?90:1);d->frame=now;d->cursor=p;d->x=p.x;d->y=p.y;d->clientW=800;d->clientH=600;d->usable=d->ready=d->sealed=true;d->actual=.5f;d->viewFormat=DXGI_FORMAT_D32_FLOAT;d->boundary="multi-device-fixture";m.depth=std::move(d);result.meshes.push_back(std::move(m));
  }return result;
 };
 for(bool reverse:{false,true})for(unsigned slot=0;slot<2;++slot){
  touchInput.cancel();touchCpuPicker.stop();POINT p{slot?600:200,300};touch_pointer(p,true,GetTickCount64());touch_pointer(p,false,GetTickCount64());auto image=scene(p,1-slot);if(reverse)std::reverse(image.meshes.begin(),image.meshes.end());touchGeometryView.publish(std::move(image));touchCpuPicker.present(nullptr,800,600,800,600,GetTickCount64());auto actions=touchInput.take();check(actions.size()==1&&actions[0].first.surface.slot==slot,"Newer ticket from the OTHER phone stole/cancelled the contact");
 }
 for(unsigned slot=0;slot<2;++slot){
  touchInput.cancel();touchCpuPicker.stop();POINT bottom{slot?600:200,510},top{bottom.x,180};touch_pointer(bottom,true,GetTickCount64());touchGeometryView.publish(scene(bottom,1-slot));touchCpuPicker.present(nullptr,800,600,800,600,GetTickCount64());TouchInput::Request request;check(touchInput.request(request)&&request.slot==slot,"Swipe did not lock to its physical phone");touch_pointer(top,false,GetTickCount64());touchGeometryView.publish(scene(top,1-slot));touchCpuPicker.present(nullptr,800,600,800,600,GetTickCount64());auto actions=touchInput.take();check(actions.size()==1&&actions[0].swipe&&actions[0].first.surface.slot==slot,"Swipe was transferred to/cancelled by the other phone");
 }
 auto boot=managed_snapshot(0);boot.prefs.bootHome=1;boot.prefs.bootHomeSeconds=2;managedDevices[30]=managedDevices[31]=boot;
 for(unsigned slot:{30u,31u}){managed_page_sample(slot,boot,1000,false,true);managed_page_presented(slot,managed_page_visual(slot).serial,1000);}
 managed_page_destination(30,false);const auto opened=managed_page_sample(30,boot,2100,false,true);const auto waiting=managed_page_sample(31,boot,2100,false,true);const auto expired=managed_page_sample(31,boot,3100,false,true);
 check(opened.page==DevicePage::Notrip&&opened.cause==PageCause::TouchMap&&waiting.page==DevicePage::Home&&expired.page==DevicePage::Notrip&&expired.cause==PageCause::BootTimer&&devicePages[31].destination==0,"An independent boot timer was confused with broadcasting a phone tap");managedDevices[30]=managedDevices[31]=ManagedDevice{};
 managedDevices[1]=old;touchInput.cancel();touchCpuPicker.stop();std::cout<<"PASS: both physical phone contacts/swipes stay independent with reversed draw order/newer unrelated depth tickets; first boot timer is explicitly distinguished from a touch command.\n";
}
