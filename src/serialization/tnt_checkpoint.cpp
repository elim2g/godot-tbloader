#include "tnt_checkpoint.h"
#include "serialization_helpers.h"

namespace godot {

PackedByteArray TntCheckpoint::serialize() const {
	PackedByteArray bytes;
	bytes.resize(TntCheckpointPOD::SERIALIZED_SIZE);
	size_t offset = 0;

	BinarySerializer::write_u8(bytes, offset, data.checkpoint_id);
	BinarySerializer::write_i32(bytes, offset, data.tick_achieved);

	return bytes;
}

bool TntCheckpoint::deserialize(const PackedByteArray& bytes) {
	if (bytes.size() < TntCheckpointPOD::SERIALIZED_SIZE) {
		return false;
	}

	size_t offset = 0;
	data.checkpoint_id = BinarySerializer::read_u8(bytes, offset);
	data.tick_achieved = BinarySerializer::read_i32(bytes, offset);

	return true;
}

Ref<TntCheckpoint> TntCheckpoint::duplicate() const {
	Ref<TntCheckpoint> dup;
	dup.instantiate();
	dup->data = data;
	return dup;
}

void TntCheckpoint::copy_from(const Ref<TntCheckpoint>& other) {
	if (other.is_valid()) {
		data = other->data;
	}
}

void TntCheckpoint::_bind_methods() {
	// Serialization methods
	ClassDB::bind_static_method("TntCheckpoint", D_METHOD("get_serialized_size"),
		&TntCheckpoint::get_serialized_size);
	ClassDB::bind_method(D_METHOD("serialize"), &TntCheckpoint::serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "bytes"), &TntCheckpoint::deserialize);

	// Utility methods
	ClassDB::bind_method(D_METHOD("duplicate"), &TntCheckpoint::duplicate);
	ClassDB::bind_method(D_METHOD("copy_from", "other"), &TntCheckpoint::copy_from);

	// Property accessors
	ClassDB::bind_method(D_METHOD("set_checkpoint_id", "id"), &TntCheckpoint::set_checkpoint_id);
	ClassDB::bind_method(D_METHOD("get_checkpoint_id"), &TntCheckpoint::get_checkpoint_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "checkpoint_id"),
		"set_checkpoint_id", "get_checkpoint_id");

	ClassDB::bind_method(D_METHOD("set_tick_achieved", "tick"), &TntCheckpoint::set_tick_achieved);
	ClassDB::bind_method(D_METHOD("get_tick_achieved"), &TntCheckpoint::get_tick_achieved);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_achieved"),
		"set_tick_achieved", "get_tick_achieved");

	// Constants
	BIND_CONSTANT(SERIALIZED_SIZE);
	BIND_CONSTANT(CHECKPOINT_ID_UNKNOWN);
	BIND_CONSTANT(CHECKPOINT_NOT_CROSSED);
}

} // namespace godot
