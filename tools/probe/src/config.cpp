#include "config.h"
#include "probe_record.h"
#include <sstream>

namespace probe {

uint32_t rebase_address(uint32_t config_addr, uint32_t config_base, uint32_t runtime_base) {
    return config_addr - config_base + runtime_base;
}

static uint32_t parse_u32(const std::string& v) {
    return static_cast<uint32_t>(std::stoul(v, nullptr, 0));  // accepts hex with 0x prefix or decimal
}

// splits key value tokens of a line into ordered pairs
static std::vector<std::pair<std::string, std::string>> tokens(const std::string& line) {
    std::vector<std::pair<std::string, std::string>> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        const size_t eq = tok.find('=');
        if (eq == std::string::npos) continue;
        out.emplace_back(tok.substr(0, eq), tok.substr(eq + 1));
    }
    return out;
}

static bool parse_source(const std::string& v, SourceKind& kind, uint32_t& index) {
    if (v == "eax") { kind = SourceKind::Eax; return true; }
    if (v == "ecx") { kind = SourceKind::Ecx; return true; }
    if (v == "edx") { kind = SourceKind::Edx; return true; }
    if (v.rfind("arg", 0) == 0) { kind = SourceKind::Arg; index = parse_u32(v.substr(3)); return true; }
    if (v.rfind("esp+", 0) == 0) { kind = SourceKind::EspOff; index = parse_u32(v.substr(4)); return true; }
    if (v.rfind("abs:", 0) == 0) { kind = SourceKind::Abs; index = parse_u32(v.substr(4)); return true; }
    return false;
}

static void parse_deref(const std::string& v, std::vector<uint32_t>& out) {
    std::istringstream ss(v);
    std::string part;
    while (std::getline(ss, part, ',')) if (!part.empty()) out.push_back(parse_u32(part));
}

bool parse_config(const std::string& text, ProbeConfig& out, std::string& error) {
    std::istringstream lines(text);
    std::string line;
    int lineno = 0;
    while (std::getline(lines, line, '\n')) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        try {
            const bool is_hook_line = line.rfind("hook=", 0) == 0;
            const bool is_poll_line = line.rfind("poll=", 0) == 0;
            const bool is_capture_line = line.rfind("capture=", 0) == 0;
            if (is_hook_line || is_poll_line || is_capture_line) {
                if (is_capture_line && out.hooks.empty()) {
                    error = "capture before any hook at line " + std::to_string(lineno);
                    return false;
                }
                if (is_hook_line || is_poll_line) {
                    HookDesc h;
                    h.is_poll = is_poll_line;
                    if (is_poll_line) h.dir = 2;
                    for (const auto& kv : tokens(line)) {
                        if (kv.first == "hook" || kv.first == "poll") h.name = kv.second;
                        else if (kv.first == "addr") h.address = parse_u32(kv.second);
                        else if (kv.first == "dir") h.dir = (kv.second == "out") ? 1 : (kv.second == "state") ? 2 : 0;
                        else if (kv.first == "interval_ms") h.interval_ms = parse_u32(kv.second);
                    }
                    if (h.is_poll) h.dir = 2;  // poll is always state so any dir token is ignored
                    out.hooks.push_back(h);
                } else {
                    CaptureDesc c;
                    for (const auto& kv : tokens(line)) {
                        if (kv.first == "capture") c.label = kv.second;
                        else if (kv.first == "src") {
                            if (!parse_source(kv.second, c.source, c.source_index)) {
                                error = "bad src at line " + std::to_string(lineno);
                                return false;
                            }
                        } else if (kv.first == "deref") parse_deref(kv.second, c.deref);
                        else if (kv.first == "size") c.size = parse_u32(kv.second);
                        else if (kv.first == "as") c.mode = (kv.second == "val") ? CaptureMode::Val : CaptureMode::Ptr;
                    }
                    if (out.hooks.back().is_poll && c.source != SourceKind::Abs) {
                        error = "poll capture must use src=abs: at line " + std::to_string(lineno);
                        return false;
                    }
                    if (static_cast<int>(out.hooks.back().captures.size()) >= kMaxCapturesPerHook) {
                        error = "hook '" + out.hooks.back().name + "' exceeds kMaxCapturesPerHook at line " + std::to_string(lineno);
                        return false;
                    }
                    if ((c.source == SourceKind::Arg && c.source_index > 32) ||
                        (c.source == SourceKind::EspOff && c.source_index > 4096)) {
                        error = "out-of-range source index at line " + std::to_string(lineno);
                        return false;
                    }
                    out.hooks.back().captures.push_back(c);
                }
                continue;
            }

            const size_t eq = line.find('=');
            if (eq == std::string::npos) { error = "bad line " + std::to_string(lineno); return false; }
            const std::string key = line.substr(0, eq), val = line.substr(eq + 1);
            if (key == "module") out.module = val;
            else if (key == "image_base") out.image_base = parse_u32(val);
            else if (key == "jsonl") out.jsonl_path = val;
            else if (key == "tcp_port") out.tcp_port = static_cast<uint16_t>(parse_u32(val));
            else if (key == "ring_capacity") out.ring_capacity = parse_u32(val);
            else if (key == "max_capture_bytes") out.max_capture_bytes = parse_u32(val);
        } catch (const std::exception&) {
            error = "bad number at line " + std::to_string(lineno);
            return false;
        }
    }

    if (out.module.empty()) { error = "missing required field: module"; return false; }
    if (out.hooks.empty()) { error = "no hooks defined"; return false; }
    return true;
}

void rebase_abs_captures(ProbeConfig& cfg, uint32_t runtime_base) {
    for (HookDesc& hook : cfg.hooks)
        for (CaptureDesc& cap : hook.captures)
            if (cap.source == SourceKind::Abs)
                cap.source_index = rebase_address(cap.source_index, cfg.image_base, runtime_base);
}

}  // namespace probe
