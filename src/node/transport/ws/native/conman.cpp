#include "conman.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
#include "libwebsockets.h"
#include "transport/ws/native/connection.hpp"
#include "ws_session.hpp"
#include <cassert>
#include <csignal>
#include <cstring>
#include <list>
#include <memory>
#include <span>
using namespace std;
WSConnectionManager::WSConnectionManager(PeerServer& peerServer, uint16_t port)
    : peerServer(peerServer)
    , startOptions { port }
{
}

WSConnectionManager::~WSConnectionManager()
{
    wait_for_shutdown();
}

void WSConnectionManager::wait_for_shutdown(){
    if (worker.joinable()) {
        worker.join();
    }
}
void WSConnectionManager::start()
{
    worker = std::thread([&]() { work(); });
}


static int libwebsocket_callback(struct lws* wsi,
    lws_callback_reasons reason,
    void* /*user*/, void* in, size_t len)
{

    auto conman = [&]() -> WSConnectionManager& {
        return *reinterpret_cast<WSConnectionManager*>(lws_context_user(lws_get_context(wsi)));
    };
    auto psession
        = [&]() {
              return *reinterpret_cast<std::shared_ptr<WSSession>*>(
                  lws_get_opaque_user_data(wsi));
          };

    switch (reason) {

    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        break;
    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED: { // called on incoming
        auto fd { lws_get_socket_fd(wsi) };
        // lws_get_peer_addresses()
        assert(fd > 0);
        sockaddr_storage sa;
        socklen_t len(sizeof(sa));
        int r(getpeername(fd, (sockaddr*)&sa, &len));
        if (r != 0 || sa.ss_family != AF_INET){
            return -1;
        }

        lws_rx_flow_control(wsi, 0);
        // lws_get_peer_addresses()
        sockaddr_in* addr_i4 = (struct sockaddr_in*)&sa;
        WSSockaddr wsaddr(IPv4(ntoh32(uint32_t(addr_i4->sin_addr.s_addr))), addr_i4->sin_port);
        auto p {
            new std::shared_ptr<WSSession>(WSSession::make_new(false, wsi))
        };
        lws_set_opaque_user_data(wsi, p);
        auto& session { *p };
        session->connection = WSConnection::make_new(session, WSConnectRequest::make_inbound(wsaddr), conman());
        global().peerServer->authenticate(session->connection);
    } break;
    case LWS_CALLBACK_WSI_CREATE:
        break;
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;

    case LWS_CALLBACK_ESTABLISHED:
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        psession()->on_connected();
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        return psession()->write();

    case LWS_CALLBACK_RECEIVE:
    case LWS_CALLBACK_CLIENT_RECEIVE:
        return psession()->receive({ (uint8_t*)in, len });

    case LWS_CALLBACK_CLOSED:
        break;

    case LWS_CALLBACK_WSI_DESTROY: {
        auto p { reinterpret_cast<std::shared_ptr<WSSession>*>(
            lws_get_opaque_user_data(wsi)) };
        if (p) {
            (*p)->on_close(EWEBSOCK);
            delete p;
        }
    } break;
    default:
        break;
    }

    return 0;
}


void WSConnectionManager::handle_event(Shutdown&&)
{
    _shutdown = true;
}

void WSConnectionManager::handle_event(Connect&& c)
{
    auto& cr { c.conreq };
    auto ipstr { cr.address.ip.to_string() };

    struct lws_client_connect_info i;
    memset(&i, 0, sizeof(i));

    i.context = context;
    i.port = cr.address.port;
    i.address = ipstr.c_str();
    i.path = "/";
    i.host = i.address;
    i.origin = "";
    i.ssl_connection = 0;
    i.protocol = "binary";
    i.local_protocol_name = "binary";

    auto* p = new std::shared_ptr<WSSession>(WSSession::make_new(false));
    auto& session { *p };
    i.pwsi = &(session->wsi);
    i.opaque_user_data = p;

    if (!lws_client_connect_via_info(&i)) {
        global().core->on_failed_connect(c.conreq, Error(ESTARTWEBSOCK));
        return;
    }
    session->connection = WSConnection::make_new(session, c.conreq, *this);
};

void WSConnectionManager::handle_event(Send&& s)
{
    if (auto p { s.session.lock() }; p)
        p->queue_write({ std::move(s.data), s.size });
}

void WSConnectionManager::handle_event(Close&& c)
{
    auto p { c.session.lock() };
    if (!p)
        return;
    WSSession& session { *p };
    session.close(c.reason);
}

void WSConnectionManager::handle_event(StartRead&& c)
{
    if (auto p { c.session.lock() }; p) {
        lws_rx_flow_control(p->wsi, 1);
    }
}

void WSConnectionManager::process_events()
{
    decltype(events) tmp;
    {
        std::lock_guard l(m);
        std::swap(tmp, events);
    }

    for (auto& e : tmp) {
        std::visit([&](auto&& event) {
            handle_event(std::move(event));
        },
            std::move(e));
    }
}

void WSConnectionManager::push_event(Event e)
{
    std::lock_guard l(m);
    events.push_back(std::move(e));
    wakeup();
}

void WSConnectionManager::wakeup()
{
    if (context) {
        lws_cancel_service(context);
    }
}

namespace {
const lws_protocols protocols[] = {
    { "binary", libwebsocket_callback,
        sizeof(std::shared_ptr<WSSession>), 1024, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};
const lws_protocol_vhost_options pvo = {
    NULL, /* "next" pvo linked-list */
    nullptr, /* "child" pvo linked-list */
    "binary", /* protocol name we belong to on this vhost */
    "" /* ignored */
};
}

void WSConnectionManager::create_context()
{
    int logs = 0; // LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    lws_set_log_level(logs, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = startOptions.port;
    info.protocols = protocols;
    info.pvo = &pvo;
    info.user = this;
    info.pt_serv_buf_size = 32 * 1024;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    std::lock_guard l(m);
    assert(context == nullptr);
    auto ctx { lws_create_context(&info) };
    assert(ctx);
    context = ctx;
}

void WSConnectionManager::work()
{
    spdlog::info("Starting websocket endpoint on port {}", startOptions.port);
    create_context();
    while (true) {
        assert(lws_service(context, 0) >= 0);
        process_events();
        if (_shutdown) {
            break;
        }
    }
    // for (auto *p : sessions) {
    //     (*p)->on_close(EWEBSOCK);
    //     delete p;
    // }
    // sessions.clear();
    lws_context_destroy(context);
    context = nullptr;
}
