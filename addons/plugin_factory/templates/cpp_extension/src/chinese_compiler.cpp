#include "chinese_compiler.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace chinese {
namespace {

struct Token {
    std::string value;
    bool quoted = false;
};

bool is_ascii_word(unsigned char c) {
    return std::isalnum(c) || c == '_';
}

std::vector<Token> tokenize(const std::string &line, std::string &error) {
    std::vector<Token> tokens;
    for (std::size_t i = 0; i < line.size();) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if (std::isspace(c)) {
            ++i;
            continue;
        }
        if (line[i] == '#') {
            break;
        }
        if (line[i] == '"' || line[i] == '\'') {
            const char quote = line[i];
            const std::size_t start = i++;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    i += 2;
                    continue;
                }
                if (line[i++] == quote) {
                    closed = true;
                    break;
                }
            }
            if (!closed) {
                error = "字符串没有闭合";
                return {};
            }
            tokens.push_back({line.substr(start, i - start), true});
            continue;
        }
        if (is_ascii_word(c) || c >= 0x80) {
            const std::size_t start = i++;
            while (i < line.size()) {
                const unsigned char next = static_cast<unsigned char>(line[i]);
                if (!is_ascii_word(next) && next < 0x80) {
                    break;
                }
                ++i;
            }
            tokens.push_back({line.substr(start, i - start), false});
            continue;
        }
        if (i + 1 < line.size()) {
            const std::string pair = line.substr(i, 2);
            if (pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" ||
                pair == "&&" || pair == "||" || pair == "+=" || pair == "-=" ||
                pair == "*=" || pair == "/=") {
                tokens.push_back({pair, false});
                i += 2;
                continue;
            }
        }
        tokens.push_back({line.substr(i, 1), false});
        ++i;
    }
    return tokens;
}

bool is_chinese_identifier(const std::string &value) {
    for (unsigned char c : value) {
        if (c >= 0x80) {
            return true;
        }
    }
    return false;
}

std::string mangle_identifier(const std::string &value) {
    if (!is_chinese_identifier(value)) {
        return value;
    }
    std::ostringstream output;
    output << "_zh_";
    for (unsigned char c : value) {
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(c);
    }
    return output.str();
}

std::string expression(const std::vector<Token> &tokens, std::size_t start) {
    std::string output;
    for (std::size_t i = start; i < tokens.size(); ++i) {
        const std::string &value = tokens[i].value;
        if (!output.empty()) {
            output += ' ';
        }
        if (tokens[i].quoted) {
            output += value;
        } else if (value == "真") {
            output += "true";
        } else if (value == "假") {
            output += "false";
        } else if (value == "空") {
            output += "null";
        } else if (value == "并且") {
            output += "and";
        } else if (value == "或者") {
            output += "or";
        } else if (value == "不") {
            output += "not";
        } else if (value == "&&") {
            output += "and";
        } else if (value == "||") {
            output += "or";
        } else if (value == "!=") {
            output += "!=";
        } else {
            output += mangle_identifier(value);
        }
    }
    return output;
}

std::string trim(const std::string &value) {
    const std::size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

} // namespace

CompileResult Compiler::compile(const std::string &source) const {
    CompileResult result;
    std::istringstream input(source);
    std::ostringstream output;
    std::vector<std::string> blocks;
    std::string line;
    int line_number = 0;
    int indent = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string clean = trim(line);
        if (clean.empty() || clean[0] == '#') {
            continue;
        }

        std::string lexical_error;
        const std::vector<Token> tokens = tokenize(clean, lexical_error);
        if (!lexical_error.empty()) {
            result.error = "第 " + std::to_string(line_number) + " 行：" + lexical_error;
            return result;
        }
        if (tokens.empty()) {
            continue;
        }

        const std::string &first = tokens[0].value;
        if (first == "结束") {
            if (blocks.empty()) {
                result.error = "第 " + std::to_string(line_number) + " 行：多余的“结束”";
                return result;
            }
            --indent;
            blocks.pop_back();
            continue;
        }

        if (first == "否则") {
            if (blocks.empty() || blocks.back() != "if") {
                result.error = "第 " + std::to_string(line_number) + " 行：“否则”前面必须有未结束的“如果”";
                return result;
            }
            output << std::string(static_cast<std::size_t>(indent - 1) * 4, ' ')
                   << "else:\n";
            continue;
        }

        output << std::string(static_cast<std::size_t>(indent) * 4, ' ');

        if (first == "如果" || first == "循环") {
            if (tokens.size() < 2) {
                result.error = "第 " + std::to_string(line_number) + " 行：条件不能为空";
                return result;
            }
            output << (first == "如果" ? "if " : "while ")
                   << expression(tokens, 1) << ":\n";
            blocks.push_back(first == "如果" ? "if" : "loop");
            ++indent;
            continue;
        }

        if (first == "函数") {
            if (tokens.size() < 4 || tokens[2].value != "(") {
                result.error = "第 " + std::to_string(line_number) + " 行：函数格式应为“函数 名称(参数):”";
                return result;
            }
            const std::string name = mangle_identifier(tokens[1].value);
            output << "func " << name << "(";
            bool first_argument = true;
            for (std::size_t i = 3; i < tokens.size() && tokens[i].value != ")"; ++i) {
                if (tokens[i].value == ",") {
                    output << ", ";
                    first_argument = true;
                } else {
                    if (!first_argument && i == 3) {
                        output << ", ";
                    }
                    output << mangle_identifier(tokens[i].value);
                    first_argument = false;
                }
            }
            output << "):\n";
            blocks.push_back("function");
            ++indent;
            continue;
        }

        if (first == "变量") {
            if (tokens.size() < 4 || tokens[2].value != "=") {
                result.error = "第 " + std::to_string(line_number) + " 行：变量格式应为“变量 名称 = 表达式”";
                return result;
            }
            output << "var " << mangle_identifier(tokens[1].value)
                   << " = " << expression(tokens, 3) << "\n";
            continue;
        }

        if (first == "打印") {
            output << "print " << expression(tokens, 1) << "\n";
            continue;
        }

        if (first == "返回") {
            output << "return";
            if (tokens.size() > 1) {
                output << " " << expression(tokens, 1);
            }
            output << "\n";
            continue;
        }

        if (tokens.size() >= 3 && tokens[1].value == "=") {
            output << mangle_identifier(tokens[0].value)
                   << " = " << expression(tokens, 2) << "\n";
            continue;
        }

        output << expression(tokens, 0) << "\n";
    }

    if (!blocks.empty()) {
        result.error = "程序结束时仍有未闭合的代码块，请补充“结束”";
        return result;
    }

    result.ok = true;
    result.generated_code = output.str();
    return result;
}

} // namespace chinese
