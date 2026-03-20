#include "core/Log.h"

#include <Windows.h>
#include <string>

namespace wave::log {

void init() {
    info("Wave logger initialized");
}

void shutdown() {
    info("Wave logger shutting down");
}

static void output(std::string_view prefix, std::string_view msg) {
    std::string line;
    line.reserve(prefix.size() + msg.size() + 16);
    line += "[Wave:";
    line += prefix;
    line += "] ";
    line += msg;
    line += "\n";
    OutputDebugStringA(line.c_str());
}

void info(std::string_view msg)  { output("INFO",  msg); }
void warn(std::string_view msg)  { output("WARN",  msg); }
void error(std::string_view msg) { output("ERROR", msg); }

} // namespace wave::log
