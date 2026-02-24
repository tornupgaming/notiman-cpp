# Notiman C++ Port

High-performance C++ implementation of Notiman using Win32 APIs and Direct2D.

## Benefits

- **3.2x smaller memory footprint** (66MB vs 213MB for C# version)
- **Faster startup and execution**
- **No runtime dependencies** (static linking)
- **Native Windows integration**

## Build

```bash
# Configure
cmake -B build -G "Visual Studio 18 2025"

# Build
cmake --build build --config Release
```

Or use MSBuild directly:
```bash
msbuild build/Notiman.sln //p:Configuration=Release //v:minimal
```

## Executables

After building, find executables in:
- CLI: `build/src/cli/Release/notiman.exe` (842KB)
- Host: `build/src/host/Release/notiman-host.exe` (790KB)

## Usage

Start the host:
```bash
notiman-host.exe
```

Send notifications:
```bash
notiman.exe -t "Title" -b "Body text" -i success
notiman.exe -t "Code Example" -c "int main() { return 0; }"
```

## Cursor Hooks Integration

`notiman.exe` can be used directly from Cursor Agent hooks by piping hook JSON into stdin.

Example project hook config:

```json
{
  "version": 1,
  "hooks": {
    "postToolUse": [{ "command": "notiman.exe --ignore-tool Glob,Grep,Read,ReadFile" }],
    "postToolUseFailure": [{ "command": "notiman.exe" }],
    "afterShellExecution": [{ "command": "notiman.exe" }],
    "afterMCPExecution": [{ "command": "notiman.exe --ignore-tool Glob,Grep,Read,ReadFile" }],
    "afterFileEdit": [{ "command": "notiman.exe" }],
    "stop": [{ "command": "notiman.exe" }],
    "subagentStop": [{ "command": "notiman.exe" }],
    "sessionStart": [{ "command": "notiman.exe" }]
  }
}
```

Supported hook event names include both Claude-style (`PostToolUse`) and Cursor-style (`postToolUse`) variants.

## Architecture

- **CLI** (`src/cli`) - Command-line interface, sends notifications via WM_COPYDATA
- **Host** (`src/host`) - System tray app, renders toasts with Direct2D
- **Shared** (`src/shared`) - Common types, config, hook parser
- **Third-party** (`third_party`) - Header-only dependencies (nlohmann/json, toml11, CLI11)

## Tech Stack

- C++20
- Win32 API
- Direct2D for rendering
- DirectWrite for text
- DWM for acrylic effects
- Static CRT linking (/MT)
