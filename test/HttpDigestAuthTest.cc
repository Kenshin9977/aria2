/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2024 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "HttpDigestAuth.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class HttpDigestAuthTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpDigestAuthTest);
  CPPUNIT_TEST(testParseChallenge);
  CPPUNIT_TEST(testParseChallengeQuotedValues);
  CPPUNIT_TEST(testParseChallengeMinimal);
  CPPUNIT_TEST(testParseChallengeEmpty);
  CPPUNIT_TEST(testMapAlgorithm);
  CPPUNIT_TEST(testHashHexMD5);
  CPPUNIT_TEST(testHashHexSHA256);
  CPPUNIT_TEST(testHashHexUnsupported);
  CPPUNIT_TEST(testComputeAuthHeader);
  CPPUNIT_TEST(testComputeAuthHeaderNoQop);
  CPPUNIT_TEST_SUITE_END();

public:
  void testParseChallenge();
  void testParseChallengeQuotedValues();
  void testParseChallengeMinimal();
  void testParseChallengeEmpty();
  void testMapAlgorithm();
  void testHashHexMD5();
  void testHashHexSHA256();
  void testHashHexUnsupported();
  void testComputeAuthHeader();
  void testComputeAuthHeaderNoQop();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpDigestAuthTest);

void HttpDigestAuthTest::testParseChallenge()
{
  // RFC 7616 Section 3.9.1 challenge
  std::string header =
      "realm=\"http-auth@example.org\", "
      "qop=\"auth, auth-int\", "
      "algorithm=SHA-256, "
      "nonce=\"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\", "
      "opaque=\"FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS\"";

  DigestChallenge result;
  CPPUNIT_ASSERT(http_digest_auth::parseChallenge(header, result));
  CPPUNIT_ASSERT_EQUAL(std::string("http-auth@example.org"), result.realm);
  CPPUNIT_ASSERT_EQUAL(
      std::string("7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v"),
      result.nonce);
  CPPUNIT_ASSERT_EQUAL(
      std::string("FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"),
      result.opaque);
  CPPUNIT_ASSERT_EQUAL(std::string("auth, auth-int"), result.qop);
  CPPUNIT_ASSERT_EQUAL(std::string("SHA-256"), result.algorithm);
}

void HttpDigestAuthTest::testParseChallengeQuotedValues()
{
  std::string header =
      "realm=\"test\\\"realm\", nonce=\"abc123\", "
      "opaque=\"xyz\"";

  DigestChallenge result;
  CPPUNIT_ASSERT(http_digest_auth::parseChallenge(header, result));
  CPPUNIT_ASSERT_EQUAL(std::string("test\"realm"), result.realm);
  CPPUNIT_ASSERT_EQUAL(std::string("abc123"), result.nonce);
  CPPUNIT_ASSERT_EQUAL(std::string("xyz"), result.opaque);
}

void HttpDigestAuthTest::testParseChallengeMinimal()
{
  // Minimal valid challenge: just a nonce
  std::string header = "nonce=\"server-nonce-value\"";

  DigestChallenge result;
  CPPUNIT_ASSERT(http_digest_auth::parseChallenge(header, result));
  CPPUNIT_ASSERT_EQUAL(std::string("server-nonce-value"), result.nonce);
  CPPUNIT_ASSERT(result.realm.empty());
  CPPUNIT_ASSERT(result.algorithm.empty());
}

void HttpDigestAuthTest::testParseChallengeEmpty()
{
  DigestChallenge result;
  CPPUNIT_ASSERT(!http_digest_auth::parseChallenge("", result));
  CPPUNIT_ASSERT(!http_digest_auth::parseChallenge("realm=\"foo\"", result));
}

void HttpDigestAuthTest::testMapAlgorithm()
{
  CPPUNIT_ASSERT_EQUAL(std::string("md5"),
                        http_digest_auth::mapAlgorithm("MD5"));
  CPPUNIT_ASSERT_EQUAL(std::string("md5"),
                        http_digest_auth::mapAlgorithm("md5"));
  CPPUNIT_ASSERT_EQUAL(std::string("md5"),
                        http_digest_auth::mapAlgorithm("MD5-sess"));
  CPPUNIT_ASSERT_EQUAL(std::string("md5"),
                        http_digest_auth::mapAlgorithm(""));
  CPPUNIT_ASSERT_EQUAL(std::string("sha-256"),
                        http_digest_auth::mapAlgorithm("SHA-256"));
  CPPUNIT_ASSERT_EQUAL(std::string("sha-256"),
                        http_digest_auth::mapAlgorithm("sha-256"));
  CPPUNIT_ASSERT_EQUAL(std::string("sha-256"),
                        http_digest_auth::mapAlgorithm("SHA-256-sess"));
  CPPUNIT_ASSERT_EQUAL(std::string(""),
                        http_digest_auth::mapAlgorithm("UNSUPPORTED"));
}

void HttpDigestAuthTest::testHashHexMD5()
{
  // MD5("Mufasa:http-auth@example.org:Circle of Life")
  auto ha1 = http_digest_auth::hashHex(
      "MD5", "Mufasa:http-auth@example.org:Circle of Life");
  CPPUNIT_ASSERT(!ha1.empty());
  // MD5("") = d41d8cd98f00b204e9800998ecf8427e
  CPPUNIT_ASSERT_EQUAL(std::string("d41d8cd98f00b204e9800998ecf8427e"),
                        http_digest_auth::hashHex("MD5", ""));
}

void HttpDigestAuthTest::testHashHexSHA256()
{
  // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  CPPUNIT_ASSERT_EQUAL(
      std::string(
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
      http_digest_auth::hashHex("SHA-256", ""));
}

void HttpDigestAuthTest::testHashHexUnsupported()
{
  CPPUNIT_ASSERT_EQUAL(std::string(""),
                        http_digest_auth::hashHex("UNSUPPORTED", "data"));
}

void HttpDigestAuthTest::testComputeAuthHeader()
{
  DigestChallenge challenge;
  challenge.realm = "http-auth@example.org";
  challenge.nonce = "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v";
  challenge.opaque = "FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS";
  challenge.qop = "auth";
  challenge.algorithm = "MD5";

  auto header = http_digest_auth::computeAuthHeader(
      "Mufasa", "Circle of Life", "GET", "/dir/index.html", challenge, 1);

  // Verify the header starts with "Digest"
  CPPUNIT_ASSERT(header.find("Digest ") == 0);

  // Verify required fields are present
  CPPUNIT_ASSERT(header.find("username=\"Mufasa\"") != std::string::npos);
  CPPUNIT_ASSERT(header.find("realm=\"http-auth@example.org\"") !=
                 std::string::npos);
  CPPUNIT_ASSERT(
      header.find(
          "nonce=\"7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v\"") !=
      std::string::npos);
  CPPUNIT_ASSERT(header.find("uri=\"/dir/index.html\"") !=
                 std::string::npos);
  CPPUNIT_ASSERT(header.find("response=\"") != std::string::npos);
  CPPUNIT_ASSERT(header.find("algorithm=MD5") != std::string::npos);
  CPPUNIT_ASSERT(header.find("opaque=\"") != std::string::npos);
  CPPUNIT_ASSERT(header.find("qop=auth") != std::string::npos);
  CPPUNIT_ASSERT(header.find("nc=00000001") != std::string::npos);
  CPPUNIT_ASSERT(header.find("cnonce=\"") != std::string::npos);
}

void HttpDigestAuthTest::testComputeAuthHeaderNoQop()
{
  DigestChallenge challenge;
  challenge.realm = "testrealm@host.com";
  challenge.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
  challenge.algorithm = "MD5";

  auto header = http_digest_auth::computeAuthHeader(
      "Mufasa", "Circle of Life", "GET", "/dir/index.html", challenge, 1);

  CPPUNIT_ASSERT(header.find("Digest ") == 0);
  CPPUNIT_ASSERT(header.find("username=\"Mufasa\"") != std::string::npos);
  // No qop, nc, or cnonce in legacy mode
  CPPUNIT_ASSERT(header.find("qop=") == std::string::npos);
  CPPUNIT_ASSERT(header.find("nc=") == std::string::npos);
  CPPUNIT_ASSERT(header.find("cnonce=") == std::string::npos);
}

} // namespace aria2
