#pragma once

#include <vector>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/variant/string.hpp>

#include "chat_message.h"

using namespace godot;

/**
 * @class TcpChatClient
 * @brief Client-side chat connection handler.
 *
 * Manages:
 * - TCP connection to chat server
 * - Authentication handshake (authtoken validation)
 * - Sending chat messages with length validation
 * - Receiving chat messages from server
 * - Packet reassembly (TCP streams may fragment)
 *
 * Non-blocking polling pattern: call poll_client() every frame from GDScript _process().
 */
class TcpChatClient : public RefCounted {
	GDCLASS(TcpChatClient, RefCounted)

public:
	TcpChatClient();
	~TcpChatClient();

	// ========================================================================
	// Connection Management
	// ========================================================================

	/**
	 * Initiate connection to chat server.
	 * @param host Server hostname or IP
	 * @param port Server port
	 * @param authtoken Authentication token to send during handshake
	 * @return True if connection initiated, false if invalid parameters
	 */
	bool connect_to_server(const String& host, int port, const String& authtoken);

	/**
	 * Disconnect from server.
	 */
	void disconnect_from_server();

	/**
	 * Check if authenticated and ready to chat.
	 * @return True if in Authenticated state, false otherwise
	 */
	bool is_connected() const { return m_authenticated; }

	// ========================================================================
	// Polling (must call every frame)
	// ========================================================================

	/**
	 * Non-blocking poll for connection/auth status and incoming messages.
	 * Call this every frame from GDScript _process().
	 * Handles:
	 * - TCP connection state transitions
	 * - Sending auth packet
	 * - Receiving auth response
	 * - Receiving chat messages
	 */
	void poll_client();

	// ========================================================================
	// Messaging
	// ========================================================================

	/**
	 * Send a chat message to server.
	 * Validates message length before sending.
	 * @param message Text to send (must be 1-512 characters)
	 * @return True if message was sent, false if validation failed or not connected
	 */
	bool send_message(const String& message);

	/**
	 * Retrieve any messages received from server.
	 * @return Array of message strings ready for display
	 */
	PackedStringArray receive_messages();

	// ========================================================================
	// Status
	// ========================================================================

	/**
	 * Get the authenticated username (only valid if is_connected() is true).
	 * @return Username or empty string if not authenticated
	 */
	String get_username() const { return m_username; }

	/**
	 * Get human-readable state string for debugging.
	 * @return State name: "Disconnected", "Connecting", "WaitingForAuth", or "Authenticated"
	 */
	String get_state_string() const;

protected:
	static void _bind_methods();

private:
	// ========================================================================
	// Internal Helpers
	// ========================================================================

	void _send_auth_packet();
	void _handle_auth_response();
	void _handle_incoming_messages();
	void _disconnect_internal();

private:
	Ref<StreamPeerTCP> m_peer;
	String m_authtoken;
	String m_username;
	bool m_authenticated = false;

	enum class State {
		Disconnected,      // Not connected
		Connecting,        // TCP connect in progress
		WaitingForAuth,    // Sent auth token, waiting for response
		Authenticated      // Ready to send/receive chat messages
	};
	State m_state = State::Disconnected;

	// Receive buffer for TCP stream reassembly
	PackedByteArray m_receive_buffer;

	// Pending messages ready for GDScript (_process calls receive_messages())
	std::vector<String> m_pending_messages;
};