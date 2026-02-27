# Notiman

High-performance Windows notification manager using Win32 APIs and Direct2D. 
Written because the default Windows notifications are garbage and don't stack.
Includes a tiny HTTP proxy server if you want to trigger notifications when one service calls another.

- **No runtime dependencies**
- **Native Windows integration**

## Performance

- Host
  - ~400kb size
  - ~5mb memory usage
- CLI
  - ~850kb size
- Proxy
  - ~650kb size
  - ~2.5mb memory usage

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

Or use the provided batch script:

```bat
build.bat
```

`build.bat` calls `MSBuild.exe` using a Visual Studio install path. If your edition differs, update this path in `build.bat`:

- Community: `%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`
- Professional: `%ProgramFiles%\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe`
- Enterprise: `%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe`

## Executables

After building, find executables in:

- CLI: `build/src/cli/Release/notiman.exe`
- Host: `build/src/host/Release/notiman-host.exe`
- Proxy: `build/src/host/Release/notiman-proxy.exe`

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

### Proxy Usage

Start the proxy host:

```bash
notiman-proxy.exe
```

Right click the system tray icon and modify settings.
Set up subdomain routes that point to upstream URLs.

## Configuration File

Notiman reads config from:

1. `%APPDATA%\notiman\config.ini` (preferred, user-specific)
2. `config/default.ini` next to the executable (fallback)

Right clicking Notiman in the system tray will give you a direct link to the config file.

Example config:

```ini
[notiman]
corner=TopLeft
monitor=0
max_visible=5
duration=8000
width=400
accent_color=#7C3AED
opacity=0.45
```

Supported keys:

- `corner`: toast anchor (`TopLeft`, `TopRight`, `BottomLeft`, `BottomRight`)
- `monitor`: zero-based monitor index
- `max_visible`: max simultaneous toasts
- `duration`: toast duration in milliseconds
- `width`: toast width in pixels
- `accent_color`: hex color (`#RRGGBB`)
- `opacity`: background opacity (`0.0` to `1.0`)


### Proxy Config Example 

```ini
[proxy]
host=127.0.0.1
port=9876

[routes]
api = http://localhost:8888
auth = http://localhost:9999
service3 = http://localhost:6666
```

Then a request like `curl http://127.0.0.1:9876/user/123 -H "Host: api.localhost:9876"` will proxy to `http://localhost:8888/user/123`.

## Agent Support

`notiman.exe` can be used directly from various Agent hooks by piping hook JSON into stdin.
Supported hook event names include both Claude-style (`PostToolUse`) and Cursor-style (`postToolUse`) variants.

### Cursor Hooks Integration

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

## Claude Code Hooks Integration

Example Claude Code hook config:

```json
{
  "version": 1,
  "hooks": {
    "PostToolUse": [{ "command": "notiman.exe --ignore-tool Glob,Grep,Read,ReadFile" }],
    "PostToolUseFailure": [{ "command": "notiman.exe" }],
    "AfterShellExecution": [{ "command": "notiman.exe" }],
    "AfterMCPExecution": [{ "command": "notiman.exe --ignore-tool Glob,Grep,Read,ReadFile" }],
    "AfterFileEdit": [{ "command": "notiman.exe" }],
    "Stop": [{ "command": "notiman.exe" }],
    "SubagentStop": [{ "command": "notiman.exe" }],
    "SessionStart": [{ "command": "notiman.exe" }]
  }
}
```
