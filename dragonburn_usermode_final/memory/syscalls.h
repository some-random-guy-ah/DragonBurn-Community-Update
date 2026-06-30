#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>

// ─── NT Structures ───
typedef struct _OBJECT_ATTRIBUTES_NT {
    ULONG  Length;
    HANDLE RootDirectory;
    PVOID  ObjectName;
    ULONG  Attributes;
    PVOID  SecurityDescriptor;
    PVOID  SecurityQualityOfService;
} OBJECT_ATTRIBUTES_NT;

typedef struct _CLIENT_ID_NT {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID_NT;

typedef struct _UNICODE_STRING_NT {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_NT;

typedef struct _LARGE_UNICODE_STRING {
    ULONG  Length;
    ULONG  MaximumLength : 31;
    ULONG  bAnsi : 1;
    PVOID  Buffer;
} LARGE_UNICODE_STRING;

// ─── Hardened Syscall Invoker ───
//
// Fixes:
//   1. No RWX memory — use RX after writing (W^X policy)
//   2. Stubs mimic real ntdll layout to avoid signature detection
//   3. Stubs are placed inside ntdll's memory range (indirect syscall)
//      OR we use the "trampoline" technique

class SyscallInvoker {
public:
    ~SyscallInvoker() {
        auto cleanup = [](BYTE*& mem) {
            if (mem) {
                DWORD old;
                VirtualProtect(mem, 4096, PAGE_READWRITE, &old);
                SecureZeroMemory(mem, 4096);
                VirtualFree(mem, 0, MEM_RELEASE);
                mem = nullptr;
            }
        };
        cleanup(stub_mem_);
        cleanup(win32k_stub_mem_);
    }

    bool init_nt() {
        // ── NT syscall stubs (your existing code) ──
        stub_mem_ = (BYTE*)VirtualAlloc(
            nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
        if (!stub_mem_) return false;
        memset(stub_mem_, 0x90, 4096);

        syscall_ret_gadget_ = find_syscall_ret_gadget(L"ntdll.dll");
        if (!syscall_ret_gadget_) {
            printf("[-] Could not find syscall;ret gadget in ntdll\n");
            use_indirect_ = false;
        } else {
            printf("[+] ntdll syscall;ret gadget: %p\n", syscall_ret_gadget_);
            use_indirect_ = true;
        }

        build_stub(stub_mem_, IDX_OPEN,  0, syscall_ret_gadget_, use_indirect_);
        build_stub(stub_mem_, IDX_READ,  0, syscall_ret_gadget_, use_indirect_);
        build_stub(stub_mem_, IDX_CLOSE, 0, syscall_ret_gadget_, use_indirect_);

        DWORD old_protect;
        VirtualProtect(stub_mem_, 4096, PAGE_EXECUTE_READ, &old_protect);

        return true;
    }

    bool init_win32k() {
        // ── Win32k syscall stubs
        win32k_stub_mem_ = (BYTE*)VirtualAlloc(
            nullptr, 4096,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
        if (!win32k_stub_mem_) return false;
        memset(win32k_stub_mem_, 0x90, 4096);

        // win32u.dll gadget — separate from ntdll
        // RIP must point inside win32u for GUI syscalls
        // to look legitimate
        win32k_gadget_ = find_syscall_ret_gadget(L"win32u.dll");
        if (!win32k_gadget_) {
            printf("[-] Could not find syscall;ret gadget in win32u\n");
            use_indirect_win32k_ = false;
        } else {
            printf("[+] win32u syscall;ret gadget: %p\n", win32k_gadget_);
            use_indirect_win32k_ = true;
        }

        // Pre-build all win32k stubs with SSN=0 (patched later)
        build_stub(win32k_stub_mem_, IDX_FIND_WINDOW,    0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_SHOW_WINDOW,    0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_SET_WINDOW_POS, 0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_POST_MESSAGE,   0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_SET_WINDOW_LONG,0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_GET_DC,         0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_RELEASE_DC,     0, win32k_gadget_, use_indirect_win32k_);

        build_stub(win32k_stub_mem_, IDX_DESTROY_WINDOW,  0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_ATTACH_THREAD,   0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_SET_FOREGROUND,  0, win32k_gadget_, use_indirect_win32k_);
        build_stub(win32k_stub_mem_, IDX_SET_FOCUS,       0, win32k_gadget_, use_indirect_win32k_);

        DWORD old_protect;
        VirtualProtect(win32k_stub_mem_, 4096, PAGE_EXECUTE_READ, &old_protect);

        return true;
    }

    // ── Set SSN for NT stubs ──
    void set_ssn(int index, DWORD ssn) {
        patch_stub(stub_mem_, index, ssn, syscall_ret_gadget_, use_indirect_);
    }

    // ── Set SSN for Win32k stubs ──
    void set_win32k_ssn(int index, DWORD ssn) {
        patch_stub(win32k_stub_mem_, index, ssn, win32k_gadget_, use_indirect_win32k_);
    }

    // ── Get stub pointers ──
    void* get_stub(int index) const {
        if (!stub_mem_) return nullptr;
        return stub_mem_ + index * STRIDE;
    }

    void* get_win32k_stub(int index) const {
        if (!win32k_stub_mem_) return nullptr;
        return win32k_stub_mem_ + index * STRIDE;
    }

    bool is_ready() const {
        return stub_mem_ != nullptr && win32k_stub_mem_ != nullptr;
    }

    // ── NT syscall indices (existing) ──
    static constexpr int IDX_OPEN  = 0;
    static constexpr int IDX_READ  = 1;
    static constexpr int IDX_CLOSE = 2;

    // ── Win32k syscall indices (new) ──
    static constexpr int IDX_FIND_WINDOW     = 0;
    static constexpr int IDX_SHOW_WINDOW     = 1;
    static constexpr int IDX_SET_WINDOW_POS  = 2;
    static constexpr int IDX_POST_MESSAGE    = 3;
    static constexpr int IDX_SET_WINDOW_LONG = 4;
    static constexpr int IDX_GET_DC          = 5;
    static constexpr int IDX_RELEASE_DC      = 6;

    static constexpr int IDX_DESTROY_WINDOW   = 9;
    static constexpr int IDX_ATTACH_THREAD    = 12;
    static constexpr int IDX_SET_FOREGROUND   = 13;
    static constexpr int IDX_SET_FOCUS        = 14;

private:
    static constexpr int STRIDE = 32;

    // NT syscall infrastructure
    BYTE* stub_mem_              = nullptr;
    void* syscall_ret_gadget_    = nullptr;
    bool  use_indirect_          = false;

    // Win32k syscall infrastructure
    BYTE* win32k_stub_mem_       = nullptr;
    void* win32k_gadget_         = nullptr;
    bool  use_indirect_win32k_   = false;

    // ── Generic gadget finder (works for any module) ──
    static void* find_syscall_ret_gadget(const wchar_t* module_name) {
        HMODULE mod = GetModuleHandleW(module_name);
        if (!mod) return nullptr;

        BYTE* base = (BYTE*)mod;
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        auto* sec = IMAGE_FIRST_SECTION(nt);

        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                BYTE* start = base + sec[i].VirtualAddress;
                DWORD size  = sec[i].Misc.VirtualSize;

                for (DWORD j = 0; j < size - 2; j++) {
                    if (start[j]     == 0x0F &&
                        start[j + 1] == 0x05 &&
                        start[j + 2] == 0xC3) {
                        return start + j;
                    }
                }
            }
        }
        return nullptr;
    }

    // ── Generic stub builder ──
    static void build_stub(BYTE* base_mem, int index, DWORD ssn,
                           void* gadget, bool use_indirect)
    {
        BYTE* p = base_mem + index * STRIDE;

        if (use_indirect && gadget) {
            int off = 0;

            // mov r10, rcx
            p[off++] = 0x4C;
            p[off++] = 0x8B;
            p[off++] = 0xD1;

            // mov eax, imm32 (SSN)
            p[off++] = 0xB8;
            memcpy(p + off, &ssn, 4);
            off += 4;

            // jmp qword ptr [rip+0]
            p[off++] = 0xFF;
            p[off++] = 0x25;
            p[off++] = 0x00;
            p[off++] = 0x00;
            p[off++] = 0x00;
            p[off++] = 0x00;

            // 8-byte absolute address of gadget
            uintptr_t addr = (uintptr_t)gadget;
            memcpy(p + off, &addr, 8);
            off += 8;

            while (off < STRIDE) p[off++] = 0x90;
        } else {
            int off = 0;
            p[off++] = 0x4C; p[off++] = 0x8B; p[off++] = 0xD1;
            p[off++] = 0xB8;
            memcpy(p + off, &ssn, 4);
            off += 4;
            p[off++] = 0x0F; p[off++] = 0x05;
            p[off++] = 0xC3;
            while (off < STRIDE) p[off++] = 0x90;
        }
    }

    // ── Generic SSN patcher ──
    void patch_stub(BYTE* base_mem, int index, DWORD ssn,
                    void* gadget, bool use_indirect)
    {
        DWORD old;
        VirtualProtect(base_mem, 4096, PAGE_READWRITE, &old);
        build_stub(base_mem, index, ssn, gadget, use_indirect);
        VirtualProtect(base_mem, 4096, PAGE_EXECUTE_READ, &old);
        FlushInstructionCache(GetCurrentProcess(),
                              base_mem + index * STRIDE, STRIDE);
    }
};

// ─── SSN Resolver (reads clean ntdll from disk) ───

class SyscallResolver {
public:
    // ── NT SSNs (existing) ──
    DWORD ssn_open  = 0;
    DWORD ssn_read  = 0;
    DWORD ssn_close = 0;

    // ── Win32k SSNs (new) ──
    DWORD ssn_find_window     = 0;
    DWORD ssn_show_window     = 0;
    DWORD ssn_set_window_pos  = 0;
    DWORD ssn_post_message    = 0;
    DWORD ssn_set_window_long = 0;
    DWORD ssn_get_dc          = 0;
    DWORD ssn_release_dc      = 0;

    DWORD ssn_destroy_window  = 0;
    DWORD ssn_attach_thread   = 0;
    DWORD ssn_set_foreground  = 0;
    DWORD ssn_set_focus       = 0;

    bool resolve_nt() {
        return resolve_nt_internal();
    }

    bool resolve_win32k() {
        return resolve_win32k_internal();
    }

private:
    // ── NT resolution (your existing code, unchanged) ──
    bool resolve_nt_internal() {
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        wcscat_s(path, L"\\ntdll.dll");

        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return resolve_nt_in_memory();

        HANDLE mapping = CreateFileMappingW(
            file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr
        );
        CloseHandle(file);
        if (!mapping) return resolve_nt_in_memory();

        BYTE* base = (BYTE*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(mapping);
        if (!base) return resolve_nt_in_memory();

        ssn_open  = extract_ssn_from_mapped(base, "NtOpenProcess");
        ssn_read  = extract_ssn_from_mapped(base, "NtReadVirtualMemory");
        ssn_close = extract_ssn_from_mapped(base, "NtClose");

        UnmapViewOfFile(base);

        if (!ssn_open || !ssn_read || !ssn_close)
            return resolve_nt_in_memory();

        printf("[+] NtOpenProcess:       0x%04X\n", ssn_open);
        printf("[+] NtReadVirtualMemory: 0x%04X\n", ssn_read);
        printf("[+] NtClose:             0x%04X\n", ssn_close);
        return true;
    }

    bool resolve_nt_in_memory() {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return false;

        if (!ssn_open)  ssn_open  = extract_ssn_getproc(ntdll, "NtOpenProcess");
        if (!ssn_read)  ssn_read  = extract_ssn_getproc(ntdll, "NtReadVirtualMemory");
        if (!ssn_close) ssn_close = extract_ssn_getproc(ntdll, "NtClose");

        return ssn_open && ssn_read && ssn_close;
    }

    // ── Win32k resolution (new) ──
    bool resolve_win32k_internal() {
        // Ensure win32u.dll is loaded
        HMODULE win32u = GetModuleHandleW(L"win32u.dll");
        if (!win32u) {
            printf("[-] Failed to load win32u.dll\n");
            return false;
        }

        // Try clean-from-disk first
        if (resolve_win32k_from_disk()) {
            print_win32k_results();
            return validate_win32k();
        }

        // Fallback: manual EAT walk of in-memory win32u
        // (avoids GetProcAddress which is hookable/logged)
        if (resolve_win32k_eat(win32u)) {
            print_win32k_results();
            return validate_win32k();
        }

        // Last resort: GetProcAddress
        resolve_win32k_getproc(win32u);
        print_win32k_results();
        return validate_win32k();
    }

    bool resolve_win32k_from_disk() {
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        wcscat_s(path, L"\\win32u.dll");

        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;

        HANDLE mapping = CreateFileMappingW(
            file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr
        );
        CloseHandle(file);
        if (!mapping) return false;

        BYTE* base = (BYTE*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(mapping);
        if (!base) return false;

        ssn_find_window     = extract_ssn_from_mapped(base, "NtUserFindWindowEx");
        ssn_show_window     = extract_ssn_from_mapped(base, "NtUserShowWindow");
        ssn_set_window_pos  = extract_ssn_from_mapped(base, "NtUserSetWindowPos");
        ssn_post_message    = extract_ssn_from_mapped(base, "NtUserPostMessage");
        ssn_set_window_long = extract_ssn_from_mapped(base, "NtUserSetWindowLongPtr");
        ssn_get_dc          = extract_ssn_from_mapped(base, "NtUserGetDC");
        ssn_release_dc      = extract_ssn_from_mapped(base, "NtUserReleaseDC");

        ssn_destroy_window  = extract_ssn_from_mapped(base, "NtUserDestroyWindow");
        ssn_attach_thread          = extract_ssn_from_mapped(base, "NtUserAttachThreadInput");
        ssn_set_foreground      = extract_ssn_from_mapped(base, "NtUserSetForegroundWindow");
        ssn_set_focus          = extract_ssn_from_mapped(base, "NtUserSetFocus");

        UnmapViewOfFile(base);
        return validate_win32k();
    }

    bool resolve_win32k_eat(HMODULE mod) {
        BYTE* base = (BYTE*)mod;
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return false;

        auto* exp   = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
        auto* names = (DWORD*)(base + exp->AddressOfNames);
        auto* funcs = (DWORD*)(base + exp->AddressOfFunctions);
        auto* ords  = (WORD*)(base + exp->AddressOfNameOrdinals);

        struct { const char* name; DWORD* out; } targets[] = {
            { "NtUserFindWindowEx",     &ssn_find_window     },
            { "NtUserShowWindow",       &ssn_show_window     },
            { "NtUserSetWindowPos",     &ssn_set_window_pos  },
            { "NtUserPostMessage",      &ssn_post_message    },
            { "NtUserSetWindowLongPtr", &ssn_set_window_long },
            { "NtUserGetDC",            &ssn_get_dc          },
            { "NtUserReleaseDC",        &ssn_release_dc      },

            { "NtUserDestroyWindow",      &ssn_destroy_window  },
            { "NtUserAttachThreadInput",  &ssn_attach_thread   },
            { "NtUserSetForegroundWindow",&ssn_set_foreground  },
            { "NtUserSetFocus",           &ssn_set_focus       },
        };

        for (DWORD i = 0; i < exp->NumberOfNames; i++) {
            const char* name = (const char*)(base + names[i]);
            for (auto& t : targets) {
                if (*t.out == 0 && strcmp(name, t.name) == 0) {
                    BYTE* func = base + funcs[ords[i]];
                    *t.out = parse_stub(func);
                }
            }
        }
        return validate_win32k();
    }

    void resolve_win32k_getproc(HMODULE mod) {
        if (!ssn_find_window)     ssn_find_window     = extract_ssn_getproc(mod, "NtUserFindWindowEx");
        if (!ssn_show_window)     ssn_show_window     = extract_ssn_getproc(mod, "NtUserShowWindow");
        if (!ssn_set_window_pos)  ssn_set_window_pos  = extract_ssn_getproc(mod, "NtUserSetWindowPos");
        if (!ssn_post_message)    ssn_post_message    = extract_ssn_getproc(mod, "NtUserPostMessage");
        if (!ssn_set_window_long) ssn_set_window_long = extract_ssn_getproc(mod, "NtUserSetWindowLongPtr");
        if (!ssn_get_dc)          ssn_get_dc          = extract_ssn_getproc(mod, "NtUserGetDC");
        if (!ssn_release_dc)      ssn_release_dc      = extract_ssn_getproc(mod, "NtUserReleaseDC");

        if (!ssn_destroy_window)  ssn_set_window_pos  = extract_ssn_getproc(mod, "NtUserDestroyWindow");
        if (!ssn_attach_thread)          ssn_get_dc          = extract_ssn_getproc(mod, "NtUserAttachThreadInput");
        if (!ssn_set_foreground)      ssn_release_dc      = extract_ssn_getproc(mod, "NtUserSetForegroundWindow");
        if (!ssn_set_focus)          ssn_get_dc          = extract_ssn_getproc(mod, "NtUserSetFocus");
    }

    bool validate_win32k() {
        // At minimum we need FindWindow to be useful
        return ssn_find_window != 0;
    }

    void print_win32k_results() {
        printf("[%c] NtUserFindWindowEx:     0x%04X\n", ssn_find_window     ? '+' : '-', ssn_find_window);
        printf("[%c] NtUserShowWindow:       0x%04X\n", ssn_show_window     ? '+' : '-', ssn_show_window);
        printf("[%c] NtUserSetWindowPos:     0x%04X\n", ssn_set_window_pos  ? '+' : '-', ssn_set_window_pos);
        printf("[%c] NtUserPostMessage:      0x%04X\n", ssn_post_message    ? '+' : '-', ssn_post_message);
        printf("[%c] NtUserSetWindowLongPtr: 0x%04X\n", ssn_set_window_long ? '+' : '-', ssn_set_window_long);
        printf("[%c] NtUserGetDC:            0x%04X\n", ssn_get_dc          ? '+' : '-', ssn_get_dc);
        printf("[%c] NtUserReleaseDC:        0x%04X\n", ssn_release_dc      ? '+' : '-', ssn_release_dc);

        printf("[%c] NtUserDestroyWindow:      0x%04X\n", ssn_destroy_window  ? '+' : '-', ssn_destroy_window);
        printf("[%c] NtUserAttachThreadInput:  0x%04X\n", ssn_attach_thread   ? '+' : '-', ssn_attach_thread);
        printf("[%c] NtUserSetForegroundWindow:0x%04X\n", ssn_set_foreground  ? '+' : '-', ssn_set_foreground);
        printf("[%c] NtUserSetFocus:           0x%04X\n", ssn_set_focus       ? '+' : '-', ssn_set_focus);
    }

    // ── Shared parsing (works for both ntdll and win32u stubs) ──
    static DWORD extract_ssn_getproc(HMODULE mod, const char* name) {
        BYTE* func = (BYTE*)GetProcAddress(mod, name);
        if (!func) return 0;
        return parse_stub(func);
    }

    static DWORD extract_ssn_from_mapped(BYTE* base, const char* func_name) {
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return 0;

        auto* exp   = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
        auto* names = (DWORD*)(base + exp->AddressOfNames);
        auto* funcs = (DWORD*)(base + exp->AddressOfFunctions);
        auto* ords  = (WORD*)(base + exp->AddressOfNameOrdinals);

        for (DWORD i = 0; i < exp->NumberOfNames; i++) {
            if (strcmp((const char*)(base + names[i]), func_name) == 0) {
                BYTE* func = base + funcs[ords[i]];
                return parse_stub(func);
            }
        }
        return 0;
    }

    static DWORD parse_stub(BYTE* func) {
        // Standard: 4C 8B D1 B8 [SSN] ...
        if (func[0] == 0x4C && func[1] == 0x8B &&
            func[2] == 0xD1 && func[3] == 0xB8) {
            return *(DWORD*)(func + 4);
        }
        // Scan fallback
        for (int i = 0; i < 32; i++) {
            if (func[i] == 0xB8) {
                DWORD val = *(DWORD*)(func + i + 1);
                if (val > 0 && val < 0x2000) return val;
            }
        }
        return 0;
    }
};

class Win32kSyscall {
public:
    bool init(SyscallInvoker& invoker, SyscallResolver& resolver) {
        invoker_ = &invoker;

        // Patch all win32k stubs with resolved SSNs
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_FIND_WINDOW,    resolver.ssn_find_window);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_SHOW_WINDOW,    resolver.ssn_show_window);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_SET_WINDOW_POS, resolver.ssn_set_window_pos);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_POST_MESSAGE,   resolver.ssn_post_message);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_SET_WINDOW_LONG,resolver.ssn_set_window_long);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_GET_DC,         resolver.ssn_get_dc);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_RELEASE_DC,     resolver.ssn_release_dc);

        invoker_->set_win32k_ssn(SyscallInvoker::IDX_DESTROY_WINDOW,  resolver.ssn_destroy_window);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_ATTACH_THREAD,   resolver.ssn_attach_thread);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_SET_FOREGROUND,  resolver.ssn_set_foreground);
        invoker_->set_win32k_ssn(SyscallInvoker::IDX_SET_FOCUS,       resolver.ssn_set_focus);

        return true;
    }

    // ── NtUserFindWindowEx ──
    // Signature: HWND NtUserFindWindowEx(
    //     HWND hwndParent, HWND hwndChild,
    //     PUNICODE_STRING pClassName, PUNICODE_STRING pWindowName,
    //     DWORD dwType)
    HWND find_window(const wchar_t* class_name, const wchar_t* window_name) {
        UNICODE_STRING_NT cls = {};
        UNICODE_STRING_NT wnd = {};

        if (class_name) {
            cls.Buffer = (PWSTR)class_name;
            cls.Length = (USHORT)(wcslen(class_name) * sizeof(wchar_t));
            cls.MaximumLength = cls.Length + sizeof(wchar_t);
        }
        if (window_name) {
            wnd.Buffer = (PWSTR)window_name;
            wnd.Length = (USHORT)(wcslen(window_name) * sizeof(wchar_t));
            wnd.MaximumLength = wnd.Length + sizeof(wchar_t);
        }

        using fn_t = HWND(__stdcall*)(
            HWND, HWND,
            UNICODE_STRING_NT*, UNICODE_STRING_NT*,
            DWORD
        );

        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_FIND_WINDOW);

        return fn(
            nullptr,                        // hwndParent (desktop)
            nullptr,                        // hwndChildAfter
            &cls,
            &wnd,
            0                               // dwType
        );
    }

    // ── NtUserShowWindow ──
    // Signature: BOOL NtUserShowWindow(HWND hwnd, int nCmdShow)
    BOOL show_window(HWND hwnd, int cmd_show) {
        using fn_t = BOOL(__stdcall*)(HWND, int);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_SHOW_WINDOW);
        return fn(hwnd, cmd_show);
    }

    // ── NtUserSetWindowPos ──
    // Signature: BOOL NtUserSetWindowPos(
    //     HWND hwnd, HWND hwndInsertAfter,
    //     int x, int y, int cx, int cy, UINT uFlags)
    BOOL set_window_pos(HWND hwnd, HWND insert_after,
                        int x, int y, int cx, int cy, UINT flags)
    {
        using fn_t = BOOL(__stdcall*)(HWND, HWND, int, int, int, int, UINT);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_SET_WINDOW_POS);
        return fn(hwnd, insert_after, x, y, cx, cy, flags);
    }

    // ── NtUserPostMessage ──
    // Signature: BOOL NtUserPostMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    BOOL post_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        using fn_t = BOOL(__stdcall*)(HWND, UINT, WPARAM, LPARAM);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_POST_MESSAGE);
        return fn(hwnd, msg, wp, lp);
    }

    // ── NtUserGetDC ──
    HDC get_dc(HWND hwnd) {
        using fn_t = HDC(__stdcall*)(HWND);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_GET_DC);
        return fn(hwnd);
    }

    // ── NtUserReleaseDC ──
    int release_dc(HWND hwnd, HDC hdc) {
        using fn_t = int(__stdcall*)(HWND, HDC);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_RELEASE_DC);
        return fn(hwnd, hdc);
    }

    LONG_PTR set_window_long(HWND hwnd, int index, LONG_PTR new_value) {
        using fn_t = LONG_PTR(__stdcall*)(HWND, int, LONG_PTR);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_SET_WINDOW_LONG);
        return fn(hwnd, index, new_value);
    }

    // DestroyWindow → NtUserDestroyWindow
    BOOL destroy_window(HWND hwnd) {
        using fn_t = BOOL(__stdcall*)(HWND);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_DESTROY_WINDOW);
        return fn(hwnd);
    }

    // AttachThreadInput → NtUserAttachThreadInput
    BOOL attach_thread_input(DWORD id_attach, DWORD id_attach_to, BOOL attach) {
        using fn_t = BOOL(__stdcall*)(DWORD, DWORD, BOOL);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_ATTACH_THREAD);
        return fn(id_attach, id_attach_to, attach);
    }

    // SetForegroundWindow → NtUserSetForegroundWindow
    BOOL set_foreground_window(HWND hwnd) {
        using fn_t = BOOL(__stdcall*)(HWND);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_SET_FOREGROUND);
        return fn(hwnd);
    }

    // SetFocus → NtUserSetFocus
    HWND set_focus(HWND hwnd) {
        using fn_t = HWND(__stdcall*)(HWND);
        auto fn = (fn_t)invoker_->get_win32k_stub(SyscallInvoker::IDX_SET_FOCUS);
        return fn(hwnd);
    }

private:
    SyscallInvoker* invoker_ = nullptr;
};