#include "vast/chunk.h"

namespace vast {

chunk::writer::writer(chunk& chk)
  : chunk_(chk),
    base_stream_(chunk_.buffer_),
    compressed_stream_(make_compressed_output_stream(chunk_.compression_,
                                                     base_stream_)),
    serializer_(*compressed_stream_)
{
}

chunk::writer::~writer()
{
  chunk_.bytes_ = serializer_.bytes();
}

size_t chunk::writer::bytes() const
{
  return serializer_.bytes();
}


chunk::reader::reader(chunk const& chk)
  : chunk_(chk),
    available_(chunk_.elements_),
    base_stream_(chunk_.buffer_.data(), chunk_.buffer_.size()),
    compressed_stream_(make_compressed_input_stream(chunk_.compression_,
                                                    base_stream_)),
    deserializer_(*compressed_stream_)
{
}

uint32_t chunk::reader::available() const
{
  return available_;
}

size_t chunk::reader::bytes() const
{
  return deserializer_.bytes();
}


chunk::chunk(io::compression method)
  : compression_(method)
{
}

bool chunk::empty() const
{
  return elements_ == 0;
}

uint32_t chunk::elements() const
{
  return elements_;
}

size_t chunk::compressed_bytes() const
{
  return buffer_.size();
}

size_t chunk::uncompressed_bytes() const
{
  return bytes_;
}

void chunk::serialize(serializer& sink) const
{
  sink << compression_;
  sink << elements_;
  sink << bytes_;
  sink << buffer_;
}

void chunk::deserialize(deserializer& source)
{
  source >> compression_;
  source >> elements_;
  source >> bytes_;
  source >> buffer_;
}

bool operator==(chunk const& x, chunk const& y)
{
  return x.compression_ == y.compression_
      && x.elements_ == y.elements_
      && x.bytes_ == y.bytes_
      && x.buffer_ == y.buffer_;
}

} // namespace vast
