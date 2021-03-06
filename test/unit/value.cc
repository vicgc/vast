#include "test.h"
#include <sstream>
#include "vast/value.h"

using namespace vast;

BOOST_AUTO_TEST_SUITE(value_test_suite)

BOOST_AUTO_TEST_CASE(size)
{
  BOOST_CHECK_LE(sizeof(value), 32);
  std::cout << "A value occupies " << sizeof(value) << " bytes" << std::endl;
}

namespace {
template <value_type V, typename U>
bool constexpr value_type_check()
{
  return std::is_same<value_type_type<V>, U>::value;
}
} // namespace anonymous

BOOST_AUTO_TEST_CASE(types)
{
  BOOST_CHECK((value_type_check<invalid_value, value_invalid>()));
  BOOST_CHECK((value_type_check<bool_value, bool>()));
  BOOST_CHECK((value_type_check<int_value, int64_t>()));
  BOOST_CHECK((! value_type_check<int_value, int>()));
  BOOST_CHECK((value_type_check<uint_value, uint64_t>()));
  BOOST_CHECK((value_type_check<double_value, double>()));
  BOOST_CHECK((value_type_check<string_value, string>()));
}

BOOST_AUTO_TEST_CASE(trivial)
{
  value v1;
  BOOST_CHECK(! v1.nil());
  BOOST_CHECK(v1.invalid());
  BOOST_CHECK_EQUAL(v1.which(), invalid_value);
  BOOST_CHECK_EQUAL(to_string(v1), "<invalid>");

  value v2{true};
  BOOST_CHECK_EQUAL(v2.which(), bool_value);
  v2.clear();
  BOOST_CHECK(! v2);
  BOOST_CHECK(v2.nil());
  BOOST_CHECK(! v2.invalid());
  BOOST_CHECK_THROW(v2.get<record>(), std::bad_cast);
  v2 = "foo";
  BOOST_CHECK_EQUAL(v2.which(), string_value);
  v2.clear();

  value v3(v2);
  BOOST_CHECK(v3.nil());
  BOOST_CHECK(! v3.invalid());
  BOOST_CHECK_EQUAL(v3.which(), string_value);

  value v4(string_value);
  BOOST_CHECK(! v4);
  BOOST_CHECK(! v4.invalid());
  BOOST_CHECK(v4.nil());
}

BOOST_AUTO_TEST_CASE(relational_operators)
{
  value v1, v2;
  BOOST_CHECK(v1 == v2);
  BOOST_CHECK(! (v1 != v2));

  v1 = "foo";
  BOOST_CHECK(v1.which() == string_value);
  BOOST_CHECK(v1 != v2);
  BOOST_CHECK(! (v1 == v2));
  BOOST_CHECK(! (v1 < v2));
  BOOST_CHECK(! (v1 > v2));
  BOOST_CHECK(! (v1 <= v2));
  BOOST_CHECK(! (v1 >= v2));

  v2 = 42;
  BOOST_CHECK(v2.which() == int_value);
  BOOST_CHECK(v1 != v2);
  BOOST_CHECK(! (v1 == v2));
  BOOST_CHECK(! (v1 < v2));
  BOOST_CHECK(! (v1 > v2));
  BOOST_CHECK(! (v1 <= v2));
  BOOST_CHECK(! (v1 >= v2));

  v2 = "foo";
  BOOST_CHECK(v2.which() == string_value);
  BOOST_CHECK(v1 == v2);
  BOOST_CHECK(! (v1 != v2));
  BOOST_CHECK(! (v1 < v2));
  BOOST_CHECK(! (v1 > v2));
  BOOST_CHECK(v1 <= v2);
  BOOST_CHECK(v1 >= v2);
}

BOOST_AUTO_TEST_CASE(boolean)
{
  value v1(true);
  value v2(false);
  BOOST_CHECK_EQUAL(to_string(v1), "T");
  BOOST_CHECK_EQUAL(to_string(v2), "F");
  v1 = false;
  v2 = true;
  BOOST_CHECK_EQUAL(to_string(v1), "F");
  BOOST_CHECK_EQUAL(to_string(v2), "T");
  BOOST_CHECK(v1 != v2);
  BOOST_CHECK(v1 < v2);
  BOOST_CHECK(v1 == false);
  BOOST_CHECK(v2 == true);

  value v3(bool_value);
  BOOST_CHECK_EQUAL(to_string(v3), "<bool>");
  value v4(double_value);
  BOOST_CHECK_EQUAL(to_string(v4), "<double>");
  BOOST_CHECK_THROW(v4.get<double>(), std::bad_cast);
}

BOOST_AUTO_TEST_CASE(integers)
{
  value v1(42);
  value v2(42u);
  BOOST_CHECK(v1.which() == int_value);
  BOOST_CHECK(v2.which() == uint_value);
  BOOST_CHECK_EQUAL(to_string(v1), "+42");
  BOOST_CHECK_EQUAL(to_string(v2), "42");
  BOOST_CHECK(v1 != v2);  // Not comparable due to different signedness.
  v1 = -1;
  v2 = 0;
  BOOST_CHECK_EQUAL(to_string(v1), "-1");
  BOOST_CHECK_EQUAL(to_string(v2), "+0");
  v2 = -99999999;
  BOOST_CHECK(v1 > v2);
  BOOST_CHECK(v1 != v2);
}

BOOST_AUTO_TEST_CASE(floating_points)
{
  value v1(0.0);
  BOOST_CHECK_EQUAL(v1.which(), double_value);
  BOOST_CHECK_EQUAL(to_string(v1), "0.0000000000");
  BOOST_CHECK_EQUAL(v1, 0.0);

  value v2 = 0.123456789;
  BOOST_CHECK_EQUAL(to_string(v2), "0.1234567890");
  BOOST_CHECK(v2 < 123.456789);
  BOOST_CHECK_EQUAL(v2.get<double>(), 0.123456789);

  v2 = -123.456;
  BOOST_CHECK_EQUAL(to_string(v2), "-123.4560000000");
}

BOOST_AUTO_TEST_CASE(strings)
{
  value empty("");
  BOOST_CHECK(empty.get<string>().size() == 0);
  BOOST_CHECK(std::strcmp(empty.get<string>().data(), "") == 0);

  value v1('c');
  BOOST_CHECK(v1.which() == string_value);
  BOOST_CHECK_EQUAL(to_string(v1), "\"c\"");
  v1 = 'x';
  BOOST_CHECK_EQUAL(to_string(v1), "\"x\"");

  value v2("foo");
  BOOST_CHECK_EQUAL(v2, "foo");
  BOOST_CHECK_EQUAL(to_string(v2), "\"foo\"");
  BOOST_CHECK(v2.get<string>().size() == 3);
  v2 = "quux";
  BOOST_CHECK_EQUAL(v2, "quux");
  BOOST_CHECK_EQUAL(to_string(v2), "\"quux\"");
  BOOST_CHECK(v2.get<string>().size() == 4);
  BOOST_CHECK_EQUAL(to_string(v2), "\"quux\"");

  // Testing the limits.
  std::string str(string::in_situ_size, 'x');
  value v3(str.data());
  BOOST_CHECK(! v3.get<string>().is_heap_allocated());
  BOOST_CHECK(v3 == str.data());
  str.push_back('y');
  v3 = str.data();   // Creates a copy on the heap...
  BOOST_CHECK(v3 == str.data());
  BOOST_CHECK(v3.get<string>().is_heap_allocated());
  str.pop_back();
  v3 = str.data();   // ...and is placed back into the in-situ buffer.
  BOOST_CHECK(v3 == str.data());
  BOOST_CHECK(! v3.get<string>().is_heap_allocated());

  auto phrase = "Das ist also des Pudels Kern.";
  value v4;
  v4 = phrase;
  BOOST_CHECK(v4 == phrase);
  BOOST_CHECK(! v4.get<string>().is_heap_allocated());
  BOOST_CHECK(v4.get<string>().size() == std::strlen(phrase));

  auto nul = "ro\0ot";
  value v5(nul, 5);
  auto const& s = v5.get<string>();
  BOOST_CHECK(std::string(nul, 5) == std::string(s.begin(), s.end()));

  BOOST_CHECK(v4 == phrase);
  BOOST_CHECK(v5 == value("ro\0ot", 5));
  BOOST_CHECK(v4 < v5);
}

BOOST_AUTO_TEST_CASE(regexes)
{
  regex r{"."};
  value v1(r);
  BOOST_CHECK(v1.which() == regex_value);
  BOOST_CHECK(v1.get<regex>() == r);
  BOOST_CHECK_EQUAL(to_string(v1), "/./");
}

BOOST_AUTO_TEST_CASE(times)
{
  auto jetzt = now();
  value t(jetzt);
  value d(jetzt.since_epoch());

  BOOST_CHECK_EQUAL(jetzt, t.get<time_point>());
  BOOST_CHECK_EQUAL(jetzt.since_epoch(), d.get<time_range>());

  value r(std::chrono::seconds(72));
  BOOST_CHECK_EQUAL(r, time_range::seconds(72));

  // Testing template instantiation of operator=.
  r = std::chrono::system_clock::now();
}

BOOST_AUTO_TEST_CASE(containers)
{
  record r{"foo", 42u, -4711, address{"dead::beef"}};
  value vr(std::move(r));
  BOOST_CHECK_EQUAL(to_string(vr), "(\"foo\", 42, -4711, dead::beef)");
  vr.get<record>().emplace_back("qux");
  vr.get<record>().emplace_back("corge");
  BOOST_CHECK(vr.get<record>().size() == 6);

  table t{{-1, 10u}, {-2, 20u}, {-3, 30u}};
  value vt(t);
  BOOST_CHECK_EQUAL(to_string(vt), "{-3 -> 30, -2 -> 20, -1 -> 10}");
  auto& tbl = vt.get<table>();
  BOOST_CHECK_EQUAL(t, tbl);
  tbl[-1] = 15u;
  tbl[0] = 42u;
  BOOST_CHECK_EQUAL(tbl.begin()->second, 30u);
  BOOST_CHECK_EQUAL(tbl[0], 42u);
  BOOST_CHECK_EQUAL(tbl[-1], 15u);
  BOOST_CHECK_EQUAL(to_string(vt), "{-3 -> 30, -2 -> 20, -1 -> 15, +0 -> 42}");
}

BOOST_AUTO_TEST_CASE(addresses)
{
  value v1(address{"10.1.1.2"});
  BOOST_CHECK(v1.which() == address_value);
  BOOST_CHECK_EQUAL(to_string(v1), "10.1.1.2");
  v1 = address{"127.0.0.1"};
  BOOST_CHECK(v1.get<address>().is_loopback());
  BOOST_CHECK_EQUAL(to_string(v1), "127.0.0.1");

  value v2(address{"f00::babe"});
  BOOST_CHECK(v2.get<address>().is_v6());
  BOOST_CHECK(v1 != v2);
}

BOOST_AUTO_TEST_CASE(prefixes)
{
  value v1(prefix{address{"10.1.1.2"}, 8});
  BOOST_CHECK_EQUAL(v1.which(), prefix_value);
  BOOST_CHECK_EQUAL(to_string(v1), "10.0.0.0/8");
  BOOST_CHECK_EQUAL(v1.get<prefix>().length(), 8);
  v1 = prefix{address{"127.0.0.1"}, 32};
  BOOST_CHECK_EQUAL(to_string(v1), "127.0.0.1/32");
  BOOST_CHECK_EQUAL(v1.get<prefix>().length(), 32);
}

BOOST_AUTO_TEST_CASE(ports)
{
  value v1(port{8, port::icmp});
  BOOST_CHECK(v1.which() == port_value);
  BOOST_CHECK_EQUAL(to_string(v1), "8/icmp");
  v1 = port{25, port::tcp};
  BOOST_CHECK_EQUAL(to_string(v1), "25/tcp");
  BOOST_CHECK(v1.get<port>().number() == 25);
}

BOOST_AUTO_TEST_SUITE_END()
