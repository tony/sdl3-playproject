import 'vars.just'

# Run CTest test suite
[group: 'test']
test:
    just compiler={{compiler}} type={{type}} build_dir={{build_dir}} build::build
    ctest --test-dir {{build_dir}} --output-on-failure
