#pragma once

#include <cstdint>
#include <cstring>

#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

/**
 * @brief Compile-time size constants for binary serialization.
 *
 * These constants define the serialized byte sizes for each primitive and
 * vector type. Use these to compute SERIALIZED_SIZE in POD structs so that
 * sizes automatically update if field types change.
 *
 * Example usage:
 *   static constexpr size_t SERIALIZED_SIZE =
 *       SerializedSize::U32 +    // run_tick
 *       SerializedSize::VEC3 +   // position
 *       SerializedSize::F64;     // time_remaining
 */
namespace SerializedSize {
	// Primitive types
	static constexpr size_t U8  = sizeof(uint8_t);   // 1 byte
	static constexpr size_t U16 = sizeof(uint16_t);  // 2 bytes
	static constexpr size_t U32 = sizeof(uint32_t);  // 4 bytes
	static constexpr size_t I32 = sizeof(int32_t);   // 4 bytes
	static constexpr size_t I64 = sizeof(int64_t);   // 8 bytes
	static constexpr size_t F32 = sizeof(float);     // 4 bytes
	static constexpr size_t F64 = sizeof(double);    // 8 bytes

	// Vector types (serialized as consecutive f64 components)
	static constexpr size_t VEC2 = F64 * 2;  // 16 bytes (2 doubles)
	static constexpr size_t VEC3 = F64 * 3;  // 24 bytes (3 doubles)
}

/**
 * @class BinarySerializer
 * @brief Helper class for binary serialization/deserialization with little-endian byte order.
 *
 * Provides static methods to read and write primitive types and Godot vector types
 * to/from PackedByteArray with explicit little-endian handling.
 *
 * Performance: Uses memcpy for multi-byte writes/reads. On little-endian systems
 * (Windows, most Linux, macOS), endianness conversion is a no-op at compile time.
 */
class BinarySerializer {
public:
	// ========================================================================
	// Write Methods (primitive types)
	// ========================================================================

	static void write_u8(PackedByteArray& arr, size_t& offset, uint8_t value);
	static void write_u16(PackedByteArray& arr, size_t& offset, uint16_t value);
	static void write_u32(PackedByteArray& arr, size_t& offset, uint32_t value);
	static void write_i32(PackedByteArray& arr, size_t& offset, int32_t value);
	static void write_i64(PackedByteArray& arr, size_t& offset, int64_t value);
	static void write_f64(PackedByteArray& arr, size_t& offset, double value);

	// ========================================================================
	// Write Methods (bulk and vector)
	// ========================================================================

	/**
	 * Write raw bytes from source array to destination at offset.
	 * Copies count bytes via memcpy for efficiency.
	 * @param dest Destination array (must be pre-sized to accommodate offset + count)
	 * @param offset Current write position (updated after write)
	 * @param src Source byte array to copy from
	 * @param count Number of bytes to copy (if 0, copies entire src)
	 */
	static void write_bytes(PackedByteArray& dest, size_t& offset, const PackedByteArray& src, size_t count = 0);

	/**
	 * Write Godot vector types to byte array (as consecutive f64 components)
	 */
	static void write_vec2_f64(PackedByteArray& arr, size_t& offset, const Vector2& vec);
	static void write_vec3_f64(PackedByteArray& arr, size_t& offset, const Vector3& vec);

	// ========================================================================
	// Read Methods (primitive types)
	// ========================================================================

	static uint8_t read_u8(const PackedByteArray& arr, size_t& offset);
	static uint16_t read_u16(const PackedByteArray& arr, size_t& offset);
	static uint32_t read_u32(const PackedByteArray& arr, size_t& offset);
	static int32_t read_i32(const PackedByteArray& arr, size_t& offset);
	static int64_t read_i64(const PackedByteArray& arr, size_t& offset);
	static double read_f64(const PackedByteArray& arr, size_t& offset);

	// ========================================================================
	// Read Methods (vector)
	// ========================================================================

	static Vector2 read_vec2_f64(const PackedByteArray& arr, size_t& offset);
	static Vector3 read_vec3_f64(const PackedByteArray& arr, size_t& offset);

private:
	// ========================================================================
	// Endianness Helpers
	// ========================================================================

	static uint16_t to_little_endian_u16(uint16_t value);
	static uint16_t from_little_endian_u16(uint16_t value);

	static uint32_t to_little_endian_u32(uint32_t value);
	static uint32_t from_little_endian_u32(uint32_t value);

	static uint64_t to_little_endian_u64(uint64_t value);
	static uint64_t from_little_endian_u64(uint64_t value);

	static double to_little_endian_f64(double value);
	static double from_little_endian_f64(double value);

	/**
	 * Check if host is little-endian at compile time.
	 * Windows, x86/x64 Linux, and ARM macOS are all little-endian.
	 */
	static constexpr bool is_little_endian() {
#if defined(_MSC_VER) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
		return true;
#else
		return false;
#endif
	}
};