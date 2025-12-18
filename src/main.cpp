#include <gdextension_interface.h>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <turnt_loader.h>
#include <secure_store.h>
#include <serialization/tnt_checkpoint.h>
#include <serialization/tnt_player_state.h>
#include <serialization/debug_player_state.h>
#include <chat/chat_message.h>
#include <chat/chat_client_info.h>
#include <chat/tcp_chat_client.h>
#include <chat/tcp_chat_server.h>

using namespace godot;

void register_libturnt_types(ModuleInitializationLevel p_level)
{
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<TurntLoader>();
	ClassDB::register_class<SecureStore>();

	// Serializable classes for demo recording/playback
	ClassDB::register_class<TntCheckpoint>();
	ClassDB::register_class<TntPlayerState>();
	ClassDB::register_class<DebugPlayerState>();

	// Chat system classes
	ClassDB::register_class<ChatMessage>();
	ClassDB::register_class<ChatClientInfo>();
	ClassDB::register_class<TcpChatClient>();
	ClassDB::register_class<TcpChatServer>();
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
