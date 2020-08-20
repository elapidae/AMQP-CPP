#ifndef AMQP_SIMPLETCPSOCKET_H
#define AMQP_SIMPLETCPSOCKET_H

#include <string>
#include <memory>
#include <functional>


namespace AMQP {

class SimplePoller;

class SimpleTcpSocket final
{
public:

    std::function<void(std::string)> received;
    std::function<void()> disconnected;

    //  Poller can be null, but do it in code using your mind.
    explicit SimpleTcpSocket( SimplePoller * poller );
    virtual ~SimpleTcpSocket();

    void set_poller( SimplePoller * poller ); // Can be null.

    void connect( const std::string& host, uint16_t port );
    void close();

    /**
     * send data to peer.
     * @return true if is_connected() and system got data for transfer.
     */
    bool send( const char *buffer, size_t size );
    bool send( const std::string& data );

    std::string receive();

    bool is_connected() const noexcept;

    int fd() const noexcept;

private:
    class _pimpl; std::unique_ptr<_pimpl> _p;
};

} // namespace AMQP

#endif // AMQP_SIMPLETCPSOCKET_H
