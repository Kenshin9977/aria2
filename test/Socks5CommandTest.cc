#include "Socks5HandshakeCommand.h"
#include "Socks5ConnectCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "AbstractCommand.h"
#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "FileEntry.h"
#include "GroupId.h"
#include "MockSocketCore.h"
#include "Option.h"
#include "Request.h"
#include "RequestGroup.h"
#include "TestEngineHelper.h"
#include "prefs.h"

namespace aria2 {

namespace {

struct Socks5TestContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<FileEntry> fileEntry;
  std::shared_ptr<Request> req;
  std::shared_ptr<Request> proxyReq;
  std::shared_ptr<MockSocketCore> mockSocket;
  // engine declared last so it is destroyed first; commands hold raw
  // pointers to rg, which must outlive the engine.
  std::unique_ptr<DownloadEngine> engine;

  void setUp(const std::string& uri = "http://example.com:80/file")
  {
    engine = createTestEngine(option, "aria2_Socks5CommandTest");
    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
    fileEntry = std::make_shared<FileEntry>("dummy", 0, 0);
    req = std::make_shared<Request>();
    req->setUri(uri);
    proxyReq = std::make_shared<Request>();
    proxyReq->setUri("http://proxy:1080/");
    mockSocket = std::make_shared<MockSocketCore>();
  }
};

// Collect all data written to mock socket into one string.
std::string allWritten(const std::shared_ptr<MockSocketCore>& s)
{
  std::string result;
  for (const auto& d : s->writtenData) {
    result += d;
  }
  return result;
}

} // namespace

// =====================================================================
// Socks5HandshakeCommandTest
// =====================================================================

class Socks5HandshakeCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(Socks5HandshakeCommandTest);
  CPPUNIT_TEST(testGetProxyUri_socks5);
  CPPUNIT_TEST(testGetProxyUri_socks5Priority);
  CPPUNIT_TEST(testGreetingNoAuth);
  CPPUNIT_TEST(testGreetingWithAuth);
  CPPUNIT_TEST(testHandshakeNoAuth);
  CPPUNIT_TEST(testHandshakeWithAuth);
  CPPUNIT_TEST(testHandshakeAuthRejected);
  CPPUNIT_TEST(testHandshakeVersionError);
  CPPUNIT_TEST_SUITE_END();

  Socks5TestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testGetProxyUri_socks5();
  void testGetProxyUri_socks5Priority();
  void testGreetingNoAuth();
  void testGreetingWithAuth();
  void testHandshakeNoAuth();
  void testHandshakeWithAuth();
  void testHandshakeAuthRejected();
  void testHandshakeVersionError();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Socks5HandshakeCommandTest);

void Socks5HandshakeCommandTest::testGetProxyUri_socks5()
{
  Option op;
  op.put(PREF_SOCKS5_PROXY, "socks5://proxy:1080/");
  // SOCKS5 proxy should be returned for all protocols
  CPPUNIT_ASSERT_EQUAL(std::string("socks5://proxy:1080/"),
                       getProxyUri(Protocol::HTTP, &op));
  CPPUNIT_ASSERT_EQUAL(std::string("socks5://proxy:1080/"),
                       getProxyUri(Protocol::HTTPS, &op));
  CPPUNIT_ASSERT_EQUAL(std::string("socks5://proxy:1080/"),
                       getProxyUri(Protocol::FTP, &op));
}

void Socks5HandshakeCommandTest::testGetProxyUri_socks5Priority()
{
  Option op;
  op.put(PREF_SOCKS5_PROXY, "socks5://proxy:1080/");
  op.put(PREF_HTTP_PROXY, "http://httpproxy:8080/");
  // SOCKS5 takes priority over HTTP proxy
  CPPUNIT_ASSERT_EQUAL(std::string("socks5://proxy:1080/"),
                       getProxyUri(Protocol::HTTP, &op));
}

void Socks5HandshakeCommandTest::testGreetingNoAuth()
{
  // Pre-load server response: version=5, method=AUTH_NONE
  ctx_.mockSocket->readBuffer = std::string("\x05\x00", 2);

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  // Disable socket checks so shouldProcess() returns true with mock fd.
  // The constructor registered a write check, which a fake fd can't
  // trigger in SelectEventPoll.
  cmd->disableWriteCheckSocket();
  cmd->execute();

  // Verify greeting bytes: version=5, nmethods=1, AUTH_NONE
  std::string written = allWritten(ctx_.mockSocket);
  CPPUNIT_ASSERT_EQUAL(size_t(3), written.size());
  CPPUNIT_ASSERT_EQUAL('\x05', written[0]); // SOCKS5
  CPPUNIT_ASSERT_EQUAL('\x01', written[1]); // 1 method
  CPPUNIT_ASSERT_EQUAL('\x00', written[2]); // AUTH_NONE
}

void Socks5HandshakeCommandTest::testGreetingWithAuth()
{
  ctx_.option->put(PREF_SOCKS5_PROXY_USER, "user");
  ctx_.option->put(PREF_SOCKS5_PROXY_PASSWD, "pass");

  // Pre-load server responses: greeting (AUTH_USERPASS) + auth success
  std::string serverResp;
  serverResp += std::string("\x05\x02", 2);
  serverResp += std::string("\x01\x00", 2);
  ctx_.mockSocket->readBuffer = serverResp;

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();
  cmd->execute();

  std::string written = allWritten(ctx_.mockSocket);
  // First write: greeting (version=5, nmethods=2, AUTH_NONE, AUTH_USERPASS)
  CPPUNIT_ASSERT(written.size() >= 4);
  CPPUNIT_ASSERT_EQUAL('\x05', written[0]); // SOCKS5
  CPPUNIT_ASSERT_EQUAL('\x02', written[1]); // 2 methods
  CPPUNIT_ASSERT_EQUAL('\x00', written[2]); // AUTH_NONE
  CPPUNIT_ASSERT_EQUAL('\x02', written[3]); // AUTH_USERPASS

  // Second write: auth (version=1, ulen=4, "user", plen=4, "pass")
  CPPUNIT_ASSERT_EQUAL(size_t(4 + 1 + 1 + 4 + 1 + 4), written.size());
  size_t off = 4;
  CPPUNIT_ASSERT_EQUAL('\x01', written[off]);     // USERPASS version
  CPPUNIT_ASSERT_EQUAL('\x04', written[off + 1]); // user length
  CPPUNIT_ASSERT_EQUAL(std::string("user"),
                       written.substr(off + 2, 4));
  CPPUNIT_ASSERT_EQUAL('\x04', written[off + 6]); // passwd length
  CPPUNIT_ASSERT_EQUAL(std::string("pass"),
                       written.substr(off + 7, 4));
}

void Socks5HandshakeCommandTest::testHandshakeNoAuth()
{
  ctx_.mockSocket->readBuffer = std::string("\x05\x00", 2);

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5HandshakeCommandTest::testHandshakeWithAuth()
{
  ctx_.option->put(PREF_SOCKS5_PROXY_USER, "admin");
  ctx_.option->put(PREF_SOCKS5_PROXY_PASSWD, "secret");

  std::string serverResp;
  serverResp += std::string("\x05\x02", 2);
  serverResp += std::string("\x01\x00", 2);
  ctx_.mockSocket->readBuffer = serverResp;

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());

  std::string written = allWritten(ctx_.mockSocket);
  CPPUNIT_ASSERT(written.find("admin") != std::string::npos);
  CPPUNIT_ASSERT(written.find("secret") != std::string::npos);
}

void Socks5HandshakeCommandTest::testHandshakeAuthRejected()
{
  // Server rejects all auth methods (0xFF)
  ctx_.mockSocket->readBuffer = std::string("\x05\xff", 2);

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  // execute() catches the DlAbortEx thrown by the version check
  // and returns true via the error-handling path
  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5HandshakeCommandTest::testHandshakeVersionError()
{
  // Server sends wrong version (0x04 instead of 0x05)
  ctx_.mockSocket->readBuffer = std::string("\x04\x00", 2);

  auto cmd = make_unique<Socks5HandshakeCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

// =====================================================================
// Socks5ConnectCommandTest
// =====================================================================

class Socks5ConnectCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(Socks5ConnectCommandTest);
  CPPUNIT_TEST(testConnectRequestBytes);
  CPPUNIT_TEST(testConnectRequestPort443);
  CPPUNIT_TEST(testConnectSuccessIPv4);
  CPPUNIT_TEST(testConnectSuccessIPv6);
  CPPUNIT_TEST(testConnectSuccessDomain);
  CPPUNIT_TEST(testConnectFailure);
  CPPUNIT_TEST(testConnectBadVersion);
  CPPUNIT_TEST_SUITE_END();

  Socks5TestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testConnectRequestBytes();
  void testConnectRequestPort443();
  void testConnectSuccessIPv4();
  void testConnectSuccessIPv6();
  void testConnectSuccessDomain();
  void testConnectFailure();
  void testConnectBadVersion();
};

CPPUNIT_TEST_SUITE_REGISTRATION(Socks5ConnectCommandTest);

void Socks5ConnectCommandTest::testConnectRequestBytes()
{
  // Pre-load success response: IPv4
  std::string resp("\x05\x00\x00\x01"  // ver, success, rsv, IPv4
                   "\x7f\x00\x00\x01"  // 127.0.0.1
                   "\x00\x50",          // port 80
                   10);
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();
  cmd->execute();

  // Verify CONNECT request bytes
  std::string written = allWritten(ctx_.mockSocket);
  CPPUNIT_ASSERT(written.size() >= 7);
  CPPUNIT_ASSERT_EQUAL('\x05', written[0]); // SOCKS5
  CPPUNIT_ASSERT_EQUAL('\x01', written[1]); // CMD_CONNECT
  CPPUNIT_ASSERT_EQUAL('\x00', written[2]); // RSV
  CPPUNIT_ASSERT_EQUAL('\x03', written[3]); // ATYP_DOMAIN

  // Domain: "example.com" (11 bytes)
  CPPUNIT_ASSERT_EQUAL('\x0b', written[4]); // domain length = 11
  CPPUNIT_ASSERT_EQUAL(std::string("example.com"),
                       written.substr(5, 11));

  // Port 80 in network byte order
  CPPUNIT_ASSERT_EQUAL('\x00', written[16]); // port high byte
  CPPUNIT_ASSERT_EQUAL('\x50', written[17]); // port low byte (80)
}

void Socks5ConnectCommandTest::testConnectRequestPort443()
{
  ctx_.setUp("https://secure.example.org:443/path");

  std::string resp("\x05\x00\x00\x01"
                   "\x7f\x00\x00\x01"
                   "\x01\xbb",           // port 443
                   10);
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();
  cmd->execute();

  std::string written = allWritten(ctx_.mockSocket);
  // Domain: "secure.example.org" (18 bytes)
  CPPUNIT_ASSERT_EQUAL('\x12', written[4]); // domain length = 18
  CPPUNIT_ASSERT_EQUAL(std::string("secure.example.org"),
                       written.substr(5, 18));
  // Port 443 = 0x01BB
  size_t portOff = 5 + 18;
  CPPUNIT_ASSERT_EQUAL('\x01', written[portOff]);     // high byte
  CPPUNIT_ASSERT_EQUAL('\xbb', written[portOff + 1]); // low byte
}

void Socks5ConnectCommandTest::testConnectSuccessIPv4()
{
  std::string resp("\x05\x00\x00\x01"
                   "\xc0\xa8\x01\x01"  // 192.168.1.1
                   "\x13\x88",          // port 5000
                   10);
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5ConnectCommandTest::testConnectSuccessIPv6()
{
  std::string resp;
  resp += std::string("\x05\x00\x00\x04", 4); // ver, success, rsv, IPv6
  // ::1 in 16 bytes
  resp += std::string(15, '\x00');
  resp += '\x01';
  resp += std::string("\x00\x50", 2); // port 80
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5ConnectCommandTest::testConnectSuccessDomain()
{
  std::string resp;
  resp += std::string("\x05\x00\x00\x03", 4); // ver, success, rsv, DOMAIN
  resp += '\x0b'; // domain length 11
  resp += "example.com";
  resp += std::string("\x00\x50", 2); // port 80
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5ConnectCommandTest::testConnectFailure()
{
  // REP=0x05 (connection refused)
  std::string resp("\x05\x05\x00\x01"
                   "\x00\x00\x00\x00"
                   "\x00\x00",
                   10);
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  // execute() catches the DlAbortEx and handles it
  CPPUNIT_ASSERT(cmd->execute());
}

void Socks5ConnectCommandTest::testConnectBadVersion()
{
  std::string resp("\x04\x00\x00\x01"
                   "\x00\x00\x00\x00"
                   "\x00\x00",
                   10);
  ctx_.mockSocket->readBuffer = resp;

  auto cmd = make_unique<Socks5ConnectCommand>(
      ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
      ctx_.engine.get(), ctx_.proxyReq,
      std::static_pointer_cast<ISocketCore>(ctx_.mockSocket));
  cmd->disableWriteCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
}

} // namespace aria2
