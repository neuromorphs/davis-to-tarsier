#pragma once

#include <cstdint>
#include <utility>

/// tarsier is a collection of event handlers.
namespace tarsier {

    /// my_filter propagates only the events within the given rectangular
    /// window.
    template <typename Event, typename HandleEvent>
    class my_filter {
        public:
        my_filter(uint16_t left, uint16_t bottom, uint16_t width, uint16_t height, HandleEvent&& handle_event) :
            _left(left),
            _bottom(bottom),
            _right(left + width),
            _top(bottom + height),
            _handle_event(std::forward<HandleEvent>(handle_event)) {}
        my_filter(const my_filter&) = delete;
        my_filter(my_filter&&) = default;
        my_filter& operator=(const my_filter&) = delete;
        my_filter& operator=(my_filter&&) = default;
        virtual ~my_filter() = default;

        /// operator() handles an event.
        virtual void operator()(Event event) {
            if (event.x >= _left && event.x < _right && event.y >= _bottom && event.y < _top) {
                _handle_event(event);
            }
        }

        protected:
        const uint16_t _left;
        const uint16_t _bottom;
        const uint16_t _right;
        const uint16_t _top;
        HandleEvent _handle_event;
    };

    /// make_my_filter creates a my_filter from a functor.
    template <typename Event, typename HandleEvent>
    my_filter<Event, HandleEvent> inline make_my_filter(
        uint16_t left,
        uint16_t bottom,
        uint16_t width,
        uint16_t height,
        HandleEvent&& handle_event) {
        return my_filter<Event, HandleEvent>(
            left, bottom, width, height, std::forward<HandleEvent>(handle_event));
    }
}
