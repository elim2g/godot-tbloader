#include "debug_player_state.h"

#include <godot_cpp/core/class_db.hpp>

#include "serialization_helpers.h"


using namespace godot;


int DebugPlayerState::get_serialized_size() const {
	// Use constants from POD struct + variable array data
	size_t size =
		DebugPlayerStatePOD::HEADER_PART1_SIZE +
		SerializedSize::U32 + (SerializedSize::VEC3 * data.dbg_loop_positions_count) +
		DebugPlayerStatePOD::HEADER_PART2_SIZE +
		SerializedSize::U32 + (SerializedSize::VEC3 * data.dbg_loop_velocities_count);

	return static_cast<int>(size);
}



PackedByteArray DebugPlayerState::serialize() const {
	PackedByteArray bytes;
	bytes.resize(get_serialized_size());
	size_t offset = 0;

	// Fixed fields
	BinarySerializer::write_u32(bytes, offset, data.dbg_run_tick);

	BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_start_tick_pos);
	BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_post_grounddetect_pos);

	// Loop positions (variable-length with count header)
	BinarySerializer::write_u32(bytes, offset, data.dbg_loop_positions_count);
	for (uint32_t i = 0; i < data.dbg_loop_positions_count; ++i) {
		BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_loop_positions[i]);
	}

	BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_end_tick_pos);

	BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_start_tick_vel);
	BinarySerializer::write_u8(bytes, offset, data.dbg_accel_type);
	BinarySerializer::write_f64(bytes, offset, data.dbg_accel_out);
	BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_post_accel_vel);

	// Loop velocities (variable-length with count header)
	BinarySerializer::write_u32(bytes, offset, data.dbg_loop_velocities_count);
	for (uint32_t i = 0; i < data.dbg_loop_velocities_count; ++i) {
		BinarySerializer::write_vec3_f64(bytes, offset, data.dbg_loop_velocities[i]);
	}

	return bytes;
}



bool DebugPlayerState::deserialize(const PackedByteArray& bytes) {
	// Use the minimum size constant from POD
	if (bytes.size() < static_cast<int64_t>(DebugPlayerStatePOD::MIN_SERIALIZED_SIZE)) {
		return false;
	}

	size_t offset = 0;

	// Fixed fields (HEADER_PART1)
	data.dbg_run_tick = BinarySerializer::read_u32(bytes, offset);
	data.dbg_start_tick_pos = BinarySerializer::read_vec3_f64(bytes, offset);
	data.dbg_post_grounddetect_pos = BinarySerializer::read_vec3_f64(bytes, offset);

	// Loop positions (variable-length)
	uint32_t positions_count = BinarySerializer::read_u32(bytes, offset);
	if (positions_count > MAX_LOOP_ELEMENTS) {
		return false;  // Safety check
	}
	data.dbg_loop_positions_count = positions_count;
	for (uint32_t i = 0; i < positions_count; ++i) {
		if (offset + SerializedSize::VEC3 > static_cast<size_t>(bytes.size())) {
			return false;
		}
		data.dbg_loop_positions[i] = BinarySerializer::read_vec3_f64(bytes, offset);
	}

	// Fixed fields (HEADER_PART2)
	data.dbg_end_tick_pos = BinarySerializer::read_vec3_f64(bytes, offset);
	data.dbg_start_tick_vel = BinarySerializer::read_vec3_f64(bytes, offset);
	data.dbg_accel_type = BinarySerializer::read_u8(bytes, offset);
	data.dbg_accel_out = BinarySerializer::read_f64(bytes, offset);
	data.dbg_post_accel_vel = BinarySerializer::read_vec3_f64(bytes, offset);

	// Loop velocities (variable-length)
	uint32_t velocities_count = BinarySerializer::read_u32(bytes, offset);
	if (velocities_count > MAX_LOOP_ELEMENTS) {
		return false;  // Safety check
	}
	data.dbg_loop_velocities_count = velocities_count;
	for (uint32_t i = 0; i < velocities_count; ++i) {
		if (offset + SerializedSize::VEC3 > static_cast<size_t>(bytes.size())) {
			return false;
		}
		data.dbg_loop_velocities[i] = BinarySerializer::read_vec3_f64(bytes, offset);
	}

	return true;
}



Ref<DebugPlayerState> DebugPlayerState::duplicate() const {
	Ref<DebugPlayerState> dup;
	dup.instantiate();
	dup->data = data;
	return dup;
}



void DebugPlayerState::copy_from(const Ref<DebugPlayerState>& other) {
	if (other.is_valid()) {
		data = other->data;
	}
}



void DebugPlayerState::set_loop_positions(const PackedVector3Array& positions) {
	data.dbg_loop_positions_count = static_cast<uint32_t>(positions.size());
	if (data.dbg_loop_positions_count > MAX_LOOP_ELEMENTS) {
		data.dbg_loop_positions_count = MAX_LOOP_ELEMENTS;
	}
	for (uint32_t i = 0; i < data.dbg_loop_positions_count; ++i) {
		data.dbg_loop_positions[i] = positions[i];
	}
}



PackedVector3Array DebugPlayerState::get_loop_positions() const {
	PackedVector3Array result;
	result.resize(data.dbg_loop_positions_count);
	for (uint32_t i = 0; i < data.dbg_loop_positions_count; ++i) {
		result[i] = data.dbg_loop_positions[i];
	}
	return result;
}



void DebugPlayerState::set_loop_velocities(const PackedVector3Array& velocities) {
	data.dbg_loop_velocities_count = static_cast<uint32_t>(velocities.size());
	if (data.dbg_loop_velocities_count > MAX_LOOP_ELEMENTS) {
		data.dbg_loop_velocities_count = MAX_LOOP_ELEMENTS;
	}
	for (uint32_t i = 0; i < data.dbg_loop_velocities_count; ++i) {
		data.dbg_loop_velocities[i] = velocities[i];
	}
}



PackedVector3Array DebugPlayerState::get_loop_velocities() const {
	PackedVector3Array result;
	result.resize(data.dbg_loop_velocities_count);
	for (uint32_t i = 0; i < data.dbg_loop_velocities_count; ++i) {
		result[i] = data.dbg_loop_velocities[i];
	}
	return result;
}



void DebugPlayerState::_bind_methods() {
	// Serialization methods
	ClassDB::bind_static_method("DebugPlayerState", D_METHOD("get_max_serialized_size"),
		&DebugPlayerState::get_max_serialized_size);
	ClassDB::bind_method(D_METHOD("get_serialized_size"), &DebugPlayerState::get_serialized_size);
	ClassDB::bind_method(D_METHOD("serialize"), &DebugPlayerState::serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "bytes"), &DebugPlayerState::deserialize);

	// Utility methods
	ClassDB::bind_method(D_METHOD("duplicate"), &DebugPlayerState::duplicate);
	ClassDB::bind_method(D_METHOD("copy_from", "other"), &DebugPlayerState::copy_from);

	// Array conversion methods
	ClassDB::bind_method(D_METHOD("set_loop_positions", "positions"), &DebugPlayerState::set_loop_positions);
	ClassDB::bind_method(D_METHOD("get_loop_positions"), &DebugPlayerState::get_loop_positions);
	ClassDB::bind_method(D_METHOD("set_loop_velocities", "velocities"), &DebugPlayerState::set_loop_velocities);
	ClassDB::bind_method(D_METHOD("get_loop_velocities"), &DebugPlayerState::get_loop_velocities);

	// Property accessors
	ClassDB::bind_method(D_METHOD("set_dbg_run_tick", "value"), &DebugPlayerState::set_dbg_run_tick);
	ClassDB::bind_method(D_METHOD("get_dbg_run_tick"), &DebugPlayerState::get_dbg_run_tick);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dbg_run_tick"),
		"set_dbg_run_tick", "get_dbg_run_tick");

	ClassDB::bind_method(D_METHOD("set_dbg_start_tick_pos", "value"), &DebugPlayerState::set_dbg_start_tick_pos);
	ClassDB::bind_method(D_METHOD("get_dbg_start_tick_pos"), &DebugPlayerState::get_dbg_start_tick_pos);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "dbg_start_tick_pos"),
		"set_dbg_start_tick_pos", "get_dbg_start_tick_pos");

	ClassDB::bind_method(D_METHOD("set_dbg_post_grounddetect_pos", "value"), &DebugPlayerState::set_dbg_post_grounddetect_pos);
	ClassDB::bind_method(D_METHOD("get_dbg_post_grounddetect_pos"), &DebugPlayerState::get_dbg_post_grounddetect_pos);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "dbg_post_grounddetect_pos"),
		"set_dbg_post_grounddetect_pos", "get_dbg_post_grounddetect_pos");

	ClassDB::bind_method(D_METHOD("set_dbg_end_tick_pos", "value"), &DebugPlayerState::set_dbg_end_tick_pos);
	ClassDB::bind_method(D_METHOD("get_dbg_end_tick_pos"), &DebugPlayerState::get_dbg_end_tick_pos);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "dbg_end_tick_pos"),
		"set_dbg_end_tick_pos", "get_dbg_end_tick_pos");

	ClassDB::bind_method(D_METHOD("set_dbg_start_tick_vel", "value"), &DebugPlayerState::set_dbg_start_tick_vel);
	ClassDB::bind_method(D_METHOD("get_dbg_start_tick_vel"), &DebugPlayerState::get_dbg_start_tick_vel);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "dbg_start_tick_vel"),
		"set_dbg_start_tick_vel", "get_dbg_start_tick_vel");

	ClassDB::bind_method(D_METHOD("set_dbg_accel_type", "value"), &DebugPlayerState::set_dbg_accel_type);
	ClassDB::bind_method(D_METHOD("get_dbg_accel_type"), &DebugPlayerState::get_dbg_accel_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "dbg_accel_type"),
		"set_dbg_accel_type", "get_dbg_accel_type");

	ClassDB::bind_method(D_METHOD("set_dbg_accel_out", "value"), &DebugPlayerState::set_dbg_accel_out);
	ClassDB::bind_method(D_METHOD("get_dbg_accel_out"), &DebugPlayerState::get_dbg_accel_out);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dbg_accel_out"),
		"set_dbg_accel_out", "get_dbg_accel_out");

	ClassDB::bind_method(D_METHOD("set_dbg_post_accel_vel", "value"), &DebugPlayerState::set_dbg_post_accel_vel);
	ClassDB::bind_method(D_METHOD("get_dbg_post_accel_vel"), &DebugPlayerState::get_dbg_post_accel_vel);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "dbg_post_accel_vel"),
		"set_dbg_post_accel_vel", "get_dbg_post_accel_vel");

	// Constants
	BIND_CONSTANT(MAX_LOOP_ELEMENTS);
	BIND_CONSTANT(ACCEL_TYPE_GROUND);
	BIND_CONSTANT(ACCEL_TYPE_CROUCHSLIDE);
	BIND_CONSTANT(ACCEL_TYPE_AIR);
}