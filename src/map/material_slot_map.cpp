#include <material_slot_map.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

/*static*/ void MaterialSlotMap::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_material_slots"), &MaterialSlotMap::get_material_slots);
    ClassDB::bind_method(D_METHOD("get_slot_index_for_name", "in_name"), &MaterialSlotMap::get_slot_index_for_name);
    ClassDB::bind_method(D_METHOD("replace_material_from_tex_name", "in_name", "in_material"), &MaterialSlotMap::replace_material_from_tex_name);

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

void MaterialSlotMap::replace_material_from_tex_name(const String& in_name, Ref<Material> in_material)
{
    const int64_t slot_idx = material_slots.find(in_name);
    if (slot_idx == -1)
    {
        return;
    }

    Node* parent_node = get_parent();
    if (parent_node == nullptr)
    {
        UtilityFunctions::printerr("[MaterialSlotMap] parent Node is nullptr!");
        return;
    }

    if (parent_node->get_class() != "MeshInstance3D")
    {
        UtilityFunctions::printerr("[MaterialSlotMap] parent Node is not an instance of MeshInstance3D!");
        return;
    }

    MeshInstance3D* mesh_instance = static_cast<MeshInstance3D*>(parent_node);
    const int32_t surface_slot_count = mesh_instance->get_surface_override_material_count();
    if (slot_idx >= mesh_instance->get_surface_override_material_count())
    {
        UtilityFunctions::printerr("[MaterialSlotMap] parent MeshInstance3D only has ", surface_slot_count, " slots, but slot_idx is ", slot_idx);
        return;
    }

    mesh_instance->set_surface_override_material(slot_idx, in_material);
}