/**
 *  Message.h
 *
 *  An incoming message has the same sort of information as an outgoing
 *  message, plus some additional information.
 *
 *  Message objects can not be constructed by end users, they are only constructed
 *  by the AMQP library, and passed to user callbacks.
 *
 *  @copyright 2014 - 2018 Copernica BV
 */

/**
 *  Include guard
 */
#pragma once

/**
 *  Dependencies
 */
#include "envelope.h"
#include <limits>
#include <stdexcept>
#include <algorithm>


/**
 *  Set up namespace
 */
namespace AMQP {

/**
 *  Forward declarations
 */
class DeferredReceiver;

/**
 *  Class definition
 */
class Message : public Envelope
{

protected:
    /**
     *  The exchange to which it was originally published
     *  @var    string
     */
    std::string _exchange;

    /**
     *  The routing key that was originally used
     *  @var    string
     */
    std::string _routingkey;
    
    /**
     *  We are an open book to the consumer handler
     */
    friend class DeferredReceiver;

    /**
     *  Set the body size
     *  This field is set when the header is received
     *  @param  uint64_t
     */
    ssize_t _body_size_for_receive = -1;
    void setBodySize(uint64_t size)
    {
        // safety-check: on 32-bit platforms size_t is obviously also a 32-bit dword
        // in which case casting the uint64_t to a size_t could result in truncation
        // here we check whether the given size fits inside a size_t
        if (std::numeric_limits<size_t>::max() < size) throw std::runtime_error("message is too big for this system");

        // store the new size
        _body_size_for_receive = size;
    }

    /**
     *  Append data
     *  @param  buffer      incoming data
     *  @param  size        size of the data
     *  @return bool        true if the message is now complete
     */
    bool append(const char *buffer, uint64_t size)
    {
        _data.append( buffer, size );
            
        if ( _body_size_for_receive < 0 )
            throw std::logic_error("message size is not declared");

        if ( _body_size_for_receive > bodySize() )
            throw std::runtime_error("message size more than need");

        // check if we're done
        return _body_size_for_receive == bodySize();
    }

public:
    /**
     *  Constructor
     *
     *  @param  exchange
     *  @param  routingKey
     */
    Message(std::string exchange, std::string routingkey) :
        Envelope(nullptr, 0), _exchange(std::move(exchange)), _routingkey(std::move(routingkey))
    {}

    /**
     *  Disabled copy constructor
     *  @param  message the message to copy
     */
    Message(const Message &message) = delete;

    /**
     *  The exchange to which it was originally published
     *  @var    string
     */
    const std::string &exchange() const
    {
        // expose member
        return _exchange;
    }

    /**
     *  The routing key that was originally used
     *  @var    string
     */
    const std::string &routingkey() const
    {
        // expose member
        return _routingkey;
    }
};

/**
 *  End of namespace
 */
}

