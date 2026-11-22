#include "chinese_compiler.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    chinese::Compiler compiler;
    const auto result = compiler.compile(
        "变量 分数 = 80\n"
        "打印(分数)\n"
        "如果 分数 >= 60:\n"
        "打印(\"及格\")\n"
        "否则:\n"
        "打印(\"不及格\")\n"
        "结束\n"
    );

    require(result.ok, "valid Chinese source compiles");
    require(result.generated_code.find("var _zh_") != std::string::npos, "Chinese identifier is mangled to safe ASCII");
    require(result.generated_code.find("if _zh_") != std::string::npos, "如果 becomes if");
    require(result.generated_code.find("else:") != std::string::npos, "否则 becomes else");
    require(result.generated_code.find("print") != std::string::npos, "打印 becomes print");

    const auto unmatched = compiler.compile("结束\n");
    require(!unmatched.ok, "unmatched 结束 is rejected");

    const auto unclosed = compiler.compile("如果 真:\n打印(\"x\")\n");
    require(!unclosed.ok, "unclosed block is rejected");

    const auto bad_string = compiler.compile("打印(\"未结束)\n");
    require(!bad_string.ok, "unclosed string is rejected");

    std::cout << "Chinese compiler MVP tests passed.\n";
    return 0;
}
