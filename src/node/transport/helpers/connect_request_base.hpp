#pragma once
#include<chrono>
using steady_duration = std::chrono::steady_clock::duration;
struct ConnectRequestBase {
    const steady_duration sleptFor;
    auto inbound() const { return sleptFor.count() < 0; }
};
