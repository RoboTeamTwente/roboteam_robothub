//
// Created by rolf on 27-10-19.
//

#include "MimirCommander.h"
#include "iostream"
namespace rtt{
namespace robothub{
void MimirCommander::setColor(bool isYellow) {
    weAreYellow=isYellow;
}
void MimirCommander::setIP(const std::string &mimirIP) {
    sendIP=mimirIP;
}

void MimirCommander::setPort(quint16 port) {
    sendPort=port;
}
bool MimirCommander::sendCommand(const proto::RobotCommand &robotCommand) {
    proto::mimir_robotcommand packet;
    convertCommand(robotCommand,packet);
    QByteArray dgram;
    dgram.resize(packet.ByteSizeLong());
    packet.SerializeToArray(dgram.data(), dgram.size());

    int sentBytes=udpsocket.writeDatagram(
            dgram,
            QHostAddress(QString::fromStdString(sendIP)),
            sendPort
    );
    return sentBytes==dgram.size();
}

void MimirCommander::convertCommand(const proto::RobotCommand &robotCommand, proto::mimir_robotcommand &mimirCommand) {
    mimirCommand.set_id(robotCommand.id());
    mimirCommand.set_teamisyellow(weAreYellow);
    //TODO: fix kicker commands
    mimirCommand.mutable_kicker()->set_chip(true);
    mimirCommand.mutable_kicker()->set_genevaangle(robotCommand.geneva_state());
    mimirCommand.mutable_kicker()->set_kickchippower(robotCommand.chip_kick_vel());

//    mimirCommand.mutable_robotvel()->set_veltangent(robotCommand.vel().x());
//    mimirCommand.mutable_robotvel()->set_velnormal(robotCommand.vel().y());
//    mimirCommand.mutable_robotvel()->set_velangle(0);
    mimirCommand.mutable_globalvel()->set_velx(robotCommand.vel().x());
    mimirCommand.mutable_globalvel()->set_vely(robotCommand.vel().y());
    mimirCommand.mutable_globalvel()->set_anglevel(0.05);
    //TODO: fix angle/angular velocity stuff
}
}
}