#pragma once

#include <cstdio>
#include <format>
#include <initializer_list>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.h>

namespace TomlUtil {

inline int& warningCounter() {
  static int counter = 0;
  return counter;
}

inline void resetWarningCount() {
  warningCounter() = 0;
}

inline int warningCount() {
  return warningCounter();
}

inline void warnLine(std::string_view prefix, std::string_view message) {
  static constexpr std::string_view kWarningPrefix = ": warning: ";
  (void)std::fwrite(prefix.data(), 1, prefix.size(), stderr);
  (void)std::fwrite(kWarningPrefix.data(), 1, kWarningPrefix.size(), stderr);
  (void)std::fwrite(message.data(), 1, message.size(), stderr);
  (void)std::fwrite("\n", 1, 1, stderr);
}

template <typename... Args>
inline void warnf(const char* path, std::format_string<Args...> fmt, Args&&... args) {
  ++warningCounter();
  const std::string message = std::format(fmt, std::forward<Args>(args)...);
  const std::string_view prefix =
      (path != nullptr) ? std::string_view{path} : std::string_view{"<toml>"};
  warnLine(prefix, message);
}

inline bool allowedKey(std::string_view key, std::initializer_list<std::string_view> allowed) {
  return std::ranges::any_of(allowed, [key](std::string_view a) { return key == a; });
}

inline void warnUnknownKeys(const toml::table& tbl,
                            const char* path,
                            std::string_view scope,
                            std::initializer_list<std::string_view> allowed) {
  for (const auto& [key, node] : tbl) {
    (void)node;
    std::string_view k = key.str();
    if (allowedKey(k, allowed)) {
      continue;
    }
    warnf(path, "unknown key '{}' in {}", k, scope);
  }
}

}  // namespace TomlUtil
