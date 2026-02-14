# Shared Components

Core types and parsers used across CLI and Host.

## HookParser
Detects Claude Code hook JSON from stdin and maps to NotificationPayload.

Supported events:
- `Notification` → permission prompts, idle alerts, auth success
- `PostToolUse` → tool completions (filters Glob/Read)
- `PostToolUseFailure` → tool errors
- `Stop` → session finished
- `SubagentStop` → agent completion
- `SessionStart` → session resumed

Tool summaries:
- Bash: extracts `command`
- Write/Edit/Read: extracts `file_path`, strips cwd prefix
- Extracts project name from `cwd` field
