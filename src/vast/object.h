#ifndef VAST_OBJECT_H
#define VAST_OBJECT_H

#include "vast/util/operators.h"
#include "vast/type_info.h"

namespace vast {

class global_type_info;

/// Wraps an heap-allocated value of an announced type.
class object : util::equality_comparable<object>
{
public:
  /// Creates an object by transferring ownership of an heap-allocated pointer.
  /// @tparam T An announced type.
  /// @param x The instance to move.
  /// @returns An object encapsulating *x*.
  /// @pre *x* must be a heap-allocated instance of `T`.
  template <typename T>
  static object adopt(T* x)
  {
    assert(x != nullptr);
    auto ti = global_typeid(typeid(*x));
    if (! ti)
      throw std::invalid_argument("missing type info for type T");
    return {ti, x};
  }

  /// Default-constructs an empty object.
  object() = default;

  /// Constructs an object from an announced type.
  /// @tparam T An announced type.
  /// @param x The instance to copy.
  template <typename T>
  object(T x)
  {
    type_ = global_typeid(typeid(x));
    if (! type_)
      throw std::invalid_argument("missing type info for type T");
    value_ = new T(std::move(x));
  }

  /// Constructs an object from an existing value.
  /// @param type The type of the object.
  /// @param value An heap-allocated instance of type *type*.
  /// @pre `type != nullptr && value != nullptr`
  /// @warning Takes ownership of *value*.
  object(global_type_info const* type, void* value);

  /// Copy-constructs an object by creating a deep copy.
  /// @param other The object to copy.
  object(const object& other);

  /// Move-constructs an object.
  /// @param other The object to move.
  object(object&& other);

  /// Destructs an object.
  ~object();

  /// Assigns on object to this instance.
  /// @param other The RHS of the assignment.
  object& operator=(object other);

  explicit operator bool() const;

  friend bool operator==(object const& x, object const& y);

  /// Retrieves the type of the object.
  /// @returns The type information for this object.
  global_type_info const* type() const;

  /// Retrieves the raw object.
  /// @returns The raw `void const` pointer of this object.
  void const* value() const;

  /// Retrieves the raw object.
  /// @returns The raw `void` pointer of this object.
  void* value();

  /// Checks whether the object is convertible to a given type.
  /// @tparam T The type to check.
  /// @returns `true` iff the object is convertible to `T`.
  template <typename T>
  bool convertible_to() const
  {
    return *this &&
      (type()->equals(typeid(T)) || is_convertible(type(), typeid(T)));
  }

  /// Relinquishes ownership of the object's contained instance.
  ///
  /// @returns A `void*` pointing to an heap-allocated pointer that the caller
  /// must now properly cast and delete.
  ///
  /// @post `! *this`
  void* release();

  /// Checks whether the object is convertible to a given type.
  /// @tparam T The type to check.
  /// @returns `true` iff the object is convertible to `T`.
  template <typename T>
  T* release_as()
  {
    return convertible_to<T>() ? reinterpret_cast<T*>(release()) : nullptr;
  }

private:
  friend access;
  void serialize(serializer& sink) const;
  void deserialize(deserializer& source);

  global_type_info const* type_ = nullptr;
  void* value_ = nullptr;
};

/// Retrieves an object value in a type-safe manner.
/// @tparam T The type to convert the object to.
/// @returns A reference of type `T`.
template <typename T>
T& get(object& o)
{
  static_assert(! std::is_pointer<T>::value && !std::is_reference<T>::value,
                "T must not be a reference or a pointer type.");
  if (! o.convertible_to<T>())
    throw std::invalid_argument("cannot convert object to requested type");
  return *reinterpret_cast<T*>(o.value());
}

template <typename T>
T const& get(object const& o)
{
  static_assert(! std::is_pointer<T>::value && !std::is_reference<T>::value,
                "T must not be a reference or a pointer type.");
  if (! o.convertible_to<T>())
    throw std::invalid_argument("cannot convert object to requested type");
  return *reinterpret_cast<T const*>(o.value());
}

} // namespace vast

#endif
