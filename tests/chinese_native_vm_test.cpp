#include "chinese_frontend.hpp"
#include "chinese_vm.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
static void require(bool v,const char*m){if(!v){std::cerr<<"FAILED: "<<m<<"\n";std::exit(1);}}
int main(){chinese::Frontend f;chinese::VirtualMachine vm;
const std::string src="变量 分数: 整数 = 80\n打印(\"分数：\", 分数)\n如果 分数 >= 60:\n    打印(\"及格\")\n否则:\n    打印(\"不及格\")\n结束\n";
auto c=f.compile(src);require(c.ok,"compile Chinese source");auto bytes=c.bytecode.encode();require(bytes.size()>6&&bytes[0]=='Z'&&bytes[1]=='H',"emits native ZHBC");chinese::Bytecode decoded;std::string err;require(chinese::Bytecode::decode(bytes,decoded,err),"decode bytecode");auto r=vm.run(decoded);require(r.ok,"VM executes bytecode");require(r.output=="分数： 80\n及格\n","expected VM output");
auto unknown=f.compile("打印(不存在)\n");require(!unknown.ok,"undeclared variable rejected");
auto malformed=f.compile("打印(\"坏字符串)\n");require(!malformed.ok,"unterminated string rejected");
auto div=f.compile("打印(8 / 0)\n");require(div.ok,"division program compiles");require(!vm.run(div.bytecode).ok,"division by zero rejected at runtime");
auto corrupt=bytes;corrupt[0]='X';chinese::Bytecode invalid;require(!chinese::Bytecode::decode(corrupt,invalid,err),"invalid bytecode header rejected");
std::cout<<"Native Chinese bytecode VM tests passed.\n";return 0;}
