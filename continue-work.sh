#!/usr/bin/env bash
# Unattended "continue the work" run for this repo.
#
# The prompt is two words on purpose. `working-logic.md` already defines what continuing the work
# means -- convert change-requirements.md into tasks, implement tasks.md in order, or, if both are
# empty, hunt bugs and make things leaner -- and CLAUDE.md is loaded automatically, so the prompt
# has nothing to add. Runs in auto mode, the default: safe actions go through, unsafe ones are
# refused rather than granted.
set -euo pipefail

# `at` hands a job a near-empty environment, so the PATH is set here rather than inherited. Every
# build tool this project needs lives in /usr/bin; claude itself is a static binary in ~/.local/bin.
export PATH="/home/joaquin/.local/bin:/usr/local/bin:/usr/bin:/bin"

PROJECT="/home/joaquin/dev/autotel-plugin-presets-librarian"
CLAUDE="/home/joaquin/.local/bin/claude"

# Checkouts CLAUDE.md points at. This repo has no build of its own: it is verified by building
# mepSynth against it, and the other consumers are what a PresetHost change must not break.
ADD_DIRS=(
  --add-dir /home/joaquin/dev/JUCE
  --add-dir /home/joaquin/dev/Sampler
  --add-dir /home/joaquin/dev/modal-refractor-plugin
  --add-dir /home/joaquin/dev/mepSynth-plugin
  --add-dir /home/joaquin/dev/lanes-audio-plugin
  --add-dir "/home/joaquin/dev/messy system plugin"
)

LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/atl-continue"
mkdir -p "$LOG_DIR"
LOG="$LOG_DIR/$(date +%Y-%m-%d_%H%M%S).log"
ln -sfn "$LOG" "$LOG_DIR/latest.log"

# One run at a time -- a run can take a long while, and a second one starting on top of it would
# put two agents in the same working tree.
exec 9>"$LOG_DIR/lock"
flock -n 9 || { echo "another run holds the lock; exiting"; exit 0; }

cd "$PROJECT"
echo "=== continue-work $(date -Is) · from $(git rev-parse --short HEAD) ===" >> "$LOG"

# The one thing the agent cannot work out for itself: that there is no human in this session.
NOTE='This is an unattended scheduled run. Nobody will read a question or answer one, so do not
ask any: make the call yourself, state the assumption in the commit message, and carry on. Work
through tasks.md in order and commit each task as it lands, so a run that is cut short leaves
finished work behind rather than a half-edited tree. Keep the mepSynth suite (./build.sh test in ../mepSynth-plugin) green -- a commit that
does not build is worse than no commit. If a task is genuinely blocked, or a command you need is
refused, write that into tasks.md beside the task, move to the next one, and do not delete it. Continue till tasks.md is empty or credits run out'

# `|| rc=$?` rather than a bare call: under `set -e` a non-zero exit would abort the script
# before the log could record that it happened, which is the one case worth logging.
rc=0
"$CLAUDE" -p "continue the work" --append-system-prompt "$NOTE" "${ADD_DIRS[@]}" >> "$LOG" 2>&1 || rc=$?

echo "=== finished $(date -Is) rc=$rc · at $(git rev-parse --short HEAD) ===" >> "$LOG"
exit $rc
