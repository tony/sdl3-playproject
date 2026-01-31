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
        gemini -p "$PROMPT_FILE" "$INSTRUCTION"
        ;;
    claude)
        echo "🤖 Starting Ralph with Claude..."
        claude -p "$PROMPT_FILE" "$INSTRUCTION"
        ;;
    codex)
        echo "🤖 Starting Ralph with Codex..."
        codex -p "$PROMPT_FILE" "$INSTRUCTION"
        ;;
    *)
        echo "Error: Unknown agent '$AGENT'"
        echo "$USAGE"
        exit 1
        ;;
esac
