#include "chinese_language_binding.hpp"
#include "chinese_frontend.hpp"
#include "chinese_vm.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;
void ChineseLanguage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("compile", "source"), &ChineseLanguage::compile);
    ClassDB::bind_method(D_METHOD("run", "bytecode"), &ChineseLanguage::run);
}
Dictionary ChineseLanguage::compile(const String &source) const {
    const CharString utf8 = source.utf8();
    chinese::Frontend frontend;
    const chinese::FrontendResult result = frontend.compile(std::string(utf8.get_data(), (size_t)utf8.length()));
    Dictionary response;
    response["ok"] = result.ok;
    response["error"] = String::utf8(result.error.c_str());
    if (result.ok) {
        const std::vector<uint8_t> bytes = result.bytecode.encode();
        PackedByteArray packed;
        packed.resize((int64_t)bytes.size());
        for (int64_t i = 0; i < (int64_t)bytes.size(); ++i) packed.set(i, bytes[(size_t)i]);
        response["bytecode"] = packed;
        response["format"] = "ZHBC";
        response["version"] = 1;
    }
    return response;
}
Dictionary ChineseLanguage::run(const PackedByteArray &bytecode) const {
    std::vector<uint8_t> bytes;
    bytes.reserve((size_t)bytecode.size());
    for (int64_t i = 0; i < bytecode.size(); ++i) bytes.push_back(bytecode[i]);
    chinese::Bytecode decoded;
    std::string error;
    chinese::VmResult result;
    if (chinese::Bytecode::decode(bytes, decoded, error)) {
        chinese::VirtualMachine vm;
        result = vm.run(decoded);
    } else {
        result.error = error;
    }
    Dictionary response;
    response["ok"] = result.ok;
    response["output"] = String::utf8(result.output.c_str());
    response["error"] = String::utf8(result.error.c_str());
    if (result.ok && !result.output.empty()) UtilityFunctions::print(String::utf8(result.output.c_str()));
    return response;
}
