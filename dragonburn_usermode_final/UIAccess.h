#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>

static DWORD DuplicateWinloginToken(DWORD dwSessionId, DWORD dwDesiredAccess, PHANDLE phToken) {
    PRIVILEGE_SET ps;
    ps.PrivilegeCount = 1;
    ps.Control = PRIVILEGE_SET_ALL_NECESSARY;

    if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &ps.Privilege[0].Luid))
        return GetLastError();

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return GetLastError();

    DWORD dwErr = ERROR_NOT_FOUND;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    for (BOOL bCont = Process32First(hSnapshot, &pe); bCont; bCont = Process32Next(hSnapshot, &pe)) {
        if (_tcsicmp(pe.szExeFile, TEXT("winlogon.exe")) != 0)
            continue;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!hProcess)
            continue;

        HANDLE hToken;
        if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)) {
            BOOL fTcb;
            if (PrivilegeCheck(hToken, &ps, &fTcb) && fTcb) {
                DWORD sid;
                DWORD dwRetLen;
                if (GetTokenInformation(hToken, TokenSessionId, &sid, sizeof(sid), &dwRetLen) && sid == dwSessionId) {
                    if (DuplicateTokenEx(hToken, dwDesiredAccess, NULL, SecurityImpersonation, TokenImpersonation, phToken))
                        dwErr = ERROR_SUCCESS;
                    else
                        dwErr = GetLastError();
                }
            }
            CloseHandle(hToken);
        }
        CloseHandle(hProcess);
        if (dwErr == ERROR_SUCCESS)
            break;
    }

    CloseHandle(hSnapshot);
    return dwErr;
}

static DWORD CreateUIAccessToken(PHANDLE phToken) {
    HANDLE hTokenSelf;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hTokenSelf))
        return GetLastError();

    DWORD dwErr;
    DWORD dwSessionId, dwRetLen;
    if (GetTokenInformation(hTokenSelf, TokenSessionId, &dwSessionId, sizeof(dwSessionId), &dwRetLen)) {
        HANDLE hTokenSystem;
        dwErr = DuplicateWinloginToken(dwSessionId, TOKEN_IMPERSONATE, &hTokenSystem);
        if (dwErr == ERROR_SUCCESS) {
            if (SetThreadToken(NULL, hTokenSystem)) {
                if (DuplicateTokenEx(hTokenSelf, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, NULL, SecurityAnonymous, TokenPrimary, phToken)) {
                    BOOL bUIAccess = TRUE;
                    if (!SetTokenInformation(*phToken, TokenUIAccess, &bUIAccess, sizeof(bUIAccess))) {
                        dwErr = GetLastError();
                        CloseHandle(*phToken);
                    }
                } else {
                    dwErr = GetLastError();
                }
                RevertToSelf();
            } else {
                dwErr = GetLastError();
            }
            CloseHandle(hTokenSystem);
        }
    } else {
        dwErr = GetLastError();
    }

    CloseHandle(hTokenSelf);
    return dwErr;
}

static BOOL CheckForUIAccess(DWORD* pdwErr, BOOL* pfUIAccess) {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        *pdwErr = GetLastError();
        return FALSE;
    }

    DWORD dwRetLen;
    BOOL result = GetTokenInformation(hToken, TokenUIAccess, pfUIAccess, sizeof(*pfUIAccess), &dwRetLen);
    if (!result)
        *pdwErr = GetLastError();

    CloseHandle(hToken);
    return result;
}

inline DWORD PrepareForUIAccess() {
    DWORD dwErr;
    HANDLE hTokenUIAccess;
    BOOL fUIAccess;

    if (!CheckForUIAccess(&dwErr, &fUIAccess))
        return dwErr;

    if (fUIAccess)
        return ERROR_SUCCESS;

    dwErr = CreateUIAccessToken(&hTokenUIAccess);
    if (dwErr != ERROR_SUCCESS)
        return dwErr;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    GetStartupInfo(&si);

    if (CreateProcessAsUser(hTokenUIAccess, NULL, GetCommandLine(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ExitProcess(0);
    } else {
        dwErr = GetLastError();
    }

    CloseHandle(hTokenUIAccess);
    return dwErr;
}
