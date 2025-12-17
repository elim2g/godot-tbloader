#pragma once

#include <array>
#include <cmath>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

/**
 * @struct RateLimiterState
 * @brief Rate limiter enforcing 3 messages per second.
 *
 * Uses a ring buffer of 3 timestamps to track the last 3 messages.
 * A new message is allowed if current_time - oldest_timestamp >= 1.0 seconds.
 */
struct RateLimiterState {
	std::array<double, 3> message_timestamps{0.0, 0.0, 0.0};  // Last 3 message times
	int message_index = 0;  // Ring buffer index (0-2)

	/**
	 * Check if client can send a message now.
	 * @param current_time The current time in seconds (e.g., from OS::get_singleton()->get_ticks_msec() / 1000.0)
	 * @return True if enough time has passed, false if rate limited
	 */
	bool can_send_message(double current_time) const {
		double oldest_time = message_timestamps[message_index];
		// Allow message if 1+ second has passed since oldest message
		return (current_time - oldest_time) >= 1.0;
	}

	/**
	 * Record that a message was sent at the current time.
	 * @param current_time The time the message was sent
	 */
	void record_message(double current_time) {
		message_timestamps[message_index] = current_time;
		message_index = (message_index + 1) % 3;
	}

	/**
	 * Reset the rate limiter (for disconnect/reconnect).
	 */
	void reset() {
		message_timestamps.fill(0.0);
		message_index = 0;
	}
};

/**
 * @class ChatClientInfo
 * @brief Server-side tracking for a connected chat client.
 *
 * Stores:
 * - TCP connection peer
 * - Authenticated username
 * - Rate limiting state
 * - Partial packet buffer (for TCP stream reassembly)
 */
class ChatClientInfo : public RefCounted {
	GDCLASS(ChatClientInfo, RefCounted)

public:
	Ref<StreamPeerTCP> peer;           // The TCP connection to this client
	String username;                   // Authenticated username from API
	bool authenticated = false;        // Has this client passed authentication?
	RateLimiterState rate_limiter;     // Rate limiting state
	PackedByteArray receive_buffer;    // Accumulates partial packets (TCP stream)
	int client_id = -1;                // Assigned client ID

	ChatClientInfo() = default;
	~ChatClientInfo() = default;

	// ========================================================================
	// Rate Limiting Methods
	// ========================================================================

	/**
	 * Check if this client can send another message.
	 * @param current_time Current time in seconds
	 * @return True if within rate limit, false if too many messages
	 */
	bool can_send_message(double current_time) const {
		return rate_limiter.can_send_message(current_time);
	}

	/**
	 * Record that this client sent a message.
	 * @param current_time Time the message was sent
	 */
	void record_message(double current_time) {
		rate_limiter.record_message(current_time);
	}

	/**
	 * Reset rate limiter (called on disconnect).
	 */
	void reset_rate_limiter() {
		rate_limiter.reset();
	}

	// ========================================================================
	// Properties (exposed to GDScript)
	// ========================================================================

	void set_username(const String& value) {
		username = value;
	}

	String get_username() const {
		return username;
	}

	void set_authenticated(bool value) {
		authenticated = value;
	}

	bool get_authenticated() const {
		return authenticated;
	}

	// ========================================================================
	// Buffer Management
	// ========================================================================

	/**
	 * Add data to the receive buffer.
	 * @param data The new data to append
	 */
	void append_to_buffer(const PackedByteArray& data) {
		receive_buffer.append_array(data);
	}

	/**
	 * Remove processed bytes from the buffer.
	 * @param num_bytes Number of bytes to remove from front
	 */
	void consume_buffer(int num_bytes) {
		if (num_bytes <= 0) {
			return;
		}
		if (num_bytes >= receive_buffer.size()) {
			receive_buffer.clear();
			return;
		}
		receive_buffer = receive_buffer.slice(num_bytes);
	}

	/**
	 * Clear the receive buffer completely.
	 */
	void clear_buffer() {
		receive_buffer.clear();
	}

protected:
	static void _bind_methods();
};