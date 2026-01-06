#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fibril_transport_common
{

/**
 * @brief Base class for all attribute types
 * 
 * Each attribute type (ros_type, ros_map, etc.) derives from this.
 * Attributes are serialized to binary format for definition.bin metadata.
 */
class AttributeBase
{
public:
  virtual ~AttributeBase() = default;

  /**
     * @brief Get attribute name (e.g., "ros_type")
     */
  virtual std::string name() const = 0;

  /**
     * @brief Serialize to binary format (for definition.bin metadata)
     * 
     * Format: attribute-specific binary encoding
     * Each attribute type defines its own serialization format.
     * 
     * @return Binary representation of attribute value
     */
  virtual std::vector<uint8_t> serializeToBinary() const = 0;

  /**
     * @brief Get metadata key (for binary format)
     * 
     * Usually same as name(), but can be different for compatibility.
     */
  virtual std::string metadataKey() const { return name(); }
};

/**
 * @brief Attribute list container (move-only)
 * 
 * Contains a collection of attributes with type-safe lookup.
 */
class AttributeList
{
public:
  AttributeList() = default;

  // Move-only
  AttributeList(AttributeList &&) noexcept = default;
  AttributeList & operator=(AttributeList &&) noexcept = default;
  AttributeList(const AttributeList &) = delete;
  AttributeList & operator=(const AttributeList &) = delete;

  /**
     * @brief Add an attribute
     */
  void add(std::unique_ptr<AttributeBase> attr);

  /**
     * @brief Find first attribute of specific type
     * @return Pointer to attribute, or nullptr if not found
     */
  template <typename T>
  const T * find() const
  {
    for (const auto & attr : attributes_) {
      if (auto * typed = dynamic_cast<const T *>(attr.get())) {
        return typed;
      }
    }
    return nullptr;
  }

  /**
     * @brief Find all attributes of specific type
     */
  template <typename T>
  std::vector<const T *> findAll() const
  {
    std::vector<const T *> result;
    for (const auto & attr : attributes_) {
      if (auto * typed = dynamic_cast<const T *>(attr.get())) {
        result.push_back(typed);
      }
    }
    return result;
  }

  /**
     * @brief Serialize all attributes to binary metadata format
     * 
     * Returns a complete binary sequence of metadata entries.
     * Format: [Count(1)] [Entry1] [Entry2] ...
     * Each entry: [KeyLen(1)] [Key] [ValueLen(2)] [Value]
     * 
     * @return Complete binary representation of all attributes
     */
  std::vector<uint8_t> serializeToBinary() const;

  /**
     * @brief Deserialize from binary metadata format
     * 
     * @param data Binary data containing metadata entries
     * @param offset Starting offset in data (will be updated)
     * @return Deserialized attribute list
     */
  static AttributeList deserializeFromBinary(const std::vector<uint8_t> & data, size_t & offset);

  /**
     * @brief Get number of attributes
     */
  size_t size() const { return attributes_.size(); }

  /**
     * @brief Check if empty
     */
  bool empty() const { return attributes_.empty(); }

private:
  std::vector<std::unique_ptr<AttributeBase>> attributes_;
};

/**
 * @brief Factory for creating attributes from binary metadata
 */
class AttributeFactory
{
public:
  /**
     * @brief Create attribute from binary metadata
     * @param key Metadata key (attribute name)
     * @param value Binary value data
     * @return Unique pointer to attribute, or nullptr if unknown type
     */
  static std::unique_ptr<AttributeBase> fromBinary(
    const std::string & key, const std::vector<uint8_t> & value);

  /**
     * @brief Register a custom attribute deserializer
     */
  using DeserializerFunc =
    std::function<std::unique_ptr<AttributeBase>(const std::vector<uint8_t> &)>;
  static void registerDeserializer(const std::string & key, DeserializerFunc func);

private:
  static std::map<std::string, DeserializerFunc> & getDeserializers();
};

}  // namespace fibril_transport_common
