
#include <roboteam_utils/Timer.h>
#include "roboteam_proto/Setting.pb.h"

#include "GRSim.h"
#include "RobotHub.h"
#include "SerialDeviceManager.h"
#include "Packing.h"

namespace rtt::robothub {

RobotHub::RobotHub() {
    grsimCommander = std::make_shared<GRSimCommander>();

#ifdef __APPLE__
    device = std::make_shared<SerialDeviceManager>("/dev/cu.usbmodem00000000001A1");
#else
    device = std::make_shared<SerialDeviceManager>("/dev/serial/by-id/usb-RTT_BaseStation_00000000001A-if00");
#endif
}

/// subscribe to topics
void RobotHub::subscribeToTopics() {
    robotCommandSubscriber = new proto::Subscriber<proto::AICommand>(robotCommandChannel, &RobotHub::processAIBatch, this);

    settingsSubscriber = new proto::Subscriber<proto::Setting>(settingsChannel, &RobotHub::processSettings, this);

    feedbackPublisher = new proto::Publisher<proto::RobotData>(feedbackChannel);
}

void RobotHub::start() {
    int currIteration = 0;
    roboteam_utils::Timer t;
    t.loop(
        [&]() {
            std::cout << "==========| " << currIteration++ << "   " << utils::modeToString(mode) << " |==========" << std::endl;
            printStatistics();
        },
        1);
}

/// print robot ticks in a nicely formatted way
void RobotHub::printStatistics() {
    const int amountOfColumns = 4;
    for (int i = 0; i < MAX_AMOUNT_OF_ROBOTS; i += amountOfColumns) {
        for (int j = 0; j < amountOfColumns; j++) {
            const int robotId = i + j;
            if (robotId < MAX_AMOUNT_OF_ROBOTS) {
                std::cout << robotId << ": " << robotTicks[robotId] << "\t";
                robotTicks[robotId] = 0;
            }
        }
        std::cout << std::endl;
    }
}

void RobotHub::processAIBatch(proto::AICommand &cmd) {
  proto::RobotData sentCommands;
  sentCommands.set_isyellow(isYellow);
  for(const auto& command : cmd.commands()){
    bool wasSent = processCommand(command,cmd.extrapolatedworld());
    if(wasSent){
      proto::RobotCommand * sent = sentCommands.mutable_sentcommands()->Add();
      sent->CopyFrom(command);
    }
  }
  //TODO: add times command was sent
  feedbackPublisher->send(sentCommands);

}
bool RobotHub::processCommand(const proto::RobotCommand &robotCommand,const proto::World &world) {

    robotTicks[robotCommand.id()]++;
    if (mode == utils::Mode::SERIAL) {
        RobotCommandPayload payload = createLowLevelCommand(robotCommand,world,isYellow);
        return sendSerialCommand(payload);
    } else {
        return sendGrSimCommand(robotCommand);
    }
}
/// send a serial command from a given robotcommand
bool RobotHub::sendSerialCommand(RobotCommandPayload payload) {
    // convert the LLRC to a bytestream which we can send

    if (!device->ensureDeviceOpen()) {
        device->openDevice();
    }

    device->writeToDevice(payload);
    std::optional<RobotFeedbackPayload> feedback_payload= device->getMostRecentFeedback();
    if (feedback_payload) {
        publishRobotFeedback(feedback_payload.value());
        device->removeMostRecentFeedback();
    }
    return true;
}

/// send a GRSim command from a given robotcommand
bool RobotHub::sendGrSimCommand(const proto::RobotCommand &robotCommand) {
  this->grsimCommander->queueGRSimCommand(robotCommand);
  return true;
}

void RobotHub::publishRobotFeedback(RobotFeedbackPayload llrf) {
     proto::RobotFeedback feedback = feedbackFromRaw(&llrf);
    if (feedback.id() >= 0 && feedback.id() < 16) {
        proto::RobotData data;
        proto::RobotFeedback * data_feedback = data.mutable_receivedfeedback()->Add();
        data_feedback->CopyFrom(feedback);
        data.set_isyellow(isYellow);
        feedbackPublisher->send(data);
    }
}

void RobotHub::processSettings(proto::Setting &setting) {
    grsimCommander->setGrsim_ip(setting.robothubsendip());
    grsimCommander->setGrsim_port(setting.robothubsendport());
    isLeft = setting.isleft();
    grsimCommander->setColor(setting.isyellow());
    isYellow = setting.isyellow();

    if (setting.serialmode()) {
        mode = utils::Mode::SERIAL;
    } else {
        mode = utils::Mode::GRSIM;
    }
}
void RobotHub::set_robot_command_channel(const proto::ChannelType &robot_command_channel) { robotCommandChannel = robot_command_channel; }
void RobotHub::set_feedback_channel(const proto::ChannelType &feedback_channel) { feedbackChannel = feedback_channel; }
void RobotHub::set_settings_channel(const proto::ChannelType &settings_channel) { settingsChannel = settings_channel; }

}  // namespace rtt
