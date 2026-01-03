import 'vars.just'

# Configure CMake build system (uses CMakePresets.json)
[group: 'build']
configure:
    cmake --preset {{ if compiler == "clang" { if type == "Release" { "release-clang" } else { "debug-clang" } } else { if type == "Release" { "release-gcc" } else { "debug-gcc" } } }}

# Configure with Clang (explicit)
[group: 'build']
configure-clang:
    cmake --preset debug-clang

# Configure with GCC (explicit)
[group: 'build']
configure-gcc:
    cmake --preset debug-gcc

# Build the project
[group: 'build']
build: configure
    cmake --build {{build_dir}}

# Remove build artifacts
[group: 'build']
clean:
    rm -rf {{build_dir}}
