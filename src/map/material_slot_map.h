#pragma once

#include <gdextension_interface.h>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/defs.hpp>

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

class MaterialSlotMap : public Node
{
    GDCLASS(MaterialSlotMap, Node);

public:
    PackedStringArray material_slots;

protected:
    static void _bind_methods();

public:
    PackedStringArray get_material_slots() const;

public:
    int64_t get_slot_index_for_name(const String& in_name) const;
};