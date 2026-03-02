// test/FtpCommandIntegrationTest.cc
#include "FtpNegotiationCommand.h"
#include "SocketCore.h"
#include "Request.h"
#include "FileEntry.h"
#include "RequestGroup.h"
#include "DownloadContext.h"
#include "GroupId.h"
#include "Option.h"
#include "AuthConfigFactory.h"
#include "prefs.h"

#include <cppunit/extensions/HelperMacros.h>

#include "TestEngineHelper.h"

namespace aria2 {

namespace {

struct FtpIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<Request> req;
  std::shared_ptr<FileEntry> fileEntry;
  std::shared_ptr<SocketCore> clientSocket;
  std::shared_ptr<SocketCore> serverSocket;
  // engine last -- destroyed first
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_FtpCmdIntTest", true);
    engine->setAuthConfigFactory(std::make_unique<AuthConfigFactory>());

    option->put(PREF_CONNECT_TIMEOUT, "10");
    option->put(PREF_TIMEOUT, "10");
    option->put(PREF_SPLIT, "1");
    option->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
    option->put(PREF_MAX_UPLOAD_LIMIT, "0");
    option->put(PREF_FTP_PASV, "true");
    option->put(PREF_FTP_REUSE_CONNECTION, "false");
    option->put(PREF_REMOTE_TIME, "false");

    // Connected socket pair via listen/connect/accept
    SocketCore listenSock;
    listenSock.bind(0);
    listenSock.beginListen();
    listenSock.setBlockingMode();
    uint16_t port = listenSock.getAddrInfo().port;

    clientSocket = std::make_shared<SocketCore>();
    clientSocket->establishConnection("localhost", port);
    waitWrite(clientSocket);
    serverSocket = listenSock.acceptConnection();
    serverSocket->setBlockingMode();

    req = std::make_shared<Request>();
    req->setUri("ftp://localhost/file.bin");
    fileEntry = std::make_shared<FileEntry>("file.bin", 0, 0);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "file.bin"));
  }
};

} // namespace

class FtpCommandIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FtpCommandIntegrationTest);
  CPPUNIT_TEST(testGreetingAndUser);
  CPPUNIT_TEST(testAuthFailure);
  CPPUNIT_TEST_SUITE_END();

  FtpIntegrationContext ctx_;

public:
  void setUp()
  {
    ctx_.setUp();
    ctx_.engine->setNoWait(true);
  }
  void tearDown() {}

  void testGreetingAndUser()
  {
    // Write the FTP greeting on the server side before creating the command,
    // so that when the engine polls it will see read-readiness.
    ctx_.serverSocket->writeData("220 FTP ready\r\n");

    auto cmd = std::make_unique<FtpNegotiationCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.engine.get(), ctx_.clientSocket,
        FtpNegotiationCommand::SEQ_RECV_GREETING);

    ctx_.engine->addCommand(std::move(cmd));

    // One engine tick: the command reads the greeting, transitions to
    // SEQ_SEND_USER, sends "USER ...", then transitions to SEQ_RECV_USER
    // and re-enqueues itself waiting for read.
    ctx_.engine->run(true);

    // Read what the command sent back to the server.
    // waitRead ensures the data has arrived on server side.
    waitRead(ctx_.serverSocket);
    char buf[4096] = {};
    size_t len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);
    std::string sent(buf, len);
    CPPUNIT_ASSERT(sent.find("USER") != std::string::npos);

    // Drain the engine to clean up re-enqueued commands.
    ctx_.engine->addCommand(std::make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }

  void testAuthFailure()
  {
    // Step 1: greeting
    ctx_.serverSocket->writeData("220 FTP ready\r\n");

    auto cmd = std::make_unique<FtpNegotiationCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.engine.get(), ctx_.clientSocket,
        FtpNegotiationCommand::SEQ_RECV_GREETING);

    ctx_.engine->addCommand(std::move(cmd));
    ctx_.engine->run(true);

    // Read USER command from client
    waitRead(ctx_.serverSocket);
    {
      char buf[4096] = {};
      size_t len = sizeof(buf);
      ctx_.serverSocket->readData(buf, len);
      std::string sent(buf, len);
      CPPUNIT_ASSERT(sent.find("USER") != std::string::npos);
    }

    // Step 2: respond 331 to USER, expecting PASS
    ctx_.serverSocket->writeData("331 Password required\r\n");
    waitRead(ctx_.clientSocket);
    ctx_.engine->run(true);

    // Read PASS command from client
    waitRead(ctx_.serverSocket);
    {
      char buf[4096] = {};
      size_t len = sizeof(buf);
      ctx_.serverSocket->readData(buf, len);
      std::string sent(buf, len);
      CPPUNIT_ASSERT(sent.find("PASS") != std::string::npos);
    }

    // Step 3: reject login with 530
    ctx_.serverSocket->writeData("530 Login incorrect\r\n");
    waitRead(ctx_.clientSocket);

    // The 530 response triggers an exception in recvPass(), which
    // AbstractCommand::execute() catches and handles. The command will
    // be destroyed. Drain with TestHaltCommand to clean up.
    ctx_.engine->addCommand(std::make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);

    // The auth failure should not have produced a successful download.
    CPPUNIT_ASSERT(!ctx_.rg->downloadFinished());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(FtpCommandIntegrationTest);

} // namespace aria2
