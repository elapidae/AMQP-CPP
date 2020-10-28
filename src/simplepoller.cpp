#include <iostream>
static std::string _fname(const std::string& f) { return f.substr( f.rfind('/')+1 ); }
#define vpos (std::string("[") + _fname(__FILE__) + ":" + std::to_string(__LINE__) + "]")
#define vdeb std::cout << vpos << "=>\t"
#define eoln std::endl;
#define vtrace vdeb << "trace" << eoln

#include "includes.h"

#include <stdexcept>
#include <cassert>
#include <map>
#include <vector>
#include <mutex>
#include <thread>

#include <iostream>

#if (defined __WIN32) or (defined __WIN64)
    #define FOR_POLLER_WE_SHOULD_USE_WINDOWS 1
    #define FOR_POLLER_WE_SHOULD_USE_LINUX   0
#else
    #define FOR_POLLER_WE_SHOULD_USE_WINDOWS 0
    #define FOR_POLLER_WE_SHOULD_USE_LINUX   1
#endif


#if FOR_POLLER_WE_SHOULD_USE_LINUX
    //  https://man7.org/linux/man-pages/man2/select.2.html
    #include <sys/select.h>

    //  https://man7.org/linux/man-pages/man3/errno.3.html
    #include <errno.h>

    //  for ::strerror_r function
    #include <string.h>

    using sock_type = int;

    static bool is_eintr()
    {
        return errno == EINTR;
    }
    static std::string get_err_text()
    {
        char err_buf[256];
        err_buf[0] = 0;
        ::strerror_r( errno, err_buf, sizeof(err_buf) );
        return err_buf;
    }
#endif
#if FOR_POLLER_WE_SHOULD_USE_WINDOWS
    #include <winsock2.h>

    using sock_type = u_int;

    static bool is_eintr()
    {
        return false;
    }
    static std::string get_err_text()
    {
        return "LastError:" + std::to_string(GetLastError()) +
        "\nsee: https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes";
    }
#endif

using namespace AMQP;


//=======================================================================================
class SimplePoller::_pimpl
{
public:
    //  Need for callbacks.
    using Receiver       = SimplePoller::ReceiverInterface;
    using ReceiverMethod = void (Receiver::*)();
    using CallPair       = std::pair<Receiver*,ReceiverMethod>;

    using PtrMap = std::map<int, Receiver*>;
    PtrMap read_fds;
    PtrMap write_fds;
    PtrMap except_fds;

    int poll( int microsec );

private:
    int fill_sets_and_calc_nfds( fd_set * read_set,
                                 fd_set * write_set,
                                 fd_set * except_set );
    int do_select( fd_set * read_set,
                   fd_set * write_set,
                   fd_set * except_set,
                   struct timeval * tv );
};
//=======================================================================================
//  [1]
//  nfds -- This argument should be set to the highest-numbered file
//  descriptor in any of the three sets, plus 1.  The indicated
//  file descriptors in each set are checked, up to this limit
//  (but see BUGS).
//
int SimplePoller::_pimpl::fill_sets_and_calc_nfds( fd_set * read_set,
                                                   fd_set * write_set,
                                                   fd_set * except_set )
{
    FD_ZERO( read_set   );
    FD_ZERO( write_set  );
    FD_ZERO( except_set );

    for ( auto & fd: read_fds   )   FD_SET( sock_type(fd.first), read_set   );
    for ( auto & fd: write_fds  )   FD_SET( sock_type(fd.first), write_set  );
    for ( auto & fd: except_fds )   FD_SET( sock_type(fd.first), except_set );

    //  nfds -- strange var, see [1].
    int nfds = 0;
    for ( auto & fd: read_fds   )   nfds = std::max( nfds, fd.first );
    for ( auto & fd: write_fds  )   nfds = std::max( nfds, fd.first );
    for ( auto & fd: except_fds )   nfds = std::max( nfds, fd.first );

    return nfds + 1;
}
//=======================================================================================
int SimplePoller::_pimpl::do_select( fd_set *read_set,
                                     fd_set *write_set,
                                     fd_set *except_set,
                                     struct timeval * tv )
{
    auto nfds = fill_sets_and_calc_nfds( read_set, write_set, except_set );

    //  Set unused pointers to null.
    read_set   = read_fds.empty()   ? nullptr : read_set;
    write_set  = write_fds.empty()  ? nullptr : write_set;
    except_set = except_fds.empty() ? nullptr : except_set;

    if ( !read_set && !write_set && !except_set )
        throw std::runtime_error( "do_select(): nothing to select..." );

    int res = -1;
    while ( res == -1 )
    {
        res = ::select( nfds, read_set, write_set, except_set, tv );

        if ( res >= 0 )
            break;

        if ( is_eintr() )   //  In linux systems we must to check this situation.
            continue;

        auto msg = std::string("SimplePoller::poll() select error: '")
                    + get_err_text() + "'";
        std::cout << msg << std::endl;
        throw std::runtime_error( msg );
    }
    return res;
}
//=======================================================================================
int SimplePoller::_pimpl::poll( int microsec )
{
    struct timeval tv;
    tv.tv_sec  = microsec / 1000000;
    tv.tv_usec = microsec % 1000000;
    auto tv_ptr = microsec == 0 ? nullptr : &tv;

    fd_set read_set, write_set, except_set;

    //  res contains count of selected fds.
    //std::cout << "do_select with us=" << microsec << std::endl;
    auto res = do_select( &read_set, &write_set, &except_set, tv_ptr );

    //  Receivers can call del() inside circles, but we cannot change maps.
    //  We have to call methods outside of circles.
    std::vector<CallPair> callers;
    for ( auto & fd: read_fds )
    {
        if ( 0 == FD_ISSET(fd.first, &read_set) ) continue;
        callers.push_back( {fd.second, &Receiver::ready_read} );
    }
    for ( auto & fd: write_fds )
    {
        if ( 0 == FD_ISSET(fd.first, &write_set) ) continue;
        callers.push_back( {fd.second, &Receiver::ready_write} );
    }
    for ( auto & fd: except_fds )
    {
        if ( 0 == FD_ISSET(fd.first, &except_set) ) continue;
        callers.push_back( {fd.second, &Receiver::except_happened} );
    }

    for ( auto & call: callers )
        (call.first->*call.second)();

    return res;
}
//=======================================================================================


//=======================================================================================
//      OS independent SimplePoller part
//=======================================================================================
//  Вот этот код не работает в маздае, при закрытии потока происходит разименование 0.
//  Соответсвтвеноо, переделываю код на хранение через shared_ptr.
/*
thread_local std::unique_ptr<SimplePoller> _thread_poll;
SimplePoller *SimplePoller::thread_poller()
{
    if ( !_thread_poll )
        _thread_poll.reset( new SimplePoller );

    return _thread_poll.get();
}
*/

//  1. Лочим мьютекс, лезем в мапу, ищем указатель по id потока.
//  2. Если такой уже есть и он живой -- возвращаем и не паримся,
//     если мертвый -- выкидываем.
//  3. Создаем новый, откладываем вик в мапу, результат определен.
//  4. Пытаемся почистить мапу.
AMQP::SimplePoller::PollerPtr SimplePoller::thread_poller_deprecated()
{
    using WeakPtr = std::weak_ptr<SimplePoller>;

    static std::mutex pmutex;
    static std::map<std::thread::id,WeakPtr> pmap;

    auto id = std::this_thread::get_id();

    std::unique_lock<std::mutex> lock( pmutex );
    auto it = pmap.find( id );
    if ( it != pmap.end() )
    {
        auto res = (*it).second.lock();

        if ( res )
            return res;

        pmap.erase( it );
    }

    for ( auto it = pmap.begin(); it != pmap.end(); ++it )
    {
        if ( !(*it).second.lock() )
        {
            pmap.erase( it );
            break;
        }
    }

    auto res = std::make_shared<SimplePoller>();
    pmap.emplace( id, res );
    return res;
}
//=======================================================================================


//=======================================================================================
SimplePoller::SimplePoller()
    : _p( new _pimpl )
{}
//=======================================================================================
SimplePoller::~SimplePoller()
{}
//=======================================================================================
void SimplePoller::add_read( int fd, SimplePoller::ReceiverInterface *receiver )
{
    _p->read_fds.emplace( fd, receiver );
}
//=======================================================================================
void SimplePoller::add_write( int fd, SimplePoller::ReceiverInterface *receiver )
{
    _p->write_fds.emplace( fd, receiver );
}
//=======================================================================================
void SimplePoller::add_except( int fd, SimplePoller::ReceiverInterface * receiver )
{
    _p->except_fds.emplace( fd, receiver );
}
//=======================================================================================
void SimplePoller::add( int fd, SimplePoller::ReceiverInterface * receiver )
{
    add_read    ( fd, receiver );
    add_write   ( fd, receiver );
    add_except  ( fd, receiver );
}
//=======================================================================================
void SimplePoller::del_read( int fd )
{
    _p->read_fds.erase( fd );
}
//=======================================================================================
void SimplePoller::del_write( int fd )
{
    _p->write_fds.erase( fd );
}
//=======================================================================================
void SimplePoller::del_except( int fd )
{
    _p->except_fds.erase( fd );
}
//=======================================================================================
void SimplePoller::del( int fd )
{
    del_read    ( fd );
    del_write   ( fd );
    del_except  ( fd );
}
//=======================================================================================
int SimplePoller::poll()
{
    return _poll( 0 );
}
//=======================================================================================
int SimplePoller::_poll( int microsec )
{
    return _p->poll( microsec );
}
//=======================================================================================


//=======================================================================================
//      Caution throw methods for ReceiverInterface
//=======================================================================================
void SimplePoller::ReceiverInterface::ready_read()
{
    throw std::logic_error( "Forgot to override "
                            "SimplePoller::ReceiverInterface::ready_read()" );
}
//=======================================================================================
void SimplePoller::ReceiverInterface::ready_write()
{
    throw std::logic_error( "Forgot to override "
                            "SimplePoller::ReceiverInterface::ready_write()" );
}
//=======================================================================================
void SimplePoller::ReceiverInterface::except_happened()
{
    throw std::logic_error( "Forgot to override "
                            "SimplePoller::ReceiverInterface::except_happened()" );
}
//=======================================================================================
