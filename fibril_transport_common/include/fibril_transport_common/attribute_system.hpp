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

// ========================================
// Built-in Attribute Types
// ========================================

/**
 * @brief #[ros_type("geometry_msgs/msg/Twist")]
 * 
 * Maps a Fibril struct to a ROS message type.
 */
class RosTypeAttribute : public AttributeBase
{
public:
  std::string ros_message_type;

  explicit RosTypeAttribute(std::string type);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosTypeAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros_map("linear.x")]
 * 
 * Maps a Fibril field to a ROS message field path.
 */
class RosMapAttribute : public AttributeBase
{
public:
  std::string field_path;

  explicit RosMapAttribute(std::string path);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosMapAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros("/cmd_vel")] or #[ros("~/voltage")]
 * 
 * Specifies the ROS topic name for a port.
 */
class RosAttribute : public AttributeBase
{
public:
  std::string topic_name;

  explicit RosAttribute(std::string name);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[unit("m/s")]
 * 
 * Specifies the physical unit of a field (for documentation/visualization).
 */
class UnitAttribute : public AttributeBase
{
public:
  std::string unit;

  explicit UnitAttribute(std::string u);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<UnitAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief #[ros_frame_id("base_link")]
 * 
 * Specifies the ROS frame_id for coordinate frames.
 */
class RosFrameIdAttribute : public AttributeBase
{
public:
  std::string frame_id;

  explicit RosFrameIdAttribute(std::string id);

  std::string name() const override;
  std::vector<uint8_t> serializeToBinary() const override;

  static std::unique_ptr<RosFrameIdAttribute> fromBinary(const std::vector<uint8_t> & data);
};

/**
 * @brief Register all built-in attribute deserializers
 * 
 * This should be called once at program startup.
 */
void registerBuiltinAttributes();

}  // namespace fibril_transport_common
