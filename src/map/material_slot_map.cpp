#include <material_slot_map.h>

#include <godot_cpp/core/class_db.hpp>

/*static*/ void MaterialSlotMap::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_material_slots"), &MaterialSlotMap::get_material_slots);
    ClassDB::bind_method(D_METHOD("get_slot_index_for_name"), &MaterialSlotMap::get_slot_index_for_name);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "material_slots", PROPERTY_HINT_NONE, "Material Name Assigned To Slot"), "", "get_material_slots");
}

PackedStringArray MaterialSlotMap::get_material_slots() const
{
    return material_slots;
}

int64_t MaterialSlotMap::get_slot_index_for_name(const String& in_name) const
{
    for (uint64_t i = 0; i < material_slots.size(); ++i)
    {
        if (material_slots[i] == in_name)
        {
            return i;
        }
    }

    return -1;
}

void MaterialSlotMap::add_slot(const String& in_name)
{
    material_slots.append(in_name);
}