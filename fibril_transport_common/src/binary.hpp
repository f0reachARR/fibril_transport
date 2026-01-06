#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace fibril_transport_common
{

// Binary write helpers
inline void writeUInt8(std::vector<uint8_t> & buffer, uint8_t value) { buffer.push_back(value); }

inline void writeUInt16(std::vector<uint8_t> & buffer, uint16_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

inline void writeUInt32(std::vector<uint8_t> & buffer, uint32_t value)
{
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline void writeString(std::vector<uint8_t> & buffer, const std::string & str)
{
  buffer.insert(buffer.end(), str.begin(), str.end());
}

inline void writeBytes(std::vector<uint8_t> & buffer, const std::vector<uint8_t> & bytes)
{
  buffer.insert(buffer.end(), bytes.begin(), bytes.end());
}

// Binary read helpers
inline uint8_t readUInt8(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint8");
  }
  return data[offset++];
}

inline uint16_t readUInt16(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset + 1 >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint16");
  }
  uint16_t value = data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8);
  offset += 2;
  return value;
}

inline uint32_t readUInt32(const std::vector<uint8_t> & data, size_t & offset)
{
  if (offset + 3 >= data.size()) {
    throw std::runtime_error("Buffer underflow reading uint32");
  }
  uint32_t value = data[offset] | (static_cast<uint32_t>(data[offset + 1]) << 8) |
                   (static_cast<uint32_t>(data[offset + 2]) << 16) |
                   (static_cast<uint32_t>(data[offset + 3]) << 24);
  offset += 4;
  return value;
}

inline std::string readString(const std::vector<uint8_t> & data, size_t & offset, size_t length)
{
  if (offset + length > data.size()) {
    throw std::runtime_error("Buffer underflow reading string");
  }
  std::string str(reinterpret_cast<const char *>(&data[offset]), length);
  offset += length;
  return str;
}

inline std::vector<uint8_t> readBytes(
  const std::vector<uint8_t> & data, size_t & offset, size_t length)
{
  if (offset + length > data.size()) {
    throw std::runtime_error("Buffer underflow reading bytes");
  }
  std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + length);
  offset += length;
  return bytes;
}
}  // namespace fibril_transport_common