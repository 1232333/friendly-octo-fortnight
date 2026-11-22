#include "chinese_frontend.hpp"
#include "chinese_vm.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
static void require(bool v,const char*m){if(!v){std::cerr<<"FAILED: "<<m<<"\n";std::exit(1);}}
int main(){
 chinese::Frontend f;chinese::VirtualMachine vm;
 const std::string src="变量 分数: 整数 = 80\n打印(\"分数：\", 分数)\n如果 分数 >= 60:\n    打印(\"及格\")\n否则:\n    打印(\"不及格\")\n结束\n";
 auto c=f.compile(src);if(!c.ok)std::cerr<<"COMPILE ERROR: "<<c.error<<"\n";require(c.ok,"compile Chinese source");
 auto bytes=c.bytecode.encode();require(bytes.size()>6&&bytes[0]=='Z'&&bytes[1]=='H',"emits native ZHBC");
 chinese::Bytecode decoded;std::string err;require(chinese::Bytecode::decode(bytes,decoded,err),"decode bytecode");
 auto r=vm.run(decoded);require(r.ok,"VM executes bytecode");require(r.output=="分数： 80\n及格\n","expected VM output");

 auto fn_source=std::string("函数 加法(a: 整数, b: 整数) -> 整数:\n返回 a + b\n结束\n变量 结果 = 加法(20, 22)\n打印(结果)\n");
 auto fn_compiled=f.compile(fn_source);if(!fn_compiled.ok)std::cerr<<"FUNCTION COMPILE ERROR: "<<fn_compiled.error<<"\n";require(fn_compiled.ok,"compile typed native function");
 auto fn_bytes=fn_compiled.bytecode.encode();chinese::Bytecode fn_decoded;require(chinese::Bytecode::decode(fn_bytes,fn_decoded,err),"decode function bytecode");require(fn_decoded.functions.size()==1,"function table serialized");auto fn_result=vm.run(fn_decoded);require(fn_result.ok,"VM executes native function call");require(fn_result.output=="42\n","function returns value to caller");
 auto bad_fn_args=f.compile("函数 求和(a: 整数) -> 整数:\n返回 a\n结束\n打印(求和(1, 2))\n");require(!bad_fn_args.ok,"function argument count checked");
 auto bad_fn_type=f.compile("函数 求和(a: 整数) -> 整数:\n返回 a\n结束\n打印(求和(\"文字\"))\n");require(!bad_fn_type.ok,"function argument type checked");
 auto forward=f.compile("函数 先调用() -> 整数:\n返回 后续(41)\n结束\n函数 后续(x: 整数) -> 整数:\n返回 x + 1\n结束\n打印(先调用())\n");require(forward.ok,"forward function call compiles");auto forward_result=vm.run(forward.bytecode);require(forward_result.ok&&forward_result.output=="42\n","forward function call executes");
 auto missing_return=f.compile("函数 不完整(x: 整数) -> 整数:\n如果 x > 0:\n返回 x\n结束\n结束\n");require(!missing_return.ok,"non-void function must return on every path");
 auto both_return=f.compile("函数 取值(x: 整数) -> 整数:\n如果 x > 0:\n返回 x\n否则:\n返回 0\n结束\n结束\n打印(取值(9))\n");require(both_return.ok,"both conditional branches return");auto both_result=vm.run(both_return.bytecode);require(both_result.ok&&both_result.output=="9\n","conditional return execution");
 auto recursive=f.compile("函数 阶乘(n: 整数) -> 整数:\n如果 n <= 1:\n返回 1\n否则:\n返回 n * 阶乘(n - 1)\n结束\n结束\n打印(阶乘(6))\n");require(recursive.ok,"self-recursive function compiles");auto recursive_result=vm.run(recursive.bytecode);require(recursive_result.ok&&recursive_result.output=="720\n","self-recursive function executes with isolated call frames");
 auto mutual=f.compile("函数 是偶数(n: 整数) -> 布尔:\n如果 n == 0:\n返回 真\n否则:\n返回 是奇数(n - 1)\n结束\n结束\n函数 是奇数(n: 整数) -> 布尔:\n如果 n == 0:\n返回 假\n否则:\n返回 是偶数(n - 1)\n结束\n结束\n打印(是偶数(10), 是奇数(7))\n");require(mutual.ok,"mutually recursive functions compile");auto mutual_result=vm.run(mutual.bytecode);require(mutual_result.ok&&mutual_result.output=="真 真\n","mutually recursive functions execute");
 auto isolated=f.compile("函数 取甲() -> 整数:\n变量 临时值 = 11\n返回 临时值\n结束\n函数 取乙() -> 整数:\n变量 临时值 = 22\n返回 临时值\n结束\n打印(取甲(), 取乙(), 取甲())\n");require(isolated.ok,"same local variable names are allowed in separate functions");auto isolated_result=vm.run(isolated.bytecode);require(isolated_result.ok&&isolated_result.output=="11 22 11\n","function locals remain isolated across calls");
 auto wrong_integer=f.compile("变量 生命值: 整数 = \"满血\"\n");require(!wrong_integer.ok,"string cannot initialize integer");
 auto wrong_type=f.compile("变量 标记: 颜色 = 3\n");require(!wrong_type.ok,"unknown type name rejected");
 auto wrong_math=f.compile("变量 结果 = \"文字\" - 2\n");require(!wrong_math.ok,"invalid arithmetic types rejected");
 auto wrong_condition=f.compile("如果 123:\n打印(1)\n结束\n");require(!wrong_condition.ok,"if condition must be boolean");
 auto inferred=f.compile("变量 总分 = 10 + 20\n变量 名称: 字符串 = \"测试\"\n打印(总分, 名称)\n");require(inferred.ok,"type inference and valid annotations accepted");
 auto unknown=f.compile("打印(不存在)\n");require(!unknown.ok,"undeclared variable rejected");
 auto malformed=f.compile("打印(\"坏字符串)\n");require(!malformed.ok,"unterminated string rejected");
 auto div=f.compile("打印(8 / 0)\n");require(div.ok,"division program compiles");require(!vm.run(div.bytecode).ok,"division by zero rejected at runtime");
 auto host_program=f.compile("变量 按钮 = 获取节点(\"UI/按钮\")\n设置可见(按钮, 假)\n打印(\"节点已处理\")\n");
 require(host_program.ok,"parse Godot host API calls");
 int calls=0;
 chinese::HostFunction host=[&](const std::string&name,const std::vector<chinese::Value>&args,chinese::Value&out,std::string&error){
  if(name=="获取节点"){++calls;if(args.size()!=1||!std::holds_alternative<std::string>(args[0])){error="bad path";return false;}out=chinese::ObjectHandle{123};return true;}
  if(name=="设置可见"){++calls;if(args.size()!=2||!std::holds_alternative<chinese::ObjectHandle>(args[0])||!std::holds_alternative<bool>(args[1])){error="bad visibility args";return false;}require(!std::get<bool>(args[1]),"false literal passed to host");out=true;return true;}
  error="unknown host call";return false;
 };
 auto host_result=vm.run(host_program.bytecode,host);require(host_result.ok,"VM dispatches host calls");require(calls==2,"host functions called in order");require(host_result.output=="节点已处理\n","host call result continues execution");
 auto bad_host=f.compile("不存在的宿主函数()\n");require(bad_host.ok,"unknown host call compiles");require(!vm.run(bad_host.bytecode,host).ok,"unknown host function rejected by host");
 auto corrupt=bytes;corrupt[0]='X';chinese::Bytecode invalid;require(!chinese::Bytecode::decode(corrupt,invalid,err),"invalid bytecode header rejected");
 std::cout<<"Native Chinese bytecode VM tests passed.\n";return 0;
}
