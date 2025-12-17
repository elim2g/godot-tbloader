#pragma once

#include <map>
#include <vector>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "chat_message.h"
#include "chat_client_info.h"

using namespace godot;

/**
 * @class TcpChatServer
 * @brief Server-side chat system manager.
 *
 * Manages:
 * - TCP server listening on a port
 * - Accepting new client connections
 * - Authenticating clients via authtoken (HTTP GET to api.turnt.pro)
 * - Broadcasting messages to authenticated clients
 * - Enforcing rate limits (3 messages/second per client)
 * - Disconnecting misbehaving clients
 *
 * Non-blocking polling pattern: call poll_server() every frame from GDScript _process().
 */
class TcpChatServer : public RefCounted {
	GDCLASS(TcpChatServer, RefCounted)

public:
	TcpChatServer();
	~TcpChatServer();

	// ========================================================================
	// Server Control
	// ========================================================================

	/**
	 * Start the TCP chat server on a specific port.
	 * @param port Port number to listen on (1-65535)
	 * @return True if server started successfully, false on error
	 */
	bool start_server(int port);

	/**
	 * Stop the server and disconnect all clients.
	 */
	void stop_server();

	/**
	 * Check if server is running.
	 * @return True if server is listening, false otherwise
	 */
	bool is_running() const { return m_running; }

	// ========================================================================
	// Polling (must call every frame)
	// ========================================================================

	/**
	 * Non-blocking poll for new connections, auth responses, and client messages.
	 * Call this every frame from GDScript _process().
	 * Handles:
	 * - Accepting new TCP connections
	 * - Processing pending HTTP auth requests
	 * - Reading messages from authenticated clients
	 * - Broadcasting/relaying messages
	 * @param delta Time elapsed since last frame (for rate limiting)
	 */
	void poll_server(double delta);

	// ========================================================================
	// Message Handling
	// ========================================================================

	/**
	 * Broadcast a chat message to all authenticated clients.
	 * @param message The message to send
	 */
	void broadcast_message(const Ref<ChatMessage>& message);

	/**
	 * Send a message to a specific client.
	 * @param client_id The client ID to send to
	 * @param message The message to send
	 */
	void send_to_client(int client_id, const Ref<ChatMessage>& message);

	/**
	 * Disconnect a specific client.
	 * @param client_id The client ID to disconnect
	 * @param reason Reason for disconnection (for logging)
	 */
	void disconnect_client(int client_id, const String& reason);

	// ========================================================================
	// Client Queries
	// ========================================================================

	/**
	 * Get list of authenticated client usernames.
	 * @return Array of usernames currently connected
	 */
	PackedStringArray get_connected_usernames() const;

	/**
	 * Get count of authenticated clients.
	 * @return Number of authenticated connected clients
	 */
	int get_authed_client_count() const;

protected:
	static void _bind_methods();

private:
	void _poll_new_connections();
	void _poll_auth_requests(double delta);
	void _poll_client_data();
	void _handle_client_message(int client_id, const PackedByteArray& data);
	void _handle_auth_recv(Ref<ChatClientInfo> client);
	void _validate_auth_token(int client_id, const Ref<StreamPeerTCP>& peer, const String& token);
	void _on_auth_completed(int pending_auth_idx);

private:
	Ref<TCPServer> m_tcp_server;
	std::map<int, Ref<ChatClientInfo>> m_clients;  // client_id → client info
	int m_next_client_id = 0;
	bool m_running = false;

	// Tracks pending HTTP auth requests
	struct PendingAuth {
		int client_id;
		Ref<StreamPeerTCP> peer;
		String authtoken;
		double start_time;
	};
	std::vector<PendingAuth> m_pending_auths;
};
