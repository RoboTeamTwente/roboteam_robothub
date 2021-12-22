#pragma once

#include <constants.h>
#include <libusb-1.0/libusb.h>
#include <utilities.h>

#include <RobotCommandsNetworker.hpp>
#include <RobotFeedbackNetworker.hpp>
#include <SettingsNetworker.hpp>
#include <WorldNetworker.hpp>
#include <basestation/BasestationManager.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <simulation/SimulatorManager.hpp>

namespace rtt::robothub {

enum class RobotHubMode { NEITHER, SIMULATOR, BASESTATION, BOTH };

class RobotHub {
   public:
    RobotHub();

    void printStatistics();

   private:
    std::unique_ptr<simulation::SimulatorManager> simulatorManager;
    std::unique_ptr<basestation::BasestationManager> basestationManager;

    proto::Setting settings;
    utils::RobotHubMode mode;
    proto::World world;

    std::unique_ptr<rtt::net::RobotCommandsBlueSubscriber> robotCommandsBlueSubscriber;
    std::unique_ptr<rtt::net::RobotCommandsYellowSubscriber> robotCommandsYellowSubscriber;
    std::unique_ptr<rtt::net::SettingsSubscriber> settingsSubscriber;
    std::unique_ptr<rtt::net::WorldSubscriber> worldSubscriber;
    std::unique_ptr<rtt::net::RobotFeedbackPublisher> robotFeedbackPublisher;

    int commands_sent[MAX_AMOUNT_OF_ROBOTS] = {};
    int feedback_received[MAX_AMOUNT_OF_ROBOTS] = {};

    bool subscribe();

    void sendCommandsToSimulator(const proto::AICommand &commands, bool toTeamYellow);
    void sendCommandsToBasestation(const proto::AICommand &commands, bool toTeamYellow);

    void onBlueRobotCommands(const proto::AICommand &commands);
    void onYellowRobotCommands(const proto::AICommand &commands);
    void processRobotCommands(const proto::AICommand &commands, bool forTeamYellow, utils::RobotHubMode mode);

    void onSettings(const proto::Setting &setting);

    void onWorld(const proto::State &world);

    void handleRobotFeedbackFromSimulator(const simulation::RobotControlFeedback &feedback);
    void handleRobotFeedbackFromBasestation(const RobotFeedback &feedback);
    bool sendRobotFeedback(const proto::RobotData &feedback);
};

class FailedToInitializeNetworkersException : public std::exception {
    virtual const char *what() const throw();
};

}  // namespace rtt::robothub