import 'vars.just'

# Run the application with optional arguments
[group: 'run']
run *args:
    just compiler={{compiler}} type={{type}} build_dir={{build_dir}} build::build
    ./{{build_dir}}/{{exe}} {{args}}

# Run 120-frame smoke test with offscreen rendering
[group: 'run']
smoke:
    just compiler={{compiler}} type={{type}} build_dir={{build_dir}} build::build
    ./{{build_dir}}/{{exe}} --frames 120 --video-driver offscreen
