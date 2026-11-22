#pragma once
#include "chinese_frontend.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>
namespace chinese {
struct ObjectHandle { uint64_t id=0; };
using Value = std::variant<std::monostate,int64_t,double,std::string,bool,ObjectHandle>;
using HostFunction = std::function<bool(const std::string&,const std::vector<Value>&,Value&,std::string&)>;
struct VmResult { bool ok=false; std::string output; std::string error; };
class VirtualMachine { public: VmResult run(const Bytecode&, const HostFunction& host={}) const; };
}
