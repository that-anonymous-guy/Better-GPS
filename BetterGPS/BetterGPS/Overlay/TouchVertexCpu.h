#pragma once
#include "TouchGeometryMath.h"
#include <sstream>
#include <string>
#include <map>
#include <cstring>
#include <cstdlib>
#include <limits>
namespace bettergps_overlay::touch_geometry {
// Deliberately restricted, fail-closed evaluator of straight-line DXBC text.
// No guessed matrix register, row/column order or TEXCOORD index.
struct Register {V4 value;unsigned valid=0;};
struct Instruction {std::string op;std::vector<std::string> args;};
inline std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos)return {};return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline std::vector<std::string> split_args(const std::string& text){std::vector<std::string> out;int nesting=0;size_t at=0;for(size_t i=0;i<text.size();++i){if(text[i]=='('||text[i]=='[')++nesting;if(text[i]==')'||text[i]==']')--nesting;if(text[i]==','&&!nesting){out.push_back(trim(text.substr(at,i-at)));at=i+1;}}out.push_back(trim(text.substr(at)));return out;}
struct Program {
 std::vector<Instruction> code;std::string failure;
 bool parse(const std::string& assembly){
  code.clear();failure.clear();std::istringstream lines(assembly);std::string line;
  while(std::getline(lines,line)){line=trim(line);if(line.empty()||line[0]=='/'||line.rfind("vs_",0)==0||line.rfind("dcl_",0)==0)continue;
   auto space=line.find_first_of(" \t");auto op=line.substr(0,space);
   if(op=="ret")break;if(op=="if_z"||op=="if_nz"||op=="else"||op=="endif"||op=="loop"||op=="endloop"||op=="switch"||op.rfind("call",0)==0||op.rfind("retc",0)==0){failure="vertex control flow unsupported: "+op;return false;}
   if(space==line.npos){failure="unrecognized vertex instruction: "+line;return false;}
   if(code.size()>=512){failure="vertex instruction budget";return false;}
   code.push_back({op,split_args(line.substr(space+1))});
  }return !code.empty();
 }
 bool run(const std::array<Register,32>& inputs,const std::array<std::vector<V4>,14>& constants,std::array<Register,32>& outputs,std::string& why)const{
  std::array<Register,256> temps{};outputs={};
  auto read=[&](std::string operand)->Register{
   Register result;bool negative=false,absolute=false;operand=trim(operand);if(!operand.empty()&&operand[0]=='-'){negative=true;operand.erase(0,1);}if(operand.rfind("abs(",0)==0&&operand.back()==')'){absolute=true;operand=operand.substr(4,operand.size()-5);}
   if(operand.rfind("l(",0)==0&&operand.back()==')'){auto args=split_args(operand.substr(2,operand.size()-3));if(args.size()!=1&&args.size()!=4)return result;for(unsigned i=0;i<4;++i){const auto& a=args[args.size()==1?0:i];char* end=nullptr;result.value[i]=strtof(a.c_str(),&end);if(!end||*end)return Register{};}result.valid=15;}
   else {
    const auto dot=operand.find('.',operand.find(']')==operand.npos?0:operand.find(']'));const auto base=operand.substr(0,dot);Register source;unsigned index=0,cb=0;int consumed=0;
    if(sscanf(base.c_str(),"cb%u[%u]%n",&cb,&index,&consumed)==2&&consumed==int(base.size())&&cb<constants.size()&&index<constants[cb].size()){source.value=constants[cb][index];source.valid=15;}
    else if(sscanf(base.c_str(),"r%u%n",&index,&consumed)==1&&consumed==int(base.size())&&index<temps.size())source=temps[index];
    else if(sscanf(base.c_str(),"v%u%n",&index,&consumed)==1&&consumed==int(base.size())&&index<inputs.size())source=inputs[index];
    else if(sscanf(base.c_str(),"o%u%n",&index,&consumed)==1&&consumed==int(base.size())&&index<outputs.size())source=outputs[index];else return result;
    const auto swizzle=dot==operand.npos?"xyzw":operand.substr(dot+1);if(swizzle.empty()||swizzle.size()>4)return result;
    for(unsigned i=0;i<4;++i){auto at=std::string("xyzw").find(swizzle[std::min<size_t>(i,swizzle.size()-1)]);if(at==std::string::npos)return Register{};result.value[i]=source.value[at];if(source.valid&(1u<<at))result.valid|=1u<<i;}
   }
   for(unsigned i=0;i<4;++i){if(absolute)result.value[i]=std::fabs(result.value[i]);if(negative)result.value[i]=-result.value[i];}return result;
  };
  std::string unsupported;
  for(const auto& instruction:code){if(instruction.args.empty())continue;auto op=instruction.op;bool saturate=false;if(op.size()>4&&op.substr(op.size()-4)=="_sat"){op.resize(op.size()-4);saturate=true;}
   const auto& destination=instruction.args[0];const auto dot=destination.find('.');unsigned index=0;int used=0;Register* target=nullptr;const auto base=destination.substr(0,dot);
   if(sscanf(base.c_str(),"r%u%n",&index,&used)==1&&used==int(base.size())&&index<temps.size())target=&temps[index];
   else if(sscanf(base.c_str(),"o%u%n",&index,&used)==1&&used==int(base.size())&&index<outputs.size())target=&outputs[index];else {why="unsupported vertex destination: "+destination;return false;}
   Register a,b,c,result;if(instruction.args.size()>1)a=read(instruction.args[1]);if(instruction.args.size()>2)b=read(instruction.args[2]);if(instruction.args.size()>3)c=read(instruction.args[3]);
   bool known=true;unsigned count=0;
   if(op=="dp2"||op=="dp3"||op=="dp4"){count=unsigned(op.back()-'0');float sum=0;const unsigned mask=(1u<<count)-1;if((a.valid&mask)==mask&&(b.valid&mask)==mask){for(unsigned i=0;i<count;++i)sum+=a.value[i]*b.value[i];for(auto& v:result.value.x)v=sum;result.valid=15;}}
   else for(unsigned i=0;i<4;++i){const auto bit=1u<<i;bool valid=false;float value=0;
    if(op=="mov"){value=a.value[i];valid=a.valid&bit;}
    else if(op=="add"||op=="mul"||op=="div"||op=="min"||op=="max"){valid=(a.valid&b.valid&bit)!=0;value=op=="add"?a.value[i]+b.value[i]:op=="mul"?a.value[i]*b.value[i]:op=="div"?a.value[i]/b.value[i]:op=="min"?std::min(a.value[i],b.value[i]):std::max(a.value[i],b.value[i]);}
    else if(op=="mad"){valid=(a.valid&b.valid&c.valid&bit)!=0;value=a.value[i]*b.value[i]+c.value[i];}
    else if(op=="rcp"||op=="rsq"||op=="sqrt"||op=="frc"||op=="round_ni"){valid=a.valid&bit;value=op=="rcp"?1/a.value[i]:op=="rsq"?1/std::sqrt(a.value[i]):op=="sqrt"?std::sqrt(a.value[i]):op=="frc"?a.value[i]-std::floor(a.value[i]):std::floor(a.value[i]);}
    else {known=false;valid=false;}
    if(valid&&std::isfinite(value)){result.value[i]=saturate?std::clamp(value,0.f,1.f):value;result.valid|=bit;}
   }
   if(!known&&unsupported.empty())unsupported=instruction.op;
   // DXBC component masks retain source component indices (not packed writes).
   const auto mask=dot==destination.npos?"xyzw":destination.substr(dot+1);
   for(char ch:mask){auto at=std::string("xyzw").find(ch);if(at==std::string::npos){why="invalid destination mask";return false;}target->value[at]=result.value[at];target->valid=(target->valid&~(1u<<at))|(result.valid&(1u<<at));}
  }
  why=unsupported.empty()?"unresolved vertex dependency":"unsupported vertex operation: "+unsupported;return true;
 }
};
}
