#include "amqpcpp.h"


using namespace AMQP;

//=======================================================================================
SmartRPCClient::SmartRPCClient( SmartSettings sett )
    : _settings( std::move(sett) )
    , _poller  ()
    , _handler ( &_poller )
    , _msg( {}, {} )
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    _correlation = std::to_string( now );

    _connect();
}
//=======================================================================================
const Message & SmartRPCClient::execute( Envelope *env )
{
    execute( env, std::chrono::microseconds{0} );
    return last_message();
}
//=======================================================================================
bool SmartRPCClient::execute( Envelope *env, std::chrono::microseconds wait_us )
{
    _connect();

    env->setCorrelationID( _correlation );
    env->setReplyTo( _queue_name );

    _channel->publish( "", _settings.queue, *env );

    _received = false;
    while( !_received )
    {
        _poller.poll( wait_us );

        //  Переходим на новое ожидание только если указано бесконечное ожидание.
        if ( wait_us.count() == 0 )
            continue;
    }

    return _received;
}
//=======================================================================================
const Message &SmartRPCClient::last_message() const
{
    return _msg;
}
//=======================================================================================
void SmartRPCClient::_connect()
{
    if ( _handler.is_connected() )
        return;

    _handler.connect( _settings.address, _settings.port );

    auto amqp_login = Login( _settings.user, _settings.password );

    _connection.reset( new Connection(&_handler,
                                      amqp_login,
                                      _settings.vhost) );

    _channel.reset( new Channel(_connection.get()) );

    bool queue_received = false;
    auto q_callback = [this,&queue_received]( const std::string &name,
                                              int msgcount,
                                              int consumercount )
    {
        (void) msgcount;
        (void) consumercount;

        _queue_name = name;
        queue_received = true;
    };

    _channel->declareQueue(AMQP::exclusive).onSuccess( q_callback );


    auto receive_callback = [this]( const AMQP::Message &message,
                                    uint64_t deliveryTag,
                                    bool redelivered )
    {
        (void) deliveryTag;
        (void) redelivered;
        this->_on_received( message );
    };
    _channel->consume("", AMQP::noack).onReceived( receive_callback );

    //  Waiting while our exclusive queue will inited.
    while ( !queue_received )
        _poller.poll();
}
//=======================================================================================
void SmartRPCClient::_on_received( const Message &message )
{
    if( message.correlationID() != _correlation )
        return;

    _msg = message;

    _received = true;
}
//=======================================================================================
