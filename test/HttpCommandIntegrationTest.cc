// test/HttpCommandIntegrationTest.cc
#include "HttpRequestCommand.h"
#include "HttpResponseCommand.h"
#include "HttpConnection.h"
#include "SocketRecvBuffer.h"
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

struct HttpIntegrationContext {
  std::shared_ptr<Option> option;
  std::shared_ptr<RequestGroup> rg;
  std::shared_ptr<Request> req;
  std::shared_ptr<FileEntry> fileEntry;
  std::shared_ptr<SocketCore> clientSocket;
  std::shared_ptr<SocketCore> serverSocket;
  std::shared_ptr<HttpConnection> httpConn;
  // engine last -- destroyed first
  std::unique_ptr<DownloadEngine> engine;

  void setUp()
  {
    engine = createTestEngine(option, "aria2_HttpCmdIntTest", true);
    engine->setAuthConfigFactory(make_unique<AuthConfigFactory>());

    // Set prefs that the command/requestgroup constructors read
    option->put(PREF_CONNECT_TIMEOUT, "10");
    option->put(PREF_TIMEOUT, "10");
    option->put(PREF_SPLIT, "1");
    option->put(PREF_MAX_DOWNLOAD_LIMIT, "0");
    option->put(PREF_MAX_UPLOAD_LIMIT, "0");

    // Connected socket pair via listen/connect/accept
    SocketCore listenSock;
    listenSock.bind(0);
    listenSock.beginListen();
    listenSock.setBlockingMode();
    uint16_t port = listenSock.getAddrInfo().port;

    clientSocket = std::make_shared<SocketCore>();
    clientSocket->establishConnection("localhost", port);
    while (!clientSocket->isWritable(0))
      ;
    serverSocket = listenSock.acceptConnection();
    serverSocket->setBlockingMode();

    req = std::make_shared<Request>();
    req->setUri("http://localhost/file.bin");
    fileEntry = std::make_shared<FileEntry>("file.bin", 0, 0);

    rg = std::make_shared<RequestGroup>(GroupId::create(), option);
    rg->setDownloadContext(
        std::make_shared<DownloadContext>(1048576, 0, "file.bin"));

    auto recvBuf = std::make_shared<SocketRecvBuffer>(clientSocket);
    httpConn = std::make_shared<HttpConnection>(engine->newCUID(), clientSocket,
                                                recvBuf);
  }
};

} // namespace

class HttpCommandIntegrationTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpCommandIntegrationTest);
  CPPUNIT_TEST(testSendRequest);
  CPPUNIT_TEST(testResponseNotFound);
  CPPUNIT_TEST_SUITE_END();

  HttpIntegrationContext ctx_;

public:
  void setUp() { ctx_.setUp(); }
  void tearDown() {}

  void testSendRequest()
  {
    // Create HttpRequestCommand -- it registers the socket for write-check
    auto cmd = make_unique<HttpRequestCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.httpConn, ctx_.engine.get(), ctx_.clientSocket);

    // Pre-write the HTTP response on the server side so that when the
    // engine polls, it will see both write-readiness (to send the request)
    // and then read-readiness (the response waiting).
    ctx_.serverSocket->writeData("HTTP/1.1 200 OK\r\n"
                                 "Content-Length: 5\r\n"
                                 "\r\n"
                                 "hello");

    // Add command to engine and let the engine drive it.
    // The engine's poll() will detect the socket is writable, then
    // executeInternal() sends the HTTP request and creates an
    // HttpResponseCommand.
    ctx_.engine->addCommand(std::move(cmd));

    // Run one engine iteration -- poll detects write-readiness,
    // HttpRequestCommand sends request and transitions to HttpResponseCommand.
    ctx_.engine->run(true);

    // The command should have written the HTTP request to the socket.
    // Read it from the server side.
    char buf[4096] = {};
    size_t len = sizeof(buf);
    ctx_.serverSocket->readData(buf, len);
    std::string request(buf, len);
    CPPUNIT_ASSERT(request.find("GET /file.bin") != std::string::npos);
    CPPUNIT_ASSERT(request.find("Host: localhost") != std::string::npos);

    // Drain the engine to clean up re-enqueued commands
    // (HttpResponseCommand etc.)
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);
  }

  void testResponseNotFound()
  {
    // Pre-write a 404 response before the command sends the request.
    ctx_.serverSocket->writeData("HTTP/1.1 404 Not Found\r\n"
                                 "Content-Length: 0\r\n"
                                 "\r\n");

    auto cmd = make_unique<HttpRequestCommand>(
        ctx_.engine->newCUID(), ctx_.req, ctx_.fileEntry, ctx_.rg.get(),
        ctx_.httpConn, ctx_.engine.get(), ctx_.clientSocket);

    ctx_.engine->addCommand(std::move(cmd));

    // Run engine — HttpRequestCommand sends request, creates
    // HttpResponseCommand which reads the 404 and handles the error.
    ctx_.engine->setNoWait(true);
    ctx_.engine->addCommand(make_unique<TestHaltCommand>(ctx_.engine->newCUID(),
                                                         ctx_.engine.get()));
    ctx_.engine->run(true);

    // The 404 should not have produced a successful download.
    // The request group should not be marked as download finished.
    CPPUNIT_ASSERT(!ctx_.rg->downloadFinished());
  }
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpCommandIntegrationTest);

} // namespace aria2
