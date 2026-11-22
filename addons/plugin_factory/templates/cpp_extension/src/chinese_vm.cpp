#include "chinese_vm.hpp"
#include <cmath>
#include <sstream>
#include <vector>
namespace chinese { namespace {
double number(const Value&v,bool&ok){if(auto p=std::get_if<int64_t>(&v))return(double)*p;if(auto p=std::get_if<double>(&v))return*p;ok=false;return 0;}
std::string show(const Value&v){if(std::holds_alternative<std::monostate>(v))return"空";if(auto p=std::get_if<int64_t>(&v))return std::to_string(*p);if(auto p=std::get_if<double>(&v)){std::ostringstream s;s<<*p;return s.str();}if(auto p=std::get_if<std::string>(&v))return*p;if(auto p=std::get_if<bool>(&v))return*p?"真":"假";if(std::holds_alternative<ObjectHandle>(v))return"<节点>";return{};}
bool truth(const Value&v){if(auto p=std::get_if<bool>(&v))return*p;if(auto p=std::get_if<int64_t>(&v))return*p!=0;if(auto p=std::get_if<double>(&v))return*p!=0;return !std::holds_alternative<std::monostate>(v);}
}
VmResult VirtualMachine::run(const Bytecode&b,const HostFunction&host)const{
 VmResult r;struct Frame{std::vector<Value> locals;size_t return_pc=0;size_t stack_base=0;};std::vector<Frame> frames;frames.push_back({std::vector<Value>(b.local_count),0,0});std::vector<Value>stack;
 auto pop=[&](){Value v=stack.empty()?Value{}:stack.back();if(!stack.empty())stack.pop_back();return v;};size_t pc=0;size_t budget=1000000;
 while(pc<b.instructions.size()&&budget--){
  const auto&i=b.instructions[pc++];auto bad=[&](const std::string&m){r.error="第 "+std::to_string(i.line)+" 行："+m;};
  if((i.op==Op::PushInt||i.op==Op::PushFloat||i.op==Op::PushString||i.op==Op::PushBool)&&(i.operand<0||(size_t)i.operand>=b.constants.size())){bad("常量索引越界");return r;}
  switch(i.op){
   case Op::PushInt:stack.emplace_back(b.constants[i.operand].integer);break;
   case Op::PushFloat:stack.emplace_back(b.constants[i.operand].real);break;
   case Op::PushString:stack.emplace_back(b.constants[i.operand].text);break;
   case Op::PushBool:stack.emplace_back(b.constants[i.operand].boolean);break;
   case Op::LoadLocal:if(i.operand<0||(size_t)i.operand>=frames.back().locals.size()){bad("局部变量索引越界");return r;}stack.push_back(frames.back().locals[i.operand]);break;
   case Op::StoreLocal:if(i.operand<0||(size_t)i.operand>=frames.back().locals.size()||stack.empty()){bad("变量存储无效");return r;}frames.back().locals[i.operand]=pop();break;
   case Op::Neg:{Value a=pop();bool ok=true;double x=number(a,ok);if(!ok){bad("负号只能用于数字");return r;}stack.emplace_back(-x);break;}
   case Op::Not:stack.emplace_back(!truth(pop()));break;
   case Op::Jump:pc=(size_t)i.operand;break;
   case Op::JumpIfFalse:if(!truth(pop()))pc=(size_t)i.operand;break;
   case Op::Call:{if(frames.size()>=1024){bad("函数调用层级超过 1024");return r;}if(i.operand<0||(size_t)i.operand>=b.functions.size()){bad("函数索引越界");return r;}const auto&fn=b.functions[(size_t)i.operand];if(fn.parameter_count>stack.size()||fn.local_count<fn.parameter_count){bad("函数调用参数栈错误");return r;}std::vector<Value>args(fn.parameter_count);for(int n=(int)fn.parameter_count-1;n>=0;--n)args[(size_t)n]=pop();Frame frame;frame.locals.resize(fn.local_count);for(size_t n=0;n<args.size();++n)frame.locals[n]=std::move(args[n]);frame.return_pc=pc;frame.stack_base=stack.size();frames.push_back(std::move(frame));pc=fn.entry;break;}
   case Op::Return:{if(frames.size()<=1){bad("主程序不能执行返回指令");return r;}Value result=i.operand?pop():Value{};size_t return_pc=frames.back().return_pc,base=frames.back().stack_base;frames.pop_back();if(stack.size()>base)stack.resize(base);stack.push_back(std::move(result));pc=return_pc;break;}
   case Op::Print:{if(i.operand<0||(size_t)i.operand>stack.size()){bad("打印参数栈错误");return r;}std::vector<Value>args((size_t)i.operand);for(int n=i.operand-1;n>=0;--n)args[(size_t)n]=pop();for(size_t n=0;n<args.size();++n){if(n)r.output+=" ";r.output+=show(args[n]);}r.output+="\n";break;}
   case Op::HostCall:{if(i.operand<0||(size_t)i.operand+1>stack.size()){bad("宿主调用参数栈错误");return r;}std::vector<Value>args((size_t)i.operand);for(int n=i.operand-1;n>=0;--n)args[(size_t)n]=pop();Value fn=pop();auto name=std::get_if<std::string>(&fn);if(!name){bad("宿主函数名称无效");return r;}if(!host){bad("当前运行环境没有注册 Godot 宿主 API");return r;}Value result;std::string error;if(!host(*name,args,result,error)){bad(error.empty()?"宿主 API 调用失败":error);return r;}stack.push_back(std::move(result));break;}
   case Op::Pop:pop();break;
   case Op::Halt:r.ok=true;return r;
   default:{if(stack.size()<2){bad("表达式操作数不足");return r;}Value right=pop(),left=pop();bool ok=true;double a=number(left,ok),c=number(right,ok);switch(i.op){
    case Op::Add:if(auto x=std::get_if<std::string>(&left)){if(auto y=std::get_if<std::string>(&right))stack.emplace_back(*x+*y);else{bad("字符串只能与字符串相加");return r;}}else if(ok)stack.emplace_back(a+c);else{bad("加法需要两个数字或两个字符串");return r;}break;
    case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:if(!ok){bad("算术运算需要数字");return r;}if((i.op==Op::Div||i.op==Op::Mod)&&c==0){bad("除数不能为零");return r;}if(i.op==Op::Sub)stack.emplace_back(a-c);else if(i.op==Op::Mul)stack.emplace_back(a*c);else if(i.op==Op::Div)stack.emplace_back(a/c);else stack.emplace_back(std::fmod(a,c));break;
    case Op::Eq:stack.emplace_back(show(left)==show(right));break;case Op::Ne:stack.emplace_back(show(left)!=show(right));break;
    case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:if(!ok){bad("比较运算需要数字");return r;}if(i.op==Op::Lt)stack.emplace_back(a<c);else if(i.op==Op::Le)stack.emplace_back(a<=c);else if(i.op==Op::Gt)stack.emplace_back(a>c);else stack.emplace_back(a>=c);break;
    default:bad("遇到未知指令");return r;
   }break;}
  }
 }
 if(!budget){r.error="虚拟机执行预算耗尽";return r;}r.error="程序未正常结束";return r;
}
}
