# Default variables (override: `just build compiler=gcc`, `just run args="--no-ui"`)
compiler := "clang"
type := "Debug"
build_dir := "build"
exe := "sandbox"

# Aliases for common commands
alias b := build
alias r := run
alias t := test
alias c := check

# Default recipe (ungrouped - appears first in list)
default:
    @just --list

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

# Run the application with optional arguments
[group: 'run']
run *args: build
    ./{{build_dir}}/{{exe}} {{args}}

# Run 120-frame smoke test with offscreen rendering
[group: 'run']
smoke: build
    ./{{build_dir}}/{{exe}} --frames 120 --video-driver offscreen

# Run 120-frame smoke test without rendering
[group: 'run']
smoke-headless: build
    ./{{build_dir}}/{{exe}} --frames 120 --no-render

# Run CTest test suite
[group: 'test']
test: build
    ctest --test-dir {{build_dir}} --output-on-failure

# Validate asset configurations
[group: 'test']
validate: build
    ./{{build_dir}}/{{exe}} --validate

# Validate asset configurations (strict mode)
[group: 'test']
validate-strict: build
    ./{{build_dir}}/{{exe}} --validate --strict

# Run all linters (cppcheck + cpplint + iwyu)
[group: 'lint']
lint: cppcheck cpplint iwyu

# Run cppcheck static analysis
[group: 'lint']
cppcheck: configure
    cmake --build {{build_dir}} --target cppcheck

# Run cpplint style checker
[group: 'lint']
cpplint: configure
    cmake --build {{build_dir}} --target cpplint

# Run include-what-you-use
[group: 'lint']
iwyu: configure
    cmake --build {{build_dir}} --target iwyu

# Run clang-tidy static analysis
[group: 'lint']
tidy: configure
    cmake --build {{build_dir}} --target tidy

# Format code with clang-format
[group: 'format']
format: configure
    cmake --build {{build_dir}} --target format

# Quick verification (format + lint + test + validate)
[group: 'check']
check: format lint test validate-strict
    @echo "✅ checks passed"

# LLM-friendly verification (same as check)
[group: 'check']
check-llm: check
    @echo "✅ check-llm passed"

# Full verification including clang-tidy
[group: 'check']
check-full: format lint tidy test validate-strict
    @echo "✅ full checks passed"
