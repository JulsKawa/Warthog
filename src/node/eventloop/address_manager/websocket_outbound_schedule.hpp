#pragma once

#include "init_arg.hpp"
#include "transport/ws/browser/connect_request.hpp"
#include "wakeup_time.hpp"
#include <chrono>
#include <map>

#ifdef DISABLE_LIBUV
namespace connection_schedule {
class WSConnectionSchedule {
    struct Element {
        Element(WSBrowserConnectRequest r)
            : request(std::move(r))
        {
        }
        std::optional<std::chrono::steady_clock::time_point> expires;
        WSBrowserConnectRequest request;
    };
    using vector_t = std::vector<Element>;
    using steady_clock = std::chrono::steady_clock;
    using time_point = steady_clock::time_point;
    using InitArg = address_manager::InitArg;

public:
    WSConnectionSchedule(InitArg);
    void outbound_closed(const WSBrowserConnectRequest& r, bool success, int32_t reason);
    void outbound_failed(const WSBrowserConnectRequest& r);
    void insert(const WSUrladdr& addr);
    void connect_expired(time_point now = steady_clock::now());
    bool erase(const WSUrladdr& addr);
    [[nodiscard]] std::optional<time_point> pop_wakeup_time();
    auto& state() const { return pinnedRequests; }

private: // private methods
    vector_t::iterator find_element(const WSUrladdr& addr);
    void set_element_expiration(Element&, std::chrono::steady_clock::duration);

private: // private data
    vector_t pinnedRequests;
    WakeupTime wakeup_tp;
};
}

// "export" from namespace
using WSConnectionSchedule = connection_schedule::WSConnectionSchedule;
#endif
