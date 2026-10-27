#pragma once
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2008 The Android Open Source Project.
// Native adaptation of Calculator 4.2.2 Logic/CalculatorEditable/EventListener.
// Android/Arity execution is replaced by the bounded native evaluator below.
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
namespace bettergps_calculator {
inline bool is_operator(char c){return c=='+'||c=='-'||c=='*'||c=='/';}
class Expression {
 const std::string& text;size_t at=0;unsigned depth=0;
 char peek()const{return at<text.size()?text[at]:0;}
 bool take(char c){if(peek()!=c)return false;++at;return true;}
 [[noreturn]] static void invalid(){throw std::runtime_error("Invalid expression");}
 double sum(){double a=product();for(;;){if(take('+'))a+=product();else if(take('-'))a-=product();else return a;}}
 double product(){double a=unary();for(;;){if(take('*'))a*=unary();else if(take('/'))a/=unary();else if(peek()=='('||(peek()>='0'&&peek()<='9')||(peek()>='a'&&peek()<='z'))a*=unary();else return a;}}
 double unary(){if(++depth>64)invalid();double a;if(take('+'))a=unary();else if(take('-'))a=-unary();else {a=primary();while(take('!')){if(a<0||a>170)invalid();a=std::tgamma(a+1);}if(take('^'))a=std::pow(a,unary());}--depth;return a;}
 double primary(){
  if(take('(')){double a=sum();if(!take(')')&&at!=text.size())invalid();return a;}
  if(peek()>='a'&&peek()<='z'){
   const auto start=at;while(peek()>='a'&&peek()<='z')++at;const auto name=text.substr(start,at-start);
   if(name=="pi")return 3.14159265358979323846;if(name=="e")return std::exp(1.);if(name=="inf")return std::numeric_limits<double>::infinity();
   if(!take('('))invalid();const double a=sum();if(!take(')')&&at!=text.size())invalid();
   if(name=="sin")return std::sin(a);if(name=="cos")return std::cos(a);if(name=="tan")return std::tan(a);if(name=="ln")return std::log(a);if(name=="log")return std::log10(a);if(name=="sqrt")return std::sqrt(a);invalid();
  }
  const auto start=at;bool digits=false;while(peek()>='0'&&peek()<='9'){digits=true;++at;}if(take('.'))while(peek()>='0'&&peek()<='9'){digits=true;++at;}if(!digits)invalid();
  if(peek()=='e'){size_t e=at+1;if(e<text.size()&&(text[e]=='+'||text[e]=='-'))++e;const auto first=e;while(e<text.size()&&text[e]>='0'&&text[e]<='9')++e;if(e>first)at=e;}
  double a=0;std::istringstream number(text.substr(start,at-start));number.imbue(std::locale::classic());if(!(number>>a))invalid();return a;
 }
public:
 explicit Expression(const std::string& value):text(value){}
 double evaluate(){if(text.empty()||text.size()>256)invalid();double a=sum();if(at!=text.size()||std::isnan(a))invalid();return a;}
};
inline std::string format(double value){if(std::isinf(value))return value<0?"-inf":"inf";if(value==0)value=0;std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12)<<value;auto text=out.str();auto e=text.find('e');if(e!=std::string::npos)text=text.substr(0,e)+"e"+std::to_string(std::stoi(text.substr(e+1)));return text;}
struct View {std::string text;bool result=false,error=false;uint64_t revision=0;};
class Model {
 mutable std::mutex mutex;View state;std::string expression;
public:
 View view()const{std::lock_guard<std::mutex> lock(mutex);return state;}
 void press(const std::string& key){
  std::lock_guard<std::mutex> lock(mutex);
  if(key=="clear"){state.text.clear();state.result=state.error=false;}
  else if(key=="del"){if(state.result||state.error){state.text.clear();state.result=state.error=false;}else if(!state.text.empty())state.text.pop_back();}
  else if(key=="="){
   if(state.result||state.error){state.text=expression;state.result=state.error=false;}
   else if(!state.text.empty()){expression=state.text;auto input=state.text;while(!input.empty()&&is_operator(input.back()))input.pop_back();try{state.text=format(Expression(input).evaluate());state.result=true;}catch(const std::exception&){state.text="Error";state.error=true;}}
  }else{
   const bool op=key.size()==1&&is_operator(key[0]);
   if(state.error||(state.result&&!op)){state.text.clear();state.error=false;}state.result=false;
   if(key=="."){size_t p=state.text.size();while(p&&state.text[p-1]>='0'&&state.text[p-1]<='9')--p;if(p&&state.text[p-1]=='.')return;}
   if(op){const char c=key[0];if(c=='-'&&!state.text.empty()&&state.text.back()=='-')return;while(!state.text.empty()&&is_operator(state.text.back())&&(c!='-'||state.text.back()=='+'))state.text.pop_back();if(state.text.empty()&&c!='-')return;}
   if(state.text.size()+key.size()>256)return;state.text+=key;
  }
  ++state.revision;
 }
};
inline Model calculator;
}
