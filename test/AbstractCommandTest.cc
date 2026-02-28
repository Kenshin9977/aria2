#include "AbstractCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadContext.h"
#include "DownloadEngine.h"
#include "FileEntry.h"
#include "GroupId.h"
#include "MockSocketCore.h"
#include "Option.h"
#include "Request.h"
#include "RequestGroup.h"
#include "SocketRecvBuffer.h"
#include "TestEngineHelper.h"
#include "MockPieceStorage.h"
#include "prefs.h"

namespace aria2 {

namespace {

class StubCommand : public AbstractCommand {
public:
  int executeInternalCount = 0;
  bool executeInternalReturn = true;

  StubCommand(cuid_t cuid, const std::shared_ptr<Request>& req,
              const std::shared_ptr<FileEntry>& fileEntry,
              RequestGroup* requestGroup, DownloadEngine* e,
              const std::shared_ptr<ISocketCore>& s = nullptr,
              const std::shared_ptr<SocketRecvBuffer>& recvBuf = nullptr)
      : AbstractCommand(cuid, req, fileEntry, requestGroup, e, s, recvBuf)
  {
  }

protected:
  bool executeInternal() override
  {
    ++executeInternalCount;
    return executeInternalReturn;
  }
};

struct CommandTestContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<FileEntry> fileEntry;
  // engine must be declared last: its commands hold raw pointers to rg,
  // so rg must outlive the engine.
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_AbstractCommandTest");
    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "dummy"));
    fileEntry = std::make_shared<FileEntry>("dummy", 0, 0);
  }
};

} // namespace

class AbstractCommandTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(AbstractCommandTest);
  CPPUNIT_TEST(testGetProxyUri);
  CPPUNIT_TEST(testGetProxyUri_sftp);
  CPPUNIT_TEST(testExecute_haltReturnsTrue);
  CPPUNIT_TEST(testExecute_downloadFinishedReturnsTrue);
  CPPUNIT_TEST(testExecute_noSocketCallsInternal);
  CPPUNIT_TEST(testExecute_noCheckFallthrough);
  CPPUNIT_TEST(testExecute_mockSocketPassedToInternal);
  CPPUNIT_TEST(testResolveProxyMethod);
  CPPUNIT_TEST(testCreateProxyRequest);
  CPPUNIT_TEST(testIsProxyDefined);
  CPPUNIT_TEST(testWriteCheckSocket);
  CPPUNIT_TEST(testSwapSocket);
  CPPUNIT_TEST_SUITE_END();

  CommandTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testGetProxyUri();
  void testGetProxyUri_sftp();
  void testExecute_haltReturnsTrue();
  void testExecute_downloadFinishedReturnsTrue();
  void testExecute_noSocketCallsInternal();
  void testExecute_noCheckFallthrough();
  void testExecute_mockSocketPassedToInternal();
  void testResolveProxyMethod();
  void testCreateProxyRequest();
  void testIsProxyDefined();
  void testWriteCheckSocket();
  void testSwapSocket();
};

CPPUNIT_TEST_SUITE_REGISTRATION(AbstractCommandTest);

void AbstractCommandTest::testGetProxyUri()
{
  Option op;
  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri(Protocol::HTTP, &op));

  op.put(PREF_HTTP_PROXY, "http://hu:hp@httpproxy/");
  op.put(PREF_FTP_PROXY, "ftp://fu:fp@ftpproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hu:hp@httpproxy/"),
                       getProxyUri(Protocol::HTTP, &op));
  CPPUNIT_ASSERT_EQUAL(std::string("ftp://fu:fp@ftpproxy/"),
                       getProxyUri(Protocol::FTP, &op));

  op.put(PREF_ALL_PROXY, "http://au:ap@allproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://au:ap@allproxy/"),
                       getProxyUri(Protocol::HTTPS, &op));

  op.put(PREF_ALL_PROXY_USER, "aunew");
  op.put(PREF_ALL_PROXY_PASSWD, "apnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://aunew:apnew@allproxy/"),
                       getProxyUri(Protocol::HTTPS, &op));

  op.put(PREF_HTTPS_PROXY, "http://hsu:hsp@httpsproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hsu:hsp@httpsproxy/"),
                       getProxyUri(Protocol::HTTPS, &op));

  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri(Protocol::UNKNOWN, &op));

  op.put(PREF_HTTP_PROXY_USER, "hunew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hp@httpproxy/"),
                       getProxyUri(Protocol::HTTP, &op));

  op.put(PREF_HTTP_PROXY_PASSWD, "hpnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hpnew@httpproxy/"),
                       getProxyUri(Protocol::HTTP, &op));

  op.put(PREF_HTTP_PROXY_USER, "");
  CPPUNIT_ASSERT_EQUAL(std::string("http://httpproxy/"),
                       getProxyUri(Protocol::HTTP, &op));
}

void AbstractCommandTest::testExecute_haltReturnsTrue()
{
  ctx_.rg->setHaltRequested(true);

  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());
  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(0, cmd->executeInternalCount);
}

void AbstractCommandTest::testExecute_noSocketCallsInternal()
{
  // No socket, no read/write checks → shouldProcess() returns true
  // → executeInternal() is called.
  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());
  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(1, cmd->executeInternalCount);
}

void AbstractCommandTest::testExecute_noCheckFallthrough()
{
  auto mockSocket = std::make_shared<MockSocketCore>();
  mockSocket->readable = true;

  // The AbstractCommand constructor calls setReadCheckSocket(socket_)
  // when socket_->isOpen() is true.  The engine then registers
  // readCheckTarget_ for event polling.  However, in our test the
  // SelectEventPoll won't report readability because it's a mock fd.
  //
  // Instead we test the path where checkSocketIsReadable_==false and
  // checkSocketIsWritable_==false (no socket check), which means
  // shouldProcess() returns true.  We verify executeInternal is called.
  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get(), mockSocket);

  // The constructor registered the mock socket for read check.
  // Disable it so shouldProcess() falls through to the
  // "no check" path.
  cmd->disableReadCheckSocket();

  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(1, cmd->executeInternalCount);
}

void AbstractCommandTest::testExecute_mockSocketPassedToInternal()
{
  auto mockSocket = std::make_shared<MockSocketCore>();
  mockSocket->readBuffer = "hello";

  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get(), mockSocket);

  // Verify getSocket() returns our mock.
  CPPUNIT_ASSERT(cmd->getSocket().get() == mockSocket.get());

  // The subclass can read through the mock socket.
  char buf[16] = {};
  size_t len = sizeof(buf);
  cmd->getSocket()->readData(buf, len);
  CPPUNIT_ASSERT_EQUAL(std::string("hello"), std::string(buf, len));

  // The subclass can write through the mock socket.
  cmd->getSocket()->writeData("world", 5);
  CPPUNIT_ASSERT_EQUAL(size_t(1), mockSocket->writtenData.size());
  CPPUNIT_ASSERT_EQUAL(std::string("world"), mockSocket->writtenData[0]);

  // Disable read check (constructor registered it) so shouldProcess()
  // returns true, then verify executeInternal() is called.
  cmd->disableReadCheckSocket();
  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(1, cmd->executeInternalCount);
}

void AbstractCommandTest::testGetProxyUri_sftp()
{
  Option op;
  // sftp should use FTP proxy settings
  op.put(PREF_FTP_PROXY, "http://ftpproxy:8080/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://ftpproxy:8080/"),
                       getProxyUri(Protocol::SFTP, &op));

  // Falls back to ALL_PROXY when no FTP proxy
  Option op2;
  op2.put(PREF_ALL_PROXY, "http://allproxy:3128/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://allproxy:3128/"),
                       getProxyUri(Protocol::SFTP, &op2));
}

void AbstractCommandTest::testExecute_downloadFinishedReturnsTrue()
{
  auto ps = std::make_shared<MockPieceStorage>();
  ps->setDownloadFinished(true);
  ctx_.rg->setPieceStorage(ps);

  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());
  // downloadFinished() returns true → execute() returns true early
  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(0, cmd->executeInternalCount);
}

void AbstractCommandTest::testResolveProxyMethod()
{
  // Create a command with a request so resolveProxyMethod() works
  auto req = std::make_shared<Request>();
  req->setUri("http://example.com/file");
  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), req, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());

  // Default: GET for HTTP
  CPPUNIT_ASSERT_EQUAL(std::string("get"),
                       cmd->resolveProxyMethod(Protocol::HTTP));

  // HTTPS always uses tunnel
  CPPUNIT_ASSERT_EQUAL(std::string("tunnel"),
                       cmd->resolveProxyMethod(Protocol::HTTPS));

  // SFTP always uses tunnel
  CPPUNIT_ASSERT_EQUAL(std::string("tunnel"),
                       cmd->resolveProxyMethod(Protocol::SFTP));

  // When PREF_PROXY_METHOD is tunnel, HTTP also uses tunnel
  ctx_.option->put(PREF_PROXY_METHOD, "tunnel");
  CPPUNIT_ASSERT_EQUAL(std::string("tunnel"),
                       cmd->resolveProxyMethod(Protocol::HTTP));
}

void AbstractCommandTest::testCreateProxyRequest()
{
  auto req = std::make_shared<Request>();
  req->setUri("http://example.com/file");
  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), req, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());

  // No proxy defined — returns null
  auto proxyReq = cmd->createProxyRequest();
  CPPUNIT_ASSERT(!proxyReq);

  // Set HTTP proxy
  ctx_.option->put(PREF_HTTP_PROXY, "http://proxy:8080/");
  proxyReq = cmd->createProxyRequest();
  CPPUNIT_ASSERT(proxyReq);
  CPPUNIT_ASSERT_EQUAL(std::string("proxy"), proxyReq->getHost());
  CPPUNIT_ASSERT_EQUAL(static_cast<uint16_t>(8080), proxyReq->getPort());

  // Set no-proxy for this host
  ctx_.option->put(PREF_NO_PROXY, "example.com");
  proxyReq = cmd->createProxyRequest();
  CPPUNIT_ASSERT(!proxyReq);
}

void AbstractCommandTest::testIsProxyDefined()
{
  auto req = std::make_shared<Request>();
  req->setUri("http://example.com/file");
  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), req, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());

  // No proxy — not defined
  CPPUNIT_ASSERT(!cmd->isProxyDefined());

  // Set HTTP proxy — defined
  ctx_.option->put(PREF_HTTP_PROXY, "http://proxy:8080/");
  CPPUNIT_ASSERT(cmd->isProxyDefined());

  // Set no-proxy matching the host — not defined
  ctx_.option->put(PREF_NO_PROXY, "example.com");
  CPPUNIT_ASSERT(!cmd->isProxyDefined());
}

void AbstractCommandTest::testWriteCheckSocket()
{
  auto mockSocket = std::make_shared<MockSocketCore>();

  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get(), mockSocket);

  // Disable the read check that constructor registered
  cmd->disableReadCheckSocket();

  // Set write check — should not throw
  cmd->setWriteCheckSocket(mockSocket);

  // setWriteCheckSocketIf with false — disables write check
  cmd->setWriteCheckSocketIf(mockSocket, false);

  // setWriteCheckSocketIf with true — enables write check
  cmd->setWriteCheckSocketIf(mockSocket, true);

  // Disable explicitly
  cmd->disableWriteCheckSocket();
}

void AbstractCommandTest::testSwapSocket()
{
  auto mockSocket1 = std::make_shared<MockSocketCore>();
  auto mockSocket2 = std::make_shared<MockSocketCore>();
  mockSocket2->readBuffer = "swapped";

  auto cmd =
      std::make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get(), mockSocket1);

  // Disable read check so destructor doesn't crash
  cmd->disableReadCheckSocket();

  // Swap to a new socket — swapSocket takes shared_ptr<ISocketCore>&
  std::shared_ptr<ISocketCore> newSocket = mockSocket2;
  cmd->swapSocket(newSocket);

  // After swap, getSocket() should return the new socket
  char buf[16] = {};
  size_t len = sizeof(buf);
  cmd->getSocket()->readData(buf, len);
  CPPUNIT_ASSERT_EQUAL(std::string("swapped"), std::string(buf, len));
}

} // namespace aria2
