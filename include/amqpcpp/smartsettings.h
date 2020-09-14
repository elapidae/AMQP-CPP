#ifndef AMQP_SMARTSETTINGS_H
#define AMQP_SMARTSETTINGS_H

#include <string>

namespace AMQP {

class SmartSettings
{
public:

    std::string address     = "rabbitmq";
    uint16_t    port        = 5672;
    std::string user        = "guest";
    std::string password    = "guest";
    std::string vhost       = "/";
    std::string queue       = "AgentChannel";
    std::string binding_key = "AgentQueue";
};

} // namespace AMQP

#endif // AMQP_SMARTSETTINGS_H
