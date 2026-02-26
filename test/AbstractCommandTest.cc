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
  CPPUNIT_TEST(testExecute_haltReturnsTrue);
  CPPUNIT_TEST(testExecute_noSocketCallsInternal);
  CPPUNIT_TEST(testExecute_noCheckFallthrough);
  CPPUNIT_TEST(testExecute_mockSocketPassedToInternal);
  CPPUNIT_TEST_SUITE_END();

  CommandTestContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testGetProxyUri();
  void testExecute_haltReturnsTrue();
  void testExecute_noSocketCallsInternal();
  void testExecute_noCheckFallthrough();
  void testExecute_mockSocketPassedToInternal();
};

CPPUNIT_TEST_SUITE_REGISTRATION(AbstractCommandTest);

void AbstractCommandTest::testGetProxyUri()
{
  Option op;
  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY, "http://hu:hp@httpproxy/");
  op.put(PREF_FTP_PROXY, "ftp://fu:fp@ftpproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hu:hp@httpproxy/"),
                       getProxyUri("http", &op));
  CPPUNIT_ASSERT_EQUAL(std::string("ftp://fu:fp@ftpproxy/"),
                       getProxyUri("ftp", &op));

  op.put(PREF_ALL_PROXY, "http://au:ap@allproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://au:ap@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_ALL_PROXY_USER, "aunew");
  op.put(PREF_ALL_PROXY_PASSWD, "apnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://aunew:apnew@allproxy/"),
                       getProxyUri("https", &op));

  op.put(PREF_HTTPS_PROXY, "http://hsu:hsp@httpsproxy/");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hsu:hsp@httpsproxy/"),
                       getProxyUri("https", &op));

  CPPUNIT_ASSERT_EQUAL(std::string(), getProxyUri("unknown", &op));

  op.put(PREF_HTTP_PROXY_USER, "hunew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hp@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_PASSWD, "hpnew");
  CPPUNIT_ASSERT_EQUAL(std::string("http://hunew:hpnew@httpproxy/"),
                       getProxyUri("http", &op));

  op.put(PREF_HTTP_PROXY_USER, "");
  CPPUNIT_ASSERT_EQUAL(std::string("http://httpproxy/"),
                       getProxyUri("http", &op));
}

void AbstractCommandTest::testExecute_haltReturnsTrue()
{
  ctx_.rg->setHaltRequested(true);

  auto cmd =
      make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
                               ctx_.rg.get(), ctx_.engine.get());
  CPPUNIT_ASSERT(cmd->execute());
  CPPUNIT_ASSERT_EQUAL(0, cmd->executeInternalCount);
}

void AbstractCommandTest::testExecute_noSocketCallsInternal()
{
  // No socket, no read/write checks → shouldProcess() returns true
  // → executeInternal() is called.
  auto cmd =
      make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
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
      make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
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
      make_unique<StubCommand>(ctx_.engine->newCUID(), nullptr, ctx_.fileEntry,
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

} // namespace aria2
