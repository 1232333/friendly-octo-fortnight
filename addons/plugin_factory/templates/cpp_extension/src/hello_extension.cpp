#include "hello_extension.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void HelloNative::_bind_methods() {
    ClassDB::bind_method(D_METHOD("greeting"), &HelloNative::greeting);
}

String HelloNative::greeting() const {
    return String::utf8("C++ 扩展已成功加载。下一步将用 C++ 实现中文编程语言。");
}
