#include "chat_client_info.h"

#include <godot_cpp/core/class_db.hpp>



using namespace godot;



void ChatClientInfo::_bind_methods() {
	// Properties
	ClassDB::bind_method(D_METHOD("set_username", "value"), &ChatClientInfo::set_username);
	ClassDB::bind_method(D_METHOD("get_username"), &ChatClientInfo::get_username);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "username"),
		"set_username", "get_username");

	ClassDB::bind_method(D_METHOD("set_authenticated", "value"), &ChatClientInfo::set_authenticated);
	ClassDB::bind_method(D_METHOD("get_authenticated"), &ChatClientInfo::get_authenticated);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "authenticated"),
		"set_authenticated", "get_authenticated");

	// Rate limiting methods
	ClassDB::bind_method(D_METHOD("can_send_message", "current_time"), &ChatClientInfo::can_send_message);
	ClassDB::bind_method(D_METHOD("record_message", "current_time"), &ChatClientInfo::record_message);
	ClassDB::bind_method(D_METHOD("reset_rate_limiter"), &ChatClientInfo::reset_rate_limiter);

	// Buffer management
	ClassDB::bind_method(D_METHOD("append_to_buffer", "data"), &ChatClientInfo::append_to_buffer);
	ClassDB::bind_method(D_METHOD("consume_buffer", "num_bytes"), &ChatClientInfo::consume_buffer);
	ClassDB::bind_method(D_METHOD("clear_buffer"), &ChatClientInfo::clear_buffer);
}