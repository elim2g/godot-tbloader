#pragma once

#include <cstdint>
#include <ctime>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

#include "../serialization/serialization_helpers.h"

using namespace godot;

/**
 * @struct ChatMessagePOD
 * @brief Plain Old Data struct for chat message serialization.
 *
 * Fixed header (9 bytes) followed by variable-length username and message.
 * Total serialized size: 9 + username_length + message_length (max 585 bytes)
 *
 * Wire format:
 * [timestamp: 4 bytes][username_length: 2 bytes][message_length: 2 bytes][message_type: 1 byte][username][message]
 */
struct ChatMessagePOD {
	uint32_t timestamp;           // Unix timestamp
	uint16_t username_length;     // Username byte count (1-64)
	uint16_t message_length;      // Message byte count (1-512)
	uint8_t message_type;         // 0=chat, 1=system, 2=whisper

	// Size constants computed from field types (auto-updates if fields change)
	static constexpr size_t HEADER_SIZE =
		SerializedSize::U32 +     // timestamp
		SerializedSize::U16 +     // username_length
		SerializedSize::U16 +     // message_length
		SerializedSize::U8;       // message_type

	static constexpr size_t MAX_USERNAME_LENGTH = 64;
	static constexpr size_t MAX_MESSAGE_LENGTH = 512;
	static constexpr size_t MAX_TOTAL_SIZE = HEADER_SIZE + MAX_USERNAME_LENGTH + MAX_MESSAGE_LENGTH;

	// Message type constants
	static constexpr uint8_t TYPE_CHAT = 0;
	static constexpr uint8_t TYPE_SYSTEM = 1;
	static constexpr uint8_t TYPE_WHISPER = 2;
};

/**
 * @class ChatMessage
 * @brief GDExtension wrapper for ChatMessagePOD with serialization support.
 *
 * Provides:
 * - Serialization to/from PackedByteArray (variable-size)
 * - Validation methods for length constraints
 * - Properties exposed to GDScript
 * - Type-safe message handling
 */
class ChatMessage : public RefCounted {
	GDCLASS(ChatMessage, RefCounted)

public:
	// Size constants (exposed to GDScript)
	static constexpr int HEADER_SIZE = ChatMessagePOD::HEADER_SIZE;
	static constexpr int MAX_USERNAME_LENGTH = ChatMessagePOD::MAX_USERNAME_LENGTH;
	static constexpr int MAX_MESSAGE_LENGTH = ChatMessagePOD::MAX_MESSAGE_LENGTH;
	static constexpr int MAX_TOTAL_SIZE = ChatMessagePOD::MAX_TOTAL_SIZE;

	// Message type constants
	static constexpr int TYPE_CHAT = ChatMessagePOD::TYPE_CHAT;
	static constexpr int TYPE_SYSTEM = ChatMessagePOD::TYPE_SYSTEM;
	static constexpr int TYPE_WHISPER = ChatMessagePOD::TYPE_WHISPER;

	// The actual POD data
	ChatMessagePOD header{};
	String username;
	String message;

	ChatMessage() = default;
	~ChatMessage() = default;

	// ========================================================================
	// Serialization Methods
	// ========================================================================

	/**
	 * Serialize this message to a byte array.
	 * @return PackedByteArray containing serialized message (variable size, max 585 bytes)
	 */
	PackedByteArray serialize() const;

	/**
	 * Deserialize message from a byte array.
	 * @param bytes The byte array to deserialize from
	 * @return True if deserialization succeeded, false if data is invalid
	 */
	bool deserialize(const PackedByteArray& bytes);

	/**
	 * Get the serialized size of this message.
	 * @return Byte count including header and variable data (9 + username_len + message_len)
	 */
	int get_serialized_size() const;

	// ========================================================================
	// Validation Methods
	// ========================================================================

	/**
	 * Check if a message string is within length limits.
	 * @param msg The message to validate
	 * @return True if 1-512 characters, false otherwise
	 */
	static bool validate_message_length(const String& msg);

	/**
	 * Check if a username string is within length limits.
	 * @param user The username to validate
	 * @return True if 1-64 characters, false otherwise
	 */
	static bool validate_username_length(const String& user);

	/**
	 * Validate entire message packet (header + data).
	 * @param bytes The serialized data to validate
	 * @return True if valid, false if exceeds size limits or malformed
	 */
	static bool validate_packet(const PackedByteArray& bytes);

	// ========================================================================
	// Property Accessors (for GDScript)
	// ========================================================================

	void set_username(const String& value);
	String get_username() const;

	void set_message(const String& value);
	String get_message() const;

	void set_timestamp(int value);
	int get_timestamp() const;

	void set_message_type(int value);
	int get_message_type() const;

protected:
	static void _bind_methods();
};