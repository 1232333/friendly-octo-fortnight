#pragma once

#include <string>

namespace chinese {

struct CompileResult {
    bool ok = false;
    std::string generated_code;
    std::string error;
};

class Compiler {
public:
    CompileResult compile(const std::string &source) const;
};

} // namespace chinese
