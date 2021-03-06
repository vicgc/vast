#include "test.h"
#include "vast/bitstream.h"
#include "vast/io/serialization.h"
#include "vast/util/convert.h"

using namespace vast;

struct bitstream_fixture
{
  bitstream_fixture()
  {
    ewah.append(10, true);
    ewah.append(20, false);

    // Cause the first dirty block to overflow and bumps the dirty counter of
    // the first marker to 1.
    ewah.append(40, true);

    // Fill up another dirty block.
    ewah.push_back(false);
    ewah.push_back(true);
    ewah.push_back(false);
    ewah.append(53, true);
    ewah.push_back(false);
    ewah.push_back(false);

    BOOST_CHECK_EQUAL(ewah.size(), 128);

    // Bump the dirty count to 2 and fill up the current dirty block.
    ewah.push_back(true);
    ewah.append(63, true);

    auto str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1111111111111111111111111111111111111111111111111111111111111111";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);

    // Appending anything now transforms the last block into a marker, because
    // it it turns out it was all 1s.
    ewah.push_back(true);

    str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1000000000000000000000000000000010000000000000000000000000000000\n"
      "                                                               1";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);
    BOOST_CHECK_EQUAL(ewah.size(), 193);

    // Fill up the dirty block and append another full block. This bumps the
    // clean count of the last marker to 2.
    ewah.append(63, true);
    ewah.append(64, true);

    // Now we'll add some 0 bits. We had a complete block left, so that make the
    // clean count of the last marker 3.
    ewah.append(64, false);

    BOOST_CHECK_EQUAL(ewah.size(), 384);

    // Add 15 clean blocks of 0, of which 14 get merged with the previous
    // marker and 1 remains a non-marker block. That yields a marker count of
    // 1111 (15).
    ewah.append(64 * 15, false);

    str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1000000000000000000000000000000110000000000000000000000000000000\n"
      "0000000000000000000000000000011110000000000000000000000000000000\n"
      "0000000000000000000000000000000000000000000000000000000000000000";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);
    BOOST_CHECK_EQUAL(ewah.size(), 384 + 64 * 15);


    // Now we're add the maximum number of new blocks with value 1. This
    // amounts to 64 * (2^32-1) = 274,877,906,880 bits in 2^32-2 blocks. Note
    // that the maximum value of a clean block is 2^32-1, but the invariant
    // requires the last block to be dirty, so we have to subtract yet another
    // block.
    ewah.append(64ull * ((1ull << 32) - 1), true);

    // Appending a single bit here just triggers the coalescing of the last
    // block with the current marker, making the clean count have the maximum
    // value of 2^32-1.
    ewah.push_back(false);

    str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1000000000000000000000000000000110000000000000000000000000000000\n"
      "0000000000000000000000000000100000000000000000000000000000000000\n"
      "1111111111111111111111111111111110000000000000000000000000000000\n"
      "                                                               0";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);
    BOOST_CHECK_EQUAL(ewah.size(), 1344 + 274877906880ull + 1);

    /// Complete the block as dirty.
    ewah.append(63, true);

    /// Create another full dirty block, just so that we can check that the
    /// dirty counter works properly.
    for (auto i = 0; i < 64; ++i) ewah.push_back(i % 2 == 0);

    BOOST_CHECK_EQUAL(ewah.size(), 274877908352ull);

    // Now we add 2^3 full markers. Because the maximum clean count is 2^32-1,
    // we end up with 8 full markers and 7 clean blocks.
    ewah.append((1ull << (32 + 3)) * 64, false);

    str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1000000000000000000000000000000110000000000000000000000000000000\n"
      "0000000000000000000000000000100000000000000000000000000000000000\n"
      "1111111111111111111111111111111110000000000000000000000000000010\n"
      "1111111111111111111111111111111111111111111111111111111111111110\n"
      "0101010101010101010101010101010101010101010101010101010101010101\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0000000000000000000000000000001110000000000000000000000000000000\n"
      "0000000000000000000000000000000000000000000000000000000000000000";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);
    BOOST_CHECK_EQUAL(ewah.size(), 274877908352ull + 2199023255552ull);

    /// Adding another bit just consolidates the last clean block with the
    /// last marker.
    ewah.push_back(true);

    str =
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "1111111111111111111111111111111111000000000000000000001111111111\n"
      "0011111111111111111111111111111111111111111111111111111010111111\n"
      "1000000000000000000000000000000110000000000000000000000000000000\n"
      "0000000000000000000000000000100000000000000000000000000000000000\n"
      "1111111111111111111111111111111110000000000000000000000000000010\n"
      "1111111111111111111111111111111111111111111111111111111111111110\n"
      "0101010101010101010101010101010101010101010101010101010101010101\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0111111111111111111111111111111110000000000000000000000000000000\n"
      "0000000000000000000000000000010000000000000000000000000000000000\n"
      "                                                               1";

    BOOST_REQUIRE_EQUAL(to_string(ewah), str);
    BOOST_CHECK_EQUAL(ewah.size(), 2473901163905);

    ewah2.push_back(false);
    ewah2.push_back(true);
    ewah2.append(421, false);
    ewah2.push_back(true);
    ewah2.push_back(true);

    str =
      "0000000000000000000000000000000000000000000000000000000000000001\n"
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "0000000000000000000000000000001010000000000000000000000000000000\n"
      "                       11000000000000000000000000000000000000000";

    BOOST_REQUIRE_EQUAL(to_string(ewah2), str);


    ewah3.append(222, true);
    ewah3.push_back(false);
    ewah3.push_back(true);
    ewah3.push_back(false);
    ewah3.append_block(0xcccccccccc);
    ewah3.push_back(false);
    ewah3.push_back(true);

    str =
      "1000000000000000000000000000000110000000000000000000000000000001\n"
      "1001100110011001100110011001100010111111111111111111111111111111\n"
      "                             10000000000000000000000000110011001";

    BOOST_REQUIRE_EQUAL(to_string(ewah3), str);
  }

  ewah_bitstream ewah;
  ewah_bitstream ewah2;
  ewah_bitstream ewah3;
};

BOOST_FIXTURE_TEST_SUITE(bitstream_testsuite, bitstream_fixture)

BOOST_AUTO_TEST_CASE(polymorphic_bitstream)
{
  bitstream empty;
  BOOST_CHECK(! empty);

  bitstream x{null_bitstream{}}, y;
  BOOST_REQUIRE(x);
  BOOST_CHECK(x.append(3, true));
  BOOST_CHECK_EQUAL(x.size(), 3);

  std::vector<uint8_t> buf;
  io::archive(buf, x);
  io::unarchive(buf, y);
  BOOST_CHECK_EQUAL(y.size(), 3);
}

BOOST_AUTO_TEST_CASE(null_operations)
{
  null_bitstream x;
  BOOST_REQUIRE(x.append(3, true));
  BOOST_REQUIRE(x.append(7, false));
  BOOST_REQUIRE(x.push_back(true));
  BOOST_CHECK_EQUAL(to_string(x),  "11100000001");
  BOOST_CHECK_EQUAL(to_string(~x), "00011111110");

  null_bitstream y;
  BOOST_REQUIRE(y.append(2, true));
  BOOST_REQUIRE(y.append(4, false));
  BOOST_REQUIRE(y.append(3, true));
  BOOST_REQUIRE(y.push_back(false));
  BOOST_REQUIRE(y.push_back(true));
  BOOST_CHECK_EQUAL(to_string(y),  "11000011101");
  BOOST_CHECK_EQUAL(to_string(~y), "00111100010");

  BOOST_CHECK_EQUAL(to_string(x & y), "11000000001");
  BOOST_CHECK_EQUAL(to_string(x | y), "11100011101");
  BOOST_CHECK_EQUAL(to_string(x ^ y), "00100011100");
  BOOST_CHECK_EQUAL(to_string(x - y), "00100000000");
  BOOST_CHECK_EQUAL(to_string(y - x), "00000011100");

  std::vector<null_bitstream> v;
  v.push_back(x);
  v.push_back(y);
  v.emplace_back(x - y);

  // The original vector contains the following (from LSB to MSB):
  // 11100000001
  // 11000011101
  // 00100000000
  std::string str;
  auto i = std::back_inserter(str);
  BOOST_REQUIRE(render(i, v));
  BOOST_CHECK_EQUAL(
      str,
      "110\n"
      "110\n"
      "101\n"
      "000\n"
      "000\n"
      "000\n"
      "010\n"
      "010\n"
      "010\n"
      "000\n"
      "110\n"
      );

  null_bitstream z;
  z.push_back(false);
  z.push_back(true);
  z.append(1337, false);
  z.trim();
  BOOST_CHECK_EQUAL(z.size(), 2);
  BOOST_CHECK_EQUAL(to_string(z), "01");
}

BOOST_AUTO_TEST_CASE(ewah_trimming)
{
  // NOPs---these all end in a 1.
  auto ewah_trimmed = ewah;
  ewah_trimmed.trim();
  BOOST_CHECK_EQUAL(ewah, ewah_trimmed);
  auto ewah2_trimmed = ewah2;
  ewah2_trimmed.trim();
  BOOST_CHECK_EQUAL(ewah2, ewah2_trimmed);
  auto ewah3_trimmed = ewah3;
  ewah3_trimmed.trim();
  BOOST_CHECK_EQUAL(ewah3, ewah3_trimmed);

  ewah_bitstream ebs;
  ebs.append(20, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 0);
  BOOST_CHECK_EQUAL(to_string(ebs), "");

  ebs.push_back(true);
  ebs.append(30, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 1);
  ebs.clear();

  ebs.append(64, true);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 64);
  ebs.clear();

  ebs.push_back(false);
  ebs.push_back(true);
  ebs.append(100, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 2);
  ebs.clear();

  ebs.append(192, true);
  ebs.append(10, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 192);
  ebs.clear();

  ebs.append(192, true);
  ebs.append(128, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 192);
  ebs.clear();

  ebs.append(192, true);
  ebs.append(128, false);
  ebs.append(192, true);
  ebs.append(128, false); // Gets eaten.
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 192 + 128 + 192);
  ebs.clear();

  ebs.append(192, true);
  ebs.append(128, false);
  ebs.append(192, true);
  ebs.append_block(0xf00f00);
  ebs.append_block(0xf00f00);
  ebs.append_block(0xf00f00); // Trimmed to length 24.
  ebs.append(128, false);
  ebs.trim();
  BOOST_CHECK_EQUAL(ebs.size(), 192 + 128 + 192 + 64 + 64 + 24);
  ebs.clear();
}

BOOST_AUTO_TEST_CASE(ewah_bitwise_iteration)
{
  auto i = ewah.begin();
  for (size_t j = 0; j < 10; ++j)
    BOOST_CHECK_EQUAL(*i++, j);

  for (size_t j = 30; j < 70; ++j)
    BOOST_CHECK_EQUAL(*i++, j);

  BOOST_CHECK_EQUAL(*i++, 71);
  for (size_t j = 73; j < 73 + 53; ++j)
    BOOST_CHECK_EQUAL(*i++, j);

  // The block at index 4 has 3 clean 1-blocks.
  for (size_t j = 128; j < 128 + 3 * 64; ++j)
    BOOST_CHECK_EQUAL(*i++, j);

  // The block at index 5 has 2^4 clean 0-blocks, which iteration should skip.
  ewah_bitstream::size_type next = 320 + 64 * (1 << 4);
  BOOST_CHECK_EQUAL(*i, next);

  // Now we're facing 2^32 clean 1-blocks. That's too much to iterate over.
  // Let's try something simpler.

  i = ewah2.begin();
  BOOST_CHECK_EQUAL(*i, 1);
  BOOST_CHECK_EQUAL(*++i, 423);
  BOOST_CHECK_EQUAL(*++i, 424);
  BOOST_CHECK(++i == ewah2.end());

  // While we're at it, let's test operator[] access as well.
  BOOST_CHECK(! ewah2[0]);
  BOOST_CHECK(ewah2[1]);
  BOOST_CHECK(! ewah2[2]);
  BOOST_CHECK(! ewah2[63]);
  BOOST_CHECK(! ewah2[64]);
  BOOST_CHECK(! ewah2[65]);
  BOOST_CHECK(! ewah2[384]);
  BOOST_CHECK(! ewah2[385]);
  BOOST_CHECK(! ewah2[422]);
  BOOST_CHECK(ewah2[423]);
  BOOST_CHECK(ewah2[424]);

  ewah_bitstream ebs;
  ebs.append(1000, false);
  for (auto i = 0; i < 256; ++i)
    ebs.push_back(i % 4 == 0);
  ebs.append(1000, false);

  ewah_bitstream::size_type cnt = 1000;
  i = ebs.begin();
  while (i != ebs.end())
  {
    BOOST_CHECK_EQUAL(*i, cnt);
    cnt += 4;
    ++i;
  }
}

BOOST_AUTO_TEST_CASE(ewah_element_access)
{
  BOOST_CHECK(ewah[0]);
  BOOST_CHECK(ewah[9]);
  BOOST_CHECK(! ewah[10]);
  BOOST_CHECK(ewah[64]);
  BOOST_CHECK(! ewah[1024]);
  BOOST_CHECK(ewah[1344]);
  BOOST_CHECK(ewah[2473901163905 - 1]);
}

BOOST_AUTO_TEST_CASE(ewah_finding)
{
  BOOST_CHECK_EQUAL(ewah.find_first(), 0);
  BOOST_CHECK_EQUAL(ewah.find_next(0), 1);
  BOOST_CHECK_EQUAL(ewah.find_next(8), 9);
  BOOST_CHECK_EQUAL(ewah.find_next(9), 30);
  BOOST_CHECK_EQUAL(ewah.find_next(10), 30);
  BOOST_CHECK_EQUAL(ewah.find_next(63), 64);
  BOOST_CHECK_EQUAL(ewah.find_next(64), 65);
  BOOST_CHECK_EQUAL(ewah.find_next(69), 71);
  BOOST_CHECK_EQUAL(ewah.find_next(319), 1344);
  BOOST_CHECK_EQUAL(ewah.find_next(320), 1344);
  BOOST_CHECK_EQUAL(ewah.find_next(2473901163903), 2473901163904);
  BOOST_CHECK_EQUAL(ewah.find_next(2473901163904), ewah_bitstream::npos);
  BOOST_CHECK_EQUAL(ewah.find_last(), 2473901163905 - 1);
  BOOST_CHECK_EQUAL(ewah.find_prev(2473901163904), 274877908288 + 62);
  BOOST_CHECK_EQUAL(ewah.find_prev(320), 319);
  BOOST_CHECK_EQUAL(ewah.find_prev(128), 125);

  BOOST_CHECK_EQUAL(ewah2.find_first(), 1);
  BOOST_CHECK_EQUAL(ewah2.find_next(1), 423);
  BOOST_CHECK_EQUAL(ewah2.find_last(), 424);
  BOOST_CHECK_EQUAL(ewah2.find_prev(424), 423);
  BOOST_CHECK_EQUAL(ewah2.find_prev(423), 1);
  BOOST_CHECK_EQUAL(ewah2.find_prev(1), ewah_bitstream::npos);

  BOOST_CHECK_EQUAL(ewah3.find_first(), 0);
  BOOST_CHECK_EQUAL(ewah3.find_next(3 * 64 + 29), 3 * 64 + 29 + 2 /* = 223 */);
  BOOST_CHECK_EQUAL(ewah3.find_next(223), 223 + 4); // Skip 3 zeros.
  BOOST_CHECK_EQUAL(ewah3.find_last(), ewah3.size() - 1);
  BOOST_CHECK_EQUAL(ewah3.find_prev(ewah3.size() - 1), ewah3.size() - 1 - 26);

  ewah_bitstream ebs;
  ebs.append(44, false);
  ebs.append(3, true);
  ebs.append(17, false);
  ebs.append(31, false);
  ebs.append(4, true);

  BOOST_CHECK_EQUAL(ebs.find_first(), 44);
  BOOST_CHECK_EQUAL(ebs.find_next(44), 45);
  BOOST_CHECK_EQUAL(ebs.find_next(45), 46);
  BOOST_CHECK_EQUAL(ebs.find_next(46), 44 + 3 + 17 + 31);
  BOOST_CHECK_EQUAL(ebs.find_next(49), 44 + 3 + 17 + 31);
  BOOST_CHECK_EQUAL(ebs.find_last(), ebs.size() - 1);
}

BOOST_AUTO_TEST_CASE(ewah_bitwise_not)
{
  ewah_bitstream ebs;
  ebs.push_back(true);
  ebs.push_back(false);
  ebs.append(30, true);
  ebs.push_back(false);

  ewah_bitstream comp;
  comp.push_back(false);
  comp.push_back(true);
  comp.append(30, false);
  comp.push_back(true);

  auto str =
    "0000000000000000000000000000000000000000000000000000000000000000\n"
    "                               100000000000000000000000000000010";

  BOOST_CHECK_EQUAL(~ebs, comp);
  BOOST_CHECK_EQUAL(ebs, ~comp);
  BOOST_CHECK_EQUAL(~~ebs, ebs);
  BOOST_CHECK_EQUAL(to_string(~ebs), str);

  str =
    "0000000000000000000000000000000000000000000000000000000000000010\n"
    "0000000000000000000000000000000000111111111111111111110000000000\n"
    "1100000000000000000000000000000000000000000000000000000101000000\n"
    "0000000000000000000000000000000110000000000000000000000000000000\n"
    "1000000000000000000000000000100000000000000000000000000000000000\n"
    "0111111111111111111111111111111110000000000000000000000000000010\n"
    "0000000000000000000000000000000000000000000000000000000000000001\n"
    "1010101010101010101010101010101010101010101010101010101010101010\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1111111111111111111111111111111110000000000000000000000000000000\n"
    "1000000000000000000000000000010000000000000000000000000000000000\n"
    "                                                               0";

  BOOST_CHECK_EQUAL(to_string(~ewah), str);
}


BOOST_AUTO_TEST_CASE(ewah_bitwise_and)
{
  auto str =
      "0000000000000000000000000000000000000000000000000000000000000001\n"
      "0000000000000000000000000000000000000000000000000000000000000010\n"
      "0000000000000000000000000000001010000000000000000000000000000000\n"
      "                       00000000000000000000000000000000000000000";

  auto max_size = std::max(ewah2.size(), ewah3.size());
  BOOST_CHECK_EQUAL(to_string(ewah2 & ewah3), str);
  BOOST_CHECK_EQUAL((ewah2 & ewah3).size(), max_size);
  BOOST_CHECK_EQUAL((ewah3 & ewah2).size(), max_size);

  ewah_bitstream ebs1, ebs2;
  ebs1.push_back(false);
  ebs1.append(63, true);
  ebs1.append(32, true);
  ebs2.append_block(0xfcfcfcfc, 48);

  str =
    "0000000000000000000000000000000000000000000000000000000000000001\n"
    "0000000000000000000000000000000011111100111111001111110011111100\n"
    "                                00000000000000000000000000000000";

  max_size = std::max(ebs1.size(), ebs2.size());
  BOOST_CHECK_EQUAL(to_string(ebs1 & ebs2), str);
  BOOST_CHECK_EQUAL((ebs1 & ebs2).size(), max_size);
  BOOST_CHECK_EQUAL((ebs2 & ebs1).size(), max_size);
}

BOOST_AUTO_TEST_CASE(ewah_bitwise_or)
{
  auto str =
    "1000000000000000000000000000000110000000000000000000000000000010\n"
    "1001100110011001100110011001100010111111111111111111111111111111\n"
    "0000000000000000000000000000010000000000000000000000000110011001\n"
    "0000000000000000000000000000000010000000000000000000000000000000\n"
    "                       11000000000000000000000000000000000000000";

  BOOST_CHECK_EQUAL(to_string(ewah2 | ewah3), str);

  ewah_bitstream ebs1, ebs2;
  ebs1.append(50, true);
  ebs2.append(50, false);
  ebs2.append(50, true);

  str =
    "1000000000000000000000000000000010000000000000000000000000000000\n"
    "                            111111111111111111111111111111111111";

  BOOST_CHECK_EQUAL(to_string(ebs1 | ebs2), str);
}

BOOST_AUTO_TEST_CASE(ewah_bitwise_xor)
{
  auto str =
    "0000000000000000000000000000000000000000000000000000000000000001\n"
    "1111111111111111111111111111111111111111111111111111111111111101\n"
    "1000000000000000000000000000000100000000000000000000000000000010\n"
    "1001100110011001100110011001100010111111111111111111111111111111\n"
    "0000000000000000000000000000010000000000000000000000000110011001\n"
    "0000000000000000000000000000000010000000000000000000000000000000\n"
    "                       11000000000000000000000000000000000000000";

  BOOST_CHECK_EQUAL(to_string(ewah2 ^ ewah3), str);
}

BOOST_AUTO_TEST_CASE(ewah_bitwise_nand)
{
  auto str =
    "0000000000000000000000000000001100000000000000000000000000000000\n"
    "                       11000000000000000000000000000000000000000";

  BOOST_CHECK_EQUAL(to_string(ewah2 - ewah3), str);

  ewah_bitstream ebs1, ebs2;
  ebs1.append(100, true);
  ebs2.push_back(true);
  ebs2.append(50, false);
  ebs2.append(13, true);

  str =
    "0000000000000000000000000000000000000000000000000000000000000001\n"
    "0000000000000111111111111111111111111111111111111111111111111110\n"
    "                            111111111111111111111111111111111111";

  BOOST_CHECK_EQUAL(to_string(ebs1 - ebs2), str);
}

BOOST_AUTO_TEST_CASE(ewah_sequence_iteration)
{
  auto range = ewah_bitstream::sequence_range{ewah};

  // The first two blocks are literal.
  auto i = range.begin();
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->length, bitvector::block_width);
  BOOST_CHECK_EQUAL(i->data, ewah.bits().block(1));
  ++i;
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->length, bitvector::block_width);
  BOOST_CHECK_EQUAL(i->data, ewah.bits().block(2));

  ++i;
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, bitvector::all_one);
  BOOST_CHECK_EQUAL(i->length, 3 * bitvector::block_width);

  ++i;
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, 0);
  BOOST_CHECK_EQUAL(i->length, (1 << 4) * bitvector::block_width);

  ++i;
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, bitvector::all_one);
  BOOST_CHECK_EQUAL(i->length, ((1ull << 32) - 1) * bitvector::block_width);

  ++i;
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->data, ewah.bits().block(6));
  BOOST_CHECK_EQUAL(i->length, bitvector::block_width);

  ++i;
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->data, ewah.bits().block(7));
  BOOST_CHECK_EQUAL(i->length, bitvector::block_width);

  ++i;
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, 0);
  BOOST_CHECK_EQUAL(i->length, (1ull << (32 + 3)) * 64);

  ++i;
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->data, 1);
  BOOST_CHECK_EQUAL(i->length, 1);

  BOOST_CHECK(++i == range.end());
}

BOOST_AUTO_TEST_CASE(ewah_block_append)
{
  ewah_bitstream ebs;
  ebs.append(10, true);
  ebs.append_block(0xf00);
  BOOST_CHECK_EQUAL(ebs.size(), 10 + bitvector::block_width);
  BOOST_CHECK(! ebs[17]);
  BOOST_CHECK(ebs[18]);
  BOOST_CHECK(ebs[19]);
  BOOST_CHECK(ebs[20]);
  BOOST_CHECK(ebs[21]);
  BOOST_CHECK(! ebs[22]);

  ebs.append(2048, true);
  ebs.append_block(0xff00);

  auto str =
    "0000000000000000000000000000000000000000000000000000000000000010\n"
    "0000000000000000000000000000000000000000001111000000001111111111\n"
    "1111111111111111111111111111111111111111111111111111110000000000\n"
    "1000000000000000000000000000111110000000000000000000000000000001\n"
    "0000000000000000000000000000000000000011111111000000001111111111\n"
    "                                                      0000000000";

  BOOST_CHECK_EQUAL(to_string(ebs), str);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(polymorphic_bitstream_iterators)
{
  bitstream bs = null_bitstream{};
  bs.push_back(true);
  bs.append(10, false);
  bs.append(2, true);

  auto i = bs.begin();
  BOOST_CHECK_EQUAL(*i, 0);
  BOOST_CHECK_EQUAL(*++i, 11);
  BOOST_CHECK_EQUAL(*++i, 12);
  BOOST_CHECK(++i == bs.end());

  bs = ewah_bitstream{};
  bs.push_back(false);
  bs.push_back(true);
  bs.append(421, false);
  bs.push_back(true);
  bs.push_back(true);

  i = bs.begin();
  BOOST_CHECK_EQUAL(*i, 1);
  BOOST_CHECK_EQUAL(*++i, 423);
  BOOST_CHECK_EQUAL(*++i, 424);
  BOOST_CHECK(++i == bs.end());
}

BOOST_AUTO_TEST_CASE(sequence_iteration)
{
  null_bitstream nbs;
  nbs.push_back(true);
  nbs.push_back(false);
  nbs.append(62, true);
  nbs.append(320, false);
  nbs.append(512, true);

  auto range = null_bitstream::sequence_range{nbs};
  auto i = range.begin();
  BOOST_CHECK(i != range.end());
  BOOST_CHECK_EQUAL(i->offset, 0);
  BOOST_CHECK(i->is_literal());
  BOOST_CHECK_EQUAL(i->data, bitvector::all_one & ~2);

  ++i;
  BOOST_CHECK(i != range.end());
  BOOST_CHECK_EQUAL(i->offset, 64);
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, 0);
  BOOST_CHECK_EQUAL(i->length, 320);

  ++i;
  BOOST_CHECK(i != range.end());
  BOOST_CHECK_EQUAL(i->offset, 64 + 320);
  BOOST_CHECK(i->is_fill());
  BOOST_CHECK_EQUAL(i->data, bitvector::all_one);
  BOOST_CHECK_EQUAL(i->length, 512);

  BOOST_CHECK(++i == range.end());
}

BOOST_AUTO_TEST_CASE(pop_count)
{
  null_bitstream nbs;
  nbs.push_back(true);
  nbs.push_back(false);
  nbs.append(62, true);
  nbs.append(320, false);
  nbs.append(512, true);
  nbs.append(47, false);
  BOOST_CHECK(nbs.count() == 575);

  ewah_bitstream ebs;
  ebs.push_back(true);
  ebs.push_back(false);
  ebs.append(62, true);
  ebs.append(320, false);
  ebs.append(512, true);
  ebs.append(47, false);
  BOOST_CHECK(ebs.count() == 575);
}
