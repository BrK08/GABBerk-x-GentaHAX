//GABBerk X GENTAHAX
//By Berk
// discord : mr_berk.

#include <windows.h>
#include <tlhelp32.h>
#include <conio.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cwctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN64
#error Build this tool with an x64 compiler. 32-bit builds cannot read 64-bit handles.
#endif

#define SystemExtendedHandleInformation 64
#define ObjectNameInformation 1
#define ObjectTypeInformation 2
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004
#define STATUS_BUFFER_TOO_SMALL 0xC0000023
#define STATUS_BUFFER_OVERFLOW 0x80000005

typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pfnNtQueryObject)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pfnNtSuspendProcess)(HANDLE);
typedef NTSTATUS(NTAPI* pfnNtResumeProcess)(HANDLE);
typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef BOOL(WINAPI* pfnIsWow64Process2)(HANDLE, USHORT*, USHORT*);

enum class TargetArch { Unknown, X86, X64 };

struct UNICODE_STRING_L {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
};

struct OBJECT_NAME_INFORMATION { UNICODE_STRING_L Name; };
typedef OBJECT_NAME_INFORMATION* POBJECT_NAME_INFORMATION;

struct PUBLIC_OBJECT_TYPE_INFORMATION {
    UNICODE_STRING_L TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
};

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    PVOID Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
};

struct SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
};

struct PROCESS_BASIC_INFORMATION_L {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
};

struct TargetHandleInfo {
    DWORD pid;
    HANDLE handleValue;
    std::wstring name;
    std::string reason;
};

struct PublicMutexInfo {
    std::wstring name;
    HANDLE handle;
};

struct WindowInfo {
    HWND hwnd;
    bool visible;
    std::wstring title;
    std::wstring className;
    std::vector<std::wstring> childTexts;
};

struct InstanceResult {
    int index;
    DWORD pid;
    HANDLE processHandle;
    bool createProcessOk;
    bool opened;
    bool stable;
    bool genta;
    DWORD exitCode;
    std::string status;
};

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string output(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), &output[0], size, NULL, NULL);
    return output;
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), NULL, 0);
    if (size <= 0) return L"";
    std::wstring output(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), &output[0], size);
    return output;
}

std::wstring ToLowerCopy(std::wstring value) {
    for (wchar_t& ch : value) ch = (wchar_t)towlower(ch);
    return value;
}

std::wstring CleanUnicodeString(const UNICODE_STRING_L& value, bool lowerCase) {
    std::wstring result;
    if (value.Buffer == NULL || value.Length == 0) return result;
    USHORT count = value.Length / sizeof(wchar_t);
    result.reserve(count);
    for (USHORT i = 0; i < count; i++) {
        wchar_t ch = value.Buffer[i];
        if (ch == L'\0') break;
        result.push_back(lowerCase ? (wchar_t)towlower(ch) : ch);
    }
    while (!result.empty() && (result.back() == L'\0' || result.back() == L' ' || result.back() == L'\r' || result.back() == L'\n' || result.back() == L'\t')) {
        result.pop_back();
    }
    return result;
}

std::string HexNt(NTSTATUS value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << (ULONG)value << std::dec;
    return stream.str();
}

std::string HexWord(WORD value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << (ULONG)value << std::dec;
    return stream.str();
}

std::string HexU64(ULONGLONG value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << value;
    return stream.str();
}

std::string TrimCopy(const std::string& input) {
    size_t start = 0;
    size_t end = input.size();
    while (start < end && (input[start] == ' ' || input[start] == '\t' || input[start] == '\r' || input[start] == '\n')) start++;
    while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t' || input[end - 1] == '\r' || input[end - 1] == '\n')) end--;
    return input.substr(start, end - start);
}

// ---------------------------------------------------------------------------
// tutucuuu
// ---------------------------------------------------------------------------
class Logger {
public:
    explicit Logger(const std::string& path) : path_(path), file_(path, std::ios::binary | std::ios::out | std::ios::trunc) {
        if (file_.is_open()) {
            const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            file_.write(reinterpret_cast<const char*>(bom), sizeof(bom));
            Log("LOGGER", "Log file opened: " + path_);
        }
    }
    bool IsOpen() const { return file_.is_open(); }
    void Log(const std::string& tag, const std::string& message) {
        std::string line = Timestamp() + " [" + tag + "] " + message;
        if (file_.is_open()) { file_ << line << "\r\n"; file_.flush(); }
    }
    void LogWide(const std::string& tag, const std::wstring& message) { Log(tag, WideToUtf8(message)); }
    void Console(const std::string& message) { std::cout << message << std::endl; Log("CONSOLE", message); }
private:
    std::string Timestamp() const {
        SYSTEMTIME time; GetLocalTime(&time);
        std::ostringstream stream;
        stream << std::setfill('0') << std::setw(4) << time.wYear << "-" << std::setw(2) << time.wMonth << "-"
               << std::setw(2) << time.wDay << " " << std::setw(2) << time.wHour << ":" << std::setw(2)
               << time.wMinute << ":" << std::setw(2) << time.wSecond << "." << std::setw(3) << time.wMilliseconds;
        return stream.str();
    }
    std::string path_;
    std::ofstream file_;
};

// ---------------------------------------------------------------------------
// yardkomutlar
// ---------------------------------------------------------------------------
bool EnableDebugPrivilege(Logger& log) {
    HANDLE hToken = NULL; LUID luid; TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        log.Log("PRIV", "OpenProcessToken failed. Win32=" + std::to_string(GetLastError()));
        return false;
    }
    if (!LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &luid)) {
        log.Log("PRIV", "LookupPrivilegeValueA failed. Win32=" + std::to_string(GetLastError()));
        CloseHandle(hToken); return false;
    }
    tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        log.Log("PRIV", "AdjustTokenPrivileges failed. Win32=" + std::to_string(GetLastError()));
        CloseHandle(hToken); return false;
    }
    DWORD lastError = GetLastError(); CloseHandle(hToken);
    if (lastError != ERROR_SUCCESS) {
        log.Log("PRIV", "SeDebugPrivilege not assigned. Win32=" + std::to_string(lastError));
        return false;
    }
    log.Log("PRIV", "SeDebugPrivilege enabled.");
    return true;
}

bool FileExistsW(const wchar_t* path) {
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirectoryExistsW(const wchar_t* path) {
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::vector<DWORD> GetGrowtopiaPids(Logger* log = NULL) {
    std::vector<DWORD> pids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32W entry; entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"Growtopia.exe") == 0) pids.push_back(entry.th32ProcessID);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (log) log->Log("PROCESS", "Growtopia process count=" + std::to_string(pids.size()));
    return pids;
}

std::vector<DWORD> SinglePidList(DWORD pid) {
    std::vector<DWORD> pids;
    if (pid != 0) pids.push_back(pid);
    return pids;
}

// ---------------------------------------------------------------------------
// işlemyapankısım onu çok özledimm
// ---------------------------------------------------------------------------
TargetArch DetectProcessArch(HANDLE hProcess, Logger& log, DWORD pid) {
    static pfnIsWow64Process2 fn = nullptr;
    static bool resolved = false;
    if (!resolved) {
        fn = (pfnIsWow64Process2)GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process2");
        resolved = true;
    }
    if (fn) {
        USHORT procMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
        if (fn(hProcess, &procMachine, &nativeMachine)) {
            if (procMachine == IMAGE_FILE_MACHINE_UNKNOWN) {
                log.Log("ARCH", "PID=" + std::to_string(pid) + " native x64 (host native=" + HexWord(nativeMachine) + ")");
                return TargetArch::X64;
            }
            if (procMachine == IMAGE_FILE_MACHINE_I386) {
                log.Log("ARCH", "PID=" + std::to_string(pid) + " WOW64 x86");
                return TargetArch::X86;
            }
            log.Log("ARCH", "PID=" + std::to_string(pid) + " machine=" + HexWord(procMachine));
            return TargetArch::Unknown;
        }
    }
    BOOL wow = FALSE;
    if (IsWow64Process(hProcess, &wow)) return wow ? TargetArch::X86 : TargetArch::X64;
    return TargetArch::Unknown;
}

WORD PeMachineFromFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;

    BYTE header[0x400] = { 0 };
    DWORD read = 0;
    WORD machine = 0;
    if (ReadFile(file, header, sizeof(header), &read, NULL) && read >= 0x40) {
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)header;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 &&
            (size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS32) <= read) {
            IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(header + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) machine = nt->FileHeader.Machine;
        }
    }
    CloseHandle(file);
    return machine;
}

bool DllMatchesArch(const std::wstring& dllPath, TargetArch arch, Logger& log) {
    WORD machine = PeMachineFromFile(dllPath);
    if (machine == 0) {
        log.LogWide("ARCH", L"Could not read DLL machine type: " + dllPath);
        return false;
    }
    WORD wanted = (arch == TargetArch::X64) ? IMAGE_FILE_MACHINE_AMD64 : IMAGE_FILE_MACHINE_I386;
    bool ok = (machine == wanted);
    log.Log("ARCH", "DLL machine=" + HexWord(machine) + " wanted=" + HexWord(wanted) + " ok=" + std::to_string(ok));
    return ok;
}

// ---------------------------------------------------------------------------
// sorgular
// ---------------------------------------------------------------------------
bool QuerySystemHandles(pfnNtQuerySystemInformation fn, std::vector<unsigned char>& buffer, Logger& log) {
    ULONG bufferSize = 0x40000; ULONG returnLength = 0;
    buffer.assign(bufferSize, 0);
    NTSTATUS status = fn(SystemExtendedHandleInformation, buffer.data(), bufferSize, &returnLength);
    while (status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL || status == (NTSTATUS)STATUS_BUFFER_OVERFLOW) {
        bufferSize = returnLength + 0x20000;
        buffer.assign(bufferSize, 0);
        status = fn(SystemExtendedHandleInformation, buffer.data(), bufferSize, &returnLength);
    }
    if (status != 0) { log.Log("HANDLE", "NtQuerySystemInformation failed. NTSTATUS=" + HexNt(status)); buffer.clear(); return false; }
    return true;
}

bool QueryObjectTypeName(HANDLE hDup, pfnNtQueryObject fn, std::wstring& typeName, Logger& log) {
    ULONG size = 0x1000, returnLength = 0;
    std::vector<unsigned char> buffer(size, 0);
    NTSTATUS status = fn(hDup, ObjectTypeInformation, buffer.data(), size, &returnLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL || status == (NTSTATUS)STATUS_BUFFER_OVERFLOW) {
        size = returnLength + 0x100; buffer.assign(size, 0);
        status = fn(hDup, ObjectTypeInformation, buffer.data(), size, &returnLength);
    }
    if (status != 0) return false;
    PUBLIC_OBJECT_TYPE_INFORMATION* typeInfo = reinterpret_cast<PUBLIC_OBJECT_TYPE_INFORMATION*>(buffer.data());
    typeName = CleanUnicodeString(typeInfo->TypeName, true);
    return !typeName.empty();
}

bool QueryObjectName(HANDLE hDup, pfnNtQueryObject fn, std::wstring& objectName, Logger& log) {
    ULONG size = 0x2000, returnLength = 0;
    std::vector<unsigned char> buffer(size, 0);
    NTSTATUS status = fn(hDup, ObjectNameInformation, buffer.data(), size, &returnLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH || status == STATUS_BUFFER_TOO_SMALL || status == (NTSTATUS)STATUS_BUFFER_OVERFLOW) {
        size = returnLength + 0x100; buffer.assign(size, 0);
        status = fn(hDup, ObjectNameInformation, buffer.data(), size, &returnLength);
    }
    if (status != 0) return false;
    POBJECT_NAME_INFORMATION nameInfo = reinterpret_cast<POBJECT_NAME_INFORMATION>(buffer.data());
    objectName = CleanUnicodeString(nameInfo->Name, true);
    return !objectName.empty();
}

bool ContainsAny(const std::wstring& value, const std::vector<std::wstring>& needles, std::wstring* matchedNeedle = NULL) {
    for (const std::wstring& needle : needles) {
        if (value.find(needle) != std::wstring::npos) { if (matchedNeedle) *matchedNeedle = needle; return true; }
    }
    return false;
}

bool LooksLikeGuidSingletonLock(const std::wstring& lowerName) {
    if (lowerName.find(L"basenamedobjects\\") == std::wstring::npos) return false;
    size_t slash = lowerName.find_last_of(L'\\');
    std::wstring baseName = slash == std::wstring::npos ? lowerName : lowerName.substr(slash + 1);
    size_t closeBrace = baseName.find(L'}');
    return baseName.size() > 4 && baseName[0] == L'{' && closeBrace != std::wstring::npos &&
        closeBrace + 2 < baseName.size() && baseName[closeBrace + 1] == L'_';
}

bool IsConfirmedGrowtopiaLock(const std::wstring& lowerName, bool includeFallbackSingleton, std::string& reason) {
    const std::vector<std::wstring> internalNames = { L"wilstaging", L"audio", L"render", L"crash", L"dbwin", L"ddraw", L"midimapper" };
    std::wstring internalMatch;
    if (ContainsAny(lowerName, internalNames, &internalMatch)) { reason = "excluded_internal_" + WideToUtf8(internalMatch); return false; }
    const std::wstring exactSuffix = L"\\basenamedobjects\\growtopia";
    if (lowerName.size() >= exactSuffix.size() &&
        lowerName.compare(lowerName.size() - exactSuffix.size(), exactSuffix.size(), exactSuffix) == 0) {
        reason = "exact_main_suffix"; return true;
    }
    if (lowerName.find(L"growtopia") != std::wstring::npos) { reason = "contains_growtopia_non_internal"; return true; }
    if (includeFallbackSingleton && LooksLikeGuidSingletonLock(lowerName)) { reason = "fallback_guid_singleton"; return true; }
    reason = "not_a_growtopia_lock"; return false;
}

std::vector<TargetHandleInfo> ScanGrowtopiaMutexes(
    pfnNtQuerySystemInformation NtQuerySystemInformation,
    pfnNtQueryObject NtQueryObject,
    Logger& log, bool diagnosticMode,
    const std::vector<DWORD>* explicitPids = NULL,
    bool includeFallbackSingleton = false
) {
    std::vector<TargetHandleInfo> targets;
    std::vector<DWORD> pids = explicitPids ? *explicitPids : GetGrowtopiaPids(&log);
    if (pids.empty()) return targets;

    std::vector<unsigned char> buffer;
    if (!QuerySystemHandles(NtQuerySystemInformation, buffer, log)) return targets;

    SYSTEM_HANDLE_INFORMATION_EX* handleInfo = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer.data());

    for (ULONG_PTR i = 0; i < handleInfo->NumberOfHandles; i++) {
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = handleInfo->Handles[i];
        for (DWORD pid : pids) {
            if (handle.UniqueProcessId != pid) continue;

            HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
            if (hProcess == NULL) break;

            HANDLE hDup = NULL;
            if (!DuplicateHandle(hProcess, (HANDLE)handle.HandleValue, GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                CloseHandle(hProcess); break;
            }

            std::wstring typeName;
            if (QueryObjectTypeName(hDup, NtQueryObject, typeName, log) && typeName.find(L"mutant") != std::wstring::npos) {
                std::wstring objectName;
                if (QueryObjectName(hDup, NtQueryObject, objectName, log)) {
                    std::string reason;
                    bool isTarget = IsConfirmedGrowtopiaLock(objectName, includeFallbackSingleton, reason);
                    log.Log("MUTEX", std::string("decision=") + (isTarget ? "target" : "skip") + " reason=" + reason + " name=" + WideToUtf8(objectName));
                    if (isTarget && !diagnosticMode) {
                        TargetHandleInfo info; info.pid = pid; info.handleValue = (HANDLE)handle.HandleValue;
                        info.name = objectName; info.reason = reason;
                        targets.push_back(info);
                    }
                }
            }

            CloseHandle(hDup);
            CloseHandle(hProcess);
            break;
        }
    }
    return targets;
}

std::vector<DWORD> SuspendProcesses(const std::vector<DWORD>& pids, pfnNtSuspendProcess fn, Logger& log) {
    std::vector<DWORD> suspended;
    for (DWORD pid : pids) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (!hProcess) continue;
        NTSTATUS status = fn(hProcess);
        CloseHandle(hProcess);
        if (status == 0) { suspended.push_back(pid); log.Log("SUSPEND", "Suspended PID=" + std::to_string(pid)); }
    }
    return suspended;
}

void ResumeProcesses(const std::vector<DWORD>& pids, pfnNtResumeProcess fn, Logger& log) {
    for (DWORD pid : pids) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (!hProcess) continue;
        fn(hProcess);
        CloseHandle(hProcess);
        log.Log("RESUME", "Resumed PID=" + std::to_string(pid));
    }
}

int CloseTargetMutexes(const std::vector<TargetHandleInfo>& targets, Logger& log, std::vector<std::wstring>* closedNames = NULL) {
    int closed = 0;
    for (const TargetHandleInfo& target : targets) {
        HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, target.pid);
        if (!hProcess) continue;
        HANDLE hClosed = NULL;
        BOOL ok = DuplicateHandle(hProcess, target.handleValue, GetCurrentProcess(), &hClosed, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
        if (ok) {
            if (hClosed) CloseHandle(hClosed);
            closed++;
            if (closedNames) closedNames->push_back(target.name);
            log.Log("ACTIVE", "Closed mutex. PID=" + std::to_string(target.pid) + " reason=" + target.reason + " name=" + WideToUtf8(target.name));
        }
        CloseHandle(hProcess);
    }
    return closed;
}

std::wstring MutexObjectNameToCreateName(const std::wstring& objectName) {
    const std::wstring marker = L"basenamedobjects\\";
    size_t markerPos = objectName.find(marker);
    if (markerPos != std::wstring::npos) return objectName.substr(markerPos + marker.size());
    size_t slash = objectName.find_last_of(L'\\');
    if (slash != std::wstring::npos && slash + 1 < objectName.size()) return objectName.substr(slash + 1);
    return objectName;
}

bool HasPublicMutex(const std::vector<PublicMutexInfo>& publicMutexes, const std::wstring& name) {
    for (const PublicMutexInfo& info : publicMutexes) if (_wcsicmp(info.name.c_str(), name.c_str()) == 0) return true;
    return false;
}

int CreatePublicMutexesForNames(const std::vector<std::wstring>& objectNames, std::vector<PublicMutexInfo>& publicMutexes, Logger& log) {
    int createdCount = 0;
    for (const std::wstring& objectName : objectNames) {
        std::wstring createName = MutexObjectNameToCreateName(objectName);
        if (createName.empty() || HasPublicMutex(publicMutexes, createName)) continue;
        HANDLE hMutex = CreateMutexW(NULL, FALSE, createName.c_str());
        if (!hMutex) continue;
        PublicMutexInfo info; info.name = createName; info.handle = hMutex;
        publicMutexes.push_back(info); createdCount++;
    }
    return createdCount;
}

std::vector<std::wstring> ReleasePublicMutexes(std::vector<PublicMutexInfo>& publicMutexes, Logger& log) {
    std::vector<std::wstring> releasedNames;
    for (PublicMutexInfo& info : publicMutexes) {
        releasedNames.push_back(info.name);
        if (info.handle) CloseHandle(info.handle);
    }
    publicMutexes.clear();
    return releasedNames;
}

void AppendNames(std::vector<std::wstring>& destination, const std::vector<std::wstring>& source) {
    for (const std::wstring& name : source) {
        bool exists = false;
        for (const std::wstring& existing : destination) if (_wcsicmp(existing.c_str(), name.c_str()) == 0) { exists = true; break; }
        if (!exists) destination.push_back(name);
    }
}

// ilk kısmı kilitler ondan sonracıma devam eder ve bakar , benim ona baktığım gibi İÇİNDEEE AŞKK VAR 
// gt+GUİd
int WaitForNewProcessMutexAndClose(
    DWORD pid, pfnNtQuerySystemInformation NtQuerySystemInformation, pfnNtQueryObject NtQueryObject,
    Logger& log, bool includeFallbackSingleton, std::vector<std::wstring>* closedNames, DWORD maxWaitMs
) {
    std::vector<DWORD> pids = SinglePidList(pid);
    const DWORD intervalMs = 100;
    const DWORD graceMs = 1500;
    DWORD waitedMs = 0;
    DWORD graceRemaining = 0;
    int totalClosed = 0;
    std::vector<std::wstring> seen;

    while (waitedMs <= maxWaitMs) {
        std::vector<TargetHandleInfo> targets = ScanGrowtopiaMutexes(NtQuerySystemInformation, NtQueryObject, log, false, &pids, includeFallbackSingleton);

        std::vector<TargetHandleInfo> fresh;
        for (const TargetHandleInfo& t : targets) {
            bool dup = false;
            for (const std::wstring& s : seen) if (_wcsicmp(s.c_str(), t.name.c_str()) == 0) { dup = true; break; }
            if (!dup) { seen.push_back(t.name); fresh.push_back(t); }
        }

        if (!fresh.empty()) {
            totalClosed += CloseTargetMutexes(fresh, log, closedNames);
            if (graceRemaining == 0) graceRemaining = graceMs;
        }

        if (totalClosed > 0) {
            if (graceRemaining == 0) return totalClosed;
            if (graceRemaining <= intervalMs) return totalClosed;
            graceRemaining -= intervalMs;
        }

        Sleep(intervalMs); waitedMs += intervalMs;
    }
    return totalClosed;
}

// ---------------------------------------------------------------------------
// pencere
// ---------------------------------------------------------------------------
std::wstring GetWindowTextSafe(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return L"";
    std::wstring text(length + 1, L'\0');
    int copied = GetWindowTextW(hwnd, &text[0], length + 1);
    if (copied <= 0) return L"";
    text.resize(copied);
    return text;
}

std::wstring GetClassNameSafe(HWND hwnd) {
    wchar_t className[256] = { 0 };
    int copied = GetClassNameW(hwnd, className, 255);
    if (copied <= 0) return L"";
    return std::wstring(className, copied);
}

struct ChildTextEnumData { std::vector<std::wstring>* texts; };
BOOL CALLBACK EnumChildTextProc(HWND hwnd, LPARAM lParam) {
    ChildTextEnumData* data = reinterpret_cast<ChildTextEnumData*>(lParam);
    std::wstring text = GetWindowTextSafe(hwnd);
    if (!text.empty()) data->texts->push_back(text);
    return TRUE;
}

struct WindowEnumData { DWORD pid; std::vector<WindowInfo>* windows; };
BOOL CALLBACK EnumWindowsForPidProc(HWND hwnd, LPARAM lParam) {
    WindowEnumData* data = reinterpret_cast<WindowEnumData*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != data->pid) return TRUE;
    WindowInfo info;
    info.hwnd = hwnd;
    info.visible = IsWindowVisible(hwnd) != FALSE;
    info.title = GetWindowTextSafe(hwnd);
    info.className = GetClassNameSafe(hwnd);
    ChildTextEnumData childData; childData.texts = &info.childTexts;
    EnumChildWindows(hwnd, EnumChildTextProc, reinterpret_cast<LPARAM>(&childData));
    data->windows->push_back(info);
    return TRUE;
}

std::vector<WindowInfo> GetWindowsForPid(DWORD pid) {
    std::vector<WindowInfo> windows;
    WindowEnumData data; data.pid = pid; data.windows = &windows;
    EnumWindows(EnumWindowsForPidProc, reinterpret_cast<LPARAM>(&data));
    return windows;
}

bool DetectKnownErrorDialog(const std::vector<WindowInfo>& windows, Logger& log, DWORD pid, std::string& failureReason) {
    for (const WindowInfo& window : windows) {
        std::wstring joined = ToLowerCopy(window.title + L" " + window.className);
        for (const std::wstring& childText : window.childTexts) { joined += L" "; joined += ToLowerCopy(childText); }

        if (joined.find(L"internal memory error") != std::wstring::npos) {
            failureReason = "internal_memory_error_dialog_detected";
            log.Log("WINDOW", "Internal memory error dialog. PID=" + std::to_string(pid) + " text=" + WideToUtf8(joined));
            return true;
        }
        if (joined.find(L"gentahax") != std::wstring::npos ||
            joined.find(L"unable to enable hooks") != std::wstring::npos ||
            joined.find(L"failed to initialize opengl") != std::wstring::npos) {
            failureReason = "gentahax_error_dialog_detected";
            log.Log("WINDOW", "GENTAHAX error dialog. PID=" + std::to_string(pid) + " text=" + WideToUtf8(joined));
            return true;
        }
        if (joined.find(L"unable to load stuff") != std::wstring::npos ||
            joined.find(L"error initializing the game") != std::wstring::npos) {
            failureReason = "load_stuff_error_dialog_detected";
            log.Log("WINDOW", "Load-stuff error dialog. PID=" + std::to_string(pid) + " text=" + WideToUtf8(joined));
            return true;
        }
        if (joined.find(L"already running") != std::wstring::npos ||
            (joined.find(L"instance") != std::wstring::npos && joined.find(L"running") != std::wstring::npos) ||
            joined.find(L"go play") != std::wstring::npos) {
            failureReason = "already_running_error_dialog_detected";
            log.Log("WINDOW", "Already-running dialog. PID=" + std::to_string(pid) + " text=" + WideToUtf8(joined));
            return true;
        }
    }
    return false;
}

bool IsProcessAlive(HANDLE processHandle, DWORD* exitCodeOut = NULL) {
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processHandle, &exitCode)) return false;
    if (exitCodeOut) *exitCodeOut = exitCode;
    return exitCode == STILL_ACTIVE;
}

std::vector<DWORD> GetAliveResultPids(const std::vector<InstanceResult>& results) {
    std::vector<DWORD> pids;
    for (const InstanceResult& result : results)
        if (result.createProcessOk && result.processHandle != NULL && IsProcessAlive(result.processHandle)) pids.push_back(result.pid);
    return pids;
}

bool VerifyVisibleWindowOrFailure(HANDLE processHandle, DWORD pid, Logger& log, std::string& failureReason, DWORD deadlineMs) {
    const DWORD intervalMs = 250; DWORD waited = 0;
    while (waited <= deadlineMs) {
        DWORD exitCode = 0;
        if (!IsProcessAlive(processHandle, &exitCode)) {
            failureReason = "process_exited_exit_code_" + std::to_string(exitCode);
            return false;
        }
        std::vector<WindowInfo> windows = GetWindowsForPid(pid);
        if (DetectKnownErrorDialog(windows, log, pid, failureReason)) return false;
        for (const WindowInfo& window : windows) {
            if (window.visible && window.className == L"AppClass") {
                log.Log("VERIFY", "AppClass window verified. PID=" + std::to_string(pid));
                return true;
            }
        }
        Sleep(intervalMs); waited += intervalMs;
    }
    failureReason = "no_verified_app_window_within_timeout";
    return false;
}

bool WaitForProcessQuiet(HANDLE processHandle, DWORD pid, Logger& log, DWORD milliseconds, std::string& failureReason) {
    const DWORD intervalMs = 250; DWORD waited = 0;
    while (waited <= milliseconds) {
        DWORD exitCode = 0;
        if (!IsProcessAlive(processHandle, &exitCode)) {
            failureReason = "process_exited_during_quiet_wait_exit_code_" + std::to_string(exitCode);
            return false;
        }
        std::vector<WindowInfo> windows = GetWindowsForPid(pid);
        if (DetectKnownErrorDialog(windows, log, pid, failureReason)) return false;
        Sleep(intervalMs); waited += intervalMs;
    }
    return true;
}



bool WaitForGentaWindow(DWORD pid, const std::vector<HWND>& baseline, Logger& log, DWORD timeoutMs) {
    const DWORD intervalMs = 250; DWORD waited = 0;
    while (waited <= timeoutMs) {
        for (const WindowInfo& window : GetWindowsForPid(pid)) {
            if (!window.visible) continue;
            if (window.className == L"AppClass") continue;
            bool known = false;
            for (HWND base : baseline) if (base == window.hwnd) { known = true; break; }
            if (known) continue;
            log.Log("GENTA", "New window detected. class=" + WideToUtf8(window.className) + " title=" + WideToUtf8(window.title));
            return true;
        }
        Sleep(intervalMs); waited += intervalMs;
    }
    log.Log("GENTA", "No new window detected. PID=" + std::to_string(pid));
    return false;
}

void MonitorStability(std::vector<InstanceResult>& results, Logger& log, DWORD seconds) {
    for (DWORD elapsed = 0; elapsed < seconds; elapsed++) {
        for (InstanceResult& result : results) {
            if (!result.createProcessOk || !result.opened || !result.stable) continue;
            DWORD exitCode = 0;
            if (!IsProcessAlive(result.processHandle, &exitCode)) {
                result.stable = false; result.exitCode = exitCode;
                result.status = "failed: process exited during stability check";
                log.Log("STABILITY", "Process exited. PID=" + std::to_string(result.pid));
                continue;
            }
            std::string dialogFailure;
            std::vector<WindowInfo> windows = GetWindowsForPid(result.pid);
            if (DetectKnownErrorDialog(windows, log, result.pid, dialogFailure)) {
                result.stable = false; result.status = "failed: " + dialogFailure;
                log.Log("STABILITY", "Error dialog during monitor. PID=" + std::to_string(result.pid) + " reason=" + dialogFailure);
            }
        }
        Sleep(1000);
    }
}

// ---------------------------------------------------------------------------
// module lookup + export resolution 86x64
// ---------------------------------------------------------------------------
ULONGLONG RemoteModule(HANDLE hp, const wchar_t* want, TargetArch arch) {
    static pfnNtQueryInformationProcess fn = nullptr;
    if (!fn) fn = (pfnNtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    if (!fn || arch == TargetArch::Unknown) return 0;

    ULONG_PTR peb = 0;
    if (arch == TargetArch::X86) {
        if (fn(hp, 26, &peb, sizeof(peb), NULL) < 0 || !peb) return 0;
    } else {
        PROCESS_BASIC_INFORMATION_L pbi = { 0 };
        if (fn(hp, 0, &pbi, sizeof(pbi), NULL) < 0) return 0;
        peb = (ULONG_PTR)pbi.PebBaseAddress;
        if (!peb) return 0;
    }

    const bool x64 = (arch == TargetArch::X64);
    const ULONGLONG ldrOff = x64 ? 0x18 : 0x0C;
    const ULONGLONG headOff = x64 ? 0x10 : 0x0C;
    const ULONGLONG baseOff = x64 ? 0x30 : 0x18;
    const ULONGLONG lenOff = x64 ? 0x58 : 0x2C;
    const ULONGLONG bufOff = x64 ? 0x60 : 0x30;

    ULONGLONG ldr = 0;
    if (!ReadProcessMemory(hp, (void*)(peb + ldrOff), &ldr, sizeof(ldr), NULL) || !ldr) return 0;

    ULONGLONG head = ldr + headOff, cur = 0;
    if (!ReadProcessMemory(hp, (void*)head, &cur, sizeof(cur), NULL)) return 0;

    for (int i = 0; i < 512 && cur && cur != head; ++i) {
        ULONGLONG base = 0;
        USHORT len = 0;
        ULONGLONG nbuf = 0;
        if (!ReadProcessMemory(hp, (void*)(cur + baseOff), &base, sizeof(base), NULL)) break;
        if (!ReadProcessMemory(hp, (void*)(cur + lenOff), &len, sizeof(len), NULL)) break;
        if (!ReadProcessMemory(hp, (void*)(cur + bufOff), &nbuf, sizeof(nbuf), NULL)) break;
        if (base && nbuf && len && len < 128) {
            wchar_t nm[64] = { 0 };
            if (ReadProcessMemory(hp, (void*)nbuf, nm, len, NULL)) {
                nm[len / 2] = 0;
                if (_wcsicmp(nm, want) == 0) return base;
            }
        }
        if (!ReadProcessMemory(hp, (void*)cur, &cur, sizeof(cur), NULL)) break;
    }
    return 0;
}

ULONGLONG RemoteExport(HANDLE hp, ULONGLONG base, const char* name, TargetArch arch, int depth = 0) {
    if (!base || depth > 2) return 0;

    BYTE hdr[0x400] = { 0 };
    if (!ReadProcessMemory(hp, (void*)base, hdr, sizeof(hdr), NULL)) return 0;

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hdr;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return 0;
    if ((size_t)dos->e_lfanew + 0x18 + 0x78 > sizeof(hdr)) return 0;

    BYTE* ntBase = hdr + dos->e_lfanew;
    if (*(DWORD*)ntBase != IMAGE_NT_SIGNATURE) return 0;

    BYTE* opt = ntBase + 0x18;
    DWORD magic = *(DWORD*)opt;
    DWORD ddOff = 0;
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) ddOff = 0x70;
    else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) ddOff = 0x60;
    else return 0;

    DWORD expRva = *(DWORD*)(opt + ddOff + 0);
    DWORD expSize = *(DWORD*)(opt + ddOff + 4);
    if (!expRva) return 0;

    IMAGE_EXPORT_DIRECTORY ed = { 0 };
    if (!ReadProcessMemory(hp, (void*)(base + expRva), &ed, sizeof(ed), NULL)) return 0;
    if (!ed.NumberOfNames || ed.NumberOfNames > 8192) return 0;

    std::vector<DWORD> names(ed.NumberOfNames), funcs(ed.NumberOfFunctions);
    std::vector<WORD> ords(ed.NumberOfNames);
    if (!ReadProcessMemory(hp, (void*)(base + ed.AddressOfNames), names.data(), names.size() * 4, NULL)) return 0;
    if (!ReadProcessMemory(hp, (void*)(base + ed.AddressOfNameOrdinals), ords.data(), ords.size() * 2, NULL)) return 0;
    if (!ReadProcessMemory(hp, (void*)(base + ed.AddressOfFunctions), funcs.data(), funcs.size() * 4, NULL)) return 0;

    for (DWORD i = 0; i < ed.NumberOfNames; ++i) {
        char nm[64] = { 0 };
        if (!ReadProcessMemory(hp, (void*)(base + names[i]), nm, 63, NULL)) continue;
        if (strcmp(nm, name) != 0) continue;
        WORD o = ords[i];
        if (o >= funcs.size()) return 0;
        DWORD f = funcs[o];
        if (!f) return 0;
        if (f >= expRva && f < expRva + expSize) {
            char fwd[128] = { 0 };
            if (!ReadProcessMemory(hp, (void*)(base + f), fwd, 127, NULL)) return 0;
            char* dot = strchr(fwd, '.');
            if (!dot) return 0;
            *dot = 0;
            wchar_t wmod[64] = { 0 };
            MultiByteToWideChar(CP_ACP, 0, fwd, -1, wmod, 64);
            wcscat(wmod, L".dll");
            ULONGLONG fb = RemoteModule(hp, wmod, arch);
            return RemoteExport(hp, fb, dot + 1, arch, depth + 1);
        }
        return base + f;
    }
    return 0;
}

int SuspendAllThreads(DWORD pid, bool suspend, Logger& log) {
    int affected = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return -1;
    THREADENTRY32 entry; entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == 0) continue;
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
            if (!thread) continue;
            DWORD prev = suspend ? SuspendThread(thread) : ResumeThread(thread);
            if (prev != (DWORD)-1) affected++;
            CloseHandle(thread);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    log.Log("THREAD", std::string(suspend ? "Suspend" : "Resume") + " all. PID=" + std::to_string(pid) + " affected=" + std::to_string(affected));
    return affected;
}

bool UnhookNtdll(DWORD pid, HANDLE hp, TargetArch arch, Logger& log, std::string& why) {
    if (arch == TargetArch::Unknown) { why = "unknown_arch"; return false; }

    ULONGLONG base = RemoteModule(hp, L"ntdll.dll", arch);
    if (!base) { why = "ntdll_base_not_found"; return false; }

    wchar_t sysDir[MAX_PATH] = { 0 };
    if (arch == TargetArch::X86) {
        if (!GetSystemWow64DirectoryW(sysDir, MAX_PATH)) { why = "no_syswow64"; return false; }
    } else {
        UINT got = GetSystemDirectoryW(sysDir, MAX_PATH);
        if (!got || got >= MAX_PATH) { why = "no_system32"; return false; }
    }
    std::wstring path = std::wstring(sysDir) + L"\\ntdll.dll";

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) { why = "open_ntdll_" + std::to_string(GetLastError()); return false; }

    HANDLE mapping = CreateFileMappingW(file, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    if (!mapping) { why = "map_ntdll_" + std::to_string(GetLastError()); CloseHandle(file); return false; }

    LPVOID view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) { why = "view_ntdll_" + std::to_string(GetLastError()); CloseHandle(mapping); CloseHandle(file); return false; }

    bool ok = false;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)view;
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        BYTE* ntBase = (BYTE*)view + dos->e_lfanew;
        if (*(DWORD*)ntBase == IMAGE_NT_SIGNATURE) {
            IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)ntBase;
            IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
            for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
                if (memcmp(section->Name, ".text", 5) != 0) continue;

                LPVOID dest = (LPVOID)(base + section->VirtualAddress);
                SIZE_T size = section->Misc.VirtualSize;
                if (size == 0) { why = "empty_text"; break; }

                DWORD oldProtect = 0;
                if (!VirtualProtectEx(hp, dest, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    why = "vpe_" + std::to_string(GetLastError());
                    break;
                }

                SIZE_T written = 0;
                BOOL wpm = WriteProcessMemory(hp, dest, (BYTE*)view + section->VirtualAddress, size, &written);
                DWORD wpmError = GetLastError();

                DWORD restore = 0;
                VirtualProtectEx(hp, dest, size, oldProtect, &restore);
                FlushInstructionCache(hp, dest, size);

                if (wpm && written == size) ok = true;
                else why = "wpm_ntdll_" + std::to_string(wpmError);
                break;
            }
        }
    }

    UnmapViewOfFile(view); CloseHandle(mapping); CloseHandle(file);
    log.Log("NTDLL", "unhook .text PID=" + std::to_string(pid) + " arch=" + (arch == TargetArch::X64 ? "x64" : "x86") +
        " base=" + HexU64(base) + " ok=" + std::to_string(ok) + (ok ? "" : " why=" + why));
    return ok;
}

bool GrantSeDebug(HANDLE hp, Logger& log, DWORD pid) {
    HANDLE token = NULL;
    if (!OpenProcessToken(hp, TOKEN_ADJUST_PRIVILEGES, &token)) return false;
    TOKEN_PRIVILEGES tp = { 0 };
    tp.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) { CloseHandle(token); return false; }
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL adjusted = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(token);
    bool ok = adjusted && err == ERROR_SUCCESS;
    log.Log("PRIV", "Game SeDebug PID=" + std::to_string(pid) + " ok=" + std::to_string(ok));
    return ok;
}

bool IsModuleLoaded(DWORD pid, const std::wstring& baseLower) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W module; module.dwSize = sizeof(module);
    bool found = false;
    if (Module32FirstW(snapshot, &module)) {
        do {
            if (ToLowerCopy(module.szModule) == baseLower) { found = true; break; }
        } while (Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
    return found;
}


ULONGLONG ResolveLoadLibraryW(HANDLE hp, TargetArch arch, Logger& log, std::string& why) {
    HMODULE localK32 = GetModuleHandleW(L"kernel32.dll");
    if (!localK32) { why = "local_kernel32_missing"; return 0; }

    if (arch == TargetArch::X64) {
        FARPROC localLoad = GetProcAddress(localK32, "LoadLibraryW");
        if (!localLoad) localLoad = GetProcAddress(localK32, "LoadLibraryA");
        if (!localLoad) { why = "local_loadlibrary_missing"; return 0; }

        ULONGLONG offset = (ULONGLONG)localLoad - (ULONGLONG)localK32;
        ULONGLONG remoteK32 = RemoteModule(hp, L"kernel32.dll", arch);
        if (!remoteK32) { why = "kernel32_not_found"; return 0; }

        log.Log("INJECT", "offset method offset=" + HexU64(offset) + " remoteK32=" + HexU64(remoteK32));
        return remoteK32 + offset;
    }

    ULONGLONG rk = RemoteModule(hp, L"kernel32.dll", arch);
    if (rk) {
        ULONGLONG fn = RemoteExport(hp, rk, "LoadLibraryW", arch);
        if (fn) { log.Log("INJECT", "export method (kernel32)"); return fn; }
    }

    ULONGLONG kb = RemoteModule(hp, L"kernelbase.dll", arch);
    if (kb) {
        ULONGLONG fn = RemoteExport(hp, kb, "LoadLibraryW", arch);
        if (fn) { log.Log("INJECT", "export method (kernelbase)"); return fn; }
    }

    why = "loadlibraryw_not_found";
    return 0;
}

bool InjectDll(DWORD pid, HANDLE hp, const std::wstring& dllPath, TargetArch arch, Logger& log, std::string& why) {
    if (arch == TargetArch::Unknown) { why = "unknown_arch"; return false; }

    ULONGLONG loadW = ResolveLoadLibraryW(hp, arch, log, why);
    if (!loadW) return false;
    log.Log("INJECT", "LoadLibraryW=" + HexU64(loadW));

    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remotePath = VirtualAllocEx(hp, NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remotePath) { why = "virtualallocex_" + std::to_string(GetLastError()); return false; }

    SIZE_T written = 0;
    if (!WriteProcessMemory(hp, remotePath, dllPath.c_str(), bytes, &written) || written != bytes) {
        why = "writeprocessmemory_" + std::to_string(GetLastError());
        VirtualFreeEx(hp, remotePath, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)(ULONG_PTR)loadW, remotePath, 0, NULL);
    if (!thread) {
        why = "createremotethread_" + std::to_string(GetLastError());
        VirtualFreeEx(hp, remotePath, 0, MEM_RELEASE);
        return false;
    }

    size_t slash = dllPath.find_last_of(L'\\');
    std::wstring baseName = ToLowerCopy(slash == std::wstring::npos ? dllPath : dllPath.substr(slash + 1));

    DWORD waited = 0;
    while (waited < 30000) {
        if (WaitForSingleObject(thread, 500) == WAIT_OBJECT_0) break;
        waited += 500;
        if (IsModuleLoaded(pid, baseName)) break;
    }

    DWORD threadExit = 0;
    GetExitCodeThread(thread, &threadExit);
    log.Log("INJECT", "remote thread finished exit=" + HexU64(threadExit));

    CloseHandle(thread);
    VirtualFreeEx(hp, remotePath, 0, MEM_RELEASE);

    if (IsModuleLoaded(pid, baseName)) {
        log.Log("INJECT", "module confirmed. PID=" + std::to_string(pid));
        return true;
    }

    why = (threadExit == 0) ? "loadlibrary_returned_null" : "module_not_seen";
    return false;
}

// ---------------------------------------------------------------------------
// DLL 
// ---------------------------------------------------------------------------
std::vector<std::wstring> DesktopFolders() {
    std::vector<std::wstring> folders;
    const wchar_t* patterns[] = {
        L"%USERPROFILE%\\Desktop", L"%USERPROFILE%\\OneDrive\\Desktop",
        L"%USERPROFILE%\\Downloads", L"%USERPROFILE%\\OneDrive\\Desktop"
    };
    for (const wchar_t* p : patterns) {
        wchar_t exp[MAX_PATH] = { 0 };
        if (ExpandEnvironmentStringsW(p, exp, MAX_PATH) && exp[0]) folders.push_back(std::wstring(exp));
    }
    return folders;
}

std::vector<std::wstring> FindDllsInFolder(const std::wstring& folder) {
    std::vector<std::wstring> dlls;
    if (!DirectoryExistsW(folder.c_str())) return dlls;
    std::wstring search = folder + L"\\*.dll";
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(search.c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return dlls;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        dlls.push_back(folder + L"\\" + fd.cFileName);
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return dlls;
}

int ScoreDll(const std::wstring& path) {
    size_t slash = path.find_last_of(L'\\');
    std::wstring lower = ToLowerCopy(slash == std::wstring::npos ? path : path.substr(slash + 1));
    int score = 0;
    if (lower.find(L"gentahax") != std::wstring::npos) score += 100;
    if (lower.find(L"gtinternal") != std::wstring::npos) score += 50;
    return score;
}

bool FindGentaDll(std::wstring& out, Logger& log) {
    std::vector<std::wstring> candidates;
    for (const std::wstring& desktop : DesktopFolders()) {
        std::wstring search = desktop + L"\\GH*";
        WIN32_FIND_DATAW fd;
        HANDLE find = FindFirstFileW(search.c_str(), &fd);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::vector<std::wstring> dlls = FindDllsInFolder(desktop + L"\\" + fd.cFileName);
            candidates.insert(candidates.end(), dlls.begin(), dlls.end());
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }

    std::wstring best;
    int bestScore = 0;
    for (const std::wstring& c : candidates) {
        int s = ScoreDll(c);
        if (s > bestScore) { bestScore = s; best = c; }
    }
    if (bestScore > 0) { out = best; log.LogWide("GENTA", L"found automatically: " + best); return true; }
    return false;
}

// ---------------------------------------------------------------------------
// çalışır artık inş
// ---------------------------------------------------------------------------
bool LaunchGrowtopia(const wchar_t* exePath, const wchar_t* workingDirectory, int index, InstanceResult& result, Logger& log) {
    STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    std::wstring commandLine = L"\"" + std::wstring(exePath) + L"\"";
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    BOOL created = CreateProcessW(exePath, mutableCommandLine.data(), NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi);
    if (!created) {
        DWORD error = GetLastError();
        result.index = index; result.pid = 0; result.processHandle = NULL;
        result.createProcessOk = false; result.opened = false; result.stable = false;
        result.exitCode = error; result.status = "failed: CreateProcessW error " + std::to_string(error);
        log.Log("CREATE", "CreateProcessW failed. Win32=" + std::to_string(error));
        return false;
    }

    result.index = index; result.pid = pi.dwProcessId; result.processHandle = pi.hProcess;
    result.createProcessOk = true; result.exitCode = STILL_ACTIVE; result.status = "created";
    CloseHandle(pi.hThread);
    log.Log("CREATE", "CreateProcessW ok. PID=" + std::to_string(pi.dwProcessId));
    return true;
}

int main(int argc, char** argv) {
    bool diagnosticOnly = false;
    bool holdPublicMutexes = false;
    bool noGenta = false;
    int requestedCount = 0;
    std::wstring gentaDll;

    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--diagnostic") == 0) diagnosticOnly = true;
        else if (_stricmp(argv[i], "--hold-public") == 0) holdPublicMutexes = true;
        else if (_stricmp(argv[i], "--no-inject") == 0 || _stricmp(argv[i], "--no-genta") == 0) noGenta = true;
        else if (_stricmp(argv[i], "--count") == 0 && i + 1 < argc) requestedCount = atoi(argv[++i]);
        else if (_stricmp(argv[i], "--genta") == 0 && i + 1 < argc) gentaDll = Utf8ToWide(argv[++i]);
    }

    Logger log("gabb_debug.log");

    std::cout << "==================================================" << std::endl;
    std::cout << "          GABB STABILIZER v43 (x64)               " << std::endl;
    std::cout << "  1st window GENTAHAX / rest vanilla Growtopia   " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "[*] Log: gabb_debug.log" << std::endl;

    if (!log.IsOpen()) { std::cout << "[-] Could not open the log file." << std::endl; system("pause"); return 1; }

    log.Log("START", "v43. genta=" + std::string(noGenta ? "off" : "on"));

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) { log.Console("[-] ntdll is missing."); system("pause"); return 1; }

    pfnNtQuerySystemInformation NtQuerySystemInformation = (pfnNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
    pfnNtQueryObject NtQueryObject = (pfnNtQueryObject)GetProcAddress(hNtdll, "NtQueryObject");
    pfnNtSuspendProcess NtSuspendProcess = (pfnNtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
    pfnNtResumeProcess NtResumeProcess = (pfnNtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    if (!NtQuerySystemInformation || !NtQueryObject || !NtSuspendProcess || !NtResumeProcess) {
        log.Console("[-] Required NTAPI functions are missing."); system("pause"); return 1;
    }

    if (!EnableDebugPrivilege(log)) { log.Console("[-] Privilege error. Run the tool as administrator."); system("pause"); return 1; }
    log.Console("[+] SeDebugPrivilege OK.");

    wchar_t gameFolder[MAX_PATH] = { 0 }, gameExe[MAX_PATH] = { 0 };
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Growtopia", gameFolder, MAX_PATH);
    ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Growtopia\\Growtopia.exe", gameExe, MAX_PATH);
    log.LogWide("PATH", L"exe=" + std::wstring(gameExe));

    if (!FileExistsW(gameExe)) {
        log.Console("[-] Growtopia.exe not found at %LOCALAPPDATA%\\Growtopia\\Growtopia.exe");
        system("pause"); return 1;
    }

    WORD exeMachine = PeMachineFromFile(gameExe);
    TargetArch targetArch = TargetArch::Unknown;
    if (exeMachine == IMAGE_FILE_MACHINE_AMD64) targetArch = TargetArch::X64;
    else if (exeMachine == IMAGE_FILE_MACHINE_I386) targetArch = TargetArch::X86;
    log.Log("ARCH", "Growtopia.exe machine=" + HexWord(exeMachine));
    log.Console("[*] Target architecture: " + std::string(targetArch == TargetArch::X64 ? "x64" :
        (targetArch == TargetArch::X86 ? "x86" : "unknown")));

    if (diagnosticOnly) {
        ScanGrowtopiaMutexes(NtQuerySystemInformation, NtQueryObject, log, true);
        log.Console("[+] Scan finished. See gabb_debug.log.");
        system("pause"); return 0;
    }

    if (!noGenta && gentaDll.empty()) {
        std::cout << std::endl;
        std::cout << "[>] Full path to the GENTAHAX DLL" << std::endl;
        std::cout << "    (Enter = auto-detect, 0 = no injection)" << std::endl;
        std::cout << "    -> ";
        std::string line;
        std::getline(std::cin, line);
        line = TrimCopy(line);

        if (line == "0") {
            noGenta = true;
            log.Console("[*] Injection disabled.");
        } else if (line.empty()) {
            if (!FindGentaDll(gentaDll, log)) {
                noGenta = true;
                log.Console("[*] Not found automatically, continuing without injection.");
            }
        } else {
            if (line.front() == '"') {
                line.erase(line.begin());
                size_t q = line.find('"');
                if (q != std::string::npos) line = line.substr(0, q);
            }
            gentaDll = Utf8ToWide(TrimCopy(line));
        }
    }

    if (!noGenta) {
        if (gentaDll.empty()) { noGenta = true; }
        else if (!FileExistsW(gentaDll.c_str())) {
            log.Console("[-] DLL not found, continuing without injection: " + WideToUtf8(gentaDll));
            gentaDll.clear(); noGenta = true;
        } else {
            log.Console("[+] GENTAHAX DLL: " + WideToUtf8(gentaDll));
        }
    }

    if (!noGenta) {
        if (targetArch == TargetArch::Unknown) {
            log.Console("[!] Target architecture unknown, injection may be unreliable.");
        } else if (!DllMatchesArch(gentaDll, targetArch, log)) {
            log.Console("[-] DLL architecture does not match the target (DLL=" + HexWord(PeMachineFromFile(gentaDll)) +
                " target=" + HexWord(exeMachine) + "). An x64 Growtopia needs an x64 GTInternal.dll.");
            system("pause"); return 1;
        } else {
            log.Console("[+] DLL architecture matches the target.");
        }
    }

    if (requestedCount <= 0) {
        std::cout << "\n[>] How many windows? (1st is Genta, the rest are vanilla) -> ";
        std::cin >> requestedCount;
        std::cin.ignore(1024, '\n');
    }
    if (requestedCount <= 0) { log.Console("[-] Invalid count."); system("pause"); return 0; }

    log.Log("INPUT", "requestedCount=" + std::to_string(requestedCount) + " genta=" + std::string(noGenta ? "off" : "on"));

    std::vector<InstanceResult> results;
    results.reserve(requestedCount);
    std::vector<PublicMutexInfo> publicMutexes;

    for (int i = 0; i < requestedCount; i++) {
        const std::string tag = "[" + std::to_string(i + 1) + "/" + std::to_string(requestedCount) + "]";
        const bool isGenta = (i == 0 && !noGenta);

        log.Console("");
        log.Console("[*] " + tag + " preparing (" + (isGenta ? "GENTAHAX" : "vanilla") + ")...");

        std::vector<DWORD> suspendedPids;
        std::vector<std::wstring> releasedPublicNames;
        std::vector<std::wstring> preLaunchClosedNames;

        if (i > 0) {
            std::vector<DWORD> alivePids = GetAliveResultPids(results);
            suspendedPids = SuspendProcesses(alivePids, NtSuspendProcess, log);
            releasedPublicNames = ReleasePublicMutexes(publicMutexes, log);
            if (!alivePids.empty()) {
                std::vector<TargetHandleInfo> existing = ScanGrowtopiaMutexes(NtQuerySystemInformation, NtQueryObject, log, false, &alivePids, true);
                int preClosed = CloseTargetMutexes(existing, log, &preLaunchClosedNames);
                log.Console("[RESULT] Previous locks closed: " + std::to_string(preClosed));
            }
            Sleep(150);
        }

        InstanceResult result = {};
        result.index = i + 1;
        result.genta = isGenta;
        result.status = "skipped";

        if (!LaunchGrowtopia(gameExe, gameFolder, i + 1, result, log)) {
            CreatePublicMutexesForNames(releasedPublicNames, publicMutexes, log);
            ResumeProcesses(suspendedPids, NtResumeProcess, log);
            results.push_back(result);
            log.Console("[-] " + tag + " FAILED: CreateProcessW");
            continue;
        }
        log.Console("[+] " + tag + " PID " + std::to_string(result.pid));

        TargetArch procArch = DetectProcessArch(result.processHandle, log, result.pid);
        TargetArch useArch = (procArch != TargetArch::Unknown) ? procArch : targetArch;

        std::vector<std::wstring> closedNames;
        int caught = WaitForNewProcessMutexAndClose(result.pid, NtQuerySystemInformation, NtQueryObject, log, true, &closedNames, 10000);
        log.Console("[RESULT] Locks caught: " + std::to_string(caught));
        AppendNames(closedNames, preLaunchClosedNames);

        std::string failureReason;
        bool verified = false;

        if (caught > 0 && isGenta) {
            verified = VerifyVisibleWindowOrFailure(result.processHandle, result.pid, log, failureReason, 30000);
            if (verified) {
                std::vector<HWND> baseline;
                for (const WindowInfo& w : GetWindowsForPid(result.pid)) baseline.push_back(w.hwnd);

                int susp = SuspendAllThreads(result.pid, true, log);
                log.Console("[*] " + tag + " threads suspended: " + std::to_string(susp));

                std::string unhookWhy;
                bool unhooked = UnhookNtdll(result.pid, result.processHandle, useArch, log, unhookWhy);
                log.Console("[*] " + tag + " ntdll unhook: " + (unhooked ? "OK" : "FAILED (" + unhookWhy + ")"));

                GrantSeDebug(result.processHandle, log, result.pid);

                std::string injectWhy;
                bool injected = InjectDll(result.pid, result.processHandle, gentaDll, useArch, log, injectWhy);
                log.Console("[*] " + tag + " injection: " + (injected ? "OK" : "FAILED (" + injectWhy + ")"));

                SuspendAllThreads(result.pid, false, log);

                if (injected) {
                    if (!WaitForGentaWindow(result.pid, baseline, log, 15000))
                        log.Console("[!] " + tag + " genta window never appeared, the game may have opened vanilla.");
                    else
                        log.Console("[+] " + tag + " genta window detected.");
                } else {
                    failureReason = "inject_failed_" + injectWhy;
                    verified = false;
                }
            }
        } else if (caught > 0) {
            verified = VerifyVisibleWindowOrFailure(result.processHandle, result.pid, log, failureReason, 12000);
        }

        if (verified) {
            std::string quietFailure;
            if (WaitForProcessQuiet(result.processHandle, result.pid, log, 3500, quietFailure)) {
                int publicCount = 0;
                if (holdPublicMutexes && i < requestedCount - 1)
                    publicCount = CreatePublicMutexesForNames(closedNames, publicMutexes, log);

                result.opened = true;
                result.stable = true;
                result.status = "opened: " + std::string(isGenta ? "GENTAHAX" : "vanilla") + ", public=" + std::to_string(publicCount);
                log.Console("[+] " + tag + " OPENED (" + (isGenta ? "GENTAHAX" : "vanilla") + ").");
            } else {
                CreatePublicMutexesForNames(releasedPublicNames, publicMutexes, log);
                result.opened = true; result.stable = false;
                result.status = "failed: " + quietFailure;
                log.Console("[-] " + tag + " FAILED: " + quietFailure);
            }
        } else {
            CreatePublicMutexesForNames(releasedPublicNames, publicMutexes, log);
            result.opened = false; result.stable = false;
            result.status = "failed: " + (caught > 0 ? failureReason : std::string("mutex_not_caught"));
            log.Console("[-] " + tag + " FAILED: " + result.status.substr(8));
        }

        ResumeProcesses(suspendedPids, NtResumeProcess, log);
        results.push_back(result);
        Sleep(700);
    }

    MonitorStability(results, log, 30);

    bool allStable = true;
    log.Console("");
    log.Console("==================== SUMMARY ====================");
    for (InstanceResult& result : results) {
        if (result.createProcessOk && result.processHandle != NULL) {
            DWORD exitCode = 0;
            if (!IsProcessAlive(result.processHandle, &exitCode)) {
                result.stable = false;
                if (result.status.find("failed") == std::string::npos) result.status = "failed: process exited";
            }
        }
        std::string line = "[" + std::to_string(result.index) + "/" + std::to_string(requestedCount) + "] ";
        if (!result.createProcessOk || !result.opened || !result.stable) {
            allStable = false;
            line += "FAILED pid=" + std::to_string(result.pid) + " " + result.status;
        } else {
            line += "OPENED_STABLE pid=" + std::to_string(result.pid) + " " + result.status;
        }
        log.Console(line);
    }

    for (InstanceResult& result : results)
        if (result.processHandle != NULL) { CloseHandle(result.processHandle); result.processHandle = NULL; }

    log.Console(allStable ? "[+] DONE - every window opened and stayed stable." : "[-] INCOMPLETE - send gabb_debug.log.");
    system("pause");
    return allStable ? 0 : 2;
}
