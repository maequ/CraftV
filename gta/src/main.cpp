// CraftV.asi entry point. Script Hook V loads the .asi, calls DllMain, then runs ScriptMain on its script
// thread; WAIT(0) yields one game frame (SDK readme, NativeTrainer sample).
//
// Safety (brief §7): a C++ exception in a tick is caught inside HostPlugin; a hardware fault (access
// violation) is caught here by SEH. Either way CraftV switches itself off and the game keeps running.
// Story mode only: HostPlugin switches itself off for good at the first sign of an online session.
#include "core/config.h"
#include "core/host_log.h"
#include "core/host_plugin.h"
#include "compositor.h"
#include "gta_game.h"

#include "craftv/clock.h"

#pragma warning(push, 0)
#include "main.h"
#pragma warning(pop)

#include <memory>
#include <string>

namespace
{
	HMODULE                                   g_module = nullptr;
	craftv::host::GtaGame                     g_game;
	std::unique_ptr<craftv::host::HostPlugin> g_plugin;
	bool                                      g_faulted = false;

	// Folder of the game executable: CraftV.log goes there (brief §7), next to GTA5.exe.
	std::wstring GameDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(nullptr, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	// Folder of this .asi: CraftV.ini sits next to it.
	std::wstring ModuleDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(g_module, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	__declspec(noinline) void TickOnce()
	{
		compositor::try_register(g_module);  // ReShade may load after us; the passthrough waits for it
		g_plugin->Tick(craftv::clock::NowMs(), craftv::clock::NowUs());
		if (g_plugin->State() == craftv::host::PluginState::kActive && craftv::host::HostPlugin::PlayerUsable(g_plugin->LastSample())) {
			g_game.MeasurePropCandidates();
		}
	}

	// F7 turns the Minecraft view on and off (Script Hook V calls this on the game's window thread).
	void OnKeyboard(DWORD a_key, WORD, BYTE, BOOL, BOOL, BOOL a_wasDownBefore, BOOL a_isUpNow)
	{
		if (a_key == VK_F7 && !a_wasDownBefore && !a_isUpNow && g_plugin) {
			g_plugin->RequestPassthroughToggle();
		}
	}

	// No C++ objects with destructors in this frame, so SEH is allowed here.
	void SafeTick()
	{
		__try {
			TickOnce();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			g_faulted = true;
		}
	}

	void ScriptMain()
	{
		craftv::host::HostLog::Open(GameDir() + L"CraftV.log");
		craftv::host::Config config;
		config.software = "CraftV-GTA5 0.1.0";
		const std::wstring ini = ModuleDir() + L"CraftV.ini";
		const bool         found = config.Load(ini);
		craftv::host::HostLog::Info("CraftV %s loaded (protocol %u.%u); config %s", craftv::host::kPluginVersion, craftv::proto::kVersionMajor,
			craftv::proto::kVersionMinor, found ? "CraftV.ini" : "defaults (CraftV.ini not found)");
		g_plugin = std::make_unique<craftv::host::HostPlugin>(g_game, config);
		while (true) {
			if (!g_faulted) {
				SafeTick();
				if (g_faulted) {
					craftv::host::HostLog::Error("hardware fault inside a tick: CraftV is off until the game restarts; the game keeps running");
				}
			}
			WAIT(0);
		}
	}
}

BOOL APIENTRY DllMain(HMODULE a_module, DWORD a_reason, LPVOID a_reserved)
{
	switch (a_reason) {
	case DLL_PROCESS_ATTACH:
		g_module = a_module;
		scriptRegister(a_module, ScriptMain);
		keyboardHandlerRegister(OnKeyboard);
		break;
	case DLL_PROCESS_DETACH:
		keyboardHandlerUnregister(OnKeyboard);
		scriptUnregister(a_module);
		compositor::unregister(a_module);
		// On a normal unload (Ctrl+R reload with ScriptHookV.dev) detach cleanly. During process exit
		// (a_reserved != null) other threads are already gone; Minecraft notices the stopped heartbeat instead.
		if (a_reserved == nullptr && g_plugin) {
			g_plugin->Shutdown();
			g_plugin.reset();
			craftv::host::HostLog::Close();
		}
		break;
	default:
		break;
	}
	return TRUE;
}
