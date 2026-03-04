#ifndef D_TEST_ENGINE_HELPER_H
#define D_TEST_ENGINE_HELPER_H

#include "common.h"

#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

#include <cppunit/extensions/HelperMacros.h>

#include "CheckIntegrityEntry.h"
#include "CheckIntegrityMan.h"
#include "SocketCore.h"
#include "Command.h"
#include "DownloadEngine.h"
#include "FileAllocationEntry.h"
#include "FileAllocationMan.h"
#include "SelectEventPoll.h"
#include "RequestGroupMan.h"
#include "RequestGroup.h"
#include "Option.h"
#include "prefs.h"
#include "a2functional.h"
#include "File.h"
#include "TestUtil.h"

namespace aria2 {

// Creates a minimal DownloadEngine suitable for testing.
// Sets option with PREF_DIR and PREF_PIECE_LENGTH, creates output dir,
// and configures a RequestGroupMan with the given keepRunning flag.
inline std::unique_ptr<DownloadEngine>
createTestEngine(std::shared_ptr<Option>& option,
                 const std::string& testDirName, bool keepRunning = false)
{
  option = std::make_shared<Option>();
  option->put(PREF_DIR, A2_TEST_OUT_DIR "/" + testDirName);
  option->put(PREF_PIECE_LENGTH, "1048576");
  File(option->get(PREF_DIR)).mkdirs();

  auto e =
      std::make_unique<DownloadEngine>(std::make_unique<SelectEventPoll>());
  e->setOption(option.get());
  auto rgman = std::make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option.get());
  rgman->setKeepRunning(keepRunning);
  e->setRequestGroupMan(std::move(rgman));
  e->setCheckIntegrityMan(std::make_unique<CheckIntegrityMan>());
  e->setFileAllocationMan(std::make_unique<FileAllocationMan>());
  return e;
}

// A Command that requests force halt on the engine.
// Use this in tests to stop the engine after other commands have executed.
class TestHaltCommand : public Command {
public:
  TestHaltCommand(cuid_t cuid, DownloadEngine* engine, bool force = true)
      : Command(cuid), e_(engine), forceHalt_(force)
  {
  }

  bool execute() override
  {
    if (forceHalt_) {
      e_->requestForceHalt();
    }
    else {
      e_->requestHalt();
    }
    return true;
  }

private:
  DownloadEngine* e_;
  bool forceHalt_;
};

// Timeout-safe socket wait helpers.  Replace bare busy-wait loops
// (while(!isReadable(0));) so tests fail fast instead of hanging.
inline void waitRead(const std::shared_ptr<SocketCore>& socket,
                     int timeoutMs = 15000)
{
  for (int i = 0; i < timeoutMs; ++i) {
    if (socket->isReadable(0)) {
      return;
    }
    usleep(1000);
  }
  CPPUNIT_FAIL("waitRead timed out");
}

inline void waitWrite(const std::shared_ptr<SocketCore>& socket,
                      int timeoutMs = 15000)
{
  for (int i = 0; i < timeoutMs; ++i) {
    if (socket->isWritable(0)) {
      return;
    }
    usleep(1000);
  }
  CPPUNIT_FAIL("waitWrite timed out");
}

// Overloads for stack-allocated SocketCore (used by HttpServerTest, etc.)
inline void waitRead(SocketCore& socket, int timeoutMs = 15000)
{
  for (int i = 0; i < timeoutMs; ++i) {
    if (socket.isReadable(0)) {
      return;
    }
    usleep(1000);
  }
  CPPUNIT_FAIL("waitRead timed out");
}

inline void waitWrite(SocketCore& socket, int timeoutMs = 15000)
{
  for (int i = 0; i < timeoutMs; ++i) {
    if (socket.isWritable(0)) {
      return;
    }
    usleep(1000);
  }
  CPPUNIT_FAIL("waitWrite timed out");
}

} // namespace aria2

#endif // D_TEST_ENGINE_HELPER_H
