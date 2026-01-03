import 'vars.just'

# Run all linters (cppcheck + cpplint + iwyu)
[group: 'lint']
lint:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target cppcheck
    cmake --build {{build_dir}} --target cpplint
    cmake --build {{build_dir}} --target iwyu

# Run cppcheck static analysis
[group: 'lint']
cppcheck:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target cppcheck

# Run cpplint style checker
[group: 'lint']
cpplint:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target cpplint

# Run include-what-you-use
[group: 'lint']
iwyu:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target iwyu

# Run clang-tidy static analysis
[group: 'lint']
tidy:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target tidy
