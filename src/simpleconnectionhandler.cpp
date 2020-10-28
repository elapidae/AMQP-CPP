#include "includes.h"

using namespace AMQP;


//=======================================================================================
SimpleConnectionHandler::SimpleConnectionHandler( SimplePoller *poller )
    : _poller( poller )
    , _socket( poller )
{
    _socket.received = [this]( const std::string& data )
    {
        _received_data.append( data );
        if ( !_connection )
            return; //  Something wrong happened.

        auto count = _connection->parse( _received_data.c_str(),
                                         _received_data.size() );
        _received_data.erase( 0, count );
    };
}
//=======================================================================================
void SimpleConnectionHandler::connect( const std::string& address, uint16_t port )
{
    _socket.connect( address, port );
}
//=======================================================================================
bool SimpleConnectionHandler::is_connected() const noexcept
{
    return _socket.is_connected();
}
//=======================================================================================
void SimpleConnectionHandler::onData( Connection *connection,
                                      const char *buffer, size_t size )
{
    _connection = connection;
    _socket.send( buffer, size );
}
//=======================================================================================
void SimpleConnectionHandler::onClosed( Connection *connection )
{
    (void)connection;
    _socket.close();
}
//=======================================================================================
