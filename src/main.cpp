#include <gdextension_interface.h>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <turnt_loader.h>
#include <material_slot_map.h>
#include <secure_store.h>
#include <serialization/tnt_checkpoint.h>
#include <serialization/tnt_player_state.h>
#include <serialization/debug_player_state.h>

using namespace godot;

void register_libturnt_types(ModuleInitializationLevel p_level)
{
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<TurntLoader>();
	ClassDB::register_class<MaterialSlotMap>();
	ClassDB::register_class<SecureStore>();

	// Serializable classes for demo recording/playback
	ClassDB::register_class<TntCheckpoint>();
	ClassDB::register_class<TntPlayerState>();
	ClassDB::register_class<DebugPlayerState>();
}

void unregister_libturnt_types(ModuleInitializationLevel p_level)
{
}

extern "C"
{
	GDExtensionBool GDE_EXPORT libturnt_init(
			GDExtensionInterfaceGetProcAddress p_get_proc_address,
			GDExtensionClassLibraryPtr p_library,
			GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

		init_obj.register_initializer(register_libturnt_types);
		init_obj.register_terminator(unregister_libturnt_types);

		return init_obj.init();
	}
}
