import 'vars.just'

# Format code with clang-format
[group: 'format']
format:
    just compiler={{compiler}} type={{type}} build::configure
    cmake --build {{build_dir}} --target format
