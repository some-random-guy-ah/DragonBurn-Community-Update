// memory/shared.h
#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <Windows.h>
#include <winioctl.h>
#endif

// Standard driver (SCM loaded, test signing)
#define DRIVER_DEVICE_NAME      L"\\Device\\dragonburn_driver"
#define DRIVER_SYMBOLIC_LINK    L"\\DosDevices\\dragonburn_driver"
#define DRIVER_USER_PATH        L"\\\\.\\dragonburn_driver"
#define DRIVER_SERVICE_NAME     L"dragonburn_driver"
#define DRIVER_FILE_NAME        L"dragonburn_driver.sys"

// kdmapper driver
#define KDMP_DRIVER_NAME        L"\\Driver\\dragonburn_driver"
#define KDMP_DEVICE_NAME        L"\\Device\\dragonburn_driver"
#define KDMP_SYMBOLIC_LINK      L"\\DosDevices\\dragonburn_driver"
#define KDMP_USER_PATH          L"\\\\.\\dragonburn_driver"
#define KDMP_FILE_NAME          L"dragonburn_driver.sys"

#define IOCTL_READ_MEMORY     CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PING            CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define PING_MAGIC            0xDEADBEEFCAFEBABEULL

#pragma pack(push, 1)
typedef struct _READ_MEMORY_REQUEST {
    unsigned long    target_pid;
    unsigned __int64 source_address;
    unsigned long    read_size;
    unsigned long    padding;
} READ_MEMORY_REQUEST, *PREAD_MEMORY_REQUEST;
#pragma pack(pop)

typedef struct _MODULE_BASE_REQUEST {
    unsigned long    target_pid;
    wchar_t          module_name[256];
    unsigned __int64 base_address;    // OUTPUT: filled by driver
    unsigned long    module_size;     // OUTPUT: filled by driver
    unsigned long    padding;         // keep 8-byte alignment
} MODULE_BASE_REQUEST, *PMODULE_BASE_REQUEST;

typedef struct _PING_RESPONSE {
    unsigned __int64 magic;
} PING_RESPONSE, *PPING_RESPONSE;