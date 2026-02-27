#ifndef D_MOCK_EXTENSION_MESSAGE_H
#define D_MOCK_EXTENSION_MESSAGE_H

#include "ExtensionMessage.h"

namespace aria2 {

struct MockExtensionMessageEventCheck {
  MockExtensionMessageEventCheck() : doReceivedActionCalled{false} {}
  bool doReceivedActionCalled;
};

class MockExtensionMessage : public ExtensionMessage {
public:
  std::string extensionName_;
  uint8_t extensionMessageID_;
  std::string data_;
  MockExtensionMessageEventCheck* evcheck_;

  MockExtensionMessage(const std::string& extensionName,
                       uint8_t extensionMessageID, const unsigned char* data,
                       size_t length, MockExtensionMessageEventCheck* evcheck)
      : extensionName_{extensionName},
        extensionMessageID_{extensionMessageID},
        data_{&data[0], &data[length]},
        evcheck_{evcheck}
  {
  }

  MockExtensionMessage(const std::string& extensionName,
                       uint8_t extensionMessageID, const std::string& data,
                       MockExtensionMessageEventCheck* evcheck)
      : extensionName_{extensionName},
        extensionMessageID_{extensionMessageID},
        data_{data},
        evcheck_{evcheck}
  {
  }

  std::string getPayload() override { return data_; }

  uint8_t getExtensionMessageID() const override { return extensionMessageID_; }

  const char* getExtensionName() const override
  {
    return extensionName_.c_str();
  }

  std::string toString() const override { return extensionName_; }

  void doReceivedAction() override
  {
    if (evcheck_) {
      evcheck_->doReceivedActionCalled = true;
    }
  }
};

} // namespace aria2

#endif // D_MOCK_EXTENSION_MESSAGE_H
