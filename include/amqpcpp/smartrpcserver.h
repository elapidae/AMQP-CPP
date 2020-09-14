#ifndef AMQP_SMARTRPCSERVER_H
#define AMQP_SMARTRPCSERVER_H

#include <functional>
#include <memory>

#include "amqpcpp.h"
#include "smartsettings.h"

//=======================================================================================
namespace AMQP
{
    //===================================================================================
    using EnvelopeUPtr = std::unique_ptr<Envelope>;

    EnvelopeUPtr make_envelop( const char *body, uint64_t size );
    EnvelopeUPtr make_envelop( const std::string &body );

    //===================================================================================
    class SmartRPCServer
    {
    public:
        //  Use make_envelop for quick init ptr.
        using Callback = std::function<EnvelopeUPtr(const Message& msg)>;

        SmartRPCServer( SmartSettings settings, Callback cb );

        void poll();


    private:
        SmartSettings   _settings;
        Callback        _callback;

        SimpleConnectionHandler     _handler;
        std::unique_ptr<Connection> _connection;
        std::unique_ptr<Channel>    _channel;

        void _connect();

        void _on_amqp_received( const AMQP::Message &message,
                                uint64_t deliveryTag,
                                bool redelivered );

        bool _something_received = false;
    };
    //===================================================================================
} // namespace AMQP
//=======================================================================================

#endif // AMQP_SMARTRPCSERVER_H
