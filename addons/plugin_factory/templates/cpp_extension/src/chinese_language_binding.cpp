#include "chinese_language_binding.hpp"

#include "chinese_compiler.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void ChineseLanguage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("compile", "source"), &ChineseLanguage::compile);
}

Dictionary ChineseLanguage::compile(const String &source) const {
    const CharString utf8 = source.utf8();
    chinese::Compiler compiler;
    const chinese::CompileResult result = compiler.compile(std::string(utf8.get_data()));

    Dictionary response;
    response["ok"] = result.ok;
    response["code"] = String::utf8(result.generated_code.c_str());
    response["error"] = String::utf8(result.error.c_str());
    return response;
}
