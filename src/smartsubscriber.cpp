#include "amqpcpp.h"

#include <cassert>
#include <iostream>

using namespace AMQP;
using namespace std;

//=======================================================================================
SmartSubscriber::SmartSubscriber(SmartSettings sett, Subscribe sub)
    : _settings  ( std::move(sett) )
    , _subscribe ( std::move(sub)  )
    , _poller    ()
    , _handler   ( &_poller )
{
    assert( _subscribe );
    _connect();
}
//=======================================================================================
void SmartSubscriber::poll_once()
{
    _connect();
    _poller.poll();
}
//=======================================================================================
void SmartSubscriber::_connect()
{
    if ( _handler.is_connected() ) return;

    _handler.connect( _settings.address, _settings.port );

    auto amqp_login = Login( _settings.user, _settings.password );
    _connection.reset( new Connection(&_handler, amqp_login, _settings.vhost) );
    _channel.reset( new Channel(_connection.get()) );


    auto receiveMessageCallback = [this]( const AMQP::Message &message, uint64_t, bool )
    {
        _subscribe( message );
    };

    bool binded = false;
    AMQP::QueueCallback callback = [&,this]( const std::string &name, int, int )
    {
        _channel->bindQueue( _settings.exchange, name, _settings.binding_key );
        cout << "AMQP::SmartSubscriber: binding key: " << _settings.binding_key << endl;
        auto &defer = _channel->consume( name, AMQP::noack);
        defer.onReceived( receiveMessageCallback );
        defer.onSuccess( [&](){binded = true;} );
        defer.onError( [](const char* err){ throw std::runtime_error(err);} );
    };

    AMQP::SuccessCallback success = [&]()
    {
        _channel->declareQueue(AMQP::exclusive).onSuccess( callback );
    };

    //_channel->declareExchange(_settings.exchange, AMQP::fanout).onSuccess( success );
    _channel->declareExchange(_settings.exchange, AMQP::topic).onSuccess( success );

    while ( !binded )
        _poller.poll();
}
//=======================================================================================
