#include "tcp_chat_server.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "chat_message.h"
#include "chat_client_info.h"
#include "../serialization/serialization_helpers.h"



using namespace godot;



void TcpChatServer::_bind_methods() {
	// Server control
	ClassDB::bind_method(D_METHOD("start_server", "port"), &TcpChatServer::start_server);
	ClassDB::bind_method(D_METHOD("stop_server"), &TcpChatServer::stop_server);
	ClassDB::bind_method(D_METHOD("is_running"), &TcpChatServer::is_running);

	// Polling
	ClassDB::bind_method(D_METHOD("poll_server", "delta"), &TcpChatServer::poll_server);

	// Message handling
	ClassDB::bind_method(D_METHOD("broadcast_message", "message"), &TcpChatServer::broadcast_message);
	ClassDB::bind_method(D_METHOD("send_to_client", "client_id", "message"), &TcpChatServer::send_to_client);
	ClassDB::bind_method(D_METHOD("disconnect_client", "client_id", "reason"), &TcpChatServer::disconnect_client);

	// Client queries
	ClassDB::bind_method(D_METHOD("get_connected_usernames"), &TcpChatServer::get_connected_usernames);
	ClassDB::bind_method(D_METHOD("get_client_count"), &TcpChatServer::get_authed_client_count);
}



TcpChatServer::TcpChatServer() {
	m_tcp_server = Ref<TCPServer>(memnew(TCPServer));
	m_running = false;
}



TcpChatServer::~TcpChatServer() {
	stop_server();
}



bool TcpChatServer::start_server(int port) {
	if (m_running) {
		return false;
	}

	if (port <= 0 || port > 65535) {
		return false;
	}

	if (!m_tcp_server.is_valid()) {
		m_tcp_server = Ref<TCPServer>(memnew(TCPServer));
	}

	// Try to listen on the port
	Error err = m_tcp_server->listen(port);
	if (err != OK) {
		return false;
	}

	m_running = true;
	m_clients.clear();
	m_pending_auths.clear();
	m_next_client_id = 1;

	return true;
}



void TcpChatServer::stop_server() {
	if ((!m_running || !m_tcp_server.is_valid()) && m_clients.empty()) {
		return;
	}

	// Disconnect all clients
	for (auto& pair : m_clients) {
		disconnect_client(pair.first, "Server stopping");
	}

	// Stop listening
	if (m_tcp_server.is_valid()) {
		m_tcp_server->stop();
	}

	m_running = false;
	m_pending_auths.clear();
	m_clients.clear();
}



void TcpChatServer::poll_server(double delta) {
	if (!m_running || !m_tcp_server.is_valid()) {
		return;
	}

	_poll_new_connections();
	_poll_auth_requests(delta);
	_poll_client_data();
}



void TcpChatServer::broadcast_message(const Ref<ChatMessage>& message) {
	if (!message.is_valid()) {
		return;
	}

	PackedByteArray serialized = message->serialize();

	// Send to all authenticated clients
	for (auto& pair : m_clients) {
		auto client = pair.second;
		if (client->authenticated && client->peer.is_valid()) {
			client->peer->put_data(serialized);
		}
	}
}



void TcpChatServer::send_to_client(int client_id, const Ref<ChatMessage>& message) {
	if (!message.is_valid()) {
		return;
	}

	auto it = m_clients.find(client_id);
	if (it == m_clients.end() || !it->second->authenticated || !it->second->peer.is_valid()) {
		return;
	}

	PackedByteArray serialized = message->serialize();
	it->second->peer->put_data(serialized);
}



void TcpChatServer::disconnect_client(int client_id, const String& reason) {
	auto it = m_clients.find(client_id);
	if (it == m_clients.end()) {
		return;
	}

	Ref<ChatClientInfo> client = it->second;

	if (client->peer.is_valid()) {
		client->peer->disconnect_from_host();
	}

	m_clients.erase(it);

	// Also remove from pending auths if applicable
	for (size_t i = m_pending_auths.size() - 1; i >= 0; --i) {
		if (m_pending_auths[i].client_id == client_id) {
			m_pending_auths.erase(m_pending_auths.begin() + static_cast<ptrdiff_t>(i));
		}
	}
}



PackedStringArray TcpChatServer::get_connected_usernames() const {
	PackedStringArray result;
	for (const auto& pair : m_clients) {
		if (pair.second->authenticated) {
			result.append(pair.second->username);
		}
	}
	return result;
}



int TcpChatServer::get_authed_client_count() const {
	int count = 0;
	for (const auto& pair : m_clients) {
		count += static_cast<int>(pair.second->authenticated);
	}
	return count;
}



void TcpChatServer::_poll_new_connections() {
	while (m_tcp_server->is_connection_available()) {
		Ref<StreamPeerTCP> peer = m_tcp_server->take_connection();
		if (!peer.is_valid()) {
			break;
		}

		// Create client info
		int client_id = m_next_client_id++;
		Ref<ChatClientInfo> client = memnew(ChatClientInfo);
		client->peer = peer;
		client->authenticated = false;
		client->client_id = client_id;
		m_clients[client_id] = client;
	}
}



void TcpChatServer::_poll_auth_requests(double delta) {
	// Check each pending auth request
	for (int i = (int)m_pending_auths.size() - 1; i >= 0; --i) {
		PendingAuth& pending = m_pending_auths[i];

		// Check for timeout (20 second default)
		double current_msec = godot::Time::get_singleton()->get_ticks_msec();
		double start_msec = pending.start_time * 1000.0;
		double elapsed_msec = current_msec - start_msec;

		if (elapsed_msec > 20000.0) {
			// Auth request timed out - for now, accept any token (placeholder)
			_on_auth_completed(i);
		}
	}
}



void TcpChatServer::_poll_client_data() {
	double current_time = godot::Time::get_singleton()->get_ticks_msec() / 1000.0;
	for (auto& pair : m_clients) {
		int client_id = pair.first;
		auto& client = pair.second;

		if (!client->peer.is_valid()) {
			disconnect_client(client_id, "Peer invalid");
			continue;
		}

		// Poll the socket
		client->peer->poll();
		StreamPeerTCP::Status status = (StreamPeerTCP::Status)client->peer->get_status();

		// Check connection status
		if (status == StreamPeerTCP::STATUS_ERROR || status == StreamPeerTCP::STATUS_NONE) {
			disconnect_client(client_id, "Connection lost");
			continue;
		}

		// If not authenticated yet, wait for auth packet
		if (!client->authenticated) {
			_handle_auth_recv(client);
		} else {
			// Already authenticated - read chat messages
			if (client->peer->get_available_bytes() > 0) {
				// Try to read message header
				while (client->peer->get_available_bytes() > 0) {
					if (client->receive_buffer.size() < ChatMessage::HEADER_SIZE) {
						int available = client->peer->get_available_bytes();
						int needed = ChatMessage::HEADER_SIZE - client->receive_buffer.size();
						int to_read = needed < available ? needed : available;

						if (to_read > 0) {
							PackedByteArray chunk = client->peer->get_partial_data(to_read);
							client->append_to_buffer(chunk);
						}

						if (client->receive_buffer.size() < ChatMessage::HEADER_SIZE) {
							break;
						}
					}

					// Parse header
					size_t hdr_offset = 0;
					uint32_t timestamp = BinarySerializer::read_u32(client->receive_buffer, hdr_offset);
					uint16_t username_len = BinarySerializer::read_u16(client->receive_buffer, hdr_offset);
					uint16_t message_len = BinarySerializer::read_u16(client->receive_buffer, hdr_offset);
					uint8_t msg_type = BinarySerializer::read_u8(client->receive_buffer, hdr_offset);

					// Validate lengths
					if (username_len == 0 || username_len > ChatMessage::MAX_USERNAME_LENGTH ||
						message_len == 0 || message_len > ChatMessage::MAX_MESSAGE_LENGTH) {
						disconnect_client(client_id, "Invalid message lengths");
						break;
					}

					int total_needed = ChatMessage::HEADER_SIZE + username_len + message_len;

					// Check total size limit
					if (total_needed > ChatMessage::MAX_TOTAL_SIZE) {
						disconnect_client(client_id, "Message too large");
						break;
					}

					if (client->receive_buffer.size() < total_needed) {
						int available = client->peer->get_available_bytes();
						if (available > 0) {
							int to_read = available;
							PackedByteArray chunk = client->peer->get_partial_data(to_read);
							client->append_to_buffer(chunk);
						}

						if (client->receive_buffer.size() < total_needed) {
							break; // Need more data
						}
					}

					// We have complete message
					PackedByteArray msg_bytes = client->receive_buffer.slice(0, total_needed);

					// Check rate limit BEFORE processing
					if (!client->can_send_message(current_time)) {
						disconnect_client(client_id, "Rate limit exceeded");
						break;
					}

					// Deserialize message
					Ref<ChatMessage> msg = memnew(ChatMessage);
					if (!msg->deserialize(msg_bytes)) {
						disconnect_client(client_id, "Failed to deserialize message");
						break;
					}

					// Validate message is from this client
					msg->set_username(client->username);  // Enforce server-side username

					// Record message for rate limiting
					client->record_message(current_time);

					// Broadcast to all clients
					broadcast_message(msg);

					// Remove processed bytes from buffer
					client->consume_buffer(total_needed);
				}
			}
		}
	}
}



void TcpChatServer::_handle_auth_recv(Ref<ChatClientInfo> client) {
	if (client->peer->get_available_bytes() > 0) {
		// Read auth token length
		if (client->receive_buffer.size() < 2) {
			int available = client->peer->get_available_bytes();
			int needed = 2 - client->receive_buffer.size();
			int to_read = needed < available ? needed : available;

			if (to_read > 0) {
				PackedByteArray chunk = client->peer->get_partial_data(to_read);
				client->append_to_buffer(chunk);
			}
		}

		if (client->receive_buffer.size() >= 2) {
			// Parse token length
			size_t offset = 0;
			uint16_t token_len = BinarySerializer::read_u16(client->receive_buffer, offset);

			if (token_len == 0 || token_len > 2048) {
				disconnect_client(client->client_id, "Invalid auth packet");
				return;
			}

			int needed = 2 + token_len;
			if (client->receive_buffer.size() < needed) {
				int available = client->peer->get_available_bytes();
				if (available > 0) {
					int to_read = available;
					PackedByteArray chunk = client->peer->get_partial_data(to_read);
					client->append_to_buffer(chunk);
				}
			}

			if (client->receive_buffer.size() >= needed) {
				// Extract auth token using slice
				String authtoken = client->receive_buffer.slice(2, needed).get_string_from_utf8();
				client->clear_buffer();

				// Start auth validation
				_validate_auth_token(client->client_id, client->peer, authtoken);
			}
		}
	}
}



void TcpChatServer::_validate_auth_token(int client_id, const Ref<StreamPeerTCP>& peer, const String& token) {
	if (!peer.is_valid()) {
		disconnect_client(client_id, "Peer lost during auth");
		return;
	}

	// Store pending auth request
	// In production, this would call HTTPRequest to validate against https://api.turnt.pro/api/users
	// For now, placeholder: accept all tokens (validation TODO)
	PendingAuth pending;
	pending.client_id = client_id;
	pending.peer = peer;
	pending.authtoken = token;
	pending.start_time = godot::Time::get_singleton()->get_ticks_msec() / 1000.0;

	m_pending_auths.push_back(pending);
}



void TcpChatServer::_on_auth_completed(int pending_auth_idx) {
	if (pending_auth_idx < 0 || pending_auth_idx >= (int)m_pending_auths.size()) {
		return;
	}

	PendingAuth& pending = m_pending_auths[pending_auth_idx];
	int client_id = pending.client_id;

	auto client_it = m_clients.find(client_id);
	if (client_it == m_clients.end() || !client_it->second->peer.is_valid()) {
		// Client already disconnected
		m_pending_auths.erase(m_pending_auths.begin() + pending_auth_idx);
		return;
	}

	Ref<ChatClientInfo> client = client_it->second;

	// For now, accept all tokens (placeholder implementation)
	// In production, validate against https://api.turnt.pro/api/users using HTTPRequest
	String username = "user_" + String::num(client_id);

	// Send success response: [success: 1 byte][username_len: 2 bytes][username: N bytes]
	PackedByteArray username_bytes = username.to_utf8_buffer();
	PackedByteArray auth_response;
	auth_response.resize(3 + username_bytes.size());
	size_t offset = 0;
	BinarySerializer::write_u8(auth_response, offset, 1);  // success
	BinarySerializer::write_u16(auth_response, offset, username_bytes.size());
	BinarySerializer::write_bytes(auth_response, offset, username_bytes);

	client->username = username;
	client->authenticated = true;
	client->peer->put_data(auth_response);

	m_pending_auths.erase(m_pending_auths.begin() + pending_auth_idx);
}
