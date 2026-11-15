# GABB Stabilizer

A small Windows CLI tool that launches several isolated Growtopia instances side by side and
injects **GENTAHAX** into the first window only. Every window after the first opens as vanilla
Growtopia.

Growtopia refuses to run twice because it takes a named mutex
(`\Sessions\<n>\BaseNamedObjects\Growtopia`). GABB Stabilizer finds that handle inside the
process you just started, closes it from the outside, and lets the game continue — so each
instance believes it is the only one running. The first instance also gets GENTAHAX loaded
into it.

- x64 native, no .NET, no runtime install
- Works with the current x64 Growtopia client and GENTAHAX v5.61 (`GTInternal.dll`)
- Verifies every window really opened before reporting success
- Writes a full diagnostic log to `gabb_debug.log`

> Not affiliated with Ubisoft, Roblox or GENTAHAX. Use at your own risk.

---

## Requirements

| | |
|---|---|
| OS | Windows 10 1809 or newer, 64-bit |
| Rights | Administrator (SeDebugPrivilege) |
| Growtopia | Installed through the normal installer, at `%LOCALAPPDATA%\Growtopia\Growtopia.exe` |
| GENTAHAX | `GENTAHAX - GTInternal.dll` — **must be the x64 build** |

The tool refuses to continue if the DLL architecture does not match the client.

---

## Build

### Option A — MSYS2 / MinGW-w64 (recommended, produces a single static .exe)

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **UCRT64** shell (not MSYS, not MINGW32).
3. Install the compiler:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
