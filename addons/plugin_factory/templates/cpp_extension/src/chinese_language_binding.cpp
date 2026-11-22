#include "chinese_language_binding.hpp"
#include "chinese_frontend.hpp"
#include "chinese_vm.hpp"
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node2d.hpp>
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
    ClassDB::bind_method(D_METHOD("run", "bytecode", "scene_root"), &ChineseLanguage::run);
}
Dictionary ChineseLanguage::compile(const String &source) const {
    const CharString utf8=source.utf8();
    chinese::Frontend frontend;
    const chinese::FrontendResult result=frontend.compile(std::string(utf8.get_data(),(size_t)utf8.length()));
    Dictionary response; response["ok"]=result.ok; response["error"]=String::utf8(result.error.c_str());
    if(result.ok){const std::vector<uint8_t> bytes=result.bytecode.encode();PackedByteArray packed;packed.resize((int64_t)bytes.size());for(int64_t i=0;i<(int64_t)bytes.size();++i)packed.set(i,bytes[(size_t)i]);response["bytecode"]=packed;response["format"]="ZHBC";response["version"]=1;}
    return response;
}
Dictionary ChineseLanguage::run(const PackedByteArray &bytecode,Node *scene_root) const {
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
        chinese::VirtualMachine vm;result=vm.run(decoded,host);
    }else result.error=error;
    Dictionary response;response["ok"]=result.ok;response["output"]=String::utf8(result.output.c_str());response["error"]=String::utf8(result.error.c_str());
    if(result.ok&&!result.output.empty())UtilityFunctions::print(String::utf8(result.output.c_str()));
    return response;
}
