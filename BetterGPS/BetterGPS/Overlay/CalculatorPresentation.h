#pragma once
// Calculator AOSP 4.2.2 adapted to the supplied DEA600 landscape reference.
// Shared text rendering is already available from the DeskClock presentation.
struct CalculatorKey {const char* label;const char* input;Placement bounds;bool digit=false;};
static const std::vector<CalculatorKey>& calculator_keys(){
 static const auto keys=[](){std::vector<CalculatorKey> k;const char* scientific[]={"sin","ln","cos","log","tan","e","pi","^","!","sqrt"};const char* labels[]={"sin","ln","cos","log","tan","e",u8"\u03c0","^","!",u8"\u221a"};
  for(unsigned i=0;i<10;++i)k.push_back({labels[i],scientific[i],{12+int(i%2)*122,171+int(i/2)*61,120,59},false});
  const char* number[]={"7","8","9","4","5","6","1","2","3"};for(int i=0;i<9;++i)k.push_back({number[i],number[i],{274+i%3*123,171+i/3*76,121,74},true});
  k.push_back({"0","0",{274,399,244,74},true});k.push_back({".",".",{520,399,121,74},true});
  const char* operators[]={u8"\u00f7",u8"\u00d7",u8"\u2212","+"};const char* inputs[]={"/","*","-","+"};for(int i=0;i<4;++i)k.push_back({operators[i],inputs[i],{643,171+i*76,121,74},false});
  k.push_back({"(","(",{766,171,122,74},false});k.push_back({")",")",{766,247,122,74},false});k.push_back({"=","=",{766,323,122,150},false});return k;}();return keys;
}
static std::string calculator_display(std::string text){
 const std::pair<const char*,const char*> substitutions[]={{"sqrt",u8"\u221a"},{"pi",u8"\u03c0"},{"inf",u8"\u221e"},{"*",u8"\u00d7"},{"/",u8"\u00f7"},{"-",u8"\u2212"}};
 for(const auto& item:substitutions){size_t at=0;while((at=text.find(item.first,at))!=std::string::npos){text.replace(at,std::strlen(item.first),item.second);at+=std::strlen(item.second);}}return text;
}
static std::vector<uint8_t> copilot_calculator_layer(){
 std::vector<uint8_t> pixels(900*544*4,10);for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;DeskClockCanvas c{&pixels,{}};
 c.fill({12,79,876,76},RGB(15,43,63));c.fill({778,79,2,76},RGB(139,167,180));const auto v=bettergps_calculator::calculator.view();const auto text=calculator_display(v.text);
 // Scale long expressions into the input field rather than clipping controls.
 const auto glyph=copilot_clock_glyph(text,46,FW_NORMAL);const int textWidth=std::min(742,glyph.width);c.text(text,{768-textWidth,85,textWidth,64},46,FW_NORMAL,RGB(255,255,255),false);
 const auto& icon=copilot_image("calculator/calc_clear_symbol.dds");overlay_blit(pixels,icon.pixels,icon.width,icon.height,{807,96,52,45},900,544);
 for(const auto& key:calculator_keys()){c.fill(key.bounds,key.digit?RGB(132,165,162):RGB(63,81,79));c.text(key.label,key.bounds,key.bounds.x<260?24:43,FW_MEDIUM);}
 return pixels;
}
static bool copilot_calculator_hit(float x,float y){
 if(x>=780&&x<888&&y>=79&&y<155){bettergps_calculator::calculator.press("del");return true;}
 for(const auto& key:calculator_keys())if(x>=key.bounds.x&&x<key.bounds.x+key.bounds.w&&y>=key.bounds.y&&y<key.bounds.y+key.bounds.h){std::string input=key.input;if(input=="sin"||input=="cos"||input=="tan"||input=="ln"||input=="log"||input=="sqrt")input+='(';bettergps_calculator::calculator.press(input);return true;}return false;
}
