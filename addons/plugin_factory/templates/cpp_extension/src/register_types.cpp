#include "hello_extension.hpp"
#include "chinese_language_binding.hpp"

#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_chinese_native(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<HelloNative>();
    ClassDB::register_class<ChineseLanguage>();
}

void uninitialize_chinese_native(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT chinese_native_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization
) {
    GDExtensionBinding::InitObject init_obj(get_proc_address, library, initialization);
    init_obj.register_initializer(initialize_chinese_native);
    init_obj.register_terminator(uninitialize_chinese_native);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
