#include "serialization_helpers.h"



// ============================================================================
// Write Methods
// ============================================================================

void BinarySerializer::write_u8(PackedByteArray& arr, size_t& offset, uint8_t value) {
	arr[offset] = value;
	offset += 1;
}



void BinarySerializer::write_u16(PackedByteArray& arr, size_t& offset, uint16_t value) {
	uint16_t le_value = to_little_endian_u16(value);
	memcpy(arr.ptrw() + offset, &le_value, 2);
	offset += 2;
}



void BinarySerializer::write_u32(PackedByteArray& arr, size_t& offset, uint32_t value) {
	uint32_t le_value = to_little_endian_u32(value);
	memcpy(arr.ptrw() + offset, &le_value, 4);
	offset += 4;
}



void BinarySerializer::write_i32(PackedByteArray& arr, size_t& offset, int32_t value) {
	write_u32(arr, offset, static_cast<uint32_t>(value));
}



void BinarySerializer::write_i64(PackedByteArray& arr, size_t& offset, int64_t value) {
	uint64_t le_value = to_little_endian_u64(static_cast<uint64_t>(value));
	memcpy(arr.ptrw() + offset, &le_value, 8);
	offset += 8;
}



void BinarySerializer::write_f64(PackedByteArray& arr, size_t& offset, double value) {
	double le_value = to_little_endian_f64(value);
	memcpy(arr.ptrw() + offset, &le_value, 8);
	offset += 8;
}



void BinarySerializer::write_bytes(PackedByteArray& dest, size_t& offset, const PackedByteArray& src, size_t count) {
	size_t bytes_to_copy = (count == 0) ? src.size() : count;
	if (bytes_to_copy == 0) {
		return;
	}
	memcpy(dest.ptrw() + offset, src.ptr(), bytes_to_copy);
	offset += bytes_to_copy;
}



void BinarySerializer::write_vec2_f64(PackedByteArray& arr, size_t& offset, const Vector2& vec) {
	write_f64(arr, offset, vec.x);
	write_f64(arr, offset, vec.y);
}



void BinarySerializer::write_vec3_f64(PackedByteArray& arr, size_t& offset, const Vector3& vec) {
	write_f64(arr, offset, vec.x);
	write_f64(arr, offset, vec.y);
	write_f64(arr, offset, vec.z);
}



// ============================================================================
// Read Methods
// ============================================================================

uint8_t BinarySerializer::read_u8(const PackedByteArray& arr, size_t& offset) {
	uint8_t value = arr[offset];
	offset += 1;
	return value;
}



uint16_t BinarySerializer::read_u16(const PackedByteArray& arr, size_t& offset) {
	uint16_t le_value;
	memcpy(&le_value, arr.ptr() + offset, 2);
	offset += 2;
	return from_little_endian_u16(le_value);
}



uint32_t BinarySerializer::read_u32(const PackedByteArray& arr, size_t& offset) {
	uint32_t le_value;
	memcpy(&le_value, arr.ptr() + offset, 4);
	offset += 4;
	return from_little_endian_u32(le_value);
}



int32_t BinarySerializer::read_i32(const PackedByteArray& arr, size_t& offset) {
	return static_cast<int32_t>(read_u32(arr, offset));
}



int64_t BinarySerializer::read_i64(const PackedByteArray& arr, size_t& offset) {
	uint64_t le_value;
	memcpy(&le_value, arr.ptr() + offset, 8);
	offset += 8;
	return static_cast<int64_t>(from_little_endian_u64(le_value));
}



double BinarySerializer::read_f64(const PackedByteArray& arr, size_t& offset) {
	double le_value;
	memcpy(&le_value, arr.ptr() + offset, 8);
	offset += 8;
	return from_little_endian_f64(le_value);
}



Vector2 BinarySerializer::read_vec2_f64(const PackedByteArray& arr, size_t& offset) {
	double x = read_f64(arr, offset);
	double y = read_f64(arr, offset);
	return Vector2(x, y);
}



Vector3 BinarySerializer::read_vec3_f64(const PackedByteArray& arr, size_t& offset) {
	double x = read_f64(arr, offset);
	double y = read_f64(arr, offset);
	double z = read_f64(arr, offset);
	return Vector3(x, y, z);
}



// ============================================================================
// Endianness Conversion Methods
// ============================================================================

uint16_t BinarySerializer::to_little_endian_u16(uint16_t value) {
	if constexpr (is_little_endian()) {
		return value;
	} else {
		return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
	}
}



uint16_t BinarySerializer::from_little_endian_u16(uint16_t value) {
	return to_little_endian_u16(value);
}



uint32_t BinarySerializer::to_little_endian_u32(uint32_t value) {
	if constexpr (is_little_endian()) {
		return value;
	} else {
		return ((value & 0xFF) << 24) |
		       (((value >> 8) & 0xFF) << 16) |
		       (((value >> 16) & 0xFF) << 8) |
		       ((value >> 24) & 0xFF);
	}
}



uint32_t BinarySerializer::from_little_endian_u32(uint32_t value) {
	return to_little_endian_u32(value);
}



uint64_t BinarySerializer::to_little_endian_u64(uint64_t value) {
	if constexpr (is_little_endian()) {
		return value;
	} else {
		return ((value & 0xFF) << 56) |
		       (((value >> 8) & 0xFF) << 48) |
		       (((value >> 16) & 0xFF) << 40) |
		       (((value >> 24) & 0xFF) << 32) |
		       (((value >> 32) & 0xFF) << 24) |
		       (((value >> 40) & 0xFF) << 16) |
		       (((value >> 48) & 0xFF) << 8) |
		       ((value >> 56) & 0xFF);
	}
}



uint64_t BinarySerializer::from_little_endian_u64(uint64_t value) {
	return to_little_endian_u64(value);
}



double BinarySerializer::to_little_endian_f64(double value) {
	if constexpr (is_little_endian()) {
		return value;
	} else {
		uint64_t bits;
		memcpy(&bits, &value, 8);
		uint64_t swapped = to_little_endian_u64(bits);
		double result;
		memcpy(&result, &swapped, 8);
		return result;
	}
}



double BinarySerializer::from_little_endian_f64(double value) {
	return to_little_endian_f64(value);
}