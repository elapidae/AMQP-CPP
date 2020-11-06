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

    //  Поллинг с ожиданием, {0} -- ждать до ответа.
    bool execute( Envelope * env, std::chrono::microseconds wait_us );

    template<typename Period>
    bool execute( Envelope * env, Period wait_period )
    {
        using namespace std::chrono;
        return execute( env, duration_cast<microseconds>(wait_period) );
    }

    //  Если был поллинг с ожиданием, то ответ забирать здесь.
    const Message& last_message() const;

private:
    SmartSettings _settings;

    SimplePoller                _poller;
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

    void _on_received( const AMQP::Message& message );
};

} // namespace AMQP

#endif // AMQP_SMARTRPCCLIENT_H
