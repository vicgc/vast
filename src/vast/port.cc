#include "vast/port.h"

#include <utility>
#include "vast/logger.h"
#include "vast/serialization.h"

namespace vast {

port::port(number_type number, port_type type)
  : number_(number)
  , type_(type)
{
}

port::port(port const& other)
  : number_(other.number_)
  , type_(other.type_)
{
}

port::port(port&& other)
  : number_(other.number_)
  , type_(other.type_)
{
  other.number_ = 0u;
  other.type_ = unknown;
}

port& port::operator=(port other)
{
  using std::swap;
  swap(number_, other.number_);
  swap(type_, other.type_);
  return *this;
}

port::number_type port::number() const
{
  return number_;
}

port::port_type port::type() const
{
  return type_;
}

void port::type(port_type t)
{
  type_ = t;
}

void port::serialize(serializer& sink) const
{
  VAST_ENTER(VAST_THIS);
  typedef std::underlying_type<port::port_type>::type underlying_type;
  sink << number_;
  sink << static_cast<underlying_type>(type_);
}

void port::deserialize(deserializer& source)
{
  VAST_ENTER();
  source >> number_;
  std::underlying_type<port::port_type>::type type;
  source >> type;
  type_ = static_cast<port::port_type>(type);
  VAST_LEAVE(VAST_THIS);
}

bool operator==(port const& x, port const& y)
{
  return x.number_ == y.number_ && x.type_ == y.type_;
}

bool operator<(port const& x, port const& y)
{
  return std::tie(x.number_, x.type_) < std::tie(y.number_, y.type_);
}

} // namespace vast
