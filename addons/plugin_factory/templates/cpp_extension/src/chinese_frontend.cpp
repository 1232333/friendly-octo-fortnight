#include "chinese_frontend.hpp"
#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
namespace chinese { namespace {
enum class K { End, Id, Number, String, Function, Return, Arrow, LParen, RParen, Colon, Comma, Plus, Minus, Star, Slash, Percent, Eq, EqEq, Ne, Lt, Le, Gt, Ge, If, Else, EndBlock, Var, Print, True, False };
struct Tok { K k=K::End; std::string s; int line=1; };
class Lexer {
 const std::string& s; size_t p=0; int line=1;
 bool starts(const char*x)const{return s.compare(p,std::strlen(x),x)==0;}
 public: explicit Lexer(const std::string&v):s(v){}
 std::vector<Tok> run(std::string&err){
  std::vector<Tok> out;
  while(p<s.size()){
   unsigned char c=s[p]; if(c==' '||c=='\t'||c=='\r'){++p;continue;} if(c=='\n'){++line;++p;continue;}
   if(c=='#'){while(p<s.size()&&s[p]!='\n')++p;continue;}
   int ln=line;
   if(c=='"'||c=='\''){char q=s[p++];std::string v;bool done=false;while(p<s.size()){char x=s[p++];if(x==q){done=true;break;}if(x=='\n'){err="第 "+std::to_string(ln)+" 行：字符串不能跨行";return{};}if(x=='\\'&&p<s.size()){char e=s[p++];v+=e=='n'?'\n':e=='t'?'\t':e;}else v+=x;}if(!done){err="第 "+std::to_string(ln)+" 行：字符串没有闭合";return{};}out.push_back({K::String,v,ln});continue;}
   if(std::isdigit(c)){size_t b=p++;while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;if(p<s.size()&&s[p]=='.'){++p;while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;}out.push_back({K::Number,s.substr(b,p-b),ln});continue;}
   if(std::isalpha(c)||c=='_'||c>=0x80){size_t b=p++;while(p<s.size()){unsigned char x=s[p];if(std::isalnum(x)||x=='_'||x>=0x80)++p;else break;}std::string v=s.substr(b,p-b);K k=K::Id;
    if(v=="变量")k=K::Var;else if(v=="打印")k=K::Print;else if(v=="如果")k=K::If;else if(v=="否则")k=K::Else;else if(v=="结束")k=K::EndBlock;else if(v=="真")k=K::True;else if(v=="假")k=K::False;else if(v=="函数")k=K::Function;else if(v=="返回")k=K::Return;
    out.push_back({k,v,ln});continue;}
   if(starts("->")){out.push_back({K::Arrow,"->",ln});p+=2;continue;}if(starts("==")){out.push_back({K::EqEq,"==",ln});p+=2;continue;}if(starts("!=")){out.push_back({K::Ne,"!=",ln});p+=2;continue;}if(starts("<=")){out.push_back({K::Le,"<=",ln});p+=2;continue;}if(starts(">=")){out.push_back({K::Ge,">=",ln});p+=2;continue;}
   K k;switch(c){case '(':k=K::LParen;break;case ')':k=K::RParen;break;case ':':k=K::Colon;break;case ',':k=K::Comma;break;case '+':k=K::Plus;break;case '-':k=K::Minus;break;case '*':k=K::Star;break;case '/':k=K::Slash;break;case '%':k=K::Percent;break;case '=':k=K::Eq;break;case '<':k=K::Lt;break;case '>':k=K::Gt;break;default:err="第 "+std::to_string(ln)+" 行：非法字符";return{};}out.push_back({k,std::string(1,(char)c),ln});++p;
  }out.push_back({K::End,"",line});return out;
 }
};
class Parser {
 enum class Type { Any, Integer, Float, String, Boolean, Node };
 std::vector<Tok> t;size_t p=0;Bytecode b;std::unordered_map<std::string,int> locals;std::unordered_map<std::string,Type> local_types;std::unordered_map<std::string,int> functions;std::vector<std::vector<Type>> function_params;std::vector<Type> function_returns;bool in_function=false;Type current_return=Type::Any;std::string err;
 Type parse_type(const std::string&name){if(name=="整数")return Type::Integer;if(name=="小数")return Type::Float;if(name=="字符串")return Type::String;if(name=="布尔")return Type::Boolean;fail("不支持的类型“"+name+"”；支持整数、小数、字符串、布尔");return Type::Any;}
 const char* type_name(Type x){switch(x){case Type::Integer:return "整数";case Type::Float:return "小数";case Type::String:return "字符串";case Type::Boolean:return "布尔";case Type::Node:return "节点";default:return "动态";}}
 bool compatible(Type expected,Type actual){return expected==Type::Any||actual==Type::Any||expected==actual||(expected==Type::Float&&actual==Type::Integer);}

 const Tok& cur()const{return t[p];} bool is_else()const{return cur().k==K::Else||cur().s=="否则";} bool eat(K k){if(cur().k==k){++p;return true;}return false;}
 bool need(K k,const char*m){if(eat(k))return true;fail(m);return false;}
 void fail(const std::string&m){if(err.empty())err="第 "+std::to_string(cur().line)+" 行："+m;}
 int constant(Constant c){b.constants.push_back(std::move(c));return (int)b.constants.size()-1;}
 void emit(Op op,int arg=0,int line=-1){b.instructions.push_back({op,arg,line<0?cur().line:line});}
 int precedence(K k){switch(k){case K::EqEq:case K::Ne:case K::Lt:case K::Le:case K::Gt:case K::Ge:return 1;case K::Plus:case K::Minus:return 2;case K::Star:case K::Slash:case K::Percent:return 3;default:return 0;}}
 Type expr(int min=1){
  Tok tok=cur();Type result=Type::Any;
  if(eat(K::Minus)){Type inner=expr(4);if(inner!=Type::Any&&inner!=Type::Integer&&inner!=Type::Float)fail("一元负号只能用于数字");emit(Op::Neg,0,tok.line);result=inner;}
  else if(eat(K::LParen)){result=expr();need(K::RParen,"缺少右括号“)”");}
  else if(eat(K::Number)){Constant c;if(tok.s.find('.')!=std::string::npos){c.kind=Constant::Kind::Float;c.real=std::stod(tok.s);result=Type::Float;}else{c.kind=Constant::Kind::Integer;c.integer=std::stoll(tok.s);result=Type::Integer;}emit(c.kind==Constant::Kind::Float?Op::PushFloat:Op::PushInt,constant(c),tok.line);}
  else if(eat(K::String)){Constant c;c.kind=Constant::Kind::String;c.text=tok.s;emit(Op::PushString,constant(c),tok.line);result=Type::String;}
  else if(eat(K::True)||eat(K::False)){Constant c;c.kind=Constant::Kind::Boolean;c.boolean=tok.k==K::True;emit(Op::PushBool,constant(c),tok.line);result=Type::Boolean;}
  else if(eat(K::Id)){if(eat(K::LParen)){auto fit=functions.find(tok.s);if(fit!=functions.end()){int argc=0;std::vector<Type> actuals;if(cur().k!=K::RParen){do{actuals.push_back(expr());++argc;}while(eat(K::Comma));}need(K::RParen,"函数调用缺少右括号");if((size_t)fit->second>=function_params.size()||actuals.size()!=function_params[fit->second].size()){fail("函数“"+tok.s+"”的参数数量不正确");return Type::Any;}for(size_t ai=0;ai<actuals.size();++ai)if(!compatible(function_params[fit->second][ai],actuals[ai])){fail("函数“"+tok.s+"”的参数类型不匹配");return Type::Any;}emit(Op::Call,fit->second,tok.line);result=function_returns[fit->second];}else{Constant fn;fn.kind=Constant::Kind::String;fn.text=tok.s;emit(Op::PushString,constant(fn),tok.line);int argc=0;if(cur().k!=K::RParen){do{expr();++argc;}while(eat(K::Comma));}need(K::RParen,"函数调用缺少右括号");emit(Op::HostCall,argc,tok.line);result=(tok.s=="获取节点"?Type::Node:Type::Any);}}else{auto it=locals.find(tok.s);if(it==locals.end()){fail("使用了未声明的变量“"+tok.s+"”");return Type::Any;}emit(Op::LoadLocal,it->second,tok.line);result=local_types[tok.s];}}
  else{fail("此处需要表达式");return Type::Any;}
  while(err.empty()){int prec=precedence(cur().k);if(prec<min)break;K op=cur().k;int ln=cur().line;++p;Type right=expr(prec+1);Type left=result;Op code=Op::Add;switch(op){case K::Plus:code=Op::Add;break;case K::Minus:code=Op::Sub;break;case K::Star:code=Op::Mul;break;case K::Slash:code=Op::Div;break;case K::Percent:code=Op::Mod;break;case K::EqEq:code=Op::Eq;break;case K::Ne:code=Op::Ne;break;case K::Lt:code=Op::Lt;break;case K::Le:code=Op::Le;break;case K::Gt:code=Op::Gt;break;case K::Ge:code=Op::Ge;break;default:break;}
   if(op==K::Plus){if(left==Type::String&&right==Type::String)result=Type::String;else if((left==Type::Integer||left==Type::Float)&&(right==Type::Integer||right==Type::Float))result=(left==Type::Float||right==Type::Float)?Type::Float:Type::Integer;else if(left!=Type::Any&&right!=Type::Any){fail("加法两侧类型不兼容："+std::string(type_name(left))+" 与 "+type_name(right));return Type::Any;}}
   else if(op==K::Minus||op==K::Star||op==K::Slash||op==K::Percent){bool ln_ok=left==Type::Integer||left==Type::Float||left==Type::Any;bool rn_ok=right==Type::Integer||right==Type::Float||right==Type::Any;if(!ln_ok||!rn_ok){fail("算术运算只能用于数字");return Type::Any;}result=(left==Type::Float||right==Type::Float||op==K::Slash)?Type::Float:(left==Type::Any||right==Type::Any?Type::Any:Type::Integer);}
   else{bool numeric=(left==Type::Integer||left==Type::Float||left==Type::Any)&&(right==Type::Integer||right==Type::Float||right==Type::Any);bool equality=(op==K::EqEq||op==K::Ne)&&left==right;if(!numeric&&!equality&&left!=Type::Any&&right!=Type::Any){fail("比较两侧类型不兼容："+std::string(type_name(left))+" 与 "+type_name(right));return Type::Any;}result=Type::Boolean;}
   emit(code,0,ln);
  }
  return result;
 }
 void statement(){
  if(eat(K::Var)){Tok name=cur();if(!need(K::Id,"变量声明后需要名称"))return;if(locals.count(name.s)){fail("变量重复声明“"+name.s+"”");return;}Type declared=Type::Any;if(eat(K::Colon)){if(cur().k!=K::Id){fail("类型标注后需要类型名称");return;}declared=parse_type(cur().s);++p;}if(!need(K::Eq,"变量声明需要“=”"))return;Type actual=expr();if(!compatible(declared,actual)){fail("变量“"+name.s+"”声明为"+type_name(declared)+"，但初始值是"+type_name(actual));return;}Type final_type=declared==Type::Any?actual:declared;int slot=(int)locals.size();locals[name.s]=slot;local_types[name.s]=final_type;emit(Op::StoreLocal,slot,name.line);b.local_count=(uint32_t)locals.size();return;}
  if(eat(K::Print)){int n=0;if(eat(K::LParen)){if(cur().k!=K::RParen){do{expr();++n;}while(eat(K::Comma));}need(K::RParen,"打印语句缺少右括号");}else{expr();n=1;}emit(Op::Print,n);return;}
  if(eat(K::If)){Type condition=expr();if(condition!=Type::Boolean&&condition!=Type::Any)fail("如果条件必须是布尔类型，当前是"+std::string(type_name(condition)));need(K::Colon,"如果条件后需要冒号“:”");int jf=(int)b.instructions.size();emit(Op::JumpIfFalse,0);while(!is_else()&&cur().k!=K::EndBlock&&cur().k!=K::End&&err.empty())statement();if(is_else()){++p;eat(K::Colon);int j=(int)b.instructions.size();emit(Op::Jump,0);b.instructions[jf].operand=(int)b.instructions.size();while(cur().k!=K::EndBlock&&cur().k!=K::End&&err.empty())statement();b.instructions[j].operand=(int)b.instructions.size();}else b.instructions[jf].operand=(int)b.instructions.size();if(cur().k==K::EndBlock)++p;else fail("如果代码块缺少“结束”");return;}
  if(eat(K::Return)){if(!in_function){fail("返回只能在函数内部使用");return;}if(cur().k==K::EndBlock||cur().k==K::End){if(current_return!=Type::Any&&current_return!=Type::Node){fail("函数必须返回"+std::string(type_name(current_return)));return;}emit(Op::Return,0);return;}Type actual=expr();if(!compatible(current_return,actual)){fail("函数返回类型不匹配：需要"+std::string(type_name(current_return))+"，实际是"+type_name(actual));return;}emit(Op::Return,1);return;}
  if(eat(K::EndBlock)||eat(K::Else)){fail("此处出现多余的代码块结束标记");return;}
  if(cur().k==K::Id){expr();if(err.empty())emit(Op::Pop);return;}
  fail("不支持的语句“"+cur().s+"”；目前支持变量、打印和如果");
 }
 void parse_function(){Tok start=cur();++p;Tok name=cur();if(!need(K::Id,"函数关键字后需要函数名称"))return;if(functions.count(name.s)){fail("函数重复定义“"+name.s+"”");return;}if(!need(K::LParen,"函数名称后需要左括号"))return;int index=(int)b.functions.size();functions[name.s]=index;b.functions.push_back({name.s,0,0,0});function_params.emplace_back();function_returns.push_back(Type::Any);locals.clear();local_types.clear();in_function=true;current_return=Type::Any;while(cur().k!=K::RParen&&cur().k!=K::End&&err.empty()){Tok param=cur();if(!need(K::Id,"参数需要名称"))break;if(!need(K::Colon,"参数名称后需要类型标注"))break;if(cur().k!=K::Id){fail("参数类型名称无效");break;}Type pt=parse_type(cur().s);++p;int slot=(int)locals.size();locals[param.s]=slot;local_types[param.s]=pt;function_params[index].push_back(pt);++b.functions[index].parameter_count;if(!eat(K::Comma))break;}need(K::RParen,"函数参数列表缺少右括号");if(eat(K::Arrow)){if(cur().k!=K::Id)fail("返回箭头后需要返回类型");else{current_return=parse_type(cur().s);function_returns[index]=current_return;++p;}}need(K::Colon,"函数签名后需要冒号");b.functions[index].entry=(uint32_t)b.instructions.size();while(cur().k!=K::EndBlock&&cur().k!=K::End&&err.empty())statement();if(cur().k==K::EndBlock)++p;else fail("函数定义缺少“结束”");if(err.empty())emit(Op::Return,0,start.line);b.functions[index].local_count=(uint32_t)locals.size();locals.clear();local_types.clear();in_function=false;current_return=Type::Any;}
 public: explicit Parser(std::vector<Tok>v):t(std::move(v)){}
 FrontendResult run(){int main_jump=(int)b.instructions.size();emit(Op::Jump,0);while(cur().k==K::Function&&err.empty())parse_function();b.instructions[main_jump].operand=(int)b.instructions.size();while(cur().k!=K::End&&err.empty())statement();if(err.empty())emit(Op::Halt);FrontendResult r;r.ok=err.empty();r.error=err;r.bytecode=std::move(b);return r;}
};
template<class T>void put(std::vector<uint8_t>&o,T v){const uint8_t*p=(const uint8_t*)&v;o.insert(o.end(),p,p+sizeof(T));}
template<class T>bool get(const std::vector<uint8_t>&d,size_t&p,T&v){if(p+sizeof(T)>d.size())return false;std::memcpy(&v,d.data()+p,sizeof(T));p+=sizeof(T);return true;}
}
FrontendResult Frontend::compile(const std::string&s)const{std::string e;Lexer l(s);auto tokens=l.run(e);if(!e.empty())return {false,{},e};return Parser(std::move(tokens)).run();}
std::vector<uint8_t> Bytecode::encode()const{std::vector<uint8_t>o={'Z','H','B','C'};put<uint16_t>(o,2);put<uint32_t>(o,(uint32_t)functions.size());for(const auto&f:functions){put<uint32_t>(o,(uint32_t)f.name.size());o.insert(o.end(),f.name.begin(),f.name.end());put<uint32_t>(o,f.entry);put<uint32_t>(o,f.parameter_count);put<uint32_t>(o,f.local_count);}put<uint32_t>(o,local_count);put<uint32_t>(o,(uint32_t)constants.size());for(const auto&c:constants){put<uint8_t>(o,(uint8_t)c.kind);switch(c.kind){case Constant::Kind::Integer:put<int64_t>(o,c.integer);break;case Constant::Kind::Float:put<double>(o,c.real);break;case Constant::Kind::Boolean:put<uint8_t>(o,c.boolean?1:0);break;case Constant::Kind::String:put<uint32_t>(o,(uint32_t)c.text.size());o.insert(o.end(),c.text.begin(),c.text.end());break;}}put<uint32_t>(o,(uint32_t)instructions.size());for(auto&i:instructions){put<uint8_t>(o,(uint8_t)i.op);put<int32_t>(o,i.operand);put<int32_t>(o,i.line);}return o;}
bool Bytecode::decode(const std::vector<uint8_t>&d,Bytecode&b,std::string&e){if(d.size()<6||std::memcmp(d.data(),"ZHBC",4)!=0){e="无效的 ZHBC 文件头";return false;}size_t p=4;uint16_t ver;if(!get(d,p,ver)||ver!=2){e="不支持的字节码版本";return false;}uint32_t n;if(!get(d,p,n)||n>10000){e="函数表损坏";return false;}for(uint32_t i=0;i<n;++i){uint32_t z;FunctionInfo f;if(!get(d,p,z)||z>4096||p+z>d.size()){e="函数名称数据损坏";return false;}f.name.assign((const char*)d.data()+p,z);p+=z;if(!get(d,p,f.entry)||!get(d,p,f.parameter_count)||!get(d,p,f.local_count)||f.parameter_count>f.local_count){e="函数元数据损坏";return false;}b.functions.push_back(std::move(f));}if(!get(d,p,b.local_count)||!get(d,p,n)||n>100000){e="损坏的常量池";return false;}for(uint32_t i=0;i<n;++i){uint8_t k;if(!get(d,p,k)){e="常量池截断";return false;}Constant c;c.kind=(Constant::Kind)k;switch(c.kind){case Constant::Kind::Integer:if(!get(d,p,c.integer))goto bad;break;case Constant::Kind::Float:if(!get(d,p,c.real))goto bad;break;case Constant::Kind::Boolean:{uint8_t x;if(!get(d,p,x))goto bad;c.boolean=x!=0;break;}case Constant::Kind::String:{uint32_t z;if(!get(d,p,z)||p+z>d.size())goto bad;c.text.assign((const char*)d.data()+p,z);p+=z;break;}default:goto bad;}b.constants.push_back(std::move(c));}if(!get(d,p,n)||n>1000000){e="指令区损坏";return false;}for(uint32_t i=0;i<n;++i){uint8_t op;Instruction x;if(!get(d,p,op)||!get(d,p,x.operand)||!get(d,p,x.line))goto bad;x.op=(Op)op;if((unsigned)op<(unsigned)Op::PushInt||(unsigned)op>(unsigned)Op::Halt)goto bad;b.instructions.push_back(x);}if(p!=d.size()){e="字节码尾部包含无效数据";return false;}for(auto&i:b.instructions)if((i.op==Op::Jump||i.op==Op::JumpIfFalse)&&(i.operand<0||(size_t)i.operand>=b.instructions.size())){e="跳转目标越界";return false;}for(const auto&f:b.functions)if(f.entry>=b.instructions.size()||f.parameter_count>f.local_count){e="函数入口或局部变量数量无效";return false;}for(auto&i:b.instructions)if(i.op==Op::Call&&(i.operand<0||(size_t)i.operand>=b.functions.size())){e="函数调用索引越界";return false;}return true;bad:e="字节码数据截断或格式错误";return false;}
}
