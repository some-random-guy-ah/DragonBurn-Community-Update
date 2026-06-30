#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

#include "kdmapper.h"
#include "utils.h"
#include "intel_driver.h"
#include "logger.h"

LONG WINAPI SimplestCrashHandler(EXCEPTION_POINTERS* ExceptionInfo);
bool CheckWindowsKernelPrefs();
void ApplyRecommendedPrefs();

int wmain()
{
	SetUnhandledExceptionFilter(SimplestCrashHandler);

	bool waitForKey = false;
	for (int i = 1; i < __argc; i++) {
		if (wcscmp(__wargv[i], L"/wait") == 0 || wcscmp(__wargv[i], L"/pause") == 0) {
			waitForKey = true;
			break;
		}
	}

	Log::Info("dragonburn_driver mapper using Intel vulnerable driver");

	HANDLE hDevice = CreateFileW(L"\\\\.\\dragonburn_driver", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hDevice != INVALID_HANDLE_VALUE) {
		CloseHandle(hDevice);
		Log::Custom("[+] dragonburn_driver already loaded", 10);
		return 0;
	}

	/* Stop anti-cheat services that may interfere with vulnerable driver load */
	system("sc stop faceit >nul 2>&1");
	system("sc stop vgc >nul 2>&1");
	system("sc stop vgk >nul 2>&1");

	/* Check and apply kernel prefs like the original DragonBurn */
	if (!CheckWindowsKernelPrefs())
	{
		Log::Warning("Some kernel preferences may interfere with driver loading.");
		std::string response;
		do
		{
			Log::Info("Apply recommended settings and reboot? (y/n)");
			std::cout << ">>> "; std::cin >> response;
		} while (response != "y" && response != "n");
		if (response == "y")
		{
			ApplyRecommendedPrefs();
			Log::Fine("Settings applied. Reboot required.");
			system("pause");
			return -1;
		}
	}

	std::wstring driver_path = kdmUtils::GetCurrentAppFolder() + L"\\dragonburn_driver.sys";
	if (!std::filesystem::exists(driver_path)) {
		Log::Error("dragonburn_driver.sys not found next to this executable");
	}

	std::vector<uint8_t> driver_data;
	if (!kdmUtils::ReadFileToMemory(driver_path, &driver_data) || driver_data.empty()) {
		Log::Error("Failed to read dragonburn_driver.sys");
	}

	Log::Fine("Read " + std::to_string(driver_data.size()) + " bytes from dragonburn_driver.sys");

	if (!NT_SUCCESS(intel_driver::Load()))
		Log::Error("Failed to connect to intel driver");

	kdmapper::AllocationMode mode = kdmapper::AllocationMode::AllocatePool;

	NTSTATUS exitCode = 0;
	if (!kdmapper::MapDriver(driver_data.data(), 0, 0, false, true, mode, false, nullptr, &exitCode))
	{
		intel_driver::Unload();
		Log::Error("Failed to map dragonburn_driver.sys");
	}

	if (!NT_SUCCESS(intel_driver::Unload()))
		Log::Warning("Warning failed to unload intel driver", true);

	Log::Fine("dragonburn_driver.sys mapped successfully");

	if (waitForKey) {
		Log::Custom("--- Mapping complete. Press any key to close this window ---", 8);
		system("pause>nul");
	}

	return 0;
}

bool CheckWindowsKernelPrefs()
{
	/* Check all relevant settings like the original DragonBurn */
	
	/* Vulnerable Driver Blocklist */
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\CI\\Config", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD data = 1, size = sizeof(data), type = 0;
			RegQueryValueExA(hKey, "VulnerableDriverBlocklistEnable", nullptr, &type, (LPBYTE)&data, &size);
			RegCloseKey(hKey);
			if (type == REG_DWORD && data != 0) return false;
		}
	}

	/* HypervisorEnforcedCodeIntegrity (HVCI) */
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD data = 1, size = sizeof(data), type = 0;
			RegQueryValueExA(hKey, "Enabled", nullptr, &type, (LPBYTE)&data, &size);
			RegCloseKey(hKey);
			if (type == REG_DWORD && data != 0) return false;
		}
	}

	/* RunAsPPL (Protected Process Light) */
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Lsa", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD data = 0, size = sizeof(data), type = 0;
			RegQueryValueExA(hKey, "RunAsPPL", nullptr, &type, (LPBYTE)&data, &size);
			RegCloseKey(hKey);
			if (type == REG_DWORD && data != 0) return false;
		}
	}

	/* VirtualizationBasedSecurity */
	{
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\DeviceGuard", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD data = 1, size = sizeof(data), type = 0;
			RegQueryValueExA(hKey, "EnableVirtualizationBasedSecurity", nullptr, &type, (LPBYTE)&data, &size);
			RegCloseKey(hKey);
			if (type == REG_DWORD && data != 0) return false;
		}
	}

	return true;
}

void ApplyRecommendedPrefs()
{
	Log::Info("Applying recommended kernel preferences...");
	system("reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity\" /v Enabled /t REG_DWORD /d 0 /f >nul 2>&1");
	system("reg add \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Lsa\" /v RunAsPPL /t REG_DWORD /d 0 /f >nul 2>&1");
	system("reg add \"HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\DeviceGuard\" /v EnableVirtualizationBasedSecurity /t REG_DWORD /d 00000000 /f >nul 2>&1");
	system("bcdedit /set hypervisorlaunchtype off >nul 2>&1");
	system("reg add \"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\CI\\Config\" /v VulnerableDriverBlocklistEnable /t REG_DWORD /d 00000000 /f >nul 2>&1");
	Log::Fine("Done.");
}

LONG WINAPI SimplestCrashHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
	if (ExceptionInfo && ExceptionInfo->ExceptionRecord)
	{
		std::ostringstream oss;
		oss << "Crash at addr 0x" << ExceptionInfo->ExceptionRecord->ExceptionAddress << L" by 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode;
		Log::Error(oss.str(), false);
	}
	else
		Log::Error("Program crashed", false);

	if (intel_driver::hDevice)
		intel_driver::Unload();

	return EXCEPTION_EXECUTE_HANDLER;
}
