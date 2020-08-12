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

    explicit SimpleTcpSocket();
    explicit SimpleTcpSocket( SimplePoller * poller );
    virtual ~SimpleTcpSocket();

    void connect( const std::string& host, uint16_t port );
    void close();

    /**
     * send data to peer.
     * @return true if is_connected() and system got data for transfer.
     */
    bool send( const char *buffer, size_t size );
    bool send( const std::string& data );

    bool is_connected() const noexcept;

    int fd() const noexcept;

private:
    class _pimpl; std::unique_ptr<_pimpl> _p;
};

} // namespace AMQP

#endif // AMQP_SIMPLETCPSOCKET_H
