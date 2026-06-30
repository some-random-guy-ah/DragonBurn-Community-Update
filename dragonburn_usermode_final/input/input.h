#pragma once
#include <cstdint>
#include <winuser.h>
#include <windef.h>

class Input
{
public:
	bool initialize()
	{
		const auto win32u = GetModuleHandleA("win32u.dll");
		if (!win32u) return false;

		m_inject_mouse = reinterpret_cast<fn_inject_mouse>(GetProcAddress(win32u, "NtUserInjectMouseInput"));
		m_inject_keyboard = reinterpret_cast<fn_inject_keyboard>(GetProcAddress(win32u, "NtUserInjectKeyboardInput"));

		return m_inject_mouse && m_inject_keyboard;
	}

	void inject_mouse(int x, int y, std::uint8_t buttons) const
	{
		if (!m_inject_mouse) return;

		unsigned long flags{0};

		if (buttons & move) flags |= MOUSEEVENTF_MOVE;
		if (buttons & left_down) flags |= MOUSEEVENTF_LEFTDOWN;
		if (buttons & left_up) flags |= MOUSEEVENTF_LEFTUP;
		if (buttons & right_down) flags |= MOUSEEVENTF_RIGHTDOWN;
		if (buttons & right_up) flags |= MOUSEEVENTF_RIGHTUP;

		mouse_info_t info{};
		info.pt.x = x;
		info.pt.y = y;
		info.data = 0;
		info.flags = flags;
		info.time = 0;
		info.extra_info = 0;

		m_inject_mouse(&info, 1);
	}

	void inject_keyboard(std::uint16_t key, bool pressed) const
	{
		if (!m_inject_keyboard) return;

		keyboard_info_t info{};
		info.vk = key;
		info.scan = static_cast<std::uint16_t>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));
		info.flags = pressed ? 0 : KEYEVENTF_KEYUP;
		info.time = 0;
		info.extra_info = 0;

		m_inject_keyboard(&info, 1);
	}

	enum mouse_buttons : std::uint8_t
	{
		none = 0,
		left_down = 1 << 0,
		left_up = 1 << 1,
		right_down = 1 << 2,
		right_up = 1 << 3,
		move = 1 << 4
	};

private:
	struct mouse_info_t
	{
		POINT pt;
		unsigned long data;
		unsigned long flags;
		unsigned long time;
		std::uintptr_t extra_info;
	};

	struct keyboard_info_t
	{
		std::uint16_t vk;
		std::uint16_t scan;
		unsigned long flags;
		unsigned long time;
		std::uintptr_t extra_info;
	};

	using fn_inject_mouse = BOOL(NTAPI*)(mouse_info_t*, int);
	using fn_inject_keyboard = BOOL(NTAPI*)(keyboard_info_t*, int);

	fn_inject_mouse m_inject_mouse{};
	fn_inject_keyboard m_inject_keyboard{};
};

inline Input g_input;