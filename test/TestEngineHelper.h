#ifndef D_TEST_ENGINE_HELPER_H
#define D_TEST_ENGINE_HELPER_H

#include "common.h"

#include <memory>
#include <string>
#include <vector>

#include "CheckIntegrityEntry.h"
#include "CheckIntegrityMan.h"
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

  auto e = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
  e->setOption(option.get());
  auto rgman = make_unique<RequestGroupMan>(
      std::vector<std::shared_ptr<RequestGroup>>{}, 1, option.get());
  rgman->setKeepRunning(keepRunning);
  e->setRequestGroupMan(std::move(rgman));
  e->setCheckIntegrityMan(make_unique<CheckIntegrityMan>());
  e->setFileAllocationMan(make_unique<FileAllocationMan>());
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

} // namespace aria2

#endif // D_TEST_ENGINE_HELPER_H
