#include "tnt_player_state.h"
#include "serialization_helpers.h"

namespace godot {

PackedByteArray TntPlayerState::serialize() const {
	PackedByteArray bytes;
	bytes.resize(TntPlayerStatePOD::SERIALIZED_SIZE);
	size_t offset = 0;

	// Inputs
	BinarySerializer::write_vec2_f64(bytes, offset, data.look_direction);
	BinarySerializer::write_u8(bytes, offset, data.pressed_keys);

	// Movement state
	BinarySerializer::write_u32(bytes, offset, data.run_tick);
	BinarySerializer::write_u32(bytes, offset, data.run_started_tick);
	BinarySerializer::write_vec3_f64(bytes, offset, data.position);
	BinarySerializer::write_vec3_f64(bytes, offset, data.velocity);

	// Ground state
	BinarySerializer::write_u8(bytes, offset, data.state_flags);
	BinarySerializer::write_vec3_f64(bytes, offset, data.ground_normal);
	BinarySerializer::write_u8(bytes, offset, data.num_ticks_grounded);

	BinarySerializer::write_f64(bytes, offset, data.crouchslide_duration_remaining_s);
	BinarySerializer::write_f64(bytes, offset, data.crouchslide_transition_remaining_s);

	// Timing
	BinarySerializer::write_f64(bytes, offset, data.hangtime_duration_s);
	BinarySerializer::write_f64(bytes, offset, data.doublejump_window_remaining_s);

	return bytes;
}

bool TntPlayerState::deserialize(const PackedByteArray& bytes) {
	if (bytes.size() < TntPlayerStatePOD::SERIALIZED_SIZE) {
		return false;
	}

	size_t offset = 0;

	// Inputs
	data.look_direction = BinarySerializer::read_vec2_f64(bytes, offset);
	data.pressed_keys = BinarySerializer::read_u8(bytes, offset);

	// Movement state
	data.run_tick = BinarySerializer::read_u32(bytes, offset);
	data.run_started_tick = BinarySerializer::read_u32(bytes, offset);
	data.position = BinarySerializer::read_vec3_f64(bytes, offset);
	data.velocity = BinarySerializer::read_vec3_f64(bytes, offset);

	// Ground state
	data.state_flags = BinarySerializer::read_u8(bytes, offset);
	data.ground_normal = BinarySerializer::read_vec3_f64(bytes, offset);
	data.num_ticks_grounded = BinarySerializer::read_u8(bytes, offset);

	data.crouchslide_duration_remaining_s = BinarySerializer::read_f64(bytes, offset);
	data.crouchslide_transition_remaining_s = BinarySerializer::read_f64(bytes, offset);

	// Timing
	data.hangtime_duration_s = BinarySerializer::read_f64(bytes, offset);
	data.doublejump_window_remaining_s = BinarySerializer::read_f64(bytes, offset);

	return true;
}

Ref<TntPlayerState> TntPlayerState::duplicate() const {
	Ref<TntPlayerState> dup;
	dup.instantiate();
	dup->data = data;
	return dup;
}

void TntPlayerState::copy_from(const Ref<TntPlayerState>& other) {
	if (other.is_valid()) {
		data = other->data;
	}
}

Ref<TntPlayerState> TntPlayerState::s_create_interpolated_state(
	const Ref<TntPlayerState>& from,
	const Ref<TntPlayerState>& to,
	double alpha) {

	if (!from.is_valid() || !to.is_valid()) {
		return nullptr;
	}

	// Clamp alpha to [0, 1]
	if (alpha < 0.0) alpha = 0.0;
	if (alpha > 1.0) alpha = 1.0;

	// Start with "from" state
	Ref<TntPlayerState> interp;
	interp.instantiate();
	interp->copy_from(from);

	// Interpolate look direction (spherical for rotations would be better, but linear is simpler)
	Vector2 look_diff = to->data.look_direction - from->data.look_direction;
	interp->data.look_direction = from->data.look_direction + (look_diff * alpha);

	// Interpolate position
	Vector3 pos_diff = to->data.position - from->data.position;
	interp->data.position = from->data.position + (pos_diff * alpha);

	// Don't interpolate: pressed_keys, run_tick, state_flags, num_ticks_grounded
	// These should remain from "from" state for consistency

	return interp;
}

void TntPlayerState::_bind_methods() {
	// Serialization methods
	ClassDB::bind_static_method("TntPlayerState", D_METHOD("get_serialized_size"),
		&TntPlayerState::get_serialized_size);
	ClassDB::bind_method(D_METHOD("serialize"), &TntPlayerState::serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "bytes"), &TntPlayerState::deserialize);

	// Utility methods
	ClassDB::bind_method(D_METHOD("duplicate"), &TntPlayerState::duplicate);
	ClassDB::bind_method(D_METHOD("copy_from", "other"), &TntPlayerState::copy_from);
	ClassDB::bind_static_method("TntPlayerState",
		D_METHOD("s_create_interpolated_state", "from", "to", "alpha"),
		&TntPlayerState::s_create_interpolated_state);

	// Key press methods
	ClassDB::bind_method(D_METHOD("is_forward_pressed"), &TntPlayerState::is_forward_pressed);
	ClassDB::bind_method(D_METHOD("is_left_pressed"), &TntPlayerState::is_left_pressed);
	ClassDB::bind_method(D_METHOD("is_right_pressed"), &TntPlayerState::is_right_pressed);
	ClassDB::bind_method(D_METHOD("is_back_pressed"), &TntPlayerState::is_back_pressed);
	ClassDB::bind_method(D_METHOD("is_jump_pressed"), &TntPlayerState::is_jump_pressed);
	ClassDB::bind_method(D_METHOD("is_crouch_pressed"), &TntPlayerState::is_crouch_pressed);
	ClassDB::bind_method(D_METHOD("is_shoot_pressed"), &TntPlayerState::is_shoot_pressed);

	// State flag methods
	ClassDB::bind_method(D_METHOD("is_player_grounded"), &TntPlayerState::is_player_grounded);
	ClassDB::bind_method(D_METHOD("mut_set_player_grounded", "value"),
		&TntPlayerState::mut_set_player_grounded);
	ClassDB::bind_method(D_METHOD("is_player_crouched"), &TntPlayerState::is_player_crouched);
	ClassDB::bind_method(D_METHOD("mut_set_player_crouched", "value"),
		&TntPlayerState::mut_set_player_crouched);
	ClassDB::bind_method(D_METHOD("is_player_crouchsliding"),
		&TntPlayerState::is_player_crouchsliding);
	ClassDB::bind_method(D_METHOD("mut_set_player_crouchsliding", "value"),
		&TntPlayerState::mut_set_player_crouchsliding);

	// Physics query methods
	ClassDB::bind_method(D_METHOD("get_planar_velocity"), &TntPlayerState::get_planar_velocity);
	ClassDB::bind_method(D_METHOD("get_active_run_time_ticks"),
		&TntPlayerState::get_active_run_time_ticks);

	// Property bindings (all 11 fields)
	ClassDB::bind_method(D_METHOD("set_look_direction", "value"),
		&TntPlayerState::set_look_direction);
	ClassDB::bind_method(D_METHOD("get_look_direction"), &TntPlayerState::get_look_direction);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "look_direction"),
		"set_look_direction", "get_look_direction");

	ClassDB::bind_method(D_METHOD("set_pressed_keys", "value"),
		&TntPlayerState::set_pressed_keys);
	ClassDB::bind_method(D_METHOD("get_pressed_keys"), &TntPlayerState::get_pressed_keys);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pressed_keys"),
		"set_pressed_keys", "get_pressed_keys");

	ClassDB::bind_method(D_METHOD("set_run_tick", "value"), &TntPlayerState::set_run_tick);
	ClassDB::bind_method(D_METHOD("get_run_tick"), &TntPlayerState::get_run_tick);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "run_tick"),
		"set_run_tick", "get_run_tick");

	ClassDB::bind_method(D_METHOD("set_run_started_tick", "value"),
		&TntPlayerState::set_run_started_tick);
	ClassDB::bind_method(D_METHOD("get_run_started_tick"), &TntPlayerState::get_run_started_tick);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "run_started_tick"),
		"set_run_started_tick", "get_run_started_tick");

	ClassDB::bind_method(D_METHOD("set_position", "value"), &TntPlayerState::set_position);
	ClassDB::bind_method(D_METHOD("get_position"), &TntPlayerState::get_position);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"),
		"set_position", "get_position");

	ClassDB::bind_method(D_METHOD("set_velocity", "value"), &TntPlayerState::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &TntPlayerState::get_velocity);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity"),
		"set_velocity", "get_velocity");

	ClassDB::bind_method(D_METHOD("set_state_flags", "value"), &TntPlayerState::set_state_flags);
	ClassDB::bind_method(D_METHOD("get_state_flags"), &TntPlayerState::get_state_flags);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "state_flags"),
		"set_state_flags", "get_state_flags");

	ClassDB::bind_method(D_METHOD("set_ground_normal", "value"),
		&TntPlayerState::set_ground_normal);
	ClassDB::bind_method(D_METHOD("get_ground_normal"), &TntPlayerState::get_ground_normal);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "ground_normal"),
		"set_ground_normal", "get_ground_normal");

	ClassDB::bind_method(D_METHOD("set_num_ticks_grounded", "value"),
		&TntPlayerState::set_num_ticks_grounded);
	ClassDB::bind_method(D_METHOD("get_num_ticks_grounded"), &TntPlayerState::get_num_ticks_grounded);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "num_ticks_grounded"),
		"set_num_ticks_grounded", "get_num_ticks_grounded");

	ClassDB::bind_method(D_METHOD("set_crouchslide_duration_remaining_s", "value"),
		&TntPlayerState::set_crouchslide_duration_remaining_s);
	ClassDB::bind_method(D_METHOD("get_crouchslide_duration_remaining_s"),
		&TntPlayerState::get_crouchslide_duration_remaining_s);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "crouchslide_duration_remaining_s"),
		"set_crouchslide_duration_remaining_s", "get_crouchslide_duration_remaining_s");

	ClassDB::bind_method(D_METHOD("set_crouchslide_transition_remaining_s", "value"),
		&TntPlayerState::set_crouchslide_transition_remaining_s);
	ClassDB::bind_method(D_METHOD("get_crouchslide_transition_remaining_s"),
		&TntPlayerState::get_crouchslide_transition_remaining_s);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "crouchslide_transition_remaining_s"),
		"set_crouchslide_transition_remaining_s", "get_crouchslide_transition_remaining_s");

	ClassDB::bind_method(D_METHOD("set_hangtime_duration_s", "value"),
		&TntPlayerState::set_hangtime_duration_s);
	ClassDB::bind_method(D_METHOD("get_hangtime_duration_s"),
		&TntPlayerState::get_hangtime_duration_s);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hangtime_duration_s"),
		"set_hangtime_duration_s", "get_hangtime_duration_s");

	ClassDB::bind_method(D_METHOD("set_doublejump_window_remaining_s", "value"),
		&TntPlayerState::set_doublejump_window_remaining_s);
	ClassDB::bind_method(D_METHOD("get_doublejump_window_remaining_s"),
		&TntPlayerState::get_doublejump_window_remaining_s);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "doublejump_window_remaining_s"),
		"set_doublejump_window_remaining_s", "get_doublejump_window_remaining_s");

	// Constants - Serialization
	BIND_CONSTANT(SERIALIZED_SIZE);

	// Constants - Key flags
	BIND_CONSTANT(F_FIDX);
	BIND_CONSTANT(F_LIDX);
	BIND_CONSTANT(F_RIDX);
	BIND_CONSTANT(F_BIDX);
	BIND_CONSTANT(F_JIDX);
	BIND_CONSTANT(F_CIDX);
	BIND_CONSTANT(F_SIDX);

	// Constants - State flags
	BIND_CONSTANT(F_IS_GROUNDED_IDX);
	BIND_CONSTANT(F_IS_CROUCHED_IDX);
	BIND_CONSTANT(F_IS_CROUCHSLIDING_IDX);
	BIND_CONSTANT(F_IS_GROUNDED);
	BIND_CONSTANT(F_IS_CROUCHED);
	BIND_CONSTANT(F_IS_CROUCHSLIDING);
}

} // namespace godot
