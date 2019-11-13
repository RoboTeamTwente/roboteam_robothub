#ifndef RTT_MIMIRCOMMANDER_H
#define RTT_MIMIRCOMMANDER_H
#include <QtNetwork>

#include <roboteam_proto/mimir_robotcommand.pb.h>
#include "roboteam_proto/RobotCommand.pb.h"

namespace rtt {
namespace robothub {

/*
 * This class sends commands to Mimir, our simulator.
 * For now it simply directly forwards the messages over UDP socket.
 * @author rolf
 * @since 27-10-19
 */
class MimirCommander {
    public:
        /*
         * @param sets the color of our team so we send to the correct team
         */
        void setColor(bool isYellow);
        void setIP(const std::string &mimirIP);
        void setPort(quint16 port);
        bool sendCommand(const proto::RobotCommand& robotCommand);
    private:
        void convertCommand(const proto::RobotCommand& robotCommand,proto::mimir_robotcommand &mimirCommand);
        QUdpSocket udpsocket;
        std::string sendIP = "127.0.0.1";
        quint16 sendPort = 10004;
        bool weAreYellow=false;
        std::mutex sendMutex;


};

}
}

#endif //RTT_MIMIRCOMMANDER_H
