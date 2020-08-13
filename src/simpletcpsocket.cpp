/**
 *  SimpleTcpSocket.cpp
 *
 *  Implementation of the OS independent TCP socket.
 *
 *  @author Gromtsev Alexander <elapidae@yandex.ru>
 *  @copyright B2Broker
 */

//  See:
//  https://www.binarytides.com/winsock-socket-programming-tutorial/
//  https://man7.org/linux/man-pages/man3/errno.3.html

/**
 *  Dependencies
 */
#include "includes.h"
#include <iostream>

#include <cassert>

#include <errno.h>
#include <string.h>


//=======================================================================================
/// Define OS
//=======================================================================================
#if (defined __WIN32) or (defined __WIN64)
    #define FOR_SOCKETS_WE_SHOULD_USE_WINDOWS 1
    #define FOR_SOCKETS_WE_SHOULD_USE_LINUX   0
#else
    #define FOR_SOCKETS_WE_SHOULD_USE_WINDOWS 0
    #define FOR_SOCKETS_WE_SHOULD_USE_LINUX   1
#endif
//=======================================================================================


//=======================================================================================
/// OS-dependent includes
//=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_LINUX
    #include <unistd.h>     //  for ::close
    #include <fcntl.h>      //  for nonblock mode

    #include <sys/socket.h> //  Our work
    #include <arpa/inet.h>  //  is here ^)
#endif
//=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif
//=======================================================================================


//=======================================================================================
/// Define socket type descriptor
//=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_LINUX
    using socket_type = int;
    static constexpr socket_type invalid_socket = -1;
    static constexpr int send_flags = MSG_NOSIGNAL;

    #ifndef strerror_s
        #define strerror_s(b,s,e) strerror_r(e,b,s);
    #endif

#endif
    //=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_WINDOWS
    using socket_type = SOCKET;
    static constexpr socket_type invalid_socket = INVALID_SOCKET;
    static constexpr int send_flags = 0;
#endif
//=======================================================================================

static std::string  get_errno_error     ();
static std::string  WSA_last_error      ();
static std::string  get_error           ();
static void         check_ok            ( bool ok, const std::string error_msg );

static socket_type  do_tcp_socket       ();
static void         do_non_out_of_band  ( socket_type fd );
static void         do_nonblock         ( socket_type fd, bool nonblock );
static void         do_inet_addr        ( const std::string& addr, in_addr * store );
static bool         is_err_would_block  ();
static std::string  do_receive          ( socket_type fd );
static void         do_close            ( socket_type fd );


//=======================================================================================
//  OS-dependent calls.
//=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_LINUX
//=======================================================================================
static std::string WSA_last_error()
{
    return {};
}
//=======================================================================================
static void do_non_out_of_band( socket_type fd )
{
    int val = 1;
    auto res = ::setsockopt( fd, SOL_SOCKET, SO_OOBINLINE, &val, sizeof(val) );
    check_ok( res == 0, "Cannot unset tcp out of band" );
}
//=======================================================================================
static void do_nonblock( socket_type fd, bool nonblock )
{
    auto flags = ::fcntl( fd, F_GETFL );
    if ( flags == -1 )
        throw std::runtime_error( "Cannot get tcp flags: '" + get_error() + "'" );

    if ( nonblock ) flags |=  O_NONBLOCK;
    else            flags &= ~O_NONBLOCK;

    auto res = ::fcntl( fd, F_SETFL, flags );
    check_ok( res != -1, "Cannot set tcp flags" );
}
//=======================================================================================
static bool is_err_would_block()
{
    return errno == EAGAIN;
}
//=======================================================================================
static void do_inet_addr( const std::string& addr, in_addr * store )
{
    auto res = ::inet_aton( addr.c_str(), store );
    check_ok( res, "Bad ip: '" + addr + "'" );
}
//=======================================================================================
static void do_close( socket_type fd )
{
    ::close( fd );
}
//=======================================================================================
#endif // FOR_SOCKETS_WE_SHOULD_USE_LINUX
//=======================================================================================
#if FOR_SOCKETS_WE_SHOULD_USE_WINDOWS
//=======================================================================================
//  TODO: Get human readable text of error using FormatMessage...
static std::string WSA_last_error()
{
    return
    std::string("WSA error code: " + std::to_string(WSAGetLastError()) +
                "\n,see https://docs.microsoft.com/en-us/windows/win32/winsock"
                "/windows-sockets-error-codes-2");
}
//=======================================================================================
static void do_non_out_of_band( socket_type fd )
{
    int val = 1;
    auto ptr = static_cast<const char*>( static_cast<const void*>(&val) );
    auto res = ::setsockopt( fd, SOL_SOCKET, SO_OOBINLINE, ptr, sizeof(val) );
    check_ok( res == 0, "Cannot unset tcp out of band" );
}
//=======================================================================================
//  See https://docs.microsoft.com/en-us/windows/win32/api/winsock/
//      nf-winsock-ioctlsocket?redirectedfrom=MSDN
//-------------------------
// Set the socket I/O mode: In this case FIONBIO
// enables or disables the blocking mode for the
// socket based on the numerical value of iMode.
// If iMode = 0, blocking is enabled;
// If iMode != 0, non-blocking mode is enabled.
static void do_nonblock( socket_type fd, bool nonblock )
{    
    u_long mode = nonblock ? 1 : 0;
    auto res = ioctlsocket( fd, FIONBIO, &mode );
    check_ok( res == NO_ERROR, "Cannot set nonblock on tcp socket" );
}
//=======================================================================================
static bool is_err_would_block()
{
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
//=======================================================================================
static void do_inet_addr( const std::string& addr, in_addr * store )
{
    store->s_addr = inet_addr( addr.c_str() );
    //  Has not any check, so it will during connect...
}
//=======================================================================================
static void do_close( socket_type fd )
{
    ::closesocket( fd );
}
//=======================================================================================
#endif  // FOR_SOCKETS_WE_SHOULD_USE_WINDOWS
//=======================================================================================


//=======================================================================================
//  OS independent calls.
//=======================================================================================
static std::string get_errno_error()
{
    char buf[256];
    ::strerror_s( buf, sizeof(buf), errno );
    return std::string("errno: '") + buf + "'";
}
//=======================================================================================
static std::string get_error()
{
    return get_errno_error() + WSA_last_error();
}
//=======================================================================================
static void check_ok( bool ok, const std::string error_msg )
{
    if ( ok ) return;
    throw std::runtime_error( error_msg + ": '" + get_error() + "'" );
}
//=======================================================================================
static socket_type do_tcp_socket()
{
    auto res = ::socket( AF_INET, SOCK_STREAM, 0 );
    check_ok( res != invalid_socket, "Init TCP socket" );
    return res;
}
//=======================================================================================
static void do_connect( socket_type fd, const sockaddr_in *addr )
{
    auto ptr = static_cast<const sockaddr*>( static_cast<const void*>(addr) );
    auto res = ::connect( fd, ptr, sizeof(*addr) );
    check_ok( res == 0, "Cannot connect tcp socket" );
}
//=======================================================================================
//  Sending while all data was sent. Do not do async sending, because buffers will grow.
static bool do_send( socket_type fd, const char *ptr, size_t size )
{
    while (true)
    {
        auto sended = ::send( fd, ptr, size, send_flags );
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
            if ( is_err_would_block() )
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
//  OS independent calls.
//=======================================================================================


using namespace AMQP;


//=======================================================================================
//      Implementation class
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
//      Implementation class
//=======================================================================================


//=======================================================================================
//      SimpleTcpSocket API
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
void SimpleTcpSocket::set_poller( SimplePoller * poller )
{
    if ( _p->poller )
        _p->poller->del( fd() );

    _p->poller = poller;

    if ( is_connected() && poller )
        poller->add_read( fd(), _p.get() );
}
//=======================================================================================
void SimpleTcpSocket::connect( const std::string& host, uint16_t port )
{
    do_nonblock( fd(), false );

    struct sockaddr_in server;
    memset( &server, 0, sizeof(server) );

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
std::string SimpleTcpSocket::receive()
{
    return do_receive( fd() );
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
//      SimpleTcpSocket API
//=======================================================================================
