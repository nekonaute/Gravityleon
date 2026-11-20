#pragma once
#include <string>
#include <unordered_map>

// params.txt: "key = value" lines, '#' starts a comment. Read at startup and again whenever
// the file changes on disk, so the feel of Leon can be tuned while the game runs.
class Params {
public:
    bool load(const std::string& path);
    // From text instead of a file (a bug report's copy of params.txt); no live reloading then.
    void loadText(const std::string& text, const char* label);
    const std::string& raw() const { return raw_; }   // the text in effect, as read
    bool reloadIfChanged();               // true when the file was re-read
    float f(const char* key, float def) const;
    bool b(const char* key, bool def) const;   // True/False, yes/no, on/off, 1/0
    std::string s(const char* key, const std::string& def) const;

private:
    std::string path_;
    long mtime_ = 0;
    std::unordered_map<std::string, std::string> kv_;
    std::string raw_;
    void parse(const std::string& text);
};
