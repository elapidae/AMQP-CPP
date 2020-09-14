#ifndef AMQP_SMARTRPCCLIENT_H
#define AMQP_SMARTRPCCLIENT_H

#include "smartsettings.h"
#include "amqpcpp.h"

namespace AMQP {

class SmartRPCClient
{
public:
    SmartRPCClient( SmartSettings sett );

    //  Весь поллинг под капотом. Блокируется до получения ответа.
    const Message& execute( Envelope * env );

private:
    SmartSettings _settings;

    SimpleConnectionHandler     _handler;
    std::unique_ptr<Connection> _connection;
    std::unique_ptr<Channel>    _channel;

    std::string _correlation; // ==some uuid, now is fake, TODO.
    std::string _queue_name;

    bool    _received;
    Message _msg;

    void _connect();

    void _on_queue( const std::string &name,
                    int msgcount,
                    int consumercount );

    void _on_received( const AMQP::Message &message,
                       uint64_t deliveryTag,
                       bool redelivered );
};

} // namespace AMQP

#endif // AMQP_SMARTRPCCLIENT_H
