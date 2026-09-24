#include "RegisterWeb.h"

#include "../../lib/crypto/PasswordHash.h"
#include "db/Database.h"
#include "logging/Logger.h"

#include <httplib.h>

#include <atomic>
#include <cctype>
#include <memory>
#include <thread>

namespace knc {
namespace {

std::unique_ptr<httplib::Server> g_server;
std::thread g_thread;
std::atomic<bool> g_running{false};

/// usernames go into the client login box and into SQL so keep them boring
bool ValidUsername(const std::string& u) {
    if (u.size() < 3 || u.size() > 16) return false;
    for (char c : u) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

/// eight is the floor since the login box limits typed length 32 is the ceiling on purpose
bool ValidPassword(const std::string& p) {
    if (p.size() < 8 || p.size() > 32) return false;
    for (char c : p) {
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) return false;
    }
    return true;
}

std::string Escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string Page(const std::string& site, const std::string& notice,
                 bool ok, const std::string& user) {
    std::string banner;
    if (!notice.empty()) {
        banner = "<p class=\"note ";
        banner += ok ? "ok" : "bad";
        banner += "\">" + Escape(notice) + "</p>";
    }
    return
"<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>" + Escape(site) + " &middot; Create an account</title><style>"
":root{color-scheme:light dark;--bg:#f6f7f9;--card:#fff;--ink:#15181d;--mut:#5b6472;"
"--line:#dfe3e9;--acc:#2f6fed}"
"@media(prefers-color-scheme:dark){:root{--bg:#0f1115;--card:#171a21;--ink:#e8eaee;"
"--mut:#98a1b0;--line:#262b34;--acc:#5c8dff}}"
"*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;"
"background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,Segoe UI,Roboto,sans-serif;padding:24px}"
".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:28px;"
"width:100%;max-width:380px}"
"h1{margin:0 0 4px;font-size:20px;letter-spacing:-.01em}"
"p.sub{margin:0 0 20px;color:var(--mut);font-size:13px}"
"label{display:block;font-size:12px;color:var(--mut);margin:14px 0 5px}"
"input{width:100%;padding:10px 12px;border:1px solid var(--line);border-radius:8px;"
"background:transparent;color:var(--ink);font-size:15px}"
"input:focus{outline:2px solid var(--acc);outline-offset:1px;border-color:transparent}"
"button{width:100%;margin-top:20px;padding:11px;border:0;border-radius:8px;background:var(--acc);"
"color:#fff;font-size:15px;font-weight:600;cursor:pointer}"
"button:hover{filter:brightness(1.08)}"
".note{margin:0 0 16px;padding:10px 12px;border-radius:8px;font-size:13px}"
".ok{background:#e7f6ec;color:#1a6b35}.bad{background:#fdeaea;color:#a32020}"
"@media(prefers-color-scheme:dark){.ok{background:#12301c;color:#7fd7a0}"
".bad{background:#361618;color:#ef9a9a}}"
".hint{margin:18px 0 0;color:var(--mut);font-size:12px;text-align:center}"
"</style></head><body><form class=\"card\" method=\"post\" action=\"/register\">"
"<h1>" + Escape(site) + "</h1><p class=\"sub\">Create an account to play.</p>"
+ banner +
"<label for=\"u\">Username</label>"
"<input id=\"u\" name=\"username\" value=\"" + Escape(user) + "\" autocomplete=\"username\" "
"minlength=\"3\" maxlength=\"16\" pattern=\"[A-Za-z0-9_]+\" required autofocus>"
"<label for=\"p\">Password</label>"
"<input id=\"p\" name=\"password\" type=\"password\" autocomplete=\"new-password\" "
"minlength=\"8\" maxlength=\"32\" required>"
"<label for=\"c\">Repeat password</label>"
"<input id=\"c\" name=\"confirm\" type=\"password\" autocomplete=\"new-password\" "
"minlength=\"8\" maxlength=\"32\" required>"
"<button type=\"submit\">Create account</button>"
"<p class=\"hint\">Name 3 to 16 letters, digits or underscore. Password 8 or more. "
"Then start the game and log in.</p>"
"</form></body></html>";
}

}  // namespace

void StartRegisterWeb(uint16_t port, const std::string& siteName) {
    if (g_running.exchange(true)) return;

    g_server = std::make_unique<httplib::Server>();

    g_server->Get("/", [siteName](const httplib::Request&, httplib::Response& res) {
        res.set_content(Page(siteName, "", false, ""), "text/html; charset=utf-8");
    });

    g_server->Post("/register", [siteName](const httplib::Request& req, httplib::Response& res) {
        const std::string user    = req.has_param("username") ? req.get_param_value("username") : "";
        const std::string pass    = req.has_param("password") ? req.get_param_value("password") : "";
        const std::string confirm = req.has_param("confirm")  ? req.get_param_value("confirm")  : "";

        auto fail = [&](const std::string& why) {
            res.status = 200;
            res.set_content(Page(siteName, why, false, user), "text/html; charset=utf-8");
        };

        if (!ValidUsername(user)) return fail("Username must be 3 to 16 letters, digits or underscore.");
        if (!ValidPassword(pass)) return fail("Password must be 8 to 32 ordinary characters.");
        if (pass == user)         return fail("The password cannot be the username.");
        if (pass != confirm)      return fail("The two passwords do not match.");

        auto& db = Database::instance();
        auto taken = db.queryPrepared("SELECT id FROM accounts WHERE username = ? LIMIT 1", {user});
        if (!taken.empty()) return fail("That username is already taken.");

        std::string hash;
        try {
            hash = PasswordHash::hash(pass);
        } catch (const std::exception& e) {
            LOG_ERROR("REGWEB", std::string("hash failed: ") + e.what());
            return fail("Something went wrong on our side. Try again.");
        }

        if (!db.executePrepared("INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
                                {user, hash})) {
            LOG_ERROR("REGWEB", "insert failed for " + user);
            return fail("Could not create the account. Try again.");
        }

        LOG_INFO("REGWEB", "account created: " + user);
        res.status = 200;
        res.set_content(Page(siteName, "Account created. Start the game and log in as " + user + ".",
                             true, ""),
                        "text/html; charset=utf-8");
    });

    g_thread = std::thread([port]() {
        LOG_INFO("REGWEB", "signup page on http://0.0.0.0:" + std::to_string(port));
        // a page that fails to bind must never take the login server down with it
        if (!g_server->listen("0.0.0.0", port)) {
            LOG_WARN("REGWEB", "could not bind port " + std::to_string(port) +
                               ", signup page is off");
        }
    });
}

void StopRegisterWeb() {
    if (!g_running.exchange(false)) return;
    if (g_server) g_server->stop();
    if (g_thread.joinable()) g_thread.join();
    g_server.reset();
}

}  // namespace knc
