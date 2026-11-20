#include "params.h"
#include "raylib.h"
#include <cstdio>
#include <cctype>
#include <cstdlib>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Params::load(const std::string& path) {
    path_ = path;
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        TraceLog(LOG_WARNING, "PARAMS: %s not found -- built-in defaults are used", path.c_str());
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
    fclose(fp);
    parse(text);
    mtime_ = GetFileModTime(path.c_str());
    TraceLog(LOG_INFO, "PARAMS: %d value(s) read from %s", (int)kv_.size(), path.c_str());
    return true;
}

bool Params::reloadIfChanged() {
    if (path_.empty() || !FileExists(path_.c_str())) return false;
    long m = GetFileModTime(path_.c_str());
    if (m == mtime_) return false;
    return load(path_);
}

float Params::f(const char* key, float def) const {
    auto it = kv_.find(key);
    if (it == kv_.end()) return def;
    char* end = nullptr;
    float v = strtof(it->second.c_str(), &end);
    return (end == it->second.c_str()) ? def : v;
}

bool Params::b(const char* key, bool def) const {
    auto it = kv_.find(key);
    if (it == kv_.end()) return def;
    std::string v = it->second;
    for (auto& c : v) c = (char)tolower((unsigned char)c);
    if (v == "true" || v == "yes" || v == "on") return true;
    if (v == "false" || v == "no" || v == "off") return false;
    char* end = nullptr;
    float n = strtof(v.c_str(), &end);
    return (end == v.c_str()) ? def : n != 0;
}

std::string Params::s(const char* key, const std::string& def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : it->second;
}

void Params::parse(const std::string& text) {
    raw_ = text;
    kv_.clear();
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string l = text.substr(at, end - at);
        at = end + 1;
        size_t hash = l.find('#');
        if (hash != std::string::npos) l.resize(hash);
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        if (!k.empty() && !v.empty()) kv_[k] = v;
    }
}

void Params::loadText(const std::string& text, const char* label) {
    path_.clear();
    parse(text);
    TraceLog(LOG_INFO, "PARAMS: %d value(s) read from %s", (int)kv_.size(), label);
}
