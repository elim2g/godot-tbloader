#pragma once

#include <cstdint>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "serialization_helpers.h"

using namespace godot;

/**
 * @struct TntPlayerStatePOD
 * @brief Plain Old Data struct for player state serialization.
 *
 * Stores the complete player physics state at a single game tick, including:
 * - Input state: look direction and pressed keys
 * - Movement state: position, velocity, run progress
 * - Ground state: ground normal, grounded flag, slide state
 * - Timing: jump window, hangtime, etc.
 *
 * This is the hot path for demo recording (serialized every tick at 125Hz).
 * See SERIALIZED_SIZE for the computed size.
 */
struct TntPlayerStatePOD {
	// ========================================================================
	// Inputs (17 bytes)
	// ========================================================================

	Vector2 look_direction;    // 16 bytes: x=yaw angle, y=pitch angle (both f64)
	uint8_t pressed_keys;      // 1 byte: bit flags for F, L, R, B, J, C, S keys

	// ========================================================================
	// Movement State (56 bytes)
	// ========================================================================

	uint32_t run_tick;          // 4 bytes: current tick counter
	uint32_t run_started_tick;  // 4 bytes: tick when run started (-1 if not started)
	uint32_t run_finished_tick; // 4 bytes: tick when run finished (-1 if not finished)
	Vector3 position;           // 24 bytes: world position (3xf64)
	Vector3 velocity;           // 24 bytes: world velocity (3xf64)

	// ========================================================================
	// Ground State (50 bytes)
	// ========================================================================

	uint8_t state_flags;       // 1 byte: grounded, crouched, crouchsliding flags
	Vector3 ground_normal;     // 24 bytes: surface normal of ground contact (3xf64)
	uint8_t num_ticks_grounded; // 1 byte: how many ticks player has been grounded

	double crouchslide_duration_remaining_s;      // 8 bytes: time left in crouch slide
	double crouchslide_transition_remaining_s;    // 8 bytes: transition fade time remaining

	// ========================================================================
	// Timing/Physics
	// ========================================================================

	double hangtime_duration_s;                   // air time counter
	double doublejump_window_remaining_s;         // time to perform double jump
	double _padding1;                             // reserved for future use

	// ========================================================================
	// Serialized size computed from field types (auto-updates if fields change)
	// ========================================================================

	static constexpr size_t SERIALIZED_SIZE =
		// Inputs
		SerializedSize::VEC2 +  // look_direction
		SerializedSize::U8 +    // pressed_keys
		// Movement state
		SerializedSize::U32 +   // run_tick
		SerializedSize::U32 +   // run_started_tick
		SerializedSize::U32 +   // run_finished_tick
		SerializedSize::VEC3 +  // position
		SerializedSize::VEC3 +  // velocity
		// Ground state
		SerializedSize::U8 +    // state_flags
		SerializedSize::VEC3 +  // ground_normal
		SerializedSize::U8 +    // num_ticks_grounded
		SerializedSize::F64 +   // crouchslide_duration_remaining_s
		SerializedSize::F64 +   // crouchslide_transition_remaining_s
		// Timing/Physics
		SerializedSize::F64 +   // hangtime_duration_s
		SerializedSize::F64;    // doublejump_window_remaining_s
		// Note: _padding1 is NOT serialized (internal field only)
};

/**
 * @class TntPlayerState
 * @brief GDExtension wrapper for TntPlayerStatePOD with physics-related utilities.
 *
 * Exposes:
 * - All 13 fields as GDScript properties
 * - Serialization to/from PackedByteArray (compile-time size)
 * - Physics utility methods (key presses, state flags, interpolation)
 * - Optimization methods (copy_from to avoid allocations)
 */
class TntPlayerState : public RefCounted {
	GDCLASS(TntPlayerState, RefCounted)

public:
	// Serialized size constant (exposed to GDScript)
	static constexpr int SERIALIZED_SIZE = TntPlayerStatePOD::SERIALIZED_SIZE;

	// ========================================================================
	// Key/Button Flags
	// ========================================================================

	static constexpr int F_FIDX = (1 << 0);  // Forward
	static constexpr int F_LIDX = (1 << 1);  // Left
	static constexpr int F_RIDX = (1 << 2);  // Right
	static constexpr int F_BIDX = (1 << 3);  // Backward
	static constexpr int F_JIDX = (1 << 4);  // Jump
	static constexpr int F_CIDX = (1 << 5);  // Crouch
	static constexpr int F_SIDX = (1 << 6);  // Shoot (unused but kept for future)

	// ========================================================================
	// State Flags (bits in state_flags field)
	// ========================================================================

	static constexpr int F_IS_GROUNDED_IDX = 0;
	static constexpr int F_IS_CROUCHED_IDX = 1;
	static constexpr int F_IS_CROUCHSLIDING_IDX = 2;

	static constexpr int F_IS_GROUNDED = (1 << F_IS_GROUNDED_IDX);
	static constexpr int F_IS_CROUCHED = (1 << F_IS_CROUCHED_IDX);
	static constexpr int F_IS_CROUCHSLIDING = (1 << F_IS_CROUCHSLIDING_IDX);

	// The actual data
	TntPlayerStatePOD data{};

	TntPlayerState() = default;
	~TntPlayerState() = default;

	// ========================================================================
	// Serialization Methods
	// ========================================================================

	/**
	 * Get the serialized size of player state.
	 * @return Compile-time constant size in bytes
	 */
	static int get_serialized_size() {
		return static_cast<int>(TntPlayerStatePOD::SERIALIZED_SIZE);
	}

	/**
	 * Serialize this player state to a byte array.
	 * @return PackedByteArray containing serialized data (SERIALIZED_SIZE bytes)
	 */
	PackedByteArray serialize() const;

	/**
	 * Deserialize player state from a byte array.
	 * @param bytes The byte array to deserialize from (must be at least SERIALIZED_SIZE bytes)
	 * @return True if deserialization succeeded, false if array is too small
	 */
	bool deserialize(const PackedByteArray& bytes);

	// ========================================================================
	// Utility Methods
	// ========================================================================

	/**
	 * Create a copy of this player state.
	 * @return A new TntPlayerState instance with the same values
	 */
	Ref<TntPlayerState> duplicate() const;

	/**
	 * Copy values from another player state (performance optimization).
	 * More efficient than duplicate() when reusing an existing instance.
	 * @param other The player state to copy from
	 */
	void copy_from(const Ref<TntPlayerState>& other);

	/**
	 * Create an interpolated player state between two states.
	 * Useful for smooth rendering between game ticks.
	 * @param from The "before" state
	 * @param to The "after" state
	 * @param alpha Interpolation factor (0.0 = from, 0.5 = midpoint, 1.0 = to)
	 * @return Interpolated player state, or null if inputs are invalid
	 */
	static Ref<TntPlayerState> s_create_interpolated_state(
		const Ref<TntPlayerState>& from,
		const Ref<TntPlayerState>& to,
		double alpha);

	// ========================================================================
	// Key Press Check Methods
	// ========================================================================

	bool is_forward_pressed() const { return (data.pressed_keys & F_FIDX) != 0; }
	bool is_left_pressed() const { return (data.pressed_keys & F_LIDX) != 0; }
	bool is_right_pressed() const { return (data.pressed_keys & F_RIDX) != 0; }
	bool is_back_pressed() const { return (data.pressed_keys & F_BIDX) != 0; }
	bool is_jump_pressed() const { return (data.pressed_keys & F_JIDX) != 0; }
	bool is_crouch_pressed() const { return (data.pressed_keys & F_CIDX) != 0; }
	bool is_shoot_pressed() const { return (data.pressed_keys & F_SIDX) != 0; }

	// ========================================================================
	// State Flag Check/Mutate Methods
	// ========================================================================

	bool is_player_grounded() const {
		return (data.state_flags & F_IS_GROUNDED) != 0;
	}

	void mut_set_player_grounded(bool value) {
		data.state_flags = (data.state_flags & ~F_IS_GROUNDED) |
		                   (value ? F_IS_GROUNDED : 0);
	}

	bool is_player_crouched() const {
		return (data.state_flags & F_IS_CROUCHED) != 0;
	}

	void mut_set_player_crouched(bool value) {
		data.state_flags = (data.state_flags & ~F_IS_CROUCHED) |
		                   (value ? F_IS_CROUCHED : 0);
	}

	bool is_player_crouchsliding() const {
		return (data.state_flags & F_IS_CROUCHSLIDING) != 0;
	}

	void mut_set_player_crouchsliding(bool value) {
		data.state_flags = (data.state_flags & ~F_IS_CROUCHSLIDING) |
		                   (value ? F_IS_CROUCHSLIDING : 0);
	}

	// ========================================================================
	// Physics Query Methods
	// ========================================================================

	Vector2 get_planar_velocity() const {
		return Vector2(data.velocity.x, data.velocity.z);
	}

	int get_active_run_time_ticks() const {
		if (data.run_started_tick == static_cast<uint32_t>(-1)) {
			return 0;
		}
		// If the run is finished, return the final time
		if (data.run_finished_tick != static_cast<uint32_t>(-1)) {
			return static_cast<int>(data.run_finished_tick - data.run_started_tick);
		}
		// Otherwise return time since start
		return static_cast<int>(data.run_tick - data.run_started_tick);
	}

	// ========================================================================
	// Property Accessors (for GDScript)
	// ========================================================================

	void set_look_direction(const Vector2& value) { data.look_direction = value; }
	Vector2 get_look_direction() const { return data.look_direction; }

	void set_pressed_keys(int value) { data.pressed_keys = static_cast<uint8_t>(value & 0xFF); }
	int get_pressed_keys() const { return static_cast<int>(data.pressed_keys); }

	void set_run_tick(int value) { data.run_tick = static_cast<uint32_t>(value); }
	int get_run_tick() const { return static_cast<int>(data.run_tick); }

	void set_run_started_tick(int value) { data.run_started_tick = static_cast<uint32_t>(value); }
	int get_run_started_tick() const { return static_cast<int>(data.run_started_tick); }

	void set_run_finished_tick(int value) { data.run_finished_tick = static_cast<uint32_t>(value); }
	int get_run_finished_tick() const { return static_cast<int>(data.run_finished_tick); }

	void set_position(const Vector3& value) { data.position = value; }
	Vector3 get_position() const { return data.position; }

	void set_velocity(const Vector3& value) { data.velocity = value; }
	Vector3 get_velocity() const { return data.velocity; }

	void set_state_flags(int value) { data.state_flags = static_cast<uint8_t>(value & 0xFF); }
	int get_state_flags() const { return static_cast<int>(data.state_flags); }

	void set_ground_normal(const Vector3& value) { data.ground_normal = value; }
	Vector3 get_ground_normal() const { return data.ground_normal; }

	void set_num_ticks_grounded(int value) { data.num_ticks_grounded = static_cast<uint8_t>(value & 0xFF); }
	int get_num_ticks_grounded() const { return static_cast<int>(data.num_ticks_grounded); }

	void set_crouchslide_duration_remaining_s(double value) { data.crouchslide_duration_remaining_s = value; }
	double get_crouchslide_duration_remaining_s() const { return data.crouchslide_duration_remaining_s; }

	void set_crouchslide_transition_remaining_s(double value) { data.crouchslide_transition_remaining_s = value; }
	double get_crouchslide_transition_remaining_s() const { return data.crouchslide_transition_remaining_s; }

	void set_hangtime_duration_s(double value) { data.hangtime_duration_s = value; }
	double get_hangtime_duration_s() const { return data.hangtime_duration_s; }

	void set_doublejump_window_remaining_s(double value) { data.doublejump_window_remaining_s = value; }
	double get_doublejump_window_remaining_s() const { return data.doublejump_window_remaining_s; }

protected:
	static void _bind_methods();
};