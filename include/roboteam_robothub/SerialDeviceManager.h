//
// Created by mrlukasbos on 7-3-19.
//

#ifndef ROBOTEAM_ROBOTHUB_SERIALDEVICEMANAGER_H
#define ROBOTEAM_ROBOTHUB_SERIALDEVICEMANAGER_H

#include <fstream>
#include "Packing.h"

namespace rtt {
namespace robothub {

class SerialDeviceManager {
   public:
    explicit SerialDeviceManager() = default;
    explicit SerialDeviceManager(const std::string &deviceName);
    bool ensureDeviceOpen();
    bool writeToDevice(const RobotCommandPayload& command);
    void openDevice();
    std::optional<RobotFeedbackPayload> readDevice();
    std::optional<RobotFeedbackPayload> mostRecentFeedback = std::nullopt;
    [[nodiscard]] std::optional<RobotFeedbackPayload> getMostRecentFeedback() const;
    void removeMostRecentFeedback();

   private:
    int fileID = 0;
    std::string deviceName;
    bool iswriting = false;
};

}  // namespace robothub
}  // namespace rtt

#endif  // ROBOTEAM_ROBOTHUB_SERIALDEVICEMANAGER_H
