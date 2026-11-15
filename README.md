<div align="center">

# GABB Stabilizer

**Multi-instance Growtopia launcher with GENTAHAX injection on the first window.**

![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D4)
![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## Table of contents

- [What it does](#what-it-does)
- [Requirements](#requirements)
- [Build](#build)
- [Usage](#usage)
- [Command line](#command-line)
- [How it works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [Limitations](#limitations)
- [Disclaimer](#disclaimer)
- [License](#license)

---

## What it does

Growtopia refuses to start twice because it takes a named mutex
(`\Sessions\<n>\BaseNamedObjects\Growtopia`) and quits when the lock is already held.

GABB Stabilizer finds that handle inside the process it just created, closes it from the outside,
and lets the client continue. Each instance therefore believes it is the only one running.

On top of that, the first window gets **GENTAHAX** injected into it. Every window after the first
opens as vanilla Growtopia.

**Features**

- x64 native, no .NET, no runtime install, one static executable
- Finds the mutex by walking the target's handle table with `NtQuerySystemInformation`
- Restores a clean `ntdll` before injecting, resolves `LoadLibraryW` by offset instead of parsing exports
- Verifies a real window exists before reporting success — no false positives
- Detects the known error dialogs and aborts early instead of hanging
- Full timestamped diagnostic log in `gabb_debug.log`

---

## Requirements

| Item | Detail |
|---|---|
| OS | Windows 10 1809 or newer, 64-bit |
| Rights | Administrator (`SeDebugPrivilege`) |
| Growtopia | Installed through the official installer, at `%LOCALAPPDATA%\Growtopia\Growtopia.exe` |
| GENTAHAX | `GENTAHAX - GTInternal.dll`, **x64 build** |

The tool reads the PE machine type of both the client and the DLL and refuses to continue on a
mismatch.

---

## Build

### Option A — MSYS2 / MinGW-w64

Produces a single self-contained binary.

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **UCRT64** shell — not MSYS, not MINGW32.
3. Install the toolchain:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
