#include "SlowStartController.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class SlowStartControllerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(SlowStartControllerTest);
  CPPUNIT_TEST(testInitialLimit_belowThreshold);
  CPPUNIT_TEST(testInitialLimit_aboveThreshold);
  CPPUNIT_TEST(testInactive_belowThreshold);
  CPPUNIT_TEST(testRampUp);
  CPPUNIT_TEST(testRampUpDoubles);
  CPPUNIT_TEST(testStableNoChange);
  CPPUNIT_TEST(testBackOff);
  CPPUNIT_TEST(testCeiling);
  CPPUNIT_TEST(testBackOffFloor);
  CPPUNIT_TEST(testReset);
  CPPUNIT_TEST(testSetCeiling);
  CPPUNIT_TEST(testSpeedDecrease);
  CPPUNIT_TEST(testBackOffInactive);
  CPPUNIT_TEST_SUITE_END();

public:
  void testInitialLimit_belowThreshold();
  void testInitialLimit_aboveThreshold();
  void testInactive_belowThreshold();
  void testRampUp();
  void testRampUpDoubles();
  void testStableNoChange();
  void testBackOff();
  void testCeiling();
  void testBackOffFloor();
  void testReset();
  void testSetCeiling();
  void testSpeedDecrease();
  void testBackOffInactive();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SlowStartControllerTest);

void SlowStartControllerTest::testInitialLimit_belowThreshold()
{
  // Below threshold: no slow-start, allowed == ceiling
  SlowStartController ctrl(2);
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  CPPUNIT_ASSERT(!ctrl.isActive());
}

void SlowStartControllerTest::testInitialLimit_aboveThreshold()
{
  // Above threshold: slow-start active, starts at 1
  SlowStartController ctrl(16);
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
  CPPUNIT_ASSERT(ctrl.isActive());
  CPPUNIT_ASSERT_EQUAL(16, ctrl.getCeiling());
}

void SlowStartControllerTest::testInactive_belowThreshold()
{
  // Below threshold: update() and backOff() are no-ops
  SlowStartController ctrl(4);
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
  ctrl.update(1000);
  ctrl.update(2000);
  // Still 4, not ramped
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
  ctrl.backOff();
  // Still 4, not halved
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testRampUp()
{
  SlowStartController ctrl(16);
  // First update sets baseline, no ramp
  ctrl.update(1000);
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
  // Speed doubled — should ramp up
  ctrl.update(2000);
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testRampUpDoubles()
{
  SlowStartController ctrl(128);
  ctrl.update(1000);
  ctrl.update(2000); // 2
  ctrl.update(4000); // 4
  ctrl.update(8000); // 8
  CPPUNIT_ASSERT_EQUAL(8, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testStableNoChange()
{
  SlowStartController ctrl(16);
  ctrl.update(1000);
  ctrl.update(2000); // ramp to 2
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  // Less than 10% increase — no change
  ctrl.update(2050);
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  // Exactly 10% increase — should ramp
  ctrl.update(2255); // 2255/2050 = 1.1
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testBackOff()
{
  SlowStartController ctrl(128);
  ctrl.update(1000);
  ctrl.update(2000); // 2
  ctrl.update(4000); // 4
  ctrl.update(8000); // 8
  CPPUNIT_ASSERT_EQUAL(8, ctrl.getAllowedConnections());
  ctrl.backOff(); // halve → 4
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
  ctrl.backOff(); // halve → 2
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testCeiling()
{
  // Ceiling of 8 (above threshold), ramp-up should stop at 8
  SlowStartController ctrl(8);
  ctrl.update(1000);
  ctrl.update(2000); // 2
  ctrl.update(4000); // 4
  ctrl.update(8000); // 8 (ceiling)
  ctrl.update(16000); // still 8
  CPPUNIT_ASSERT_EQUAL(8, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testBackOffFloor()
{
  SlowStartController ctrl(16);
  // Already at 1, should stay at 1
  ctrl.backOff();
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testReset()
{
  SlowStartController ctrl(16);
  ctrl.update(1000);
  ctrl.update(2000); // 2
  ctrl.update(4000); // 4
  CPPUNIT_ASSERT_EQUAL(4, ctrl.getAllowedConnections());
  ctrl.reset();
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
  // After reset, first update sets baseline again
  ctrl.update(5000);
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testSetCeiling()
{
  // Start below threshold (inactive)
  SlowStartController ctrl(2);
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  // Raise ceiling above threshold → becomes active, resets to 1
  ctrl.setCeiling(16);
  CPPUNIT_ASSERT(ctrl.isActive());
  CPPUNIT_ASSERT_EQUAL(1, ctrl.getAllowedConnections());
  ctrl.update(1000);
  ctrl.update(2000); // ramp to 2
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testSpeedDecrease()
{
  SlowStartController ctrl(16);
  ctrl.update(2000);
  ctrl.update(4000); // 2
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
  // Speed decreased — no ramp up
  ctrl.update(3000);
  CPPUNIT_ASSERT_EQUAL(2, ctrl.getAllowedConnections());
}

void SlowStartControllerTest::testBackOffInactive()
{
  // When below threshold, backOff should not reduce connections
  SlowStartController ctrl(3);
  CPPUNIT_ASSERT_EQUAL(3, ctrl.getAllowedConnections());
  ctrl.backOff();
  CPPUNIT_ASSERT_EQUAL(3, ctrl.getAllowedConnections());
}

} // namespace aria2
