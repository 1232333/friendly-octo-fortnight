#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
namespace godot {
class ChineseLanguage : public RefCounted {
    GDCLASS(ChineseLanguage, RefCounted)
protected:
    static void _bind_methods();
public:
    Dictionary compile(const String &source) const;
    Dictionary run(const PackedByteArray &bytecode) const;
};
}
