#pragma once

#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cstdint>
#include <cstring>

namespace godot {

/**
 * @class BinarySerializer
 * @brief Helper class for binary serialization/deserialization with little-endian byte order.
 *
 * Provides static methods to read and write primitive types and Godot vector types
 * to/from PackedByteArray with explicit little-endian handling.
 */
class BinarySerializer {
public:
	/**
	 * Write primitive types to byte array
	 */
	static void write_u8(PackedByteArray& arr, size_t& offset, uint8_t value);
	static void write_u16(PackedByteArray& arr, size_t& offset, uint16_t value);
	static void write_u32(PackedByteArray& arr, size_t& offset, uint32_t value);
	static void write_i32(PackedByteArray& arr, size_t& offset, int32_t value);
	static void write_i64(PackedByteArray& arr, size_t& offset, int64_t value);
	static void write_f64(PackedByteArray& arr, size_t& offset, double value);

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

	/**
	 * Read primitive types from byte array
	 */
	static uint8_t read_u8(const PackedByteArray& arr, size_t& offset);
	static uint16_t read_u16(const PackedByteArray& arr, size_t& offset);
	static uint32_t read_u32(const PackedByteArray& arr, size_t& offset);
	static int32_t read_i32(const PackedByteArray& arr, size_t& offset);
	static int64_t read_i64(const PackedByteArray& arr, size_t& offset);
	static double read_f64(const PackedByteArray& arr, size_t& offset);

	/**
	 * Read Godot vector types from byte array
	 */
	static Vector2 read_vec2_f64(const PackedByteArray& arr, size_t& offset);
	static Vector3 read_vec3_f64(const PackedByteArray& arr, size_t& offset);

private:
	/**
	 * Helper methods for endianness conversion (host <-> little-endian)
	 */
	static uint16_t to_little_endian_u16(uint16_t value);
	static uint16_t from_little_endian_u16(uint16_t value);

	static uint32_t to_little_endian_u32(uint32_t value);
	static uint32_t from_little_endian_u32(uint32_t value);

	static uint64_t to_little_endian_u64(uint64_t value);
	static uint64_t from_little_endian_u64(uint64_t value);

	static double to_little_endian_f64(double value);
	static double from_little_endian_f64(double value);

	/**
	 * Check if host is little-endian at compile time
	 * Windows is always little-endian, most other systems are too
	 */
	static constexpr bool is_little_endian() {
#if defined(_MSC_VER) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
		return true;
#else
		return false;
#endif
	}
};

} // namespace godot
