import 'vars.just'

# Quick verification (format + lint + test)
[group: 'check']
check:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target format
    cmake --build {{build_dir}} --target cppcheck
    cmake --build {{build_dir}} --target cpplint
    cmake --build {{build_dir}} --target iwyu
    cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure
    @echo "checks passed"

# Full verification including clang-tidy
[group: 'check']
check-full:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target format
    cmake --build {{build_dir}} --target cppcheck
    cmake --build {{build_dir}} --target cpplint
    cmake --build {{build_dir}} --target iwyu
    cmake --build {{build_dir}} --target tidy
    cmake --build {{build_dir}}
    ctest --test-dir {{build_dir}} --output-on-failure
    @echo "full checks passed"
