//
// Created by mrlukasbos on 8-3-19.
//

#ifndef ROBOTEAM_ROBOTHUB_APPLICATION_H
#define ROBOTEAM_ROBOTHUB_APPLICATION_H

#include <string>
#include <roboteam_utils/constants.h>
#include "utilities.h"
#include "roboteam_proto/Setting.pb.h"
#include "constants.h"
#include <roboteam_proto/Subscriber.h>
#include <roboteam_proto/Publisher.h>

namespace rtt {
namespace robothub {

class GRSimCommander;
class MimirCommander;
class SerialDeviceManager;
class RobotHub {
public:
  RobotHub();
    void start();

    void setRobothubPublisher(const string &robothubPublisher);
    void setAiPublisher(const string &aiPublisher);
    void subscribeToTopics();

private:
    utils::Mode mode = utils::Mode::GRSIM;
    bool isLeft = true;
    roboteam_proto::Subscriber * robotCommandSubscriber;
    roboteam_proto::Subscriber * worldStateSubscriber;
    roboteam_proto::Subscriber * settingsSubscriber;
    roboteam_proto::Publisher * publisher;

    std::string ai_publisher;
    std::string robothub_publisher;
    
    // Callback functions
    roboteam_proto::World LastWorld;
    void processWorldState(roboteam_proto::World & world);
    void processRobotCommand(roboteam_proto::RobotCommand & cmd);
    void processSettings(roboteam_proto::Setting & setting);

    // Serial and grsim managers
    std::shared_ptr<SerialDeviceManager> device;
    std::shared_ptr<GRSimCommander> grsimCommander;
    std::shared_ptr<MimirCommander> mimirCommander;

    void sendSerialCommand(LowLevelRobotCommand llrc);
    void sendGrSimCommand(const roboteam_proto::RobotCommand& robotCommand);
    void sendMimirCommand(const roboteam_proto::RobotCommand& robotCommand);
    void publishRobotFeedback(LowLevelRobotFeedback llrf);
    int robotTicks[MAX_AMOUNT_OF_ROBOTS] = {};
    void printStatistics();
    std::mutex worldLock;
};

} // robothub
} // rtt

#endif //ROBOTEAM_ROBOTHUB_APPLICATION_H
