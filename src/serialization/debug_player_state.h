#pragma once

#include <array>
#include <cstdint>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include "serialization_helpers.h"

using namespace godot;

/**
 * @struct DebugPlayerStatePOD
 * @brief Plain Old Data struct for debug player state (variable-size, pre-allocated).
 *
 * Captures physics step debugging information to help diagnose non-determinism issues.
 * Uses pre-allocated fixed arrays to avoid dynamic allocations during serialization/deserialization
 * in the hot 125Hz physics tick path.
 *
 * Variable-length arrays:
 * - dbg_loop_positions: positions at each physics iteration (max 256)
 * - dbg_loop_velocities: velocities at each physics iteration (max 256)
 *
 * Fixed header (108 bytes) + variable arrays (up to 12,288 bytes for 256 elements each)
 */
struct DebugPlayerStatePOD {
	static constexpr size_t MAX_LOOP_ELEMENTS = 256;

	// Fixed header size (bytes before loop_positions array)
	static constexpr size_t HEADER_PART1_SIZE =
		SerializedSize::U32 +     // dbg_run_tick
		SerializedSize::VEC3 +    // dbg_start_tick_pos
		SerializedSize::VEC3;     // dbg_post_grounddetect_pos

	// Size between loop_positions and loop_velocities
	static constexpr size_t HEADER_PART2_SIZE =
		SerializedSize::VEC3 +    // dbg_end_tick_pos
		SerializedSize::VEC3 +    // dbg_start_tick_vel
		SerializedSize::U8 +      // dbg_accel_type
		SerializedSize::F64 +     // dbg_accel_out
		SerializedSize::VEC3;     // dbg_post_accel_vel

	// Minimum serialized size (empty arrays)
	static constexpr size_t MIN_SERIALIZED_SIZE =
		HEADER_PART1_SIZE +
		SerializedSize::U32 +     // loop_positions count
		HEADER_PART2_SIZE +
		SerializedSize::U32;      // loop_velocities count

	// Maximum serialized size (all 256 elements in both arrays)
	static constexpr size_t MAX_SERIALIZED_SIZE =
		HEADER_PART1_SIZE +
		SerializedSize::U32 + (SerializedSize::VEC3 * MAX_LOOP_ELEMENTS) +  // loop_positions
		HEADER_PART2_SIZE +
		SerializedSize::U32 + (SerializedSize::VEC3 * MAX_LOOP_ELEMENTS);   // loop_velocities

	// Fixed fields
	uint32_t dbg_run_tick = 0;
	Vector3 dbg_start_tick_pos = Vector3(0, 0, 0);
	Vector3 dbg_post_grounddetect_pos = Vector3(0, 0, 0);
	Vector3 dbg_end_tick_pos = Vector3(0, 0, 0);
	Vector3 dbg_start_tick_vel = Vector3(0, 0, 0);
	uint8_t dbg_accel_type = 0;
	double dbg_accel_out = 0.0;
	Vector3 dbg_post_accel_vel = Vector3(0, 0, 0);

	// Variable-length arrays (pre-allocated, zero-allocation serialization)
	std::array<Vector3, MAX_LOOP_ELEMENTS> dbg_loop_positions{};
	uint32_t dbg_loop_positions_count = 0;

	std::array<Vector3, MAX_LOOP_ELEMENTS> dbg_loop_velocities{};
	uint32_t dbg_loop_velocities_count = 0;
};

/**
 * @class DebugPlayerState
 * @brief GDExtension wrapper for DebugPlayerStatePOD with variable-length array support.
 *
 * Provides serialization/deserialization with zero allocations in the physics tick hot path.
 * Arrays are pre-allocated to MAX_LOOP_ELEMENTS, with actual sizes tracked separately.
 */
class DebugPlayerState : public RefCounted {
	GDCLASS(DebugPlayerState, RefCounted)

public:
	// Constants
	static constexpr int MAX_LOOP_ELEMENTS = DebugPlayerStatePOD::MAX_LOOP_ELEMENTS;
	static constexpr int ACCEL_TYPE_GROUND = 0;
	static constexpr int ACCEL_TYPE_CROUCHSLIDE = 1;
	static constexpr int ACCEL_TYPE_AIR = 2;

	// The actual data
	DebugPlayerStatePOD data{};

	DebugPlayerState() = default;
	~DebugPlayerState() = default;

	// ========================================================================
	// Serialization Methods
	// ========================================================================

	/**
	 * Get the maximum serialized size of debug player state.
	 * @return Maximum serialized size (worst case with all 256 elements)
	 */
	static int get_max_serialized_size() {
		return static_cast<int>(DebugPlayerStatePOD::MAX_SERIALIZED_SIZE);
	}

	/**
	 * Get the actual serialized size based on current array counts.
	 * @return Actual serialized size for current state
	 */
	int get_serialized_size() const;

	/**
	 * Serialize this debug player state to a byte array.
	 * @return PackedByteArray containing serialized data
	 */
	PackedByteArray serialize() const;

	/**
	 * Deserialize debug player state from a byte array.
	 * @param bytes The byte array to deserialize from
	 * @return True if deserialization succeeded, false otherwise
	 */
	bool deserialize(const PackedByteArray& bytes);

	// ========================================================================
	// Utility Methods
	// ========================================================================

	/**
	 * Create a copy of this debug player state.
	 * @return A new DebugPlayerState instance with the same values
	 */
	Ref<DebugPlayerState> duplicate() const;

	/**
	 * Copy values from another debug player state.
	 * @param other The debug player state to copy from
	 */
	void copy_from(const Ref<DebugPlayerState>& other);

	// ========================================================================
	// Array Conversion Methods (for GDScript compatibility)
	// ========================================================================

	void set_loop_positions(const PackedVector3Array& positions);
	PackedVector3Array get_loop_positions() const;

	void set_loop_velocities(const PackedVector3Array& velocities);
	PackedVector3Array get_loop_velocities() const;

	// ========================================================================
	// Property Accessors (for GDScript)
	// ========================================================================

	void set_dbg_run_tick(int value) { data.dbg_run_tick = static_cast<uint32_t>(value); }
	int get_dbg_run_tick() const { return static_cast<int>(data.dbg_run_tick); }

	void set_dbg_start_tick_pos(const Vector3& value) { data.dbg_start_tick_pos = value; }
	Vector3 get_dbg_start_tick_pos() const { return data.dbg_start_tick_pos; }

	void set_dbg_post_grounddetect_pos(const Vector3& value) { data.dbg_post_grounddetect_pos = value; }
	Vector3 get_dbg_post_grounddetect_pos() const { return data.dbg_post_grounddetect_pos; }

	void set_dbg_end_tick_pos(const Vector3& value) { data.dbg_end_tick_pos = value; }
	Vector3 get_dbg_end_tick_pos() const { return data.dbg_end_tick_pos; }

	void set_dbg_start_tick_vel(const Vector3& value) { data.dbg_start_tick_vel = value; }
	Vector3 get_dbg_start_tick_vel() const { return data.dbg_start_tick_vel; }

	void set_dbg_accel_type(int value) { data.dbg_accel_type = static_cast<uint8_t>(value & 0xFF); }
	int get_dbg_accel_type() const { return static_cast<int>(data.dbg_accel_type); }

	void set_dbg_accel_out(double value) { data.dbg_accel_out = value; }
	double get_dbg_accel_out() const { return data.dbg_accel_out; }

	void set_dbg_post_accel_vel(const Vector3& value) { data.dbg_post_accel_vel = value; }
	Vector3 get_dbg_post_accel_vel() const { return data.dbg_post_accel_vel; }

protected:
	static void _bind_methods();
};