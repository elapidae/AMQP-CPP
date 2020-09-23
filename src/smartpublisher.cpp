#include "amqpcpp.h"

using namespace AMQP;

//=======================================================================================
SmartPublisher::SmartPublisher( AMQP::SmartSettings sett )
    : _settings( std::move(sett) )
    , _handler( SimplePoller::thread_poller() )
{
    _connect();
}
//=======================================================================================
void SmartPublisher::publish( const std::string &msg )
{
    _connect();
    _channel->publish( _settings.exchange, "", msg );
}
//=======================================================================================
void SmartPublisher::publish( const Envelope& env )
{
    _connect();
    _channel->publish( _settings.exchange, "", env );
}
//=======================================================================================
void SmartPublisher::_connect()
{
    if ( _handler.is_connected() )
        return;

    _handler.connect( _settings.address, _settings.port );

    auto amqp_login = Login( _settings.user, _settings.password );

    _connection.reset( new Connection(&_handler, amqp_login, _settings.vhost) );

    _channel.reset( new Channel(_connection.get()) );

    bool let_stop = false;
    auto &defer = _channel->declareExchange( _settings.exchange, fanout );
    defer.onSuccess( [&let_stop]{ let_stop = true; } );
    defer.onError( [](const char *err){ throw std::runtime_error(err); } );

    //  Waiting while our exclusive queue will inited.
    while ( !let_stop )
        SimplePoller::thread_poller()->poll();
}
//=======================================================================================
