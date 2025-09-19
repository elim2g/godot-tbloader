#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

class SecureStore : public RefCounted
{
    GDCLASS(SecureStore, RefCounted);

protected:
    static void _bind_methods();

public:
    bool save(const String& key, const PackedByteArray &data);
    PackedByteArray load(const String& key);
    bool erase(const String& key);
};