#pragma once
#include "config/config.hpp"
#include <memory>

class BatchRegistry;
class HTTPEndpoint;
class PeerServer;
class ChainServer;
class Eventloop;
class TCPConnectionManager;
class WSConnectionManager;
namespace spdlog {
class logger;
}
namespace uvw{
    class loop;
}

struct Global {
#ifndef DISABLE_LIBUV
    TCPConnectionManager* conman;
    WSConnectionManager* wsconman;
    HTTPEndpoint* httpEndpoint;
#endif
    ChainServer* chainServer;
    PeerServer* peerServer;
    Eventloop* core;
    BatchRegistry* batchRegistry;
    std::shared_ptr<spdlog::logger> connLogger;
    std::shared_ptr<spdlog::logger> syncdebugLogger;
    std::optional<Config> conf;
};
extern std::atomic<bool> shutdownSignal;

const Global& global();
inline spdlog::logger& connection_log() { return *global().connLogger; }
inline spdlog::logger& syncdebug_log() { return *global().syncdebugLogger; }
const Config& config();
int init_config(int argc, char** argv);
void start_global_services();

#ifndef DISABLE_LIBUV
HTTPEndpoint& http_endpoint();
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, TCPConnectionManager* pcm, WSConnectionManager* wcm, Eventloop* pel, HTTPEndpoint* httpEndpoint);
#else
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Eventloop* pel);
#endif
