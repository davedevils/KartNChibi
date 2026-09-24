#include "sink.h"
#include "jsonl.h"
#include "poll.h"
#include "bp_command.h"
#include "bp_engine.h"
#include "bp_queue.h"
#include "bp_serialize.h"
#include "ghidra_client.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

namespace probe {

int drain_once(RingBuffer& ring, const ProbeConfig& cfg,
               const std::function<void(const std::string&)>& writer, int budget) {
    int drained = 0;
    ProbeRecord rec;
    while (drained < budget && ring.try_pop(rec)) {
        writer(serialize_record(rec, cfg));
        ++drained;
    }
    return drained;
}

namespace {
std::atomic<bool> g_running{false};
std::thread g_thread;
std::mutex g_clients_mutex;
std::vector<SOCKET> g_clients;
SOCKET g_listen = INVALID_SOCKET;
bool g_winsock_up = false;
std::atomic<BpQueue*> g_bp_queue_ptr{nullptr};

void send_reply(SOCKET src, const char* msg) {
    send(src, msg, static_cast<int>(strlen(msg)), 0);
}

void dispatch_line(SOCKET src, const std::string& line) {
    if (line == "snap") { request_snap(); return; }
    const BpCommand cmd = parse_bp_command(line);
    switch (cmd.kind) {
        case BpCmdKind::Arm:
            send_reply(src, bp_arm(cmd.addr, cmd.suspend, cmd.int3) ? "ok\n" : "err\n");
            break;
        case BpCmdKind::Remove:
            send_reply(src, bp_disarm(cmd.addr) ? "ok\n" : "err\n");
            break;
        case BpCmdKind::Resume:
            bp_resume(cmd.addr, cmd.resume_all);
            send_reply(src, "ok\n");
            break;
        case BpCmdKind::List: {
            const std::string reply = bp_list();
            send(src, reply.c_str(), static_cast<int>(reply.size()), 0);
            break;
        }
        default: break;
    }
}

void accept_pending() {
    if (g_listen == INVALID_SOCKET) return;
    for (;;) {
        SOCKET c = accept(g_listen, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        u_long nb = 1; ioctlsocket(c, FIONBIO, &nb);
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        g_clients.push_back(c);
    }
    // reads and dispatches line separated commands from each client without blocking
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    char buf[256];
    for (SOCKET s : g_clients) {
        const int n = recv(s, buf, sizeof buf - 1, 0);
        if (n <= 0) continue;
        buf[n] = '\0';
        // splits on newlines and dispatches each non empty line
        std::string view(buf, n);
        size_t start = 0;
        for (size_t pos = 0; pos <= view.size(); ++pos) {
            if (pos == view.size() || view[pos] == '\n') {
                if (pos > start) {
                    size_t end = pos;
                    if (end > start && view[end - 1] == '\r') --end;
                    dispatch_line(s, view.substr(start, end - start));
                }
                start = pos + 1;
            }
        }
    }
}

// best effort tap a client that cannot take a whole line at once is dropped since it can reconnect
void broadcast(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    const int len = static_cast<int>(line.size());
    for (size_t i = 0; i < g_clients.size();) {
        const int sent = send(g_clients[i], line.c_str(), len, 0);
        if (sent < len) {
            closesocket(g_clients[i]);
            g_clients.erase(g_clients.begin() + i);
        } else {
            ++i;
        }
    }
}

void open_listener(uint16_t port) {
    if (port == 0) return;
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) return;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(g_listen, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == SOCKET_ERROR ||
        listen(g_listen, 4) == SOCKET_ERROR) {
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
        return;
    }
    u_long nb = 1; ioctlsocket(g_listen, FIONBIO, &nb);
}
}  // namespace

void start_sink(RingBuffer& ring, const ProbeConfig& cfg) {
    if (g_running.exchange(true)) return;
    WSADATA wsa;
    g_winsock_up = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    if (!g_winsock_up) {
        OutputDebugStringA("hbo_probe: WSAStartup failed; TCP tap disabled, file sink only\n");
    }
    g_thread = std::thread([&ring, &cfg] {
        if (g_winsock_up) open_listener(cfg.tcp_port);
        std::ofstream file(cfg.jsonl_path, std::ios::app | std::ios::binary);
        uint64_t last_dropped = 0;
        while (g_running.load(std::memory_order_acquire)) {
            accept_pending();
            const uint64_t now_dropped = ring.dropped();
            if (now_dropped > last_dropped) {
                const std::string ctrl = "{\"control\":\"dropped\",\"count\":" +
                                         std::to_string(now_dropped - last_dropped) + "}";
                file << ctrl << '\n';
                file.flush();  // drop notice must reach the authoritative log even at low traffic
                broadcast(ctrl + "\n");
                last_dropped = now_dropped;
            }
            int n = drain_once(ring, cfg, [&](const std::string& line) {
                file << line << '\n';
                broadcast(line + "\n");
            }, 256);
            // drains bp hits writes to jsonl broadcasts to tap and annotates ghidra best effort
            BpQueue* bp_queue = g_bp_queue_ptr.load(std::memory_order_acquire);
            if (bp_queue) {
                BpHit hit{};
                while (bp_queue->try_pop(hit)) {
                    const std::string line = serialize_bp_hit(hit);
                    file << line << '\n';
                    broadcast(line + "\n");
                    char comment[64];
                    std::snprintf(comment, sizeof comment, "bp hit eax=0x%08x eip=0x%08x",
                                  hit.regs[0], hit.eip);
                    ghidra_annotate(hit.addr, comment);  // best effort failure ignored
                    ++n;
                }
            }
            if (n) file.flush();
            else std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
}

void stop_sink() {
    if (!g_running.exchange(false)) return;
    if (g_thread.joinable()) g_thread.join();
    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        for (SOCKET c : g_clients) closesocket(c);
        g_clients.clear();
        if (g_listen != INVALID_SOCKET) { closesocket(g_listen); g_listen = INVALID_SOCKET; }
    }
    if (g_winsock_up) { WSACleanup(); g_winsock_up = false; }
}

void sink_set_bp_queue(BpQueue* queue) {
    g_bp_queue_ptr.store(queue, std::memory_order_release);
}

}  // namespace probe
