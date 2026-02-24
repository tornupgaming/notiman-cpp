# Shared Components

Core types and parsers used across CLI and Host.

## HookParser
Detects Claude Code and Cursor hook JSON from stdin and maps to NotificationPayload.

Supported events:
- `Notification` / `notification` → permission prompts, idle alerts, auth success
- `PostToolUse` / `postToolUse` → tool completions (filters ignored tools)
- `PostToolUseFailure` / `postToolUseFailure` → tool errors
- `AfterShellExecution` / `afterShellExecution` → command completion
- `AfterMCPExecution` / `afterMCPExecution` → MCP tool completion
- `AfterFileEdit` / `afterFileEdit` → file edited
- `Stop` / `stop` → session finished
- `SubagentStop` / `subagentStop` → agent completion
- `SessionStart` / `sessionStart` → session resumed/started

Tool summaries:
- Bash/Shell: extracts `command`
- Write/Edit/Read/ReadFile/Delete: extracts file path, strips cwd prefix
- EditNotebook: extracts `target_notebook`, strips cwd prefix
- Extracts project name from `cwd` field
