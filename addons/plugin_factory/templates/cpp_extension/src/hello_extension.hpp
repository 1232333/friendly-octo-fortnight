#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class HelloNative : public RefCounted {
    GDCLASS(HelloNative, RefCounted)

protected:
    static void _bind_methods();

public:
    String greeting() const;
};

} // namespace godot
