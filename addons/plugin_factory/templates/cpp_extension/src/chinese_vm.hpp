#pragma once
#include "chinese_frontend.hpp"
#include <string>
namespace chinese {
struct VmResult { bool ok=false; std::string output; std::string error; };
class VirtualMachine { public: VmResult run(const Bytecode&) const; };
}
