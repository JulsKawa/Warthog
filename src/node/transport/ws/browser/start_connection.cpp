#include "../start_connection.hpp"
#include "connection.hpp"
#include "emscripten.h"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <iostream>
using namespace std;

void start_connection(const WSConnectRequest& r)
{
    auto pq { new emscripten::ProxyingQueue };
    pq->proxyAsync(emscripten_main_runtime_thread_id(), [pq, r]() {
        auto p { WSConnection::make_new(r) };
        if (!p) {
            cout << "Websocket connection failed" << endl;
            global().core->on_failed_connect(r, Error(ESTARTWEBSOCK));
        }
        p->start_read();
        delete pq;
    });
}
