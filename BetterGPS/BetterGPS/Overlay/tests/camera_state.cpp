#include "../CameraState.h"
#include "../CameraOpticsMath.h"
#include <cstdio>
#include <cstdlib>
using namespace bettergps_camera;
static int checks=0;
static void require(bool condition,const char* label){++checks;if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
int main(){
 static_assert(horizontalFov[0]==100&&horizontalFov[1]==100&&horizontalFov[2]==100&&horizontalFov[3]==100,"All camera FOV must follow the user-selected 100 degrees");
 PackedPose world{},frame{},relative{};world.sectors[0]=1;frame.position[0]=511.5f;relative_pose(world,frame,relative);require(relative.position[0]==.5f&&relative.sectors[0]==0,"packed position sectors are preserved when computing camera offsets");
 world={};frame={};world.position[2]=-1;camera_rotation(90,0,frame.rotation);relative_pose(world,frame,relative);require(std::abs(relative.position[0]-1)<.00001f&&std::abs(relative.position[2])<.00001f,"camera world offset is converted through the inverse cabin orientation");
 float q[4]{};camera_rotation(0,5,q);require(q[1]<0&&std::abs(q[2])<.00001f,"front camera pitches down five degrees");camera_rotation(180,30,q);require(std::abs(q[2])>.9f,"rear camera points rearward with downward pitch");
 State s;std::array<Pose,4> poses{};s.reset(1);s.hardware(1,15,poses);
 s.sample(false,false,false,false);require(!s.snapshot().playable(),"no camera render outside Camera app");
 s.sample(false,true,true,false);require(s.snapshot().selected==View::Forward,"automatic inputs outside app do not change selection");
 s.sample(true,false,false,false);require(s.snapshot().selected==View::Forward&&s.snapshot().playable(),"opening defaults to installed Forward");
 require(s.select(View::Right),"manual installed Right available");
 s.sample(true,false,false,false);require(s.snapshot().selected==View::Right,"manual persists with unchanged inputs");
 s.sample(true,false,true,false);require(s.snapshot().selected==View::Left,"new left trigger replaces manual");
 require(s.select(View::Right),"manual can override a held trigger");
 for(int i=0;i<20;++i)s.sample(true,false,true,false);
 require(s.snapshot().selected==View::Right,"held blinker command does not undo manual or follow lamp flashing");
 s.sample(true,true,true,false);require(s.snapshot().selected==View::Reverse,"new reverse trigger overrides manual and blinker");
 s.sample(true,true,false,true);require(s.snapshot().selected==View::Reverse,"reverse priority while indicators change");
 s.sample(true,false,false,true);require(s.snapshot().selected==View::Right,"leaving reverse restores currently requested side");
 s.sample(true,false,false,false);require(s.snapshot().selected==View::Forward,"trigger release returns Forward");
 require(s.select(View::Left),"manual Left accepted");
 s.sample(false,false,false,false);require(!s.snapshot().playable()&&!s.select(View::Reverse),"closing disables rendering and manual selector");
 s.sample(false,false,false,true);require(s.snapshot().selected==View::Left,"closed app ignores indicator changes");
 s.sample(true,false,false,true);require(s.snapshot().selected==View::Right,"reopening evaluates current trigger");
 s.sample(true,false,false,false);s.hardware(1,bit(View::Reverse),poses);
 require(s.snapshot().selected==View::Forward&&!s.snapshot().playable(),"missing Forward stays black, no fallback to Rear");
 require(!s.select(View::Left),"missing camera cannot be manually selected");
 s.sample(true,true,false,false);require(s.snapshot().playable()&&s.snapshot().selected==View::Reverse,"available reverse is usable without front");
 s.sample(true,false,true,false);require(!s.snapshot().playable()&&s.snapshot().selected==View::Left,"missing triggered camera stays black");
 s.hardware(2,15,poses);require(!s.snapshot().active&&s.snapshot().selected==View::Forward,"new vehicle epoch clears manual/active state");
 s.sample(true,false,false,false);s.select(View::Reverse);s.hardware(2,0,poses);require(!s.snapshot().playable(),"uninstalled camera stops playback");
 s.hardware(2,15,poses,false);require(!s.snapshot().playable(),"unknown hardware never permits playback");
 s.reset(3);require(!s.snapshot().known&&!s.snapshot().active&&s.snapshot().available==0,"full reset drops availability");
 std::printf("PASS: %d camera selection and app-scope checks\n",checks);
}
