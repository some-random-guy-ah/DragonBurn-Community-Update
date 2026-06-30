#pragma once
#include <Windows.h>
#include <TlHelp32.h>

#include "imemory.h"

class MemoryWinApi : public IMemory {
public:
    MemoryWinApi() = default;
    ~MemoryWinApi() { close(); }

    MemoryWinApi(const MemoryWinApi&) = delete;
    MemoryWinApi& operator=(const MemoryWinApi&) = delete;

    MemoryWinApi(MemoryWinApi&& other) noexcept
        : process(other.process), pid(other.pid) {
        this->m_modules = other.m_modules;
        other.process = nullptr;
        other.pid = 0;
        other.m_modules = {};
    }

    MemoryWinApi& operator=(MemoryWinApi&& other) noexcept {
        if (this != &other) {
            close();
            process = other.process;
            pid = other.pid;
            m_modules = other.m_modules;
            other.process = nullptr;
            other.pid = 0;
            other.m_modules = {};
        }
        return *this;
    }

    bool attach(const wchar_t* process_name) {
        pid = find_process(process_name);
        if (!pid) return false;

        process = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!process) return false;

        m_modules.client = get_module_base(pid, L"client.dll", &m_modules.client_size);
        m_modules.engine2 = get_module_base(pid, L"engine2.dll", &m_modules.engine2_size);
        m_modules.schemasystem = get_module_base(pid, L"schemasystem.dll", &m_modules.schemasystem_size);
        m_modules.tier0 = get_module_base(pid, L"tier0.dll", &m_modules.tier0_size);
        m_modules.vphysics2 = get_module_base(pid, L"vphysics2.dll", &m_modules.vphysics2_size);

        return m_modules.client != 0;
    }

    void close() {
        if (process) {
            CloseHandle(process);
            process = nullptr;
        }
        pid = 0;
        m_modules = {};
    }

    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        ReadProcessMemory(process, (LPCVOID)address, &value, sizeof(T), nullptr);
        return value;
    }

    bool read_raw(uintptr_t address, void* buffer, size_t size) const {
        return ReadProcessMemory(process, (LPCVOID)address, buffer, size, nullptr);
    }

    bool is_valid() const {
        if (!process) return false;
        DWORD code;
        return GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    }

    uintptr_t get_client_base() const { return m_modules.client; }
    DWORD get_pid() const { return pid; }

private:
    HANDLE process = nullptr;
    DWORD pid = 0;

    static DWORD find_process(const wchar_t* name) {
        DWORD result = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snap, &pe)) {
            do {
                if (!_wcsicmp(pe.szExeFile, name)) {
                    result = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return result;
    }

    static uintptr_t get_module_base(DWORD pid, const wchar_t* module_name, size_t* size) {
        uintptr_t base = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);

        if (Module32FirstW(snap, &me)) {
            do {
                if (!_wcsicmp(me.szModule, module_name)) {
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