#ifndef AMQP_SIMPLECONNECTIONHANDLER_H
#define AMQP_SIMPLECONNECTIONHANDLER_H

#include "amqpcpp/connection.h"
#include "amqpcpp/connectionhandler.h"

//  TODO -- change for correct path in prod.
#include "simpletcpsocket.h"

namespace AMQP {

class SimpleConnectionHandler : public ConnectionHandler
{
public:
    explicit SimpleConnectionHandler();
    explicit SimpleConnectionHandler( SimplePoller * poller );

    void connect( std::string address, uint16_t port );

    virtual void onData(Connection *connection, const char *buffer, size_t size) override;

    virtual void onClosed(Connection *connection) override;

private:
    std::string     _received_data;
    SimpleTcpSocket _socket;
    Connection     *_connection = nullptr;
};

} // namespace AMQP

#endif // AMQP_SIMPLECONNECTIONHANDLER_H
