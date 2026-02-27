#include "OptionHandlerImpl.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "prefs.h"
#include "Exception.h"
#include "help_tags.h"
#include "Request.h"
#include "a2io.h"

namespace aria2 {

class OptionHandlerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(OptionHandlerTest);
  CPPUNIT_TEST(testBooleanOptionHandler);
  CPPUNIT_TEST(testNumberOptionHandler);
  CPPUNIT_TEST(testNumberOptionHandler_min);
  CPPUNIT_TEST(testNumberOptionHandler_max);
  CPPUNIT_TEST(testNumberOptionHandler_min_max);
  CPPUNIT_TEST(testUnitNumberOptionHandler);
  CPPUNIT_TEST(testParameterOptionHandler);
  CPPUNIT_TEST(testDefaultOptionHandler);
  CPPUNIT_TEST(testFloatNumberOptionHandler);
  CPPUNIT_TEST(testFloatNumberOptionHandler_min);
  CPPUNIT_TEST(testFloatNumberOptionHandler_max);
  CPPUNIT_TEST(testFloatNumberOptionHandler_min_max);
  CPPUNIT_TEST(testHttpProxyOptionHandler);
  CPPUNIT_TEST(testDeprecatedOptionHandler);
  CPPUNIT_TEST(testIntegerRangeOptionHandler);
  CPPUNIT_TEST(testChecksumOptionHandler);
  CPPUNIT_TEST(testHostPortOptionHandler);
  CPPUNIT_TEST(testLocalFilePathOptionHandler);
  CPPUNIT_TEST(testPrioritizePieceOptionHandler);
  CPPUNIT_TEST(testOptimizeConcurrentDownloadsOptionHandler);
  CPPUNIT_TEST(testIndexOutOptionHandler);
  CPPUNIT_TEST(testCumulativeOptionHandler);
  CPPUNIT_TEST(testDefaultOptionHandler_noEmpty);
  CPPUNIT_TEST_SUITE_END();

public:
  void testBooleanOptionHandler();
  void testNumberOptionHandler();
  void testNumberOptionHandler_min();
  void testNumberOptionHandler_max();
  void testNumberOptionHandler_min_max();
  void testUnitNumberOptionHandler();
  void testParameterOptionHandler();
  void testDefaultOptionHandler();
  void testFloatNumberOptionHandler();
  void testFloatNumberOptionHandler_min();
  void testFloatNumberOptionHandler_max();
  void testFloatNumberOptionHandler_min_max();
  void testHttpProxyOptionHandler();
  void testDeprecatedOptionHandler();
  void testIntegerRangeOptionHandler();
  void testChecksumOptionHandler();
  void testHostPortOptionHandler();
  void testLocalFilePathOptionHandler();
  void testPrioritizePieceOptionHandler();
  void testOptimizeConcurrentDownloadsOptionHandler();
  void testIndexOutOptionHandler();
  void testCumulativeOptionHandler();
  void testDefaultOptionHandler_noEmpty();
};

CPPUNIT_TEST_SUITE_REGISTRATION(OptionHandlerTest);

void OptionHandlerTest::testBooleanOptionHandler()
{
  BooleanOptionHandler handler(PREF_DAEMON);
  Option option;
  handler.parse(option, A2_V_TRUE);
  CPPUNIT_ASSERT_EQUAL(std::string(A2_V_TRUE), option.get(PREF_DAEMON));
  handler.parse(option, A2_V_FALSE);
  CPPUNIT_ASSERT_EQUAL(std::string(A2_V_FALSE), option.get(PREF_DAEMON));
  try {
    handler.parse(option, "hello");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("true, false"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler()
{
  NumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "0");
  CPPUNIT_ASSERT_EQUAL(std::string("0"), option.get(PREF_TIMEOUT));
  CPPUNIT_ASSERT_EQUAL(std::string("*-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_min()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", 1);
  Option option;
  handler.parse(option, "1");
  CPPUNIT_ASSERT_EQUAL(std::string("1"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "0");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("1-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_max()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", -1, 100);
  Option option;
  handler.parse(option, "100");
  CPPUNIT_ASSERT_EQUAL(std::string("100"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "101");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("*-100"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testNumberOptionHandler_min_max()
{
  NumberOptionHandler handler(PREF_TIMEOUT, "", "", 1, 100);
  Option option;
  handler.parse(option, "1");
  CPPUNIT_ASSERT_EQUAL(std::string("1"), option.get(PREF_TIMEOUT));
  handler.parse(option, "100");
  CPPUNIT_ASSERT_EQUAL(std::string("100"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "0");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "101");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("1-100"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testUnitNumberOptionHandler()
{
  UnitNumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "4294967296");
  CPPUNIT_ASSERT_EQUAL(std::string("4294967296"), option.get(PREF_TIMEOUT));
  handler.parse(option, "4096M");
  CPPUNIT_ASSERT_EQUAL(std::string("4294967296"), option.get(PREF_TIMEOUT));
  handler.parse(option, "4096K");
  CPPUNIT_ASSERT_EQUAL(std::string("4194304"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "K");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "M");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
}

void OptionHandlerTest::testParameterOptionHandler()
{
  ParameterOptionHandler handler(PREF_TIMEOUT, "", "", {"value1", "value2"});
  Option option;
  handler.parse(option, "value1");
  CPPUNIT_ASSERT_EQUAL(std::string("value1"), option.get(PREF_TIMEOUT));
  handler.parse(option, "value2");
  CPPUNIT_ASSERT_EQUAL(std::string("value2"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "value3");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("value1, value2"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testDefaultOptionHandler()
{
  DefaultOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "bar");
  CPPUNIT_ASSERT_EQUAL(std::string("bar"), option.get(PREF_TIMEOUT));
  handler.parse(option, "");
  CPPUNIT_ASSERT_EQUAL(std::string(""), option.get(PREF_TIMEOUT));
  CPPUNIT_ASSERT_EQUAL(std::string(""), handler.createPossibleValuesString());

  handler.addTag(TAG_ADVANCED);
  CPPUNIT_ASSERT_EQUAL(std::string("#advanced"), handler.toTagString());
  handler.addTag(TAG_BASIC);
  CPPUNIT_ASSERT_EQUAL(std::string("#basic, #advanced"), handler.toTagString());
  CPPUNIT_ASSERT(handler.hasTag(TAG_ADVANCED));
  CPPUNIT_ASSERT(handler.hasTag(TAG_BASIC));
  CPPUNIT_ASSERT(!handler.hasTag(TAG_HTTP));
}

void OptionHandlerTest::testFloatNumberOptionHandler()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT);
  Option option;
  handler.parse(option, "1.0");
  CPPUNIT_ASSERT_EQUAL(std::string("1.0"), option.get(PREF_TIMEOUT));
  CPPUNIT_ASSERT_EQUAL(std::string("*-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_min()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", 0.0);
  Option option;
  handler.parse(option, "0.0");
  CPPUNIT_ASSERT_EQUAL(std::string("0.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "-0.1");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("0.0-*"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_max()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", -1, 10.0);
  Option option;
  handler.parse(option, "10.0");
  CPPUNIT_ASSERT_EQUAL(std::string("10.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "10.1");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("*-10.0"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testFloatNumberOptionHandler_min_max()
{
  FloatNumberOptionHandler handler(PREF_TIMEOUT, "", "", 0.0, 10.0);
  Option option;
  handler.parse(option, "0.0");
  CPPUNIT_ASSERT_EQUAL(std::string("0.0"), option.get(PREF_TIMEOUT));
  handler.parse(option, "10.0");
  CPPUNIT_ASSERT_EQUAL(std::string("10.0"), option.get(PREF_TIMEOUT));
  try {
    handler.parse(option, "-0.1");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "10.1");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("0.0-10.0"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testHttpProxyOptionHandler()
{
  HttpProxyOptionHandler handler(PREF_HTTP_PROXY, "", "");
  Option option;
  handler.parse(option, "proxy:65535");
  CPPUNIT_ASSERT_EQUAL(std::string("http://proxy:65535/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "http://proxy:8080");
  CPPUNIT_ASSERT_EQUAL(std::string("http://proxy:8080/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "");
  CPPUNIT_ASSERT(option.defined(PREF_HTTP_PROXY));
  CPPUNIT_ASSERT(option.blank(PREF_HTTP_PROXY));

  try {
    handler.parse(option, "http://bar:65536");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("[http://][USER:PASSWORD@]HOST[:PORT]"),
                       handler.createPossibleValuesString());

  handler.parse(option, "http://user%40:passwd%40@proxy:8080");
  CPPUNIT_ASSERT_EQUAL(std::string("http://user%40:passwd%40@proxy:8080/"),
                       option.get(PREF_HTTP_PROXY));

  handler.parse(option, "http://[::1]:8080");
  CPPUNIT_ASSERT_EQUAL(std::string("http://[::1]:8080/"),
                       option.get(PREF_HTTP_PROXY));
}

void OptionHandlerTest::testDeprecatedOptionHandler()
{
  {
    DeprecatedOptionHandler handler(new DefaultOptionHandler(PREF_TIMEOUT));
    Option option;
    handler.parse(option, "foo");
    CPPUNIT_ASSERT(!option.defined(PREF_TIMEOUT));
  }
  {
    DefaultOptionHandler dir(PREF_DIR);
    DeprecatedOptionHandler handler(new DefaultOptionHandler(PREF_TIMEOUT),
                                    &dir);
    Option option;
    handler.parse(option, "foo");
    CPPUNIT_ASSERT(!option.defined(PREF_TIMEOUT));
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), option.get(PREF_DIR));
  }
}

void OptionHandlerTest::testIntegerRangeOptionHandler()
{
  IntegerRangeOptionHandler handler(PREF_LISTEN_PORT, "", "", 1, 65535);
  Option option;
  handler.parse(option, "1");
  CPPUNIT_ASSERT_EQUAL(std::string("1"), option.get(PREF_LISTEN_PORT));
  handler.parse(option, "1,3,5-9");
  CPPUNIT_ASSERT_EQUAL(std::string("1,3,5-9"), option.get(PREF_LISTEN_PORT));
  handler.parse(option, "65535");
  CPPUNIT_ASSERT_EQUAL(std::string("65535"), option.get(PREF_LISTEN_PORT));
  try {
    handler.parse(option, "0");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "65536");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("1-65535"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testChecksumOptionHandler()
{
  ChecksumOptionHandler handler(PREF_CHECKSUM, "");
  Option option;
  handler.parse(option, "sha-1=a94a8fe5ccb19ba61c4c0873d391e987982fbbd3");
  CPPUNIT_ASSERT_EQUAL(
      std::string("sha-1=a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"),
      option.get(PREF_CHECKSUM));
  try {
    handler.parse(option, "sha-1=invalidhash");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  try {
    handler.parse(option, "");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  // With acceptable types
  ChecksumOptionHandler handler2(PREF_CHECKSUM, "", {"sha-1", "sha-256"});
  try {
    handler2.parse(option, "md5=d41d8cd98f00b204e9800998ecf8427e");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("HASH_TYPE=HEX_DIGEST"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testHostPortOptionHandler()
{
  HostPortOptionHandler handler(PREF_DIR, "", "", PREF_LISTEN_PORT,
                                PREF_TIMEOUT);
  Option option;
  handler.parse(option, "localhost:8080");
  CPPUNIT_ASSERT_EQUAL(std::string("localhost:8080"), option.get(PREF_DIR));
  CPPUNIT_ASSERT_EQUAL(std::string("localhost"), option.get(PREF_LISTEN_PORT));
  CPPUNIT_ASSERT_EQUAL(std::string("8080"), option.get(PREF_TIMEOUT));
  // Verify host:port parsing (no colon → default HTTP port used)
  handler.parse(option, "myhost");
  CPPUNIT_ASSERT_EQUAL(std::string("myhost"), option.get(PREF_LISTEN_PORT));
  CPPUNIT_ASSERT_EQUAL(std::string("80"), option.get(PREF_TIMEOUT));
  CPPUNIT_ASSERT_EQUAL(std::string("HOST:PORT"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testLocalFilePathOptionHandler()
{
  // Test with stdin
  {
    LocalFilePathOptionHandler handler(PREF_DIR, "", "", true);
    Option option;
    handler.parse(option, "-");
    CPPUNIT_ASSERT_EQUAL(std::string(DEV_STDIN), option.get(PREF_DIR));
  }
  // Test with normal path (no mustExist)
  {
    LocalFilePathOptionHandler handler(PREF_DIR, "", "", false, 0, false);
    Option option;
    handler.parse(option, "/tmp/some-path");
    CPPUNIT_ASSERT_EQUAL(std::string("/tmp/some-path"), option.get(PREF_DIR));
  }
  // Test with mustExist but file doesn't exist
  {
    LocalFilePathOptionHandler handler(PREF_DIR, "", "", false, 0, true);
    Option option;
    try {
      handler.parse(option, "/nonexistent/file/path");
      CPPUNIT_FAIL("exception must be thrown.");
    }
    catch (Exception& e) {
    }
  }
  // Test possibleValuesString for stdin accepting
  {
    LocalFilePathOptionHandler handler(PREF_DIR, "", "", true);
    CPPUNIT_ASSERT(!handler.createPossibleValuesString().empty());
  }
  // Test possibleValuesString for non-stdin
  {
    LocalFilePathOptionHandler handler(PREF_DIR, "", "", false);
    CPPUNIT_ASSERT(!handler.createPossibleValuesString().empty());
  }
}

void OptionHandlerTest::testPrioritizePieceOptionHandler()
{
  PrioritizePieceOptionHandler handler(PREF_DIR, "", "");
  Option option;
  handler.parse(option, "head");
  CPPUNIT_ASSERT_EQUAL(std::string("head"), option.get(PREF_DIR));
  handler.parse(option, "tail");
  CPPUNIT_ASSERT_EQUAL(std::string("tail"), option.get(PREF_DIR));
  handler.parse(option, "head=1M,tail=1M");
  CPPUNIT_ASSERT_EQUAL(std::string("head=1M,tail=1M"), option.get(PREF_DIR));
  try {
    handler.parse(option, "invalid");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("head[=SIZE], tail[=SIZE]"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testOptimizeConcurrentDownloadsOptionHandler()
{
  OptimizeConcurrentDownloadsOptionHandler handler(
      PREF_OPTIMIZE_CONCURRENT_DOWNLOADS, "", "");
  Option option;
  handler.parse(option, "true");
  CPPUNIT_ASSERT_EQUAL(std::string(A2_V_TRUE),
                       option.get(PREF_OPTIMIZE_CONCURRENT_DOWNLOADS));
  handler.parse(option, "false");
  CPPUNIT_ASSERT_EQUAL(std::string(A2_V_FALSE),
                       option.get(PREF_OPTIMIZE_CONCURRENT_DOWNLOADS));
  // Empty string treated as true (OPT_ARG)
  handler.parse(option, "");
  CPPUNIT_ASSERT_EQUAL(std::string(A2_V_TRUE),
                       option.get(PREF_OPTIMIZE_CONCURRENT_DOWNLOADS));
  // Coefficient pair
  handler.parse(option, "5.5:3.2");
  CPPUNIT_ASSERT_EQUAL(std::string("5.5"),
                       option.get(PREF_OPTIMIZE_CONCURRENT_DOWNLOADS_COEFFA));
  CPPUNIT_ASSERT_EQUAL(std::string("3.2"),
                       option.get(PREF_OPTIMIZE_CONCURRENT_DOWNLOADS_COEFFB));
  // Bad coefficient
  try {
    handler.parse(option, "abc:def");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  // Missing B coefficient
  try {
    handler.parse(option, "5.5");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("true, false, A:B"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testIndexOutOptionHandler()
{
  IndexOutOptionHandler handler(PREF_INDEX_OUT, "");
  Option option;
  handler.parse(option, "1=/tmp/file");
  CPPUNIT_ASSERT_EQUAL(std::string("1=/tmp/file\n"),
                       option.get(PREF_INDEX_OUT));
  // Accumulates
  handler.parse(option, "2=/tmp/file2");
  CPPUNIT_ASSERT_EQUAL(std::string("1=/tmp/file\n2=/tmp/file2\n"),
                       option.get(PREF_INDEX_OUT));
  try {
    handler.parse(option, "invalid");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
  CPPUNIT_ASSERT_EQUAL(std::string("INDEX=PATH"),
                       handler.createPossibleValuesString());
}

void OptionHandlerTest::testCumulativeOptionHandler()
{
  CumulativeOptionHandler handler(PREF_DIR, "", "", "\n", "");
  Option option;
  handler.parse(option, "value1");
  CPPUNIT_ASSERT_EQUAL(std::string("value1\n"), option.get(PREF_DIR));
  handler.parse(option, "value2");
  CPPUNIT_ASSERT_EQUAL(std::string("value1\nvalue2\n"), option.get(PREF_DIR));
}

void OptionHandlerTest::testDefaultOptionHandler_noEmpty()
{
  DefaultOptionHandler handler(PREF_DIR);
  handler.setAllowEmpty(false);
  Option option;
  handler.parse(option, "value");
  CPPUNIT_ASSERT_EQUAL(std::string("value"), option.get(PREF_DIR));
  try {
    handler.parse(option, "");
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (Exception& e) {
  }
}

} // namespace aria2
