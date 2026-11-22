#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace chinese {
enum class Op : uint8_t { PushInt=1, PushFloat, PushString, PushBool, LoadLocal, StoreLocal, Add, Sub, Mul, Div, Mod, Neg, Eq, Ne, Lt, Le, Gt, Ge, Not, Jump, JumpIfFalse, Print, Pop, HostCall, Call, Return, Halt };
struct Instruction { Op op{}; int32_t operand=0; int32_t line=0; };
struct Constant { enum class Kind:uint8_t { Integer=1, Float, String, Boolean }; Kind kind{}; int64_t integer=0; double real=0; bool boolean=false; std::string text; };
struct FunctionInfo { std::string name; uint32_t entry=0; uint32_t parameter_count=0; uint32_t local_count=0; };
struct Bytecode { std::vector<FunctionInfo> functions; std::vector<Constant> constants; std::vector<Instruction> instructions; uint32_t local_count=0; std::vector<uint8_t> encode() const; static bool decode(const std::vector<uint8_t>&, Bytecode&, std::string&); };
struct FrontendResult { bool ok=false; Bytecode bytecode; std::string error; };
class Frontend { public: FrontendResult compile(const std::string&) const; };
}
