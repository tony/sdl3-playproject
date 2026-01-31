#!/bin/bash
# ralph.sh - Wrapper to run the Ralph Autonomous Loop with various agents

set -e

USAGE="Usage: ./ralph.sh [gemini|claude|codex] \"<instruction>\""
AGENT=${1:-gemini}
INSTRUCTION=${2:-"Check plan and execute next task"}
PROMPT_FILE="RALPH.md"

if [ ! -f "$PROMPT_FILE" ]; then
    echo "Error: $PROMPT_FILE not found in current directory."
    exit 1
fi

case "$AGENT" in
    gemini)
        echo "🤖 Starting Ralph with Gemini..."
        # Gemini CLI doesn't support -p <file> directly as a context in the same way, 
        # so we cat the file and instruction together.
        FULL_PROMPT="$(cat $PROMPT_FILE)

Task: $INSTRUCTION"
        gemini "$FULL_PROMPT"
        ;;
    claude)
        if ! command -v claude &> /dev/null; then
             echo "Error: 'claude' command not found."
             exit 1
        fi
        echo "🤖 Starting Ralph with Claude..."
        claude -p "$PROMPT_FILE" "$INSTRUCTION"
        ;;
    codex)
        if ! command -v codex &> /dev/null; then
             echo "Error: 'codex' command not found."
             exit 1
        fi
        echo "🤖 Starting Ralph with Codex..."
        codex -p "$PROMPT_FILE" "$INSTRUCTION"
        ;;
    *)
        echo "Error: Unknown agent '$AGENT'"
        echo "$USAGE"
        exit 1
        ;;
esac
