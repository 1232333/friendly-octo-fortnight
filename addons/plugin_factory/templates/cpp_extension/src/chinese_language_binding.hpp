#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
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
    Dictionary run(const PackedByteArray &bytecode, Node *scene_root, const String &event_name=String(), double delta=0.0) const;
};
class ChineseProgram : public Node {
    GDCLASS(ChineseProgram, Node)
    PackedByteArray bytecode;
    String bytecode_path;
    Ref<ChineseLanguage> runtime;
    bool has_process_event = false;
    void _initialize_runtime();
protected:
    static void _bind_methods();
public:
    void set_bytecode(const PackedByteArray &value);
    void set_bytecode_path(const String &value);
    String get_bytecode_path() const;
    PackedByteArray get_bytecode() const;
    bool load_bytecode(const String &path);
    void _ready() override;
    void _process(double delta) override;
};

}
