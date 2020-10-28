#ifndef SMARTPUBLISHER_H
#define SMARTPUBLISHER_H

#include "amqpcpp.h"

//=======================================================================================
namespace AMQP
{

class SmartPublisher
{
public:
    SmartPublisher( SmartSettings sett );

    void publish( const std::string& msg );
    void publish( const Envelope& env );

private:
    SmartSettings _settings;

    SimplePoller                _poller;
    SimpleConnectionHandler     _handler;
    std::unique_ptr<Connection> _connection;
    std::unique_ptr<Channel>    _channel;

    void _connect();

    void _on_success();
};

} // AMQP namespace
//=======================================================================================

#endif // SMARTPUBLISHER_H
