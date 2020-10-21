#include "amqpcpp.h"


#include <functional>
#include <string>


using namespace AMQP;


//=======================================================================================
namespace AMQP
{
    //===================================================================================
    EnvelopeUPtr make_envelop( const char *body, uint64_t size )
    {
        return EnvelopeUPtr( new Envelope(body, size) );
    }
    //===================================================================================
    EnvelopeUPtr make_envelop( const std::string& body )
    {
        return make_envelop( body.c_str(), body.size() );
    }
    //===================================================================================
} // namespace AMQP
//=======================================================================================

//=======================================================================================
SmartRPCServer::SmartRPCServer( SmartSettings settings, Callback cb )
    : _settings ( std::move(settings) )
    , _callback ( std::move(cb) )
    , _handler  ( SimplePoller::thread_poller() )
{
    //  No connect here.
}
//=======================================================================================
void SmartRPCServer::poll()
{
    connect();

    _something_received = false;

    while ( !_something_received )
        SimplePoller::thread_poller()->poll();
}
//=======================================================================================
void SmartRPCServer::connect()
{
    if ( _handler.is_connected() )
        return;

    _handler.connect( _settings.address, _settings.port );

    auto amqp_login = Login( _settings.user, _settings.password );

    _connection.reset( new Connection(&_handler,
                                      amqp_login,
                                      _settings.vhost) );
    _channel.reset( new Channel(_connection.get()) );
    _channel->setQos(1);

    _channel->declareQueue( _settings.queue );

    auto on_received = [this](const AMQP::Message &message,
                              uint64_t deliveryTag,
                              bool redelivered)
    {
        this->_on_amqp_received( message, deliveryTag, redelivered );
    };

    _channel->consume("").onReceived( on_received );
}
//=======================================================================================
//  TODO
//  Разобраться с контрактами на нашей петле.
void SmartRPCServer::_on_amqp_received( const Message &message,
                                        uint64_t deliveryTag,
                                        bool redelivered )
{
    _something_received = true;
    (void) redelivered;

    auto env = _callback( message );

    env->setCorrelationID( message.correlationID() );

    _channel->publish( "", message.replyTo(), *env );
    _channel->ack( deliveryTag );
}
//=======================================================================================
