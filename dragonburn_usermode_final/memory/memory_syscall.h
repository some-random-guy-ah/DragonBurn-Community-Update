#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>

#include "imemory.h"
#include "memory_utils.h"
#include "syscalls.h"


// ─── Hardened Memory Class ───

class MemorySyscall : public IMemory {
public:
    MemorySyscall() = default;
    ~MemorySyscall() override { close(); }

    MemorySyscall(const MemorySyscall&) = delete;
    MemorySyscall& operator=(const MemorySyscall&) = delete;

    bool attach(const wchar_t* process_name) override {
        if (!initialized_) {
            if (!do_init()) return false;
            initialized_ = true;
        }

        pid_ = find_process(process_name);
        if (!pid_) {
            printf("[-] Process not found\n");
            return false;
        }

        CLIENT_ID_NT cid{};
        cid.UniqueProcess = (HANDLE)(uintptr_t)pid_;
        OBJECT_ATTRIBUTES_NT oa{};
        oa.Length = sizeof(oa);

        HANDLE handle = nullptr;
        using fn_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK,
                                       OBJECT_ATTRIBUTES_NT*, CLIENT_ID_NT*);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_OPEN);
        if (!fn) return false;

        NTSTATUS status = fn(&handle, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                             &oa, &cid);
        if (status != 0 || !handle) {
            printf("[-] NtOpenProcess: 0x%08lX\n", status);
            return false;
        }

        process_ = handle;
        m_modules.client = get_module_base(pid_, L"client.dll", &m_modules.client_size);
        m_modules.engine2 = get_module_base(pid_, L"engine2.dll", &m_modules.engine2_size);
        m_modules.schemasystem = get_module_base(pid_, L"schemasystem.dll", &m_modules.schemasystem_size);
        m_modules.tier0 = get_module_base(pid_, L"tier0.dll", &m_modules.tier0_size);
        m_modules.vphysics2 = get_module_base(pid_, L"vphysics2.dll", &m_modules.vphysics2_size);

        return m_modules.client != 0;
    }

    void close() override {
        if (process_ && invoker_.is_ready()) {
            using fn_t = NTSTATUS(NTAPI*)(HANDLE);
            auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_CLOSE);
            if (fn) fn(process_);
            process_ = nullptr;
        }
        pid_ = 0;
        m_modules = {};
    }

    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        if (!process_ || !invoker_.is_ready()) return value;
        SIZE_T br = 0;
        using fn_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_READ);
        if (fn) fn(process_, (PVOID)address, &value, sizeof(T), &br);
        return value;
    }

    bool read_raw(uintptr_t address, void* buffer, size_t size) const override {
        if (!process_ || !invoker_.is_ready()) return false;
        SIZE_T br = 0;
        using fn_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
        auto fn = (fn_t)invoker_.get_stub(SyscallInvoker::IDX_READ);
        if (!fn) return false;
        NTSTATUS s = fn(process_, (PVOID)address, buffer, size, &br);
        return s == 0 && br == size;
    }

    uintptr_t get_client_base() const override { return m_modules.client; }
    DWORD get_pid() const override { return pid_; }

private:
    HANDLE    process_     = nullptr;
    DWORD     pid_         = 0;
    bool      initialized_ = false;

    SyscallResolver resolver_;
    SyscallInvoker  invoker_;

    bool do_init() {
        if (!resolver_.resolve_nt()) return false;
        if (!invoker_.init_nt()) return false;

        invoker_.set_ssn(SyscallInvoker::IDX_OPEN,  resolver_.ssn_open);
        invoker_.set_ssn(SyscallInvoker::IDX_READ,  resolver_.ssn_read);
        invoker_.set_ssn(SyscallInvoker::IDX_CLOSE, resolver_.ssn_close);

        if (!resolver_.resolve_win32k()) return false;
        if (!invoker_.init_win32k()) return false;

        return invoker_.is_ready();
    }

    static uintptr_t get_module_base(DWORD p, const wchar_t* mod, size_t* size) {
        uintptr_t base = 0;
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, p);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                if (!_wcsicmp(me.szModule, mod)) {
                    base = (uintptr_t)me.modBaseAddr;
                    *size = me.modBaseSize;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return base;
    }
};