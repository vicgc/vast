#include "test.h"

// When defining a facility, it has to occur before including the logger.
#define VAST_LOG_FACILITY "test"
#include "vast/logger.h"

#if VAST_LOG_LEVEL >= 5

using namespace vast;

int foo()
{
  VAST_ENTER();
  VAST_RETURN(-1);
  return -1;
};

void bar(int i, std::string s, char c)
{
  VAST_ENTER(VAST_ARG(i, s, c));
  VAST_MSG("about to call foo");
  foo();
  VAST_LEAVE("leaving with a message");
};

BOOST_AUTO_TEST_CASE(logging_test)
{
  foo();
  bar(42, "***", 'A');
}

#endif
