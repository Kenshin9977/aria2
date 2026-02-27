#ifndef D_MOCK_BT_PROGRESS_INFO_FILE_H
#define D_MOCK_BT_PROGRESS_INFO_FILE_H

#include "BtProgressInfoFile.h"

namespace aria2 {

class MockBtProgressInfoFile : public BtProgressInfoFile {
private:
  std::string filename;

public:
  MockBtProgressInfoFile() {}
  ~MockBtProgressInfoFile() override {}

  std::string getFilename() override { return filename; }

  void setFilename(const std::string& filename) { this->filename = filename; }

  bool exists() override { return false; }

  void save() override {}

  void load() override {}

  void removeFile() override {}

  void updateFilename() override {}
};

} // namespace aria2

#endif // D_MOCK_BT_PROGRESS_INFO_FILE_H
