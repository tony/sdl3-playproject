# Root justfile: module index + convenience aliases
import 'vars.just'
mod build
mod app
mod test
mod lint
mod format
mod check

# Aliases for common commands
alias b := build::build
alias r := run
alias t := test::test
alias c := check::check

# Default recipe (shows modules + recipes)
default:
    @just --list --list-submodules

[group: 'run']
run *args:
    just compiler={{compiler}} type={{type}} build_dir={{build_dir}} exe={{exe}} app::run {{args}}

[group: 'run']
smoke:
    just compiler={{compiler}} type={{type}} build_dir={{build_dir}} exe={{exe}} app::smoke
