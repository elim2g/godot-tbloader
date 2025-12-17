#include "tcp_chat_client.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "chat_message.h"
#include "../serialization/serialization_helpers.h"



using namespace godot;



void TcpChatClient::_bind_methods() {
	// Connection methods
	ClassDB::bind_method(D_METHOD("connect_to_server", "host", "port", "authtoken"),
		&TcpChatClient::connect_to_server);
	ClassDB::bind_method(D_METHOD("disconnect_from_server"), &TcpChatClient::disconnect_from_server);
	ClassDB::bind_method(D_METHOD("is_connected"), &TcpChatClient::is_connected);

	// Polling
	ClassDB::bind_method(D_METHOD("poll_client"), &TcpChatClient::poll_client);

	// Messaging
	ClassDB::bind_method(D_METHOD("send_message", "message"), &TcpChatClient::send_message);
	ClassDB::bind_method(D_METHOD("receive_messages"), &TcpChatClient::receive_messages);

	// Status
	ClassDB::bind_method(D_METHOD("get_username"), &TcpChatClient::get_username);
	ClassDB::bind_method(D_METHOD("get_state_string"), &TcpChatClient::get_state_string);
}



TcpChatClient::TcpChatClient() {
	m_peer = Ref<StreamPeerTCP>(memnew(StreamPeerTCP));
	m_state = State::Disconnected;
}



TcpChatClient::~TcpChatClient() {
	if (m_peer.is_valid()) {
		m_peer->disconnect_from_host();
	}
}



bool TcpChatClient::connect_to_server(const String& host, int port, const String& authtoken) {
	if (host.is_empty() || port <= 0 || port > 65535 || authtoken.is_empty()) {
		return false;
	}

	if (!m_peer.is_valid()) {
		m_peer = Ref<StreamPeerTCP>(memnew(StreamPeerTCP));
	}

	// Initiate TCP connection
	Error err = m_peer->connect_to_host(host, port);
	if (err != OK) {
		return false;
	}

	m_authtoken = authtoken;
	m_state = State::Connecting;
	m_authenticated = false;
	m_receive_buffer.clear();
	m_pending_messages.clear();

	return true;
}



void TcpChatClient::disconnect_from_server() {
	_disconnect_internal();
}



void TcpChatClient::poll_client() {
	if (!m_peer.is_valid()) {
		return;
	}

	// Poll TCP socket status
	m_peer->poll();

	StreamPeerTCP::Status status = (StreamPeerTCP::Status)m_peer->get_status();

	switch (m_state) {
		case State::Disconnected:
			// Do nothing
			break;

		case State::Connecting: {
			// Check if connection established
			if (status == StreamPeerTCP::STATUS_CONNECTED) {
				m_state = State::WaitingForAuth;
				_send_auth_packet();
			} else if (status == StreamPeerTCP::STATUS_ERROR) {
				_disconnect_internal();
			}
			break;
		}

		case State::WaitingForAuth: {
			// Check for auth response
			if (m_peer->get_available_bytes() > 0) {
				_handle_auth_response();
			} else if (status == StreamPeerTCP::STATUS_ERROR || status == StreamPeerTCP::STATUS_NONE) {
				_disconnect_internal();
			}
			break;
		}

		case State::Authenticated: {
			// Check for incoming messages
			if (m_peer->get_available_bytes() > 0) {
				_handle_incoming_messages();
			} else if (status == StreamPeerTCP::STATUS_ERROR || status == StreamPeerTCP::STATUS_NONE) {
				_disconnect_internal();
			}
			break;
		}
	}
}



bool TcpChatClient::send_message(const String& message) {
	if (!m_authenticated || !m_peer.is_valid()) {
		return false;
	}

	// Validate message length
	if (!ChatMessage::validate_message_length(message)) {
		return false;
	}

	// Create and serialize chat message
	Ref<ChatMessage> msg = memnew(ChatMessage);
	msg->set_username(m_username);
	msg->set_message(message);
	msg->set_timestamp((int)godot::Time::get_singleton()->get_ticks_msec() / 1000);
	msg->set_message_type(ChatMessage::TYPE_CHAT);

	PackedByteArray serialized = msg->serialize();

	// Send to server
	Error err = m_peer->put_data(serialized);
	return err == OK;
}



PackedStringArray TcpChatClient::receive_messages() {
	PackedStringArray result;
	for (const String& msg : m_pending_messages) {
		result.append(msg);
	}
	m_pending_messages.clear();
	return result;
}



String TcpChatClient::get_state_string() const {
	switch (m_state) {
		case State::Disconnected:
			return "Disconnected";
		case State::Connecting:
			return "Connecting";
		case State::WaitingForAuth:
			return "WaitingForAuth";
		case State::Authenticated:
			return "Authenticated";
		default:
			return "Unknown";
	}
}



void TcpChatClient::_send_auth_packet() {
	if (!m_peer.is_valid()) {
		return;
	}

	// Auth packet format: [uint16_t token_length][char[] token]
	PackedByteArray auth_packet;
	PackedByteArray token_bytes = m_authtoken.to_utf8_buffer();
	uint16_t token_length = token_bytes.size();

	// Create packet
	auth_packet.resize(2 + token_length);
	size_t offset = 0;
	BinarySerializer::write_u16(auth_packet, offset, token_length);
	for (int i = 0; i < token_bytes.size(); ++i) {
		BinarySerializer::write_u8(auth_packet, offset, token_bytes[i]);
	}

	// Send to server
	m_peer->put_data(auth_packet);
}



void TcpChatClient::_handle_auth_response() {
	if (!m_peer.is_valid() || m_peer->get_available_bytes() < 1) {
		return;
	}

	// Read first byte: success flag
	PackedByteArray first_byte = m_peer->get_partial_data(1);
	if (first_byte.size() != 1) {
		_disconnect_internal();
		return;
	}

	uint8_t success = first_byte[0];

	if (success == 0) {
		// Authentication failed
		_disconnect_internal();
		return;
	}

	// success == 1: Read username length (uint16_t)
	if (m_peer->get_available_bytes() < 2) {
		// Wait for more data
		m_receive_buffer.append_array(first_byte);
		return;
	}

	PackedByteArray len_bytes = m_peer->get_partial_data(2);
	if (len_bytes.size() != 2) {
		_disconnect_internal();
		return;
	}

	size_t offset = 0;
	uint16_t username_length = BinarySerializer::read_u16(len_bytes, offset);

	// Validate username length
	if (username_length == 0 || username_length > ChatMessage::MAX_USERNAME_LENGTH) {
		_disconnect_internal();
		return;
	}

	// Read username
	if (m_peer->get_available_bytes() < username_length) {
		// Wait for more data
		return;
	}

	PackedByteArray username_bytes = m_peer->get_partial_data(username_length);
	if (username_bytes.size() != username_length) {
		_disconnect_internal();
		return;
	}

	m_username = username_bytes.get_string_from_utf8();
	m_authenticated = true;
	m_state = State::Authenticated;
}



void TcpChatClient::_handle_incoming_messages() {
	if (!m_peer.is_valid()) {
		return;
	}

	while (m_peer->get_available_bytes() > 0) {
		// Check if we have at least the header
		if (m_receive_buffer.size() < ChatMessage::HEADER_SIZE) {
			int available = m_peer->get_available_bytes();
			int needed = ChatMessage::HEADER_SIZE - m_receive_buffer.size();
			int to_read = needed < available ? needed : available;

			if (to_read > 0) {
				PackedByteArray chunk = m_peer->get_partial_data(to_read);
				m_receive_buffer.append_array(chunk);
			}

			if (m_receive_buffer.size() < ChatMessage::HEADER_SIZE) {
				break; // Need more data
			}
		}

		// Parse header to get message lengths
		if (m_receive_buffer.size() < ChatMessage::HEADER_SIZE) {
			break;
		}

		size_t header_offset = 0;
		uint32_t timestamp = BinarySerializer::read_u32(m_receive_buffer, header_offset);
		uint16_t username_len = BinarySerializer::read_u16(m_receive_buffer, header_offset);
		uint16_t message_len = BinarySerializer::read_u16(m_receive_buffer, header_offset);
		uint8_t msg_type = BinarySerializer::read_u8(m_receive_buffer, header_offset);

		// Validate lengths
		if (username_len == 0 || username_len > ChatMessage::MAX_USERNAME_LENGTH ||
			message_len == 0 || message_len > ChatMessage::MAX_MESSAGE_LENGTH) {
			_disconnect_internal();
			return;
		}

		int total_needed = ChatMessage::HEADER_SIZE + username_len + message_len;

		// Do we have the complete message?
		if (m_receive_buffer.size() < total_needed) {
			// Need more data from network
			int available = m_peer->get_available_bytes();
			if (available > 0) {
				int to_read = available;
				PackedByteArray chunk = m_peer->get_partial_data(to_read);
				m_receive_buffer.append_array(chunk);
			}

			if (m_receive_buffer.size() < total_needed) {
				break; // Still waiting for complete message
			}
		}

		// Extract complete message using slice
		PackedByteArray msg_bytes = m_receive_buffer.slice(0, total_needed);

		// Deserialize message
		Ref<ChatMessage> msg = memnew(ChatMessage);
		if (!msg->deserialize(msg_bytes)) {
			_disconnect_internal();
			return;
		}

		// Add to pending messages
		m_pending_messages.push_back(msg->get_username() + ": " + msg->get_message());

		// Remove processed bytes from buffer using slice
		m_receive_buffer = m_receive_buffer.slice(total_needed);
	}
}



void TcpChatClient::_disconnect_internal() {
	if (m_peer.is_valid()) {
		m_peer->disconnect_from_host();
	}
	m_state = State::Disconnected;
	m_authenticated = false;
	m_username = "";
	m_authtoken = "";
	m_receive_buffer.clear();
	m_pending_messages.clear();
}