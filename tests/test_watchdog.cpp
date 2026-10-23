#include "harness.hpp"
#include "xtrans/watchdog.hpp"

using namespace xtrans;

TEST_CASE(test_watchdog_is_idle_until_armed) {
  Watchdog dog{500};
  CHECK(!dog.expired(1000));
  CHECK(!dog.armed());
}

TEST_CASE(test_watchdog_expires_at_the_deadline) {
  Watchdog dog{500};
  dog.arm(1000);
  CHECK(dog.armed());
  CHECK(!dog.expired(1499));
  CHECK(dog.expired(1500));
  CHECK(dog.expired(1600));
}

TEST_CASE(test_watchdog_disarm_stops_reporting) {
  Watchdog dog{100};
  dog.arm(0);
  CHECK(dog.expired(200));
  dog.disarm();
  CHECK(!dog.armed());
  CHECK(!dog.expired(200));
}

TEST_CASE(test_watchdog_rearm_moves_the_deadline) {
  Watchdog dog{100};
  dog.arm(0);
  dog.arm(500);
  CHECK(!dog.expired(550));
  CHECK(dog.expired(600));
}

TEST_CASE(test_watchdog_zero_timeout_expires_immediately) {
  Watchdog dog{0};
  dog.arm(42);
  CHECK(dog.expired(42));
}
