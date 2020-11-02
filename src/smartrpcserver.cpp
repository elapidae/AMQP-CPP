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
    , _poller   ()
    , _handler  ( &_poller )
{
    connect();
}
//=======================================================================================
bool SmartRPCServer::poll( unsigned microsec )
{
    connect();

    _something_received = false;

    int cnt;
    while ( !_something_received )
        cnt = _poller.poll( std::chrono::microseconds(microsec) );

    return cnt > 0;
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

    if ( !_settings.exchange.empty() )
    {
        throw std::runtime_error
                ("Non standard exchange for RPC not implemented, TODO! -> "
                 "'" + _settings.exchange + "'");
    }

    _channel->declareQueue( _settings.queue );

    auto on_received = [this]( const AMQP::Message& message,
                               uint64_t deliveryTag,
                               bool redelivered )
    {
        (void) redelivered;
        this->_on_amqp_received( message, deliveryTag );
    };

    _channel->consume("").onReceived( on_received );
}
//=======================================================================================
//  TODO
//  Разобраться с контрактами на нашей петле.
void SmartRPCServer::_on_amqp_received( const Message& message, uint64_t deliveryTag )
{
    _something_received = true;
    (void) redelivered;

    auto env = _callback( message );

    env->setCorrelationID( message.correlationID() );

    _channel->publish( "", message.replyTo(), *env );
    _channel->ack( deliveryTag );
}
//=======================================================================================
