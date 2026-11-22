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

    const auto decimal = compiler.compile("变量 圆周率 = 3.14159\n");
    require(decimal.ok, "decimal literal compiles");
    require(decimal.generated_code.find("3.14159") != std::string::npos, "decimal literal remains intact");

    const auto function = compiler.compile("函数 问候(名字):\n打印(名字)\n结束\n");
    require(function.ok, "Chinese function declaration compiles");
    require(function.generated_code.find("func _zh_") != std::string::npos, "函数 becomes func");

    const auto game_events = compiler.compile(
        "当 游戏开始:\n"
        "打印(\"开始\")\n"
        "结束\n"
        "当 每帧(delta):\n"
        "自身.position.x += 100 * delta\n"
        "结束\n"
    );
    require(game_events.ok, "game lifecycle event handlers compile");
    require(game_events.generated_code.find("func _ready():") != std::string::npos, "游戏开始 maps to _ready");
    require(game_events.generated_code.find("func _process(delta):") != std::string::npos, "每帧 maps to _process");
    require(game_events.generated_code.find("self . position . x") != std::string::npos, "自身 maps to self");

    const auto unmatched = compiler.compile("结束\n");
    require(!unmatched.ok, "unmatched 结束 is rejected");

    const auto unclosed = compiler.compile("如果 真:\n打印(\"x\")\n");
    require(!unclosed.ok, "unclosed block is rejected");

    const auto bad_string = compiler.compile("打印(\"未结束)\n");
    require(!bad_string.ok, "unclosed string is rejected");

    std::cout << "Chinese compiler MVP tests passed.\n";
    return 0;
}
