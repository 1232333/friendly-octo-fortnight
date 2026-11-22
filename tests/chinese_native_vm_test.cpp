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
