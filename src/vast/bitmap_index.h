#ifndef VAST_BITMAP_INDEX_H
#define VAST_BITMAP_INDEX_H

#include "vast/operator.h"
#include "vast/optional.h"
#include "vast/util/operators.h"

namespace vast {

class bitstream;
class ewah_bitstream;
class null_bitstream;

/// The abstract base class for bitmap indexes.
class bitmap_index : util::equality_comparable<bitmap_index>
{
public:
  using bitstream_type = ewah_bitstream;

  /// Factory function to construct a bitmap index for a given value type.
  /// @param t The value type to create an index for.
  /// @returns A bitmap index suited for type *t*.
  static std::unique_ptr<bitmap_index> create(value_type t);

  /// Destroys a bitmap index.
  virtual ~bitmap_index() = default;

  /// Appends a single value.
  /// @param val The value to add to the index.
  /// @param id The ID to associate with *val*.
  /// @returns `true` if appending succeeded.
  bool push_back(value const& val, uint64_t id = 0);

  /// Appends a sequence of bits.
  /// @param n The number of elements to append.
  /// @param bit The value of the bits to append.
  /// @returns `true` on success.
  virtual bool append(size_t n, bool bit) = 0;

  /// Looks up a value given a relational operator.
  /// @param op The relation operator.
  /// @param val The value to lookup.
  virtual optional<bitstream>
  lookup(relational_operator op, value const& val) const = 0;

  /// Retrieves the number of elements in the bitmap index.
  /// @returns The number of rows, i.e., values in the bitmap.
  virtual uint64_t size() const = 0;

  /// Checks whether the bitmap is empty.
  /// @returns `true` if `size() == 0`.
  bool empty() const;

  /// Retrieves the number of bits appended since the last call to ::checkpoint.
  /// @returns Then number of bits since the last checkpoint.
  uint64_t appended() const;

  /// Performs a checkpoint of the number of bits appended.
  void checkpoint();

private:
  virtual bool push_back_impl(value const& val) = 0;
  virtual bool equals(bitmap_index const& other) const = 0;

  uint64_t checkpoint_size_ = 0;

private:
  friend access;

  virtual void serialize(serializer& sink) const = 0;
  virtual void deserialize(deserializer& source) = 0;
  virtual bool convert(std::string& str) const = 0;

  friend bool operator==(bitmap_index const& x, bitmap_index const& y);
};

} // namespace vast

#endif
