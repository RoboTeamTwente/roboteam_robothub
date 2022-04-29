#include <REM_RobotCommand.h>
#include <RobotHub.h>
#include <roboteam_utils/Print.h>
#include <roboteam_utils/Time.h>
#include <roboteam_utils/Format.hpp>

#include <cmath>

#include <sstream>
#include <roboteam_utils/Vector2.h>

namespace rtt::robothub {

constexpr int DEFAULT_GRSIM_FEEDBACK_PORT_BLUE_CONTROL = 30011;
constexpr int DEFAULT_GRSIM_FEEDBACK_PORT_YELLOW_CONTROL = 30012;
constexpr int DEFAULT_GRSIM_FEEDBACK_PORT_CONFIGURATION = 30013;

// These two values are properties of our physical robots. We use these in commands for simulators
constexpr float SIM_CHIPPER_ANGLE_DEGREES = 45.0f;     // The angle at which the chipper shoots
constexpr float SIM_MAX_DRIBBLER_SPEED_RPM = 1021.0f;  // The theoretical maximum speed of the dribblers

RobotHub::RobotHub() {
    simulation::SimulatorNetworkConfiguration config = {.blueFeedbackPort = DEFAULT_GRSIM_FEEDBACK_PORT_BLUE_CONTROL,
                                                        .yellowFeedbackPort = DEFAULT_GRSIM_FEEDBACK_PORT_YELLOW_CONTROL,
                                                        .configurationFeedbackPort = DEFAULT_GRSIM_FEEDBACK_PORT_CONFIGURATION};

    if (!this->initializeNetworkers()) {
        throw FailedToInitializeNetworkersException();
    }

    this->mode = utils::RobotHubMode::NEITHER;

    this->simulatorManager = std::make_unique<simulation::SimulatorManager>(config);
    this->simulatorManager->setRobotControlFeedbackCallback([&](const simulation::RobotControlFeedback &feedback) { this->handleRobotFeedbackFromSimulator(feedback); });

    this->basestationManager = std::make_unique<basestation::BasestationManager>();
    this->basestationManager->setFeedbackCallback([&](const REM_RobotFeedback &feedback, rtt::Team color) { this->handleRobotFeedbackFromBasestation(feedback, color); });
    this->basestationManager->setRobotStateInfoCallback([&](const REM_RobotStateInfo& robotStateInfo, rtt::Team color) { this->handleRobotStateInfo(robotStateInfo, color); });

    this->robotStateLogger = std::make_unique<FileLogger>(Time::getDate('-') + "_" + Time::getTime('-') + "_ROBOTSTATES.txt");
    this->robotCommandLogger = std::make_unique<FileLogger>(Time::getDate('-') + "_" + Time::getTime('-') + "_ROBOTCOMMANDS.txt");
    this->robotFeedbackLogger = std::make_unique<FileLogger>(Time::getDate('-') + "_" + Time::getTime('-') + "_ROBOTFEEDBACK.txt");
}

const RobotHubStatistics &RobotHub::getStatistics() {
    this->statistics.basestationManagerStatus = this->basestationManager->getStatus();
    return this->statistics;
}

void RobotHub::resetStatistics() { this->statistics.resetValues(); }

bool RobotHub::initializeNetworkers() {
    bool successfullyInitialized;

    try {
        this->robotCommandsBlueSubscriber =
            std::make_unique<rtt::net::RobotCommandsBlueSubscriber>([&](const rtt::RobotCommands &commands) { this->onRobotCommands(commands, rtt::Team::BLUE); });

        this->robotCommandsYellowSubscriber =
            std::make_unique<rtt::net::RobotCommandsYellowSubscriber>([&](const rtt::RobotCommands &commands) { this->onRobotCommands(commands, rtt::Team::YELLOW); });

        this->settingsSubscriber = std::make_unique<rtt::net::SettingsSubscriber>([&](const proto::Setting &settings) { this->onSettings(settings); });

        this->simulationConfigurationSubscriber =
            std::make_unique<rtt::net::SimulationConfigurationSubscriber>([&](const proto::SimulationConfiguration &config) { this->onSimulationConfiguration(config); });

        this->robotFeedbackPublisher = std::make_unique<rtt::net::RobotFeedbackPublisher>();

        successfullyInitialized = true;
    } catch (const std::exception &e) {  // TODO: Figure out the exception
        successfullyInitialized = false;
    }

    return successfullyInitialized;
}

void RobotHub::sendCommandsToSimulator(const rtt::RobotCommands &commands, rtt::Team color) {
    if (this->simulatorManager == nullptr) return;

    simulation::RobotControlCommand simCommand;
    for (const auto &robotCommand : commands) {
        int id = robotCommand.id;
        float kickSpeed = static_cast<float>(robotCommand.kickSpeed);
        float kickAngle = robotCommand.kickType == rtt::KickType::CHIP ? SIM_CHIPPER_ANGLE_DEGREES : 0.0f;
        float dribblerSpeed = static_cast<float>(robotCommand.dribblerSpeed) * SIM_MAX_DRIBBLER_SPEED_RPM;  // dribblerSpeed is range of 0 to 1
        float xVelocity = static_cast<float>(robotCommand.velocity.x);
        float yVelocity = static_cast<float>(robotCommand.velocity.y);
        float angularVelocity = static_cast<float>(robotCommand.targetAngularVelocity);

        if (!robotCommand.useAngularVelocity) {
            RTT_WARNING("Robot command used absolute angle, but simulator requires angular velocity")
        }

        simCommand.addRobotControlWithGlobalSpeeds(id, kickSpeed, kickAngle, dribblerSpeed, xVelocity, yVelocity, angularVelocity);

        // Update received commands stats
        this->statistics.incrementCommandsReceivedCounter(id, color);
    }

    int bytesSent = this->simulatorManager->sendRobotControlCommand(simCommand, color);

    // Update bytes sent/packets dropped statistics
    if (bytesSent > 0) {
        if (color == rtt::Team::YELLOW) {
            this->statistics.yellowTeamBytesSent += bytesSent;
        } else {
            this->statistics.blueTeamBytesSent += bytesSent;
        }
    } else {
        if (color == rtt::Team::YELLOW) {
            this->statistics.yellowTeamPacketsDropped++;
        } else {
            this->statistics.blueTeamPacketsDropped++;
        }
    }
}

void RobotHub::sendCommandsToBasestation(const rtt::RobotCommands &commands, rtt::Team color) {
    for (const auto &robotCommand : commands) {
        // Convert the RobotCommand to a command for the basestation

        REM_RobotCommand command;
        command.header = PACKET_TYPE_REM_ROBOT_COMMAND;
        command.remVersion = LOCAL_REM_VERSION;
        command.id = robotCommand.id;

        command.doKick = robotCommand.kickSpeed > 0.0 && robotCommand.kickType == KickType::KICK;
        command.doChip = robotCommand.kickSpeed > 0.0 && robotCommand.kickType == KickType::CHIP;
        command.doForce = !robotCommand.waitForBall;
        command.kickChipPower = static_cast<float>(robotCommand.kickSpeed);
        command.dribbler = static_cast<float>(robotCommand.dribblerSpeed);

        command.rho = static_cast<float>(robotCommand.velocity.length());
        command.theta = static_cast<float>(robotCommand.velocity.angle());

        command.angularControl = !robotCommand.useAngularVelocity;
        command.angle = robotCommand.useAngularVelocity ? static_cast<float>(robotCommand.targetAngularVelocity) : static_cast<float>(robotCommand.targetAngle.getValue());
        if (robotCommand.useAngularVelocity) {
            RTT_WARNING("Robot command used angular velocity, but robots do not support that yet")
        }

        command.useCameraAngle = robotCommand.cameraAngleOfRobotIsSet;
        command.cameraAngle = command.useCameraAngle ? static_cast<float>(robotCommand.cameraAngleOfRobot) : 0.0f;

        command.feedback = robotCommand.ignorePacket;

        int bytesSent = this->basestationManager->sendRobotCommand(command, color);

        // Update statistics
        this->statistics.incrementCommandsReceivedCounter(robotCommand.id, color);

        if (bytesSent > 0) {
            if (color == rtt::Team::YELLOW) {
                this->statistics.yellowTeamBytesSent += bytesSent;
            } else {
                this->statistics.blueTeamBytesSent += bytesSent;
            }
        } else {
            if (color == rtt::Team::YELLOW) {
                this->statistics.yellowTeamPacketsDropped++;
            } else {
                this->statistics.blueTeamPacketsDropped++;
            }
        }
    }
}

void RobotHub::onRobotCommands(const rtt::RobotCommands &commands, rtt::Team color) {
    std::scoped_lock<std::mutex> lock(this->onRobotCommandsMutex);

    switch (this->mode) {
        case utils::RobotHubMode::SIMULATOR:
            this->sendCommandsToSimulator(commands, color);
            break;
        case utils::RobotHubMode::BASESTATION:
            this->sendCommandsToBasestation(commands, color);
            break;
        case utils::RobotHubMode::NEITHER:
            // Do not handle commands
            break;
        default:
            RTT_WARNING("Unknown RobotHub mode")
            break;
    }

    this->logRobotCommands(commands, color);
}

void RobotHub::onSettings(const proto::Setting &settings) {
    this->settings = settings;

    utils::RobotHubMode newMode = settings.serialmode() ? utils::RobotHubMode::BASESTATION : utils::RobotHubMode::SIMULATOR;

    this->mode = newMode;
    this->statistics.robotHubMode = newMode;
}

void RobotHub::onSimulationConfiguration(const proto::SimulationConfiguration &configuration) {
    simulation::ConfigurationCommand configCommand;

    if (configuration.has_ball_location()) {
        const auto &ballLocation = configuration.ball_location();
        configCommand.setBallLocation(ballLocation.x(), ballLocation.y(), ballLocation.z(), ballLocation.x_velocity(), ballLocation.y_velocity(), ballLocation.z_velocity(),
                                      ballLocation.velocity_in_rolling(), ballLocation.teleport_safely(), ballLocation.by_force());
    }

    for (const auto &robotLocation : configuration.robot_locations()) {
        configCommand.addRobotLocation(robotLocation.id(), robotLocation.is_team_yellow() ? rtt::Team::YELLOW : rtt::Team::BLUE, robotLocation.x(), robotLocation.y(),
                                       robotLocation.x_velocity(), robotLocation.y_velocity(), robotLocation.angular_velocity(), robotLocation.orientation(),
                                       robotLocation.present_on_field(), robotLocation.by_force());
    }

    for (const auto &robotProperties : configuration.robot_properties()) {
        simulation::RobotProperties propertyValues = {.radius = robotProperties.radius(),
                                                      .height = robotProperties.height(),
                                                      .mass = robotProperties.mass(),
                                                      .maxKickSpeed = robotProperties.max_kick_speed(),
                                                      .maxChipSpeed = robotProperties.max_chip_speed(),
                                                      .centerToDribblerDistance = robotProperties.center_to_dribbler_distance(),
                                                      // Movement limits
                                                      .maxAcceleration = robotProperties.max_acceleration(),
                                                      .maxAngularAcceleration = robotProperties.max_angular_acceleration(),
                                                      .maxDeceleration = robotProperties.max_deceleration(),
                                                      .maxAngularDeceleration = robotProperties.max_angular_deceleration(),
                                                      .maxVelocity = robotProperties.max_velocity(),
                                                      .maxAngularVelocity = robotProperties.max_angular_velocity(),
                                                      // Wheel angles
                                                      .frontRightWheelAngle = robotProperties.front_right_wheel_angle(),
                                                      .backRightWheelAngle = robotProperties.back_right_wheel_angle(),
                                                      .backLeftWheelAngle = robotProperties.back_left_wheel_angle(),
                                                      .frontLeftWheelAngle = robotProperties.front_left_wheel_angle()};

        configCommand.addRobotSpecs(robotProperties.id(), robotProperties.is_team_yellow() ? rtt::Team::YELLOW : rtt::Team::BLUE, propertyValues);
    }

    // TODO: Put these bytes sent into nice statistics output (low priority)
    this->simulatorManager->sendConfigurationCommand(configCommand);
}

void RobotHub::handleRobotFeedbackFromSimulator(const simulation::RobotControlFeedback &feedback) {
    rtt::RobotsFeedback robotsFeedback;
    robotsFeedback.source = rtt::RobotFeedbackSource::SIMULATOR;
    robotsFeedback.team = feedback.color;

    for (auto const&[robotId, hasBall] : feedback.robotIdHasBall) {
        rtt::RobotFeedback robotFeedback = {
            .id = robotId,
            .hasBall = hasBall,
            .ballPosition = 0,
            .ballSensorIsWorking = true,
            .velocity = { 0, 0 },
            .angle = 0,
            .xSensIsCalibrated = true,
            .capacitorIsCharged = true,
            .wheelLocked = 0,
            .wheelBraking = 0,
            .batteryLevel = 23.0f,
            .signalStrength = 0
        };
        robotsFeedback.feedback.push_back(robotFeedback);

        // Increment the feedback counter of this robot
        this->statistics.incrementFeedbackReceivedCounter(robotId, feedback.color);
    }

    this->sendRobotFeedback(robotsFeedback);
}

void RobotHub::handleRobotFeedbackFromBasestation(const REM_RobotFeedback &feedback, rtt::Team basestationColor) {
    rtt::RobotsFeedback robotsFeedback;
    robotsFeedback.source = rtt::RobotFeedbackSource::BASESTATION;
    robotsFeedback.team = basestationColor;

    rtt::RobotFeedback robotFeedback = {
        .id = static_cast<int>(feedback.id),
        .hasBall = feedback.hasBall,
        .ballPosition = feedback.ballPos,
        .ballSensorIsWorking = feedback.ballSensorWorking,
        .velocity = Vector2(Angle(feedback.theta), feedback.rho),
        .angle = Angle(feedback.angle),
        .xSensIsCalibrated = feedback.XsensCalibrated,
        .capacitorIsCharged = feedback.capacitorCharged,
        .wheelLocked = static_cast<int>(feedback.wheelLocked),
        .wheelBraking = static_cast<int>(feedback.wheelBraking),
        .batteryLevel = static_cast<float>(feedback.batteryLevel),
        .signalStrength = static_cast<int>(feedback.rssi)
    };
    robotsFeedback.feedback.push_back(robotFeedback);

    this->sendRobotFeedback(robotsFeedback);

    // Increment the feedback counter of this robot
    this->statistics.incrementFeedbackReceivedCounter(feedback.id, basestationColor);
}

bool RobotHub::sendRobotFeedback(const rtt::RobotsFeedback &feedback) {
    this->statistics.feedbackBytesSent += static_cast<int>(sizeof(feedback));

    this->logRobotFeedback(feedback);

    return this->robotFeedbackPublisher->publish(feedback);
}

void RobotHub::handleRobotStateInfo(const REM_RobotStateInfo& info, rtt::Team team) {
    this->logRobotStateInfo(info, team);
}

void RobotHub::logRobotStateInfo(const REM_RobotStateInfo &info, rtt::Team team) {
    std::stringstream ss;
    ss << "[" << Time::getTimeWithMilliseconds(':') << "] "
       << "Team: " << teamToString(team)
       << "Id: " << formatString("%2i", info.id) << ", "
       << "MsgId: " << formatString("%5i", info.messageId) << ", "
       << "xSensAcc1: " << formatString("%7f", info.xsensAcc1) << ", "
       << "xSensAcc2: " << formatString("%7f", info.xsensAcc2) << ", "
       << "xSensYaw: " << formatString("%7f", info.xsensYaw) << ", "
       << "rateOfTurn: " << formatString("%7f", info.rateOfTurn) << ", "
       << "wheelSp1: " << formatString("%&7f", info.wheelSpeed1) << ", "
       << "wheelSp2: " << formatString("%7f", info.wheelSpeed2) << ", "
       << "wheelSp3: " << formatString("%7f", info.wheelSpeed3) << ", "
       << "wheelSp4: " << formatString("%7f", info.wheelSpeed4) << std::endl;
    this->robotStateLogger->writeNewLine(ss.str());
}

void RobotHub::logRobotCommands(const rtt::RobotCommands &commands, rtt::Team team) {
    std::string teamStr = teamToString(team);
    std::string timeStr = Time::getTimeWithMilliseconds(':');

    std::stringstream ss;
    for (const auto& command : commands) {
        ss << "[" << timeStr << ", " << teamStr << "] " << command << std::endl;
    }
    this->robotCommandLogger->writeNewLine(ss.str());
}

void RobotHub::logRobotFeedback(const rtt::RobotsFeedback &feedback) {
    std::string teamStr = teamToString(feedback.team);
    std::string sourceStr = robotFeedbackSourceToString(feedback.source);
    std::string timeStr = Time::getTimeWithMilliseconds(':');

    std::stringstream ss;
    for (const auto &robot : feedback.feedback) {
        ss << "[" << timeStr << ", " << teamStr << ", " << sourceStr << "] " << robot << std::endl;
    }

    this->robotFeedbackLogger->writeNewLine(ss.str());
}

const char *FailedToInitializeNetworkersException::what() const throw() { return "Failed to initialize networker(s). Is another RobotHub running?"; }

}  // namespace rtt::robothub

int main(int argc, char *argv[]) {
    rtt::robothub::RobotHub app;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        app.getStatistics().print();
        app.resetStatistics();
    }
}