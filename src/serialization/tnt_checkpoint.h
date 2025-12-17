#pragma once

#include <cstdint>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

using namespace godot;

/**
 * @struct TntCheckpointPOD
 * @brief Plain Old Data struct for checkpoint serialization (5 bytes).
 *
 * Represents a checkpoint/split point in a demo with the checkpoint ID and
 * the tick at which it was crossed.
 */
struct TntCheckpointPOD {
	uint8_t checkpoint_id;   // 1 byte: checkpoint identifier (0-255)
	int32_t tick_achieved;   // 4 bytes: tick number when crossed

	static constexpr size_t SERIALIZED_SIZE = 5;

	static_assert(sizeof(uint8_t) == 1, "uint8_t size check");
	static_assert(sizeof(int32_t) == 4, "int32_t size check");
};

/**
 * @class TntCheckpoint
 * @brief GDExtension wrapper for TntCheckpointPOD exposing serialization and manipulation.
 *
 * This class wraps the POD struct and provides:
 * - Serialization to/from PackedByteArray
 * - Property access from GDScript
 * - Constants for special values
 */
class TntCheckpoint : public RefCounted {
	GDCLASS(TntCheckpoint, RefCounted)

public:
	// Serialized size constant (exposed to GDScript)
	static constexpr int SERIALIZED_SIZE = TntCheckpointPOD::SERIALIZED_SIZE;

	// Constants for special values
	static constexpr int CHECKPOINT_ID_UNKNOWN = -1;
	static constexpr int CHECKPOINT_NOT_CROSSED = -1;

	// The actual data
	TntCheckpointPOD data{};

	TntCheckpoint() = default;
	~TntCheckpoint() = default;

	// ========================================================================
	// Serialization Methods
	// ========================================================================

	/**
	 * Get the size of serialized checkpoint data.
	 * @return The fixed serialized size (5 bytes)
	 */
	static int get_serialized_size() {
		return static_cast<int>(TntCheckpointPOD::SERIALIZED_SIZE);
	}

	/**
	 * Serialize this checkpoint to a byte array.
	 * @return PackedByteArray containing serialized data (5 bytes)
	 */
	PackedByteArray serialize() const;

	/**
	 * Deserialize checkpoint data from a byte array.
	 * @param bytes The byte array to deserialize from (must be at least 5 bytes)
	 * @return True if deserialization succeeded, false if array is too small
	 */
	bool deserialize(const PackedByteArray& bytes);

	// ========================================================================
	// Utility Methods
	// ========================================================================

	/**
	 * Create a copy of this checkpoint.
	 * @return A new TntCheckpoint instance with the same values
	 */
	Ref<TntCheckpoint> duplicate() const;

	/**
	 * Copy values from another checkpoint.
	 * @param other The checkpoint to copy from
	 */
	void copy_from(const Ref<TntCheckpoint>& other);

	// ========================================================================
	// Property Accessors (for GDScript)
	// ========================================================================

	void set_checkpoint_id(int id) { data.checkpoint_id = static_cast<uint8_t>(id & 0xFF); }
	int get_checkpoint_id() const { return static_cast<int>(data.checkpoint_id); }

	void set_tick_achieved(int tick) { data.tick_achieved = tick; }
	int get_tick_achieved() const { return data.tick_achieved; }

protected:
	static void _bind_methods();
};