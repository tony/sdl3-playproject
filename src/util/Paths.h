#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Paths {

inline std::string fileStem(std::string_view p) {
  namespace fs = std::filesystem;
  try {
    return fs::path(p).stem().string();
  } catch (...) {
    return std::string(p);
  }
}

inline bool pathExists(const std::filesystem::path& p) {
  std::error_code ec;
  return std::filesystem::exists(p, ec) && !ec;
}

inline std::string resolveAssetPath(std::string_view relativePath, const char* argv0 = nullptr) {
  namespace fs = std::filesystem;

  fs::path rel(relativePath);
  if (pathExists(rel)) {
    return rel.string();
  }

  if ((argv0 != nullptr) && (*argv0 != 0)) {
    std::error_code ec;
    fs::path exe = fs::absolute(fs::path(argv0), ec);
    if (!ec) {
      fs::path base = exe.parent_path();

      fs::path candidate = base / rel;
      if (pathExists(candidate)) {
        return candidate.lexically_normal().string();
      }

      candidate = base / ".." / rel;
      if (pathExists(candidate)) {
        return candidate.lexically_normal().string();
      }
    }
  }

  const char* basePathC = SDL_GetBasePath();
  if ((basePathC != nullptr) && (*basePathC != 0)) {
    fs::path basePath(basePathC);

    fs::path candidate = basePath / rel;
    if (pathExists(candidate)) {
      return candidate.lexically_normal().string();
    }

    candidate = basePath / ".." / rel;
    if (pathExists(candidate)) {
      return candidate.lexically_normal().string();
    }
  }

  return rel.string();
}

inline std::vector<std::string> listTomlFiles(std::string_view dirPath) {
  namespace fs = std::filesystem;
  std::vector<std::string> out;

  std::error_code ec;
  for (const auto& e : fs::directory_iterator(fs::path(dirPath), ec)) {
    if (ec) {
      break;
    }
    if (!e.is_regular_file()) {
      continue;
    }
    const fs::path& p = e.path();
    if (p.extension() != ".toml") {
      continue;
    }
    out.push_back(p.string());
  }

  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
    const std::string sa = fileStem(a);
    const std::string sb = fileStem(b);
    if (sa != sb) {
      return sa < sb;
    }
    return a < b;
  });

  return out;
}

}  // namespace Paths
