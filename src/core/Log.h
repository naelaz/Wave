#pragma once

#include <string_view>

namespace wave {

// Minimal logging utility.
// Writes to OutputDebugString in Debug builds.
// Future: swap in spdlog or file logging without changing call sites.
namespace log {

void init();
void shutdown();

void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);

} // namespace log
} // namespace wave
