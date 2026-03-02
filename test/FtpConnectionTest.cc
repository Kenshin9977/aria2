#include "FtpConnection.h"

#include <iostream>
#include <cstring>

#include <cppunit/extensions/HelperMacros.h>

#include "Exception.h"
#include "util.h"
#include "SocketCore.h"
#include "Request.h"
#include "Option.h"
#include "prefs.h"
#include "DlRetryEx.h"
#include "DlAbortEx.h"
#include "AuthConfigFactory.h"
#include "AuthConfig.h"
#include "Segment.h"
#include "TestEngineHelper.h"

namespace aria2 {

class FtpConnectionTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FtpConnectionTest);
  CPPUNIT_TEST(testReceiveResponse);
  CPPUNIT_TEST(testReceiveResponse_overflow);
  CPPUNIT_TEST(testSendMdtm);
  CPPUNIT_TEST(testReceiveMdtmResponse);
  CPPUNIT_TEST(testSendPwd);
  CPPUNIT_TEST(testReceivePwdResponse);
  CPPUNIT_TEST(testReceivePwdResponse_unquotedResponse);
  CPPUNIT_TEST(testReceivePwdResponse_badStatus);
  CPPUNIT_TEST(testSendCwd);
  CPPUNIT_TEST(testSendSize);
  CPPUNIT_TEST(testReceiveSizeResponse);
  CPPUNIT_TEST(testSendRetr);
  CPPUNIT_TEST(testReceiveEpsvResponse);
  CPPUNIT_TEST(testSendUser);
  CPPUNIT_TEST(testSendPass);
  CPPUNIT_TEST(testSendType);
  CPPUNIT_TEST(testSendEpsv);
  CPPUNIT_TEST(testSendPasv);
  CPPUNIT_TEST(testReceivePasvResponse);
  CPPUNIT_TEST(testSendRestNullSegment);
  CPPUNIT_TEST(testBaseWorkingDir);
  CPPUNIT_TEST(testGetUser);
  CPPUNIT_TEST(testReceiveSizeResponse_negative);
  CPPUNIT_TEST(testReceiveEpsvResponse_customDelimiter);
  CPPUNIT_TEST(testReceivePasvResponse_octetValidation);
  CPPUNIT_TEST_SUITE_END();

private:
  std::shared_ptr<SocketCore> serverSocket_;
  uint16_t listenPort_;
  std::shared_ptr<SocketCore> clientSocket_;
  std::shared_ptr<FtpConnection> ftp_;
  std::shared_ptr<Option> option_;
  std::shared_ptr<AuthConfigFactory> authConfigFactory_;
  std::shared_ptr<Request> req_;

public:
  void setUp()
  {
    option_.reset(new Option());
    authConfigFactory_.reset(new AuthConfigFactory());

    //_ftpServerSocket.reset(new SocketCore());
    std::shared_ptr<SocketCore> listenSocket(new SocketCore());
    listenSocket->bind(0);
    listenSocket->beginListen();
    listenSocket->setBlockingMode();
    listenPort_ = listenSocket->getAddrInfo().port;

    req_.reset(new Request());
    req_->setUri("ftp://localhost/dir%20sp/hello%20world.img");

    clientSocket_.reset(new SocketCore());
    clientSocket_->establishConnection("localhost", listenPort_);

    waitWrite(clientSocket_);

    serverSocket_ = listenSocket->acceptConnection();
    serverSocket_->setBlockingMode();
    ftp_.reset(new FtpConnection(
        1, clientSocket_, req_,
        authConfigFactory_->createAuthConfig(req_, option_.get()),
        option_.get()));
  }

  void tearDown() {}

  void testSendMdtm();
  void testReceiveMdtmResponse();
  void testReceiveResponse();
  void testReceiveResponse_overflow();
  void testSendPwd();
  void testReceivePwdResponse();
  void testReceivePwdResponse_unquotedResponse();
  void testReceivePwdResponse_badStatus();
  void testSendCwd();
  void testSendSize();
  void testReceiveSizeResponse();
  void testSendRetr();
  void testReceiveEpsvResponse();
  void testSendUser();
  void testSendPass();
  void testSendType();
  void testSendEpsv();
  void testSendPasv();
  void testReceivePasvResponse();
  void testSendRestNullSegment();
  void testBaseWorkingDir();
  void testGetUser();
  void testReceiveSizeResponse_negative();
  void testReceiveEpsvResponse_customDelimiter();
  void testReceivePasvResponse_octetValidation();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FtpConnectionTest);

// waitRead/waitWrite provided by TestEngineHelper.h

void FtpConnectionTest::testReceiveResponse()
{
  serverSocket_->writeData("100");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData(" single line response");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData("\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(100, ftp_->receiveResponse());
  // 2 responses in the buffer
  serverSocket_->writeData("101 single1\r\n"
                           "102 single2\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(101, ftp_->receiveResponse());
  CPPUNIT_ASSERT_EQUAL(102, ftp_->receiveResponse());

  serverSocket_->writeData("103-multi line response\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData("103-line2\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData("103");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData(" ");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  serverSocket_->writeData("last\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(103, ftp_->receiveResponse());

  serverSocket_->writeData("104-multi\r\n"
                           "104 \r\n"
                           "105-multi\r\n"
                           "105 \r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(104, ftp_->receiveResponse());
  CPPUNIT_ASSERT_EQUAL(105, ftp_->receiveResponse());
}

void FtpConnectionTest::testSendMdtm()
{
  ftp_->sendMdtm();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  data[len] = '\0';
  CPPUNIT_ASSERT_EQUAL(std::string("MDTM hello world.img\r\n"),
                       std::string(data));
}

void FtpConnectionTest::testReceiveMdtmResponse()
{
  {
    Time t;
    serverSocket_->writeData("213 20080908124312");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveMdtmResponse(t));
    serverSocket_->writeData("\r\n");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(213, ftp_->receiveMdtmResponse(t));
    CPPUNIT_ASSERT_EQUAL((time_t)1220877792, t.getTimeFromEpoch());
  }
  {
    // see milli second part is ignored
    Time t;
    serverSocket_->writeData("213 20080908124312.014\r\n");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(213, ftp_->receiveMdtmResponse(t));
    CPPUNIT_ASSERT_EQUAL((time_t)1220877792, t.getTimeFromEpoch());
  }
  {
    // hhmmss part is missing
    Time t;
    serverSocket_->writeData("213 20080908\r\n");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(213, ftp_->receiveMdtmResponse(t));
    CPPUNIT_ASSERT(t.bad());
  }
  {
    // invalid month: 19
    Time t;
    serverSocket_->writeData("213 20081908124312\r\n");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(213, ftp_->receiveMdtmResponse(t));
#ifdef HAVE_TIMEGM
    // Time will be normalized. Wed Jul 8 12:43:12 2009
    CPPUNIT_ASSERT_EQUAL((time_t)1247056992, t.getTimeFromEpoch());
#else  // !HAVE_TIMEGM
    // The replacement timegm does not normalize.
    CPPUNIT_ASSERT_EQUAL((time_t)-1, t.getTimeFromEpoch());
#endif // !HAVE_TIMEGM
  }
  {
    Time t;
    serverSocket_->writeData("550 File Not Found\r\n");
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(550, ftp_->receiveMdtmResponse(t));
  }
}

void FtpConnectionTest::testReceiveResponse_overflow()
{
  char data[1_k];
  memset(data, 0, sizeof(data));
  memcpy(data, "213 ", 4);
  for (int i = 0; i < 64; ++i) {
    serverSocket_->writeData(data, sizeof(data));
    waitRead(clientSocket_);
    CPPUNIT_ASSERT_EQUAL(0, ftp_->receiveResponse());
  }
  serverSocket_->writeData(data, sizeof(data));
  waitRead(clientSocket_);
  try {
    ftp_->receiveResponse();
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (DlRetryEx& e) {
    // success
  }
}

void FtpConnectionTest::testSendPwd()
{
  ftp_->sendPwd();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  CPPUNIT_ASSERT_EQUAL((size_t)5, len);
  data[len] = '\0';
  CPPUNIT_ASSERT_EQUAL(std::string("PWD\r\n"), std::string(data));
}

void FtpConnectionTest::testReceivePwdResponse()
{
  std::string pwd;
  serverSocket_->writeData("257 ");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(0, ftp_->receivePwdResponse(pwd));
  CPPUNIT_ASSERT(pwd.empty());
  serverSocket_->writeData("\"/dir/to\" is your directory.\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(257, ftp_->receivePwdResponse(pwd));
  CPPUNIT_ASSERT_EQUAL(std::string("/dir/to"), pwd);
}

void FtpConnectionTest::testReceivePwdResponse_unquotedResponse()
{
  std::string pwd;
  serverSocket_->writeData("257 /dir/to\r\n");
  waitRead(clientSocket_);
  try {
    ftp_->receivePwdResponse(pwd);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

void FtpConnectionTest::testReceivePwdResponse_badStatus()
{
  std::string pwd;
  serverSocket_->writeData("500 failed\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(500, ftp_->receivePwdResponse(pwd));
  CPPUNIT_ASSERT(pwd.empty());
}

void FtpConnectionTest::testSendCwd()
{
  ftp_->sendCwd("%2Fdir%20sp");
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  data[len] = '\0';
  CPPUNIT_ASSERT_EQUAL(std::string("CWD /dir sp\r\n"), std::string(data));
}

void FtpConnectionTest::testSendSize()
{
  ftp_->sendSize();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("SIZE hello world.img\r\n"),
                       std::string(&data[0], &data[len]));
}

void FtpConnectionTest::testReceiveSizeResponse()
{
  serverSocket_->writeData("213 4294967296\r\n");
  waitRead(clientSocket_);
  int64_t size;
  CPPUNIT_ASSERT_EQUAL(213, ftp_->receiveSizeResponse(size));
  CPPUNIT_ASSERT_EQUAL((int64_t)4294967296LL, size);
}

void FtpConnectionTest::testSendRetr()
{
  ftp_->sendRetr();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("RETR hello world.img\r\n"),
                       std::string(&data[0], &data[len]));
}

void FtpConnectionTest::testReceiveEpsvResponse()
{
  serverSocket_->writeData("229 Success (|||12000|)\r\n");
  waitRead(clientSocket_);
  uint16_t port = 0;
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)12000, port);

  serverSocket_->writeData("229 Success |||12000|)\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);

  serverSocket_->writeData("229 Success (|||12000|\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);

  serverSocket_->writeData("229 Success ()|||12000|\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);

  serverSocket_->writeData("229 Success )(|||12000|)\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);

  serverSocket_->writeData("229 Success )(||12000|)\r\n");
  waitRead(clientSocket_);
  CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
  CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);
}

void FtpConnectionTest::testSendUser()
{
  ftp_->sendUser();
  char data[128];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  // Default user is "anonymous"
  CPPUNIT_ASSERT_EQUAL(std::string("USER anonymous\r\n"), sent);
}

void FtpConnectionTest::testSendPass()
{
  ftp_->sendPass();
  char data[128];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  // Default password for anonymous FTP
  CPPUNIT_ASSERT(sent.find("PASS ") == 0);
  CPPUNIT_ASSERT(sent.find("\r\n") != std::string::npos);
}

void FtpConnectionTest::testSendType()
{
  // Default FTP type is binary (I)
  ftp_->sendType();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("TYPE I\r\n"), sent);
}

void FtpConnectionTest::testSendEpsv()
{
  ftp_->sendEpsv();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("EPSV\r\n"), sent);
}

void FtpConnectionTest::testSendPasv()
{
  ftp_->sendPasv();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("PASV\r\n"), sent);
}

void FtpConnectionTest::testReceivePasvResponse()
{
  // Valid PASV response
  {
    serverSocket_->writeData(
        "227 Entering Passive Mode (192,168,0,1,4,1).\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    CPPUNIT_ASSERT_EQUAL(227, ftp_->receivePasvResponse(dest));
    CPPUNIT_ASSERT_EQUAL(std::string("192.168.0.1"), dest.first);
    // port = 256*4 + 1 = 1025
    CPPUNIT_ASSERT_EQUAL((uint16_t)1025, dest.second);
  }
  // Non-227 response
  {
    serverSocket_->writeData("500 Command not understood\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    CPPUNIT_ASSERT_EQUAL(500, ftp_->receivePasvResponse(dest));
  }
}

void FtpConnectionTest::testSendRestNullSegment()
{
  // sendRest with null segment sends REST 0
  std::shared_ptr<Segment> nullSeg;
  ftp_->sendRest(nullSeg);
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  std::string sent(data, len);
  CPPUNIT_ASSERT_EQUAL(std::string("REST 0\r\n"), sent);
}

void FtpConnectionTest::testBaseWorkingDir()
{
  // Default base working dir is "/"
  CPPUNIT_ASSERT_EQUAL(std::string("/"), ftp_->getBaseWorkingDir());

  ftp_->setBaseWorkingDir("/home/ftp");
  CPPUNIT_ASSERT_EQUAL(std::string("/home/ftp"), ftp_->getBaseWorkingDir());

  ftp_->setBaseWorkingDir("/");
  CPPUNIT_ASSERT_EQUAL(std::string("/"), ftp_->getBaseWorkingDir());
}

void FtpConnectionTest::testGetUser()
{
  // Default user for anonymous FTP
  CPPUNIT_ASSERT_EQUAL(std::string("anonymous"), ftp_->getUser());
}

void FtpConnectionTest::testReceiveSizeResponse_negative()
{
  // Negative size should throw
  serverSocket_->writeData("213 -1\r\n");
  waitRead(clientSocket_);
  int64_t size = 0;
  try {
    ftp_->receiveSizeResponse(size);
    CPPUNIT_FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

void FtpConnectionTest::testReceiveEpsvResponse_customDelimiter()
{
  // RFC 2428: delimiter is the first char after '('
  // Test with '!' as delimiter instead of '|'
  {
    serverSocket_->writeData("229 Success (!!!12000!)\r\n");
    waitRead(clientSocket_);
    uint16_t port = 0;
    CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
    CPPUNIT_ASSERT_EQUAL((uint16_t)12000, port);
  }
  // Test with '#' as delimiter
  {
    serverSocket_->writeData("229 Success (###8080#)\r\n");
    waitRead(clientSocket_);
    uint16_t port = 0;
    CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
    CPPUNIT_ASSERT_EQUAL((uint16_t)8080, port);
  }
  // Test with '@' as delimiter
  {
    serverSocket_->writeData("229 Success (@@@21@)\r\n");
    waitRead(clientSocket_);
    uint16_t port = 0;
    CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
    CPPUNIT_ASSERT_EQUAL((uint16_t)21, port);
  }
  // Inner too short — should return port=0
  {
    serverSocket_->writeData("229 Success (||)\r\n");
    waitRead(clientSocket_);
    uint16_t port = 0;
    CPPUNIT_ASSERT_EQUAL(229, ftp_->receiveEpsvResponse(port));
    CPPUNIT_ASSERT_EQUAL((uint16_t)0, port);
  }
}

void FtpConnectionTest::testReceivePasvResponse_octetValidation()
{
  // Valid PASV with port 256*100 + 200 = 25800
  {
    serverSocket_->writeData(
        "227 Entering Passive Mode (10,0,0,1,100,200).\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    CPPUNIT_ASSERT_EQUAL(227, ftp_->receivePasvResponse(dest));
    CPPUNIT_ASSERT_EQUAL(std::string("10.0.0.1"), dest.first);
    CPPUNIT_ASSERT_EQUAL((uint16_t)25800, dest.second);
  }
  // Octet > 255 in host — should throw
  {
    serverSocket_->writeData(
        "227 Entering Passive Mode (256,0,0,1,4,1).\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    try {
      ftp_->receivePasvResponse(dest);
      CPPUNIT_FAIL("exception must be thrown for octet > 255.");
    }
    catch (DlRetryEx& e) {
      // success
    }
  }
  // Negative octet in host — should throw
  {
    serverSocket_->writeData(
        "227 Entering Passive Mode (-1,0,0,1,4,1).\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    try {
      ftp_->receivePasvResponse(dest);
      CPPUNIT_FAIL("exception must be thrown for negative octet.");
    }
    catch (DlRetryEx& e) {
      // success
    }
  }
  // Port byte > 255 — should throw
  {
    serverSocket_->writeData(
        "227 Entering Passive Mode (10,0,0,1,256,1).\r\n");
    waitRead(clientSocket_);
    std::pair<std::string, uint16_t> dest;
    try {
      ftp_->receivePasvResponse(dest);
      CPPUNIT_FAIL("exception must be thrown for port byte > 255.");
    }
    catch (DlRetryEx& e) {
      // success
    }
  }
}

} // namespace aria2
