#include "chat_message.h"

#include <godot_cpp/core/class_db.hpp>

#include "../serialization/serialization_helpers.h"



using namespace godot;



void ChatMessage::_bind_methods() {
	// Serialization methods
	ClassDB::bind_method(D_METHOD("serialize"), &ChatMessage::serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "bytes"), &ChatMessage::deserialize);
	ClassDB::bind_method(D_METHOD("get_serialized_size"), &ChatMessage::get_serialized_size);

	// Validation methods
	ClassDB::bind_static_method("ChatMessage",
		D_METHOD("validate_message_length", "message"),
		&ChatMessage::validate_message_length);
	ClassDB::bind_static_method("ChatMessage",
		D_METHOD("validate_username_length", "username"),
		&ChatMessage::validate_username_length);
	ClassDB::bind_static_method("ChatMessage",
		D_METHOD("validate_packet", "bytes"),
		&ChatMessage::validate_packet);

	// Property methods
	ClassDB::bind_method(D_METHOD("set_username", "value"), &ChatMessage::set_username);
	ClassDB::bind_method(D_METHOD("get_username"), &ChatMessage::get_username);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "username"),
		"set_username", "get_username");

	ClassDB::bind_method(D_METHOD("set_message", "value"), &ChatMessage::set_message);
	ClassDB::bind_method(D_METHOD("get_message"), &ChatMessage::get_message);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"),
		"set_message", "get_message");

	ClassDB::bind_method(D_METHOD("set_timestamp", "value"), &ChatMessage::set_timestamp);
	ClassDB::bind_method(D_METHOD("get_timestamp"), &ChatMessage::get_timestamp);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "timestamp"),
		"set_timestamp", "get_timestamp");

	ClassDB::bind_method(D_METHOD("set_message_type", "value"), &ChatMessage::set_message_type);
	ClassDB::bind_method(D_METHOD("get_message_type"), &ChatMessage::get_message_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "message_type"),
		"set_message_type", "get_message_type");

	// Size constants
	BIND_CONSTANT(HEADER_SIZE);
	BIND_CONSTANT(MAX_USERNAME_LENGTH);
	BIND_CONSTANT(MAX_MESSAGE_LENGTH);
	BIND_CONSTANT(MAX_TOTAL_SIZE);

	// Message type constants
	BIND_CONSTANT(TYPE_CHAT);
	BIND_CONSTANT(TYPE_SYSTEM);
	BIND_CONSTANT(TYPE_WHISPER);
}



PackedByteArray ChatMessage::serialize() const {
	int total_size = get_serialized_size();
	PackedByteArray bytes;
	bytes.resize(total_size);
	size_t offset = 0;

	// Write header
	BinarySerializer::write_u32(bytes, offset, header.timestamp);
	BinarySerializer::write_u16(bytes, offset, header.username_length);
	BinarySerializer::write_u16(bytes, offset, header.message_length);
	BinarySerializer::write_u8(bytes, offset, header.message_type);

	// Write username
	PackedByteArray username_bytes = username.to_utf8_buffer();
	for (int i = 0; i < username_bytes.size(); ++i) {
		BinarySerializer::write_u8(bytes, offset, username_bytes[i]);
	}

	// Write message
	PackedByteArray message_bytes = message.to_utf8_buffer();
	for (int i = 0; i < message_bytes.size(); ++i) {
		BinarySerializer::write_u8(bytes, offset, message_bytes[i]);
	}

	return bytes;
}



bool ChatMessage::deserialize(const PackedByteArray& bytes) {
	// Validate minimum size for header
	if (bytes.size() < ChatMessagePOD::HEADER_SIZE) {
		return false;
	}

	size_t offset = 0;

	// Read header
	header.timestamp = BinarySerializer::read_u32(bytes, offset);
	header.username_length = BinarySerializer::read_u16(bytes, offset);
	header.message_length = BinarySerializer::read_u16(bytes, offset);
	header.message_type = BinarySerializer::read_u8(bytes, offset);

	// Validate lengths
	if (header.username_length == 0 || header.username_length > ChatMessagePOD::MAX_USERNAME_LENGTH) {
		return false;
	}
	if (header.message_length == 0 || header.message_length > ChatMessagePOD::MAX_MESSAGE_LENGTH) {
		return false;
	}

	// Validate total size
	int expected_size = ChatMessagePOD::HEADER_SIZE + header.username_length + header.message_length;
	if (bytes.size() != expected_size) {
		return false;
	}

	// Read username bytes
	PackedByteArray username_bytes;
	username_bytes.resize(header.username_length);
	for (int i = 0; i < header.username_length; ++i) {
		username_bytes[i] = BinarySerializer::read_u8(bytes, offset);
	}
	username = username_bytes.get_string_from_utf8();

	// Read message bytes
	PackedByteArray message_bytes;
	message_bytes.resize(header.message_length);
	for (int i = 0; i < header.message_length; ++i) {
		message_bytes[i] = BinarySerializer::read_u8(bytes, offset);
	}
	message = message_bytes.get_string_from_utf8();

	return true;
}



int ChatMessage::get_serialized_size() const {
	// Re-calculate lengths from current strings (in case they changed)
	int username_len = username.to_utf8_buffer().size();
	int message_len = message.to_utf8_buffer().size();
	return ChatMessagePOD::HEADER_SIZE + username_len + message_len;
}



bool ChatMessage::validate_message_length(const String& msg) {
	int len = msg.to_utf8_buffer().size();
	return len >= 1 && len <= ChatMessagePOD::MAX_MESSAGE_LENGTH;
}



bool ChatMessage::validate_username_length(const String& user) {
	int len = user.to_utf8_buffer().size();
	return len >= 1 && len <= ChatMessagePOD::MAX_USERNAME_LENGTH;
}



bool ChatMessage::validate_packet(const PackedByteArray& bytes) {
	if (bytes.size() < ChatMessagePOD::HEADER_SIZE) {
		return false;
	}
	if (bytes.size() > ChatMessagePOD::MAX_TOTAL_SIZE) {
		return false;
	}

	// Try to deserialize to fully validate
	ChatMessage temp;
	return temp.deserialize(bytes);
}



void ChatMessage::set_username(const String& value) {
	username = value;
	header.username_length = static_cast<uint16_t>(value.to_utf8_buffer().size());
}



String ChatMessage::get_username() const {
	return username;
}



void ChatMessage::set_message(const String& value) {
	message = value;
	header.message_length = static_cast<uint16_t>(value.to_utf8_buffer().size());
}



String ChatMessage::get_message() const {
	return message;
}



void ChatMessage::set_timestamp(int value) {
	header.timestamp = static_cast<uint32_t>(value);
}



int ChatMessage::get_timestamp() const {
	return static_cast<int>(header.timestamp);
}



void ChatMessage::set_message_type(int value) {
	header.message_type = static_cast<uint8_t>(value & 0xFF);
}



int ChatMessage::get_message_type() const {
	return static_cast<int>(header.message_type);
}