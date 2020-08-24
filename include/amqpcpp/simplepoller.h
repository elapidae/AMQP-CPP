#ifndef AMQP_SIMPLEPOLLER_H
#define AMQP_SIMPLEPOLLER_H

#include <chrono>
#include <memory>
#include <iostream>

namespace AMQP
{

//=======================================================================================
//      class SimplePoller -- OS independent poller, which use system-specific calls.
//  As file descriptos it gives int values. In windows typedef SOCKET is UINT, so
//  nothing bad can be happens.
//=======================================================================================
class SimplePoller final
{
public:
    class ReceiverInterface;

    SimplePoller();
    virtual ~SimplePoller();

    /**
     * @brief add_read -- wait for read events.
     * @param fd -- descriptor for poll events
     * @param receiver
     */
    void add_read   ( int fd, ReceiverInterface * receiver );
    void add_write  ( int fd, ReceiverInterface * receiver );
    void add_except ( int fd, ReceiverInterface * receiver );
    void add        ( int fd, ReceiverInterface * receiver );

    void del_read   ( int fd );
    void del_write  ( int fd );
    void del_except ( int fd );
    void del        ( int fd );

    /**
     * @brief poll -- will poll until any event happens.
     * @return count of polled FDs
     */
    int poll();

    /**
     * @brief poll(time period) -- will poll until event happens or timeout.
     *        If period is negative, will wait until event.
     * @return count of polled FDs
     */
    template<typename Period>
    int poll( Period period );

private:
    int _poll( int microsec );
    class _pimpl; std::unique_ptr<_pimpl> _p;
};
//=======================================================================================
//  For get events, should inherit from this class.
//=======================================================================================
class SimplePoller::ReceiverInterface
{
public:
    virtual ~ReceiverInterface() = default;

    //  Most usefull override ready_read() method.
    //  These methods defined, but throw std::logic_error() exception.
    //  If user called add_write() or add_except() poller methods, but forgot to override
    //  corresponding methods, exceptions can help to find the problem.
    virtual void ready_read();
    virtual void ready_write();
    virtual void except_happened();
};
//=======================================================================================


//=======================================================================================
//      Implememntation
//=======================================================================================
template<typename Period>
int SimplePoller::poll( Period period )
{
    using namespace std::chrono;

    auto microsec = duration_cast<microseconds>(period).count();
    microsec = microsec > 0 ? microsec : 0;
    return _poll( microsec );
}
//=======================================================================================


} // namespace AMQP

#endif // AMQP_SIMPLEPOLLER_H
