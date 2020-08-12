#include "amqpcpp/simpletcpsocket.h"

#include <cassert>
#include "amqpcpp/simplepoller.h"

#include <features.h>
#ifdef _POSIX_C_SOURCE
    #define FOR_POLLER_WE_SHOULD_USE_LINUX
#endif

#ifdef FOR_POLLER_WE_SHOULD_USE_LINUX
    //  https://man7.org/linux/man-pages/man3/errno.3.html
    #include <errno.h>
    #include <string.h>     //  for ::strerror_r function

    #include <unistd.h>     //  for ::close
    #include <fcntl.h>      //  for nonblock mode

    #include <sys/socket.h> //  Our work
    #include <arpa/inet.h>  //  is here ^)
#else   //  Windows includes
#endif


#ifdef FOR_POLLER_WE_SHOULD_USE_LINUX
#else   //  Windows includes
#endif


#ifdef FOR_POLLER_WE_SHOULD_USE_LINUX
//=======================================================================================
using socket_type = int;
static constexpr socket_type invalid_socket = -1;
//=======================================================================================
static std::string get_error()
{
    char buf[256];
    ::strerror_r( errno, buf, sizeof(buf) );
    return buf;
}
//=======================================================================================
static socket_type do_tcp_socket()
{
    auto res = ::socket( AF_INET, SOCK_STREAM, 0 );

    if ( res != invalid_socket )
        return res;

    throw std::runtime_error( "Cannot init tcp socket: '" + get_error() + "'" );
}
//=======================================================================================
static std::string do_receive( socket_type fd )
{
    enum { buffer_size = 4096 };

    char buf[ buffer_size ];
    std::string res;

    while(1)
    {
        auto has_read = ::recv( fd, buf, buffer_size, 0 );

        if ( has_read == -1 )
        {
            if ( errno == EAGAIN )
                break;

            auto msg = "SimplTcpSocket has error on receive: '" + get_error() + "'";
            throw std::runtime_error( msg );
        }

        res.append( buf, size_t(has_read) );

        if ( has_read < buffer_size )
            break;
    }

    return res;
}
//=======================================================================================
static void do_close( socket_type fd )
{
    ::close( fd );
}
//=======================================================================================
static void do_inet_addr( const char* addr, in_addr * store )
{
    auto res = ::inet_aton( addr, store );

    if ( res != 0 )
        return;

    throw std::runtime_error( std::string("Bad ip:'") + addr + "'." );
}
//=======================================================================================
static void do_connect( socket_type fd, const sockaddr_in *addr )
{
    auto ptr = static_cast<const sockaddr*>( static_cast<const void*>(addr) );
    auto res = ::connect( fd, ptr, sizeof(*addr) );

    if ( res == 0 )
        return;

    throw std::runtime_error( "Cannot connect tcp socket: '" + get_error() + "'" );
}
//=======================================================================================
//  It is very strange flag for shadow mode. There is nothing good with this mode.
static void do_non_out_of_band( socket_type fd )
{
    int val = 1;
    auto res = ::setsockopt( fd, SOL_SOCKET, SO_OOBINLINE, &val, sizeof(val) );

    if ( res == 0 )
        return;

    throw std::runtime_error( "Cannot unset tcp out of band: '" +
                              get_error() + "'" );
}
//=======================================================================================
static void do_nonblock( socket_type fd, bool nonblock )
{
    auto flags = ::fcntl( fd, F_GETFL );
    if ( flags == -1 )
        throw std::runtime_error( "Cannot get tcp flags: '" + get_error() + "'" );

    if ( nonblock ) flags |=  O_NONBLOCK;
    else            flags &= ~O_NONBLOCK;

    flags = ::fcntl( fd, F_SETFL, flags );
    if ( flags == -1 )
        throw std::runtime_error( "Cannot set tcp flags: '" + get_error() + "'" );
}
//=======================================================================================
static bool do_send( socket_type fd, const char *ptr, size_t size )
{
    while (true)
    {
        auto sended = ::send( fd, ptr, size, MSG_NOSIGNAL );
        size_t usended = size_t( sended );

        if ( sended < 0 && errno == EINTR ) continue;

        if ( sended  <  0 )    return false;
        if ( usended == size ) return true;

        assert( usended < size );
        ptr  += usended;
        size -= usended;
    }
}
//=======================================================================================
#else   //  Windows includes
#endif

using namespace AMQP;


//=======================================================================================
class SimpleTcpSocket::_pimpl final : public SimplePoller::ReceiverInterface
{
public:
    void ready_read() override;

    SimpleTcpSocket *owner      = nullptr;
    SimplePoller    *poller     = nullptr;

    bool            connected   = false;
    socket_type     fd          = invalid_socket;
};
//=======================================================================================
void SimpleTcpSocket::_pimpl::ready_read()
{
    assert( owner );
    auto data = do_receive( fd );

    if ( data.empty() )
    {
        owner->close();
        return;
    }

    if ( owner->received )
        owner->received( data );
}
//=======================================================================================


//=======================================================================================
SimpleTcpSocket::SimpleTcpSocket()
    : _p( new _pimpl )
{
    _p->owner = this;
    _p->fd    = do_tcp_socket();

    do_non_out_of_band( fd() );
}
//=======================================================================================
SimpleTcpSocket::SimpleTcpSocket( SimplePoller * poller )
    : SimpleTcpSocket()
{
    _p->poller = poller;
}
//=======================================================================================
SimpleTcpSocket::~SimpleTcpSocket()
{
    if ( is_connected() )
        close();
}
//=======================================================================================
void SimpleTcpSocket::connect( const std::string& host, uint16_t port )
{
    do_nonblock( fd(), false );

    struct sockaddr_in server;
    bzero( &server, sizeof(server) );

    do_inet_addr( host.c_str(), &server.sin_addr );
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    do_connect( fd(), &server );
    _p->connected = true;

    do_nonblock( fd(), true );

    auto poller = _p->poller;
    if ( poller )
        poller->add_read( fd(), _p.get() );
}
//=======================================================================================
void SimpleTcpSocket::close()
{
    do_close( fd() );

    _p->connected = false;

    if ( _p->poller )
        _p->poller->del( fd() );

    if ( disconnected )
        disconnected();
}
//=======================================================================================
bool SimpleTcpSocket::send(const char *buffer, size_t size)
{
    return do_send( fd(), buffer, size );
}
//=======================================================================================
bool SimpleTcpSocket::send( const std::string& data )
{
    return send( data.c_str(), data.size() );
}
//=======================================================================================
bool SimpleTcpSocket::is_connected() const noexcept
{
    return _p->connected;
}
//=======================================================================================
int SimpleTcpSocket::fd() const noexcept
{
    return _p->fd;
}
//=======================================================================================
