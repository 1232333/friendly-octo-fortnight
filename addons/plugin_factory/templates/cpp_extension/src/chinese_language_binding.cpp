#include "chinese_language_binding.hpp"
#include "chinese_frontend.hpp"
#include "chinese_vm.hpp"
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;
namespace {
bool to_vm_value(const Variant &v,chinese::Value &out) {
    switch(v.get_type()) {
        case Variant::BOOL: out=(bool)v; return true;
        case Variant::INT: out=(int64_t)v; return true;
        case Variant::FLOAT: out=(double)v; return true;
        case Variant::STRING: out=std::string(String(v).utf8().get_data()); return true;
        default: return false;
    }
}
Variant to_variant(const chinese::Value &v) {
    if(auto p=std::get_if<bool>(&v)) return *p;
    if(auto p=std::get_if<int64_t>(&v)) return (int64_t)*p;
    if(auto p=std::get_if<double>(&v)) return *p;
    if(auto p=std::get_if<std::string>(&v)) return String::utf8(p->c_str());
    return Variant();
}
Object *resolve_node(const chinese::Value &v) {
    const auto *h=std::get_if<chinese::ObjectHandle>(&v);
    return h ? reinterpret_cast<Object*>(static_cast<uintptr_t>(h->id)) : nullptr;
}
}
void ChineseLanguage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("compile", "source"), &ChineseLanguage::compile);
    ClassDB::bind_method(D_METHOD("run", "bytecode", "scene_root", "event_name", "delta"), &ChineseLanguage::run, DEFVAL(String()), DEFVAL(0.0));
}
Dictionary ChineseLanguage::compile(const String &source) const {
    const CharString utf8=source.utf8();
    chinese::Frontend frontend;
    const chinese::FrontendResult result=frontend.compile(std::string(utf8.get_data(),(size_t)utf8.length()));
    Dictionary response; response["ok"]=result.ok; response["error"]=String::utf8(result.error.c_str());
    if(result.ok){const std::vector<uint8_t> bytes=result.bytecode.encode();PackedByteArray packed;packed.resize((int64_t)bytes.size());for(int64_t i=0;i<(int64_t)bytes.size();++i)packed.set(i,bytes[(size_t)i]);response["bytecode"]=packed;response["format"]="ZHBC";response["version"]=2;}
    return response;
}
Dictionary ChineseLanguage::run(const PackedByteArray &bytecode,Node *scene_root,const String &event_name,double delta) const {
    std::vector<uint8_t> bytes;bytes.reserve((size_t)bytecode.size());for(int64_t i=0;i<bytecode.size();++i)bytes.push_back(bytecode[i]);
    chinese::Bytecode decoded;std::string error;chinese::VmResult result;
    if(chinese::Bytecode::decode(bytes,decoded,error)){
        chinese::HostFunction host=[scene_root](const std::string &name,const std::vector<chinese::Value>&args,chinese::Value&out,std::string&err)->bool{
            auto count=[&](size_t n){if(args.size()!=n){err="宿主函数“"+name+"”参数数量错误";return false;}return true;};
            if(name=="获取节点"){
                if(!count(1))return false;const auto *path=std::get_if<std::string>(&args[0]);if(!path){err="获取节点需要字符串路径";return false;}if(!scene_root){err="没有提供当前场景根节点";return false;}Node *node=scene_root->get_node_or_null(NodePath(String::utf8(path->c_str())));if(!node){err="找不到节点："+*path;return false;}out=chinese::ObjectHandle{(uint64_t)reinterpret_cast<uintptr_t>(node)};return true;
            }
            if(name=="设置可见"){
                if(!count(2))return false;Object *obj=resolve_node(args[0]);const auto *visible=std::get_if<bool>(&args[1]);if(!obj||!visible){err="设置可见需要有效节点和布尔值";return false;}CanvasItem *item=Object::cast_to<CanvasItem>(obj);if(!item){err="目标节点不是 CanvasItem";return false;}item->set_visible(*visible);out=true;return true;
            }
            if(name=="设置位置"){
                if(!count(3))return false;Object *obj=resolve_node(args[0]);bool ok=true;double x=0,y=0;if(auto p=std::get_if<int64_t>(&args[1]))x=(double)*p;else if(auto p=std::get_if<double>(&args[1]))x=*p;else ok=false;if(auto p=std::get_if<int64_t>(&args[2]))y=(double)*p;else if(auto p=std::get_if<double>(&args[2]))y=*p;else ok=false;if(!obj||!ok){err="设置位置需要节点和两个数字";return false;}if(auto n=Object::cast_to<Node2D>(obj))n->set_position(Vector2((real_t)x,(real_t)y));else if(auto c=Object::cast_to<Control>(obj))c->set_position(Vector2((real_t)x,(real_t)y));else{err="目标节点不支持二维位置";return false;}out=true;return true;
            }
            if(name=="设置属性"){
                if(!count(3))return false;Object *obj=resolve_node(args[0]);const auto *property=std::get_if<std::string>(&args[1]);if(!obj||!property){err="设置属性需要节点和属性名称";return false;}Variant value=to_variant(args[2]);obj->set(StringName(String::utf8(property->c_str())),value);out=true;return true;
            }
            if(name=="读取属性"){
                if(!count(2))return false;Object *obj=resolve_node(args[0]);const auto *property=std::get_if<std::string>(&args[1]);if(!obj||!property){err="读取属性需要节点和属性名称";return false;}Variant value=obj->get(StringName(String::utf8(property->c_str())));if(!to_vm_value(value,out)){err="该属性类型暂不支持传回中文 VM";return false;}return true;
            }
            err="未知的 Godot 宿主函数："+name;return false;
        };
        int entry=-1;std::vector<chinese::Value> event_args;if(!event_name.is_empty()){const std::string wanted="@event:"+std::string(event_name.utf8().get_data());for(size_t i=0;i<decoded.functions.size();++i)if(decoded.functions[i].name==wanted){entry=(int)i;break;}if(entry<0){result.error="找不到事件入口："+std::string(event_name.utf8().get_data());}else{const auto&fn=decoded.functions[(size_t)entry];if(fn.parameter_count==1)event_args.emplace_back(delta);else if(fn.parameter_count!=0)result.error="事件入口参数数量不受支持";}}if(result.error.empty()){chinese::VirtualMachine vm;result=vm.run(decoded,host,entry,event_args);}
    }else result.error=error;
    Dictionary response;response["ok"]=result.ok;response["output"]=String::utf8(result.output.c_str());response["error"]=String::utf8(result.error.c_str());
    if(result.ok&&!result.output.empty())UtilityFunctions::print(String::utf8(result.output.c_str()));
    return response;
}


void ChineseProgram::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bytecode", "value"), &ChineseProgram::set_bytecode);
    ClassDB::bind_method(D_METHOD("get_bytecode"), &ChineseProgram::get_bytecode);
    ClassDB::bind_method(D_METHOD("set_bytecode_path", "value"), &ChineseProgram::set_bytecode_path);
    ClassDB::bind_method(D_METHOD("get_bytecode_path"), &ChineseProgram::get_bytecode_path);
    ClassDB::bind_method(D_METHOD("load_bytecode", "path"), &ChineseProgram::load_bytecode);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytecode"), "set_bytecode", "get_bytecode");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "bytecode_path", PROPERTY_HINT_FILE, "*.zhbc"), "set_bytecode_path", "get_bytecode_path");
}
void ChineseProgram::set_bytecode(const PackedByteArray &value) {
    bytecode = value;
    if (is_inside_tree()) _initialize_runtime();
}
PackedByteArray ChineseProgram::get_bytecode() const { return bytecode; }
void ChineseProgram::set_bytecode_path(const String &value) { bytecode_path = value; if (is_inside_tree()) _initialize_runtime(); }
String ChineseProgram::get_bytecode_path() const { return bytecode_path; }
bool ChineseProgram::load_bytecode(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) return false;
    set_bytecode(file->get_buffer(file->get_length()));
    return true;
}
void ChineseProgram::_initialize_runtime() {
    set_process(false);
    has_process_event = false;
    if (bytecode.is_empty() && !bytecode_path.is_empty()) {
        Ref<FileAccess> file = FileAccess::open(bytecode_path, FileAccess::READ);
        if (file.is_null()) {
            UtilityFunctions::push_error(String("无法读取中文字节码文件：") + bytecode_path);
            return;
        }
        bytecode = file->get_buffer(file->get_length());
    }
    if (bytecode.is_empty()) {
        UtilityFunctions::push_error("中文程序没有可执行的 ZHBC 字节码");
        return;
    }
    SceneTree *tree = get_tree();
    Node *scene_root = tree != nullptr ? tree->get_current_scene() : nullptr;
    if (scene_root == nullptr) scene_root = this;
    std::vector<uint8_t> bytes;
    bytes.reserve((size_t)bytecode.size());
    for (int64_t i = 0; i < bytecode.size(); ++i) bytes.push_back(bytecode[i]);
    chinese::Bytecode decoded;
    std::string error;
    if (!chinese::Bytecode::decode(bytes, decoded, error)) {
        UtilityFunctions::push_error(String::utf8(error.c_str()));
        return;
    }
    bool has_ready_event = false;
    for (const auto &fn : decoded.functions) {
        if (fn.name == "@event:游戏开始") has_ready_event = true;
        if (fn.name == "@event:每帧") has_process_event = true;
    }
    if (runtime.is_null()) runtime.instantiate();
    if (has_ready_event) {
        Dictionary result = runtime->run(bytecode, scene_root, String("游戏开始"));
        if (!bool(result.get("ok", false))) {
            UtilityFunctions::push_error(String("中文事件“游戏开始”执行失败：") + String(result.get("error", "")));
        }
    }
    set_process(has_process_event);
}
void ChineseProgram::_ready() { _initialize_runtime(); }
void ChineseProgram::_process(double delta) {
    if (!has_process_event || runtime.is_null()) return;
    SceneTree *tree = get_tree();
    Node *scene_root = tree != nullptr ? tree->get_current_scene() : nullptr;
    if (scene_root == nullptr) scene_root = this;
    Dictionary result = runtime->run(bytecode, scene_root, String("每帧"), delta);
    if (!bool(result.get("ok", false))) {
        UtilityFunctions::push_error(String("中文事件“每帧”执行失败：") + String(result.get("error", "")));
        set_process(false);
    }
}
