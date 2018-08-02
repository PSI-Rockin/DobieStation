/**
Code taken from https://bitbucket.org/KjellKod/lock-free-wait-free-circularfifo
This is a single-producer single-consumer FIFO, so if more multithreading is added
we'll need to change this. It's probably better to have multiple 1->1 queues than
a single bigger queue if at all reasonable though
**/
#ifndef CIRCULARFIFO_AQUIRE_RELEASE_H_
#define CIRCULARFIFO_AQUIRE_RELEASE_H_

#include <atomic>
#include <cstddef>

#include "errors.hpp"
template<typename Element, size_t Size>
class CircularFifo
{
public:
    enum { Capacity = Size + 1 };

    CircularFifo() : _tail(0), _head(0) {}
    virtual ~CircularFifo() {}

    void push(const Element& item); // pushByMOve?
    bool pop(Element& item);

    bool wasEmpty() const;
    bool wasFull() const;
    bool isLockFree() const;

private:
    size_t increment(size_t idx) const;

    std::atomic <size_t>  _tail;  // tail(input) index
    Element    _array[Capacity];
    std::atomic<size_t>   _head; // head(output) index
};

template<typename Element, size_t Size>
void CircularFifo<Element, Size>::push(const Element& item)
{
    const auto current_tail = _tail.load(std::memory_order_relaxed);
    const auto next_tail = increment(current_tail);
    if (next_tail != _head.load(std::memory_order_acquire))
    {
        _array[current_tail] = item;
        _tail.store(next_tail, std::memory_order_release);
    }
    else
    {
        Errors::die("FIFO FULL!");
    }
}

// Pop by Consumer can only update the head (load with relaxed, store with release)
//     the tail must be accessed with at least aquire
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::pop(Element& item)
{
    const auto current_head = _head.load(std::memory_order_relaxed);
    if (current_head == _tail.load(std::memory_order_acquire))
        return false; // empty queue

    item = _array[current_head];
    _head.store(increment(current_head), std::memory_order_release);
    return true;
}

template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::wasEmpty() const
{
    // snapshot with acceptance of that this comparison operation is not atomic
    return (_head.load() == _tail.load());
}


// snapshot with acceptance that this comparison is not atomic
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::wasFull() const
{
    const auto next_tail = increment(_tail.load()); // aquire, we dont know who call
    return (next_tail == _head.load());
}


template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::isLockFree() const
{
    return (_tail.is_lock_free() && _head.is_lock_free());
}

template<typename Element, size_t Size>
size_t CircularFifo<Element, Size>::increment(size_t idx) const
{
    return (idx + 1) % Capacity;
    }
#endif /* CIRCULARFIFO_AQUIRE_RELEASE_H_ */
