// relays tcp text lines to the knc pilot pipe inside wine since linux python cannot open wine named pipes

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIPE_NAME L"\\\\.\\pipe\\knc_pilot"
#define LINE_MAX_LEN 4096
#define REPLY_MAX 16384

static int g_warp = 1;

static int send_all(SOCKET s, const char *p, int n)
{
    while (n > 0) {
        int k = send(s, p, n, 0);
        if (k <= 0) return 0;
        p += k;
        n -= k;
    }
    return 1;
}

struct Pick {
    HWND best;
    long area;
};

static BOOL CALLBACK pick_window(HWND h, LPARAM lp)
{
    struct Pick *pick = (struct Pick *)lp;
    wchar_t title[64];
    RECT rc;
    long area;
    if (!IsWindowVisible(h) || !GetWindowTextW(h, title, 64) || wcscmp(title, L"Chibi Kart") != 0) return TRUE;
    GetClientRect(h, &rc);
    area = (long)(rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > pick->area) {
        pick->best = h;
        pick->area = area;
    }
    return TRUE;
}

// the game window is the largest one titled Chibi Kart since its message boxes carry the same title
static HWND client_window(void)
{
    struct Pick pick = {NULL, 0};
    EnumWindows(pick_window, (LPARAM)&pick);
    return pick.best;
}

// moves the real cursor on the client point so hover and click agree with the pinned cursor
static void warp_to(int x, int y, char *reply, int cap)
{
    HWND h = client_window();
    POINT p;
    if (!h) {
        snprintf(reply, cap, "err no client window");
        return;
    }
    p.x = x;
    p.y = y;
    ClientToScreen(h, &p);
    SetCursorPos(p.x, p.y);
    snprintf(reply, cap, "ok warp %d %d screen %ld %ld", x, y, p.x, p.y);
}

// the client area in screen pixels so a screen grab can be cropped to it
static void client_rect(char *reply, int cap)
{
    HWND h = client_window();
    RECT rc;
    POINT p = {0, 0};
    if (!h) {
        snprintf(reply, cap, "err no client window");
        return;
    }
    GetClientRect(h, &rc);
    ClientToScreen(h, &p);
    snprintf(reply, cap, "ok %ld %ld %ld %ld", p.x, p.y, rc.right - rc.left, rc.bottom - rc.top);
}

// one pipe transaction since the pilot serves one line per connection then disconnects
static void pipe_call(const char *line, char *reply, int cap)
{
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD start = GetTickCount();
    DWORD n = 0;
    int got = 0;
    char out[LINE_MAX_LEN + 2];

    for (;;) {
        DWORD e;
        h = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        e = GetLastError();
        if (GetTickCount() - start > 3000) {
            snprintf(reply, cap, "err pipe %lu", e);
            return;
        }
        if (e == ERROR_PIPE_BUSY) WaitNamedPipeW(PIPE_NAME, 500);
        else Sleep(100);
    }

    snprintf(out, sizeof(out), "%s\n", line);
    if (!WriteFile(h, out, (DWORD)strlen(out), &n, NULL)) {
        snprintf(reply, cap, "err pipe write %lu", GetLastError());
        CloseHandle(h);
        return;
    }
    while (got < cap - 1) {
        DWORD r = 0;
        if (!ReadFile(h, reply + got, (DWORD)(cap - 1 - got), &r, NULL) || r == 0) break;
        got += (int)r;
        if (memchr(reply + got - r, '\n', r)) break;
    }
    reply[got] = 0;
    CloseHandle(h);
    while (got > 0 && (reply[got - 1] == '\n' || reply[got - 1] == '\r')) reply[--got] = 0;
    if (!got) snprintf(reply, cap, "err pipe no reply");
}

// these verbs run on the pilot pipe thread every other verb waits for the game thread
static int pipe_thread_verb(const char *line)
{
    static const char *const kVerbs[] = {"waitstage", "shot", "hold", "release", "holdmask", "held", "car", NULL};
    size_t n = strcspn(line, " ");
    int i;
    for (i = 0; kVerbs[i]; ++i)
        if (strlen(kVerbs[i]) == n && strncmp(line, kVerbs[i], n) == 0) return 1;
    return 0;
}

// the key poll hook counter grows once per frame so two reads tell whether frames run
static long key_poll_calls(void)
{
    char r[1024];
    const char *p;
    pipe_call("held", r, sizeof(r));
    p = strstr(r, " calls=");
    return p ? atol(p + 7) : -1;
}

// second guard only the pilot dll now owns a shared command so a timeout cannot dangle
static int game_thread_live(int max_ms)
{
    DWORD start = GetTickCount();
    long a = key_poll_calls();
    for (;;) {
        long b;
        Sleep(120);
        b = key_poll_calls();
        if (a >= 0 && b > a) return 1;
        if ((int)(GetTickCount() - start) > max_ms) return 0;
        a = b;
    }
}

static int g_live_ms = 20000;

// the bridge verbs never reach the client and the mouse verbs warp the real cursor first
static void handle_line(const char *line, char *reply, int cap)
{
    int x = 0, y = 0;
    if (strncmp(line, "bridge ", 7) == 0 || strcmp(line, "bridge") == 0) {
        const char *a = line + (line[6] ? 7 : 6);
        if (strncmp(a, "rect", 4) == 0) client_rect(reply, cap);
        else if (sscanf(a, "warp %d %d", &x, &y) == 2) warp_to(x, y, reply, cap);
        else snprintf(reply, cap, "ok bridge");
        return;
    }
    if (g_warp && (sscanf(line, "click %d %d", &x, &y) == 2 || sscanf(line, "rclick %d %d", &x, &y) == 2 ||
                   sscanf(line, "move %d %d", &x, &y) == 2)) {
        char ignored[128];
        warp_to(x, y, ignored, sizeof(ignored));
    }
    if (g_live_ms > 0 && !pipe_thread_verb(line) && !game_thread_live(g_live_ms)) {
        snprintf(reply, cap, "err game thread ran no frame for %d ms, verb not sent", g_live_ms);
        return;
    }
    pipe_call(line, reply, cap);
}

static DWORD WINAPI serve(LPVOID arg)
{
    SOCKET s = (SOCKET)(ULONG_PTR)arg;
    static const int kBuf = LINE_MAX_LEN * 2;
    char *buf = (char *)malloc(kBuf);
    char *reply = (char *)malloc(REPLY_MAX + 2);
    int have = 0;

    for (;;) {
        char *nl;
        int k = recv(s, buf + have, kBuf - 1 - have, 0);
        if (k <= 0) break;
        have += k;
        buf[have] = 0;
        while ((nl = (char *)memchr(buf, '\n', have)) != NULL) {
            int len = (int)(nl - buf);
            *nl = 0;
            if (len > 0 && buf[len - 1] == '\r') buf[len - 1] = 0;
            if (buf[0]) {
                int n;
                handle_line(buf, reply, REPLY_MAX);
                n = (int)strlen(reply);
                reply[n++] = '\n';
                if (!send_all(s, reply, n)) goto done;
            }
            have -= len + 1;
            memmove(buf, nl + 1, have);
        }
        if (have >= kBuf - 1) have = 0;
    }
done:
    closesocket(s);
    free(buf);
    free(reply);
    return 0;
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    SOCKET ls;
    struct sockaddr_in addr;
    int port = argc > 1 ? atoi(argv[1]) : 47017;
    const char *warp = getenv("KNC_BRIDGE_WARP");
    int one = 1;

    const char *live = getenv("KNC_BRIDGE_LIVE_MS");

    if (warp && warp[0] == '0') g_warp = 0;
    if (live) g_live_ms = atoi(live);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(ls, 8) != 0) {
        fprintf(stderr, "pilot bridge cannot listen on %d error %d\n", port, WSAGetLastError());
        return 1;
    }
    printf("pilot bridge listening on 127.0.0.1:%d warp=%d\n", port, g_warp);
    fflush(stdout);
    for (;;) {
        SOCKET c = accept(ls, NULL, NULL);
        HANDLE t;
        if (c == INVALID_SOCKET) continue;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
        t = CreateThread(NULL, 0, serve, (LPVOID)(ULONG_PTR)c, 0, NULL);
        if (t) CloseHandle(t);
        else closesocket(c);
    }
    return 0;
}
