#include "chinese_frontend.hpp"
#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
namespace chinese { namespace {
enum class K { End, Id, Number, String, LParen, RParen, Colon, Comma, Plus, Minus, Star, Slash, Percent, Eq, EqEq, Ne, Lt, Le, Gt, Ge, If, Else, EndBlock, Var, Print, True, False };
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
    if(v=="变量")k=K::Var;else if(v=="打印")k=K::Print;else if(v=="如果")k=K::If;else if(v=="否则")k=K::Else;else if(v=="结束")k=K::EndBlock;else if(v=="真")k=K::True;else if(v=="假")k=K::False;
    out.push_back({k,v,ln});continue;}
   if(starts("==")){out.push_back({K::EqEq,"==",ln});p+=2;continue;}if(starts("!=")){out.push_back({K::Ne,"!=",ln});p+=2;continue;}if(starts("<=")){out.push_back({K::Le,"<=",ln});p+=2;continue;}if(starts(">=")){out.push_back({K::Ge,">=",ln});p+=2;continue;}
   K k;switch(c){case '(':k=K::LParen;break;case ')':k=K::RParen;break;case ':':k=K::Colon;break;case ',':k=K::Comma;break;case '+':k=K::Plus;break;case '-':k=K::Minus;break;case '*':k=K::Star;break;case '/':k=K::Slash;break;case '%':k=K::Percent;break;case '=':k=K::Eq;break;case '<':k=K::Lt;break;case '>':k=K::Gt;break;default:err="第 "+std::to_string(ln)+" 行：非法字符";return{};}out.push_back({k,std::string(1,(char)c),ln});++p;
  }out.push_back({K::End,"",line});return out;
 }
};
class Parser {
 std::vector<Tok> t;size_t p=0;Bytecode b;std::unordered_map<std::string,int> locals;std::string err;
 const Tok& cur()const{return t[p];} bool eat(K k){if(cur().k==k){++p;return true;}return false;}
 bool need(K k,const char*m){if(eat(k))return true;fail(m);return false;}
 void fail(const std::string&m){if(err.empty())err="第 "+std::to_string(cur().line)+" 行："+m;}
 int constant(Constant c){b.constants.push_back(std::move(c));return (int)b.constants.size()-1;}
 void emit(Op op,int arg=0,int line=-1){b.instructions.push_back({op,arg,line<0?cur().line:line});}
 int precedence(K k){switch(k){case K::EqEq:case K::Ne:case K::Lt:case K::Le:case K::Gt:case K::Ge:return 1;case K::Plus:case K::Minus:return 2;case K::Star:case K::Slash:case K::Percent:return 3;default:return 0;}}
 void expr(int min=1){
  Tok tok=cur();
  if(eat(K::Minus)){expr(4);emit(Op::Neg,0,tok.line);}
  else if(eat(K::LParen)){expr();need(K::RParen,"缺少右括号“)”");}
  else if(eat(K::Number)){Constant c;if(tok.s.find('.')!=std::string::npos){c.kind=Constant::Kind::Float;c.real=std::stod(tok.s);}else{c.kind=Constant::Kind::Integer;c.integer=std::stoll(tok.s);}emit(c.kind==Constant::Kind::Float?Op::PushFloat:Op::PushInt,constant(c),tok.line);}
  else if(eat(K::String)){Constant c;c.kind=Constant::Kind::String;c.text=tok.s;emit(Op::PushString,constant(c),tok.line);}
  else if(eat(K::True)||eat(K::False)){Constant c;c.kind=Constant::Kind::Boolean;c.boolean=tok.k==K::True;emit(Op::PushBool,constant(c),tok.line);}
  else if(eat(K::Id)){auto it=locals.find(tok.s);if(it==locals.end()){fail("使用了未声明的变量“"+tok.s+"”");return;}emit(Op::LoadLocal,it->second,tok.line);}
  else{fail("此处需要表达式");return;}
  while(err.empty()){int prec=precedence(cur().k);if(prec<min)break;K op=cur().k;int ln=cur().line;++p;expr(prec+1);Op code=Op::Add;switch(op){case K::Plus:code=Op::Add;break;case K::Minus:code=Op::Sub;break;case K::Star:code=Op::Mul;break;case K::Slash:code=Op::Div;break;case K::Percent:code=Op::Mod;break;case K::EqEq:code=Op::Eq;break;case K::Ne:code=Op::Ne;break;case K::Lt:code=Op::Lt;break;case K::Le:code=Op::Le;break;case K::Gt:code=Op::Gt;break;case K::Ge:code=Op::Ge;break;default:break;}emit(code,0,ln);}
 }
 void statement(){
  if(eat(K::Var)){Tok name=cur();if(!need(K::Id,"变量声明后需要名称"))return;if(locals.count(name.s)){fail("变量重复声明“"+name.s+"”");return;}if(eat(K::Colon)){if(cur().k!=K::Id){fail("类型标注后需要类型名称");return;}++p;}if(!need(K::Eq,"变量声明需要“=”"))return;expr();int slot=(int)locals.size();locals[name.s]=slot;emit(Op::StoreLocal,slot,name.line);b.local_count=(uint32_t)locals.size();return;}
  if(eat(K::Print)){int n=0;if(eat(K::LParen)){if(cur().k!=K::RParen){do{expr();++n;}while(eat(K::Comma));}need(K::RParen,"打印语句缺少右括号");}else{expr();n=1;}emit(Op::Print,n);return;}
  if(eat(K::If)){expr();need(K::Colon,"如果条件后需要冒号“:”");int jf=(int)b.instructions.size();emit(Op::JumpIfFalse,0);while(cur().k!=K::Else&&cur().k!=K::EndBlock&&cur().k!=K::End&&err.empty())statement();if(eat(K::Else)){int j=(int)b.instructions.size();emit(Op::Jump,0);b.instructions[jf].operand=(int)b.instructions.size();while(cur().k!=K::EndBlock&&cur().k!=K::End&&err.empty())statement();b.instructions[j].operand=(int)b.instructions.size();}else b.instructions[jf].operand=(int)b.instructions.size();if(cur().k==K::EndBlock)++p;else fail("如果代码块缺少“结束”");return;}
  if(eat(K::EndBlock)||eat(K::Else)){fail("此处出现多余的代码块结束标记");return;}
  fail("不支持的语句；目前支持变量、打印和如果");
 }
 public: explicit Parser(std::vector<Tok>v):t(std::move(v)){}
 FrontendResult run(){while(cur().k!=K::End&&err.empty())statement();if(err.empty())emit(Op::Halt);FrontendResult r;r.ok=err.empty();r.error=err;r.bytecode=std::move(b);return r;}
};
template<class T>void put(std::vector<uint8_t>&o,T v){const uint8_t*p=(const uint8_t*)&v;o.insert(o.end(),p,p+sizeof(T));}
template<class T>bool get(const std::vector<uint8_t>&d,size_t&p,T&v){if(p+sizeof(T)>d.size())return false;std::memcpy(&v,d.data()+p,sizeof(T));p+=sizeof(T);return true;}
}
FrontendResult Frontend::compile(const std::string&s)const{std::string e;Lexer l(s);auto tokens=l.run(e);if(!e.empty())return {false,{},e};return Parser(std::move(tokens)).run();}
std::vector<uint8_t> Bytecode::encode()const{std::vector<uint8_t>o={'Z','H','B','C'};put<uint16_t>(o,1);put<uint32_t>(o,local_count);put<uint32_t>(o,(uint32_t)constants.size());for(const auto&c:constants){put<uint8_t>(o,(uint8_t)c.kind);switch(c.kind){case Constant::Kind::Integer:put<int64_t>(o,c.integer);break;case Constant::Kind::Float:put<double>(o,c.real);break;case Constant::Kind::Boolean:put<uint8_t>(o,c.boolean?1:0);break;case Constant::Kind::String:put<uint32_t>(o,(uint32_t)c.text.size());o.insert(o.end(),c.text.begin(),c.text.end());break;}}put<uint32_t>(o,(uint32_t)instructions.size());for(auto&i:instructions){put<uint8_t>(o,(uint8_t)i.op);put<int32_t>(o,i.operand);put<int32_t>(o,i.line);}return o;}
bool Bytecode::decode(const std::vector<uint8_t>&d,Bytecode&b,std::string&e){if(d.size()<6||std::memcmp(d.data(),"ZHBC",4)!=0){e="无效的 ZHBC 文件头";return false;}size_t p=4;uint16_t ver;if(!get(d,p,ver)||ver!=1){e="不支持的字节码版本";return false;}uint32_t n;if(!get(d,p,b.local_count)||!get(d,p,n)||n>100000){e="损坏的常量池";return false;}for(uint32_t i=0;i<n;++i){uint8_t k;if(!get(d,p,k)){e="常量池截断";return false;}Constant c;c.kind=(Constant::Kind)k;switch(c.kind){case Constant::Kind::Integer:if(!get(d,p,c.integer))goto bad;break;case Constant::Kind::Float:if(!get(d,p,c.real))goto bad;break;case Constant::Kind::Boolean:{uint8_t x;if(!get(d,p,x))goto bad;c.boolean=x!=0;break;}case Constant::Kind::String:{uint32_t z;if(!get(d,p,z)||p+z>d.size())goto bad;c.text.assign((const char*)d.data()+p,z);p+=z;break;}default:goto bad;}b.constants.push_back(std::move(c));}if(!get(d,p,n)||n>1000000){e="指令区损坏";return false;}for(uint32_t i=0;i<n;++i){uint8_t op;Instruction x;if(!get(d,p,op)||!get(d,p,x.operand)||!get(d,p,x.line))goto bad;x.op=(Op)op;if((unsigned)op<(unsigned)Op::PushInt||(unsigned)op>(unsigned)Op::Halt)goto bad;b.instructions.push_back(x);}if(p!=d.size()){e="字节码尾部包含无效数据";return false;}for(auto&i:b.instructions)if((i.op==Op::Jump||i.op==Op::JumpIfFalse)&&(i.operand<0||(size_t)i.operand>=b.instructions.size())){e="跳转目标越界";return false;}return true;bad:e="字节码数据截断或格式错误";return false;}
}
