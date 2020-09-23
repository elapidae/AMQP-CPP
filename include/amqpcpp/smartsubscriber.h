#ifndef AMQP_SMARTSUBSCRIBER_H
#define AMQP_SMARTSUBSCRIBER_H

#include "amqpcpp.h"

namespace AMQP
{

class SmartSubscriber
{
public:
    using Subscribe = std::function<void(const Message& msg)>;
    SmartSubscriber( SmartSettings sett, Subscribe sub );


private:
    SmartSettings   _settings;
    Subscribe       _subscribe;

    SimpleConnectionHandler     _handler;
    std::unique_ptr<Connection> _connection;
    std::unique_ptr<Channel>    _channel;
    void _connect();
};

} // namespace AMQP

#endif // AMQP_SMARTSUBSCRIBER_H
