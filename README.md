<div align="center">

# GABB Stabilizer

**Multi-instance Growtopia launcher with GENTAHAX injection on the first window.**

![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D4)
![Language](https://img.shields.io/badge/language-C%2B%2B17-00599C)
![License](https://img.shields.io/badge/license-MIT-green)

**[English](#english) · [Türkçe](#türkçe)**

</div>

---

# English

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

## Requirements

| Item | Detail |
|---|---|
| OS | Windows 10 1809 or newer, 64-bit |
| Rights | Administrator (`SeDebugPrivilege`) |
| Growtopia | Installed through the official installer, at `%LOCALAPPDATA%\Growtopia\Growtopia.exe` |
| GENTAHAX | `GENTAHAX - GTInternal.dll`, **x64 build** |

The tool reads the PE machine type of both the client and the DLL and refuses to continue on a
mismatch.

## Build

### Option A — MSYS2 / MinGW-w64

Produces a single self-contained binary.

1. Install [MSYS2](https://www.msys2.org/).
2. Open the **UCRT64** shell — not MSYS, not MINGW32.
3. Install the toolchain:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
   ```

4. Build:

   ```bash
   g++ -O2 -static -std=c++17 src/gabb_stabilizer.cpp -o gabb_stabilizer.exe \
       -ladvapi32 -luser32 -static-libgcc -static-libstdc++
   ```

   Or run `build.bat` from a normal command prompt once MSYS2's `ucrt64\bin` is on `PATH`.

### Option B — Visual Studio

From an **x64 Native Tools Command Prompt** in the repo root:

```bat
cl /O2 /EHsc /std:c++17 src\gabb_stabilizer.cpp /link advapi32.lib user32.lib
```

`_WIN64` is defined automatically for x64 targets. A 32-bit build stops on `#error`.

### Output

One `gabb_stabilizer.exe`. No DLLs, no VC++ redistributable, nothing to register.

## Usage

1. Store `GENTAHAX - GTInternal.dll` in a permanent location, for example `C:\Genta\`.
2. Right-click `gabb_stabilizer.exe` → **Run as administrator**.
3. Paste the full DLL path when prompted.
   - `Enter` → search Desktop and Downloads for a `GH*` folder
   - `0` → no injection, vanilla multi-instance launcher only
4. Enter the window count. Window 1 is injected, the rest are vanilla.
5. Wait for `OPENED` on each line. A 30 second stability check runs afterwards.

## Command line

| Flag | Effect |
|---|---|
| `--count <n>` | Window count, skips the prompt |
| `--genta <path>` | DLL path, skips the prompt |
| `--no-inject`, `--no-genta` | Never inject |
| `--hold-public` | Keep placeholder mutexes alive between launches |
| `--diagnostic` | Enumerate and classify mutexes only, launch nothing |

```bat
gabb_stabilizer.exe --genta "C:\Genta\GENTAHAX - GTInternal.dll" --count 3
```

## How it works

| Step | Detail |
|---|---|
| 1. Elevation | Enables `SeDebugPrivilege` in its own token |
| 2. Launch | `CreateProcessW` on `%LOCALAPPDATA%\Growtopia\Growtopia.exe` |
| 3. Lock capture | `NtQuerySystemInformation(SystemExtendedHandleInformation)` enumerates the new PID's handles, duplicates each, keeps only `Mutant` objects matching Growtopia's lock, closes them via `DUPLICATE_CLOSE_SOURCE`. A 1.5 s grace window collects sibling locks |
| 4. Window check | Polls for a visible `AppClass` window, aborts on known error dialogs |
| 5. Injection | Suspends all threads, restores `ntdll`'s `.text` from a clean `SEC_IMAGE` mapping, resolves `LoadLibraryW` by offset from the local `kernel32` base, `VirtualAllocEx` → `WriteProcessMemory` → `CreateRemoteThread`, confirms the module appears in the target's module list, resumes |
| 6. Monitor | 30 s of watching for exits and error dialogs, then a summary |

First window only. Everything is logged to `gabb_debug.log`.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Privilege error` | Not running as administrator |
| `Growtopia.exe not found` | Game is installed elsewhere — reinstall via the official installer |
| `DLL architecture does not match` | You picked the 32-bit `GTInternal.dll`. Use the x64 one |
| `Locks caught: 0` | Another launcher is already running, or the client changed its mutex name. Run `--diagnostic` and open an issue with the log |
| `injection: FAILED (loadlibrary_returned_null)` | DLL failed to load. Install the VC++ 2015-2022 x64 runtime, right-click the DLL → Properties → **Unblock**, check dependencies |
| `injection: OK` but no Genta menu | Wrong Genta build for this client version |
| Defender deletes the exe | Add an exclusion — unsigned injection tools are routinely flagged |
| Windows open, then close | Resource pressure. Use fewer windows or lower graphics settings |

Always attach `gabb_debug.log` to a bug report. It carries the exact failure reason.

## Limitations

- Only the first window is injected. Multiple simultaneous GENTAHAX instances are not supported.
- Growtopia's own updater must still be able to run; no files are patched.
- Each window is a full client — RAM and CPU scale linearly.

## Disclaimer

Not affiliated with Ubisoft, Roblox Corporation or GENTAHAX. Multi-client is tolerated but not
officially supported. Use at your own risk.

## License

MIT — see [LICENSE](LICENSE).

---

# Türkçe

## İçindekiler

- [Ne yapar](#ne-yapar)
- [Gereksinimler](#gereksinimler)
- [Derleme](#derleme)
- [Kullanım](#kullanım)
- [Komut satırı](#komut-satırı)
- [Nasıl çalışır](#nasıl-çalışır)
- [Sorun giderme](#sorun-giderme)
- [Sınırlamalar](#sınırlamalar)
- [Sorumluluk reddi](#sorumluluk-reddi)
- [Lisans](#lisans)

## Ne yapar

Growtopia, adlandırılmış bir mutex (`\Sessions\<n>\BaseNamedObjects\Growtopia`) aldığı ve kilit
zaten tutuluyorsa çıkış yaptığı için ikinci kez açılmaz.

GABB Stabilizer bu handle'ı yeni başlattığı process'in içinde bulur, dışarıdan kapatır ve oyunun
devam etmesini sağlar. Böylece her instance kendini tek çalışan sanır.

Buna ek olarak ilk pencereye **GENTAHAX** enjekte edilir. İlk pencereden sonrakiler normal
Growtopia olarak açılır.

**Özellikler**

- x64 native, .NET yok, runtime kurulumu yok, tek statik exe
- Mutex'i hedefin handle tablosunu `NtQuerySystemInformation` ile tarayarak bulur
- Enjeksiyondan önce temiz `ntdll` geri yazar, `LoadLibraryW` adresini export parse etmeden offset ile çözer
- Başarı bildirmeden önce gerçek bir pencere oluştuğunu doğrular — yanlış pozitif yok
- Bilinen hata diyaloglarını yakalar ve asılı kalmak yerine erken çıkar
- `gabb_debug.log` içinde zaman damgalı tam tanı kaydı

## Gereksinimler

| Madde | Detay |
|---|---|
| İşletim sistemi | Windows 10 1809 veya üzeri, 64-bit |
| Yetki | Yönetici (`SeDebugPrivilege`) |
| Growtopia | Resmi installer ile kurulu, `%LOCALAPPDATA%\Growtopia\Growtopia.exe` |
| GENTAHAX | `GENTAHAX - GTInternal.dll`, **x64 sürüm** |

Araç hem istemcinin hem DLL'in PE makine tipini okur, uyuşmazlıkta devam etmez.

## Derleme

### Seçenek A — MSYS2 / MinGW-w64

Tek ve taşınabilir bir binary üretir.

1. [MSYS2](https://www.msys2.org/) kur.
2. **UCRT64** shell'ini aç — MSYS veya MINGW32 değil.
3. Derleyiciyi kur:

   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
   ```

4. Derle:

   ```bash
   g++ -O2 -static -std=c++17 src/gabb_stabilizer.cpp -o gabb_stabilizer.exe \
       -ladvapi32 -luser32 -static-libgcc -static-libstdc++
   ```

   MSYS2'nin `ucrt64\bin` klasörü `PATH`'te ise normal komut isteminde `build.bat` da çalışır.

### Seçenek B — Visual Studio

Repo kökünde **x64 Native Tools Command Prompt** açıp:

```bat
cl /O2 /EHsc /std:c++17 src\gabb_stabilizer.cpp /link advapi32.lib user32.lib
```

`_WIN64` x64 hedeflerinde otomatik tanımlanır. 32-bit derleme `#error` ile durur.

### Çıktı

Tek bir `gabb_stabilizer.exe`. DLL yok, VC++ redistributable yok, kayıt gerektiren bir şey yok.

## Kullanım

1. `GENTAHAX - GTInternal.dll` dosyasını kalıcı bir yere koy, örneğin `C:\Genta\`.
2. `gabb_stabilizer.exe` → sağ tık → **Yönetici olarak çalıştır**.
3. Sorulduğunda DLL'in tam yolunu yapıştır.
   - `Enter` → Masaüstü ve İndirilenler içinde `GH*` klasörü arar
   - `0` → enjeksiyon yok, sadece normal çoklu instance
4. Pencere sayısını gir. 1. pencere enjekte edilir, gerisi normal açılır.
5. Her satırda `OPENED` çıkmasını bekle. Ardından 30 saniyelik stabilite kontrolü çalışır.

## Komut satırı

| Parametre | Etkisi |
|---|---|
| `--count <n>` | Pencere sayısı, soruyu atlar |
| `--genta <yol>` | DLL yolu, soruyu atlar |
| `--no-inject`, `--no-genta` | Hiç enjeksiyon yapmaz |
| `--hold-public` | Placeholder mutex'leri açılışlar arasında canlı tutar |
| `--diagnostic` | Sadece mutex'leri listeler ve sınıflandırır, pencere açmaz |

```bat
gabb_stabilizer.exe --genta "C:\Genta\GENTAHAX - GTInternal.dll" --count 3
```

## Nasıl çalışır

| Adım | Detay |
|---|---|
| 1. Yetki | Kendi token'ında `SeDebugPrivilege` açar |
| 2. Başlatma | `%LOCALAPPDATA%\Growtopia\Growtopia.exe` üzerinde `CreateProcessW` |
| 3. Kilit yakalama | `NtQuerySystemInformation(SystemExtendedHandleInformation)` yeni PID'in handle'larını listeler, her birini kopyalar, sadece Growtopia kilidine uyan `Mutant` nesnelerini tutar ve `DUPLICATE_CLOSE_SOURCE` ile kapatır. 1,5 saniyelik grace penceresi kardeş kilitleri de toplar |
| 4. Pencere kontrolü | Görünür bir `AppClass` penceresi için bekler, bilinen hata diyaloglarında çıkar |
| 5. Enjeksiyon | Tüm thread'leri askıya alır, `ntdll` `.text` bölümünü temiz `SEC_IMAGE` eşlemesinden geri yazar, `LoadLibraryW` adresini yerel `kernel32` tabanından offset ile çözer, `VirtualAllocEx` → `WriteProcessMemory` → `CreateRemoteThread`, modülün hedefin modül listesinde göründüğünü doğrular, devam ettirir |
| 6. İzleme | 30 saniye çıkış ve hata diyaloğu izlemesi, sonra özet |

Sadece ilk pencere. Her şey `gabb_debug.log` dosyasına yazılır.

## Sorun giderme

| Belirti | Sebep ve çözüm |
|---|---|
| `Privilege error` | Yönetici olarak çalıştırılmamış |
| `Growtopia.exe not found` | Oyun başka yerde kurulu — resmi installer ile yeniden kur |
| `DLL architecture does not match` | 32-bit `GTInternal.dll` seçilmiş. x64 olanı kullan |
| `Locks caught: 0` | Başka bir launcher açık veya istemci mutex adını değiştirdi. `--diagnostic` çalıştırıp log ile issue aç |
| `injection: FAILED (loadlibrary_returned_null)` | DLL yüklenemedi. VC++ 2015-2022 x64 runtime kur, DLL → sağ tık → Özellikler → **Engellemeyi kaldır**, bağımlılıkları kontrol et |
| `injection: OK` ama Genta menüsü yok | Bu istemci sürümü için yanlış Genta build'i |
| Defender exe'yi siliyor | Hariç tutma ekle — imzasız enjeksiyon araçları sık işaretlenir |
| Pencereler açılıp kapanıyor | Kaynak yetersizliği. Daha az pencere veya daha düşük grafik ayarı |

Hata bildirimine her zaman `gabb_debug.log` ekle. Tam hata sebebi orada yazıyor.

## Sınırlamalar

- Sadece ilk pencere enjekte edilir. Aynı anda birden fazla GENTAHAX instance desteklenmez.
- Growtopia'nın kendi güncelleyicisi çalışabilmelidir; hiçbir dosya yamalanmaz.
- Her pencere tam bir istemcidir — RAM ve CPU doğrusal artar.

## Sorumluluk reddi

Ubisoft, Roblox Corporation veya GENTAHAX ile bağlantılı değildir. Çoklu istemci tolere edilir
ama resmî olarak desteklenmez. Riski sana aittir.

## Lisans

MIT — bkz. [LICENSE](LICENSE).
