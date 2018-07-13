//Code taken from https://bitbucket.org/KjellKod/lock-free-wait-free-circularfifo
#ifndef CIRCULARFIFO_SEQUENTIAL_H_
#define CIRCULARFIFO_SEQUENTIAL_H_

#include <atomic>
#include <cstddef>

template<typename Element, size_t Size>
class CircularFifo {
public:
    enum { Capacity = Size + 1 };

    CircularFifo() : _tail(0), _head(0) {}
    virtual ~CircularFifo() {}

    bool push(const Element& item); // pushByMOve?
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


// Here with memory_order_seq_cst for every operation. This is overkill but easy to reason about
//
// Push on tail. TailHead is only changed by producer and can be safely loaded using memory_order_relexed
//         head is updated by consumer and must be loaded using at least memory_order_acquire
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::push(const Element& item)
{
    const auto current_tail = _tail.load();
    const auto next_tail = increment(current_tail);
    if (next_tail != _head.load())
    {
        _array[current_tail] = item;
        _tail.store(next_tail);
        return true;
    }
    printf("[GS] ERROR: Queue full!");
    return false;  // full queue
}


// Pop by Consumer can only update the head 
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::pop(Element& item)
{
    const auto current_head = _head.load();
    if (current_head == _tail.load())
        return false;   // empty queue

    item = _array[current_head];
    _head.store(increment(current_head));
    return true;
}

// snapshot with acceptance of that this comparison function is not atomic
// (*) Used by clients or test, since pop() avoid double load overhead by not
// using wasEmpty()
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::wasEmpty() const
{
    return (_head.load() == _tail.load());
}

// snapshot with acceptance that this comparison is not atomic
// (*) Used by clients or test, since push() avoid double load overhead by not
// using wasFull()
template<typename Element, size_t Size>
bool CircularFifo<Element, Size>::wasFull() const
{
    const auto next_tail = increment(_tail.load());
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
#endif /* CIRCULARFIFO_SEQUENTIAL_H_ */