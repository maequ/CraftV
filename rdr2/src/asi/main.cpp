// RedCraft.asi entry point. ScriptHookRDR2 loads the .asi, calls DllMain, then runs ScriptMain on
// its script thread; WAIT(0) yields one game frame (SDK readme, NativeTrainer sample).
//
// Safety (brief §6): a C++ exception in a tick is caught inside HostPlugin; a hardware fault (access
// violation) is caught here by SEH. Either way RedCraft switches itself off and the game keeps running.
#include "../core/config.h"
#include "../core/host_log.h"
#include "../core/host_plugin.h"
#include "rdr2_game.h"

#include "redcraft/clock.h"

#pragma warning(push, 0)
#include "main.h"
#pragma warning(pop)

#include <memory>
#include <string>

namespace
{
	HMODULE                                     g_module = nullptr;
	redcraft::host::Rdr2Game                    g_game;
	std::unique_ptr<redcraft::host::HostPlugin> g_plugin;
	bool                                        g_faulted = false;

	// Folder of the game executable: RedCraft.log goes there (brief §6), next to RDR2.exe.
	std::wstring GameDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(nullptr, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	// Folder of this .asi: RedCraft.ini sits next to it.
	std::wstring ModuleDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(g_module, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	__declspec(noinline) void TickOnce()
	{
		g_plugin->Tick(redcraft::clock::NowMs(), redcraft::clock::NowUs());
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
		redcraft::host::HostLog::Open(GameDir() + L"RedCraft.log");
		redcraft::host::Config config;
		const std::wstring     ini = ModuleDir() + L"RedCraft.ini";
		const bool             found = config.Load(ini);
		redcraft::host::HostLog::Info("RedCraft %s loaded (protocol %u.%u); config %s", redcraft::host::kPluginVersion, redcraft::proto::kVersionMajor,
			redcraft::proto::kVersionMinor, found ? "RedCraft.ini" : "defaults (RedCraft.ini not found)");
		g_plugin = std::make_unique<redcraft::host::HostPlugin>(g_game, config);
		while (true) {
			if (!g_faulted) {
				SafeTick();
				if (g_faulted) {
					redcraft::host::HostLog::Error("hardware fault inside a tick: RedCraft is off until the game restarts; the game keeps running");
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
		break;
	case DLL_PROCESS_DETACH:
		scriptUnregister(a_module);
		// On a normal unload (Ctrl+R reload) detach cleanly. During process exit (a_reserved != null)
		// other threads are already gone; Minecraft notices the stopped heartbeat instead.
		if (a_reserved == nullptr && g_plugin) {
			g_plugin->Shutdown();
			g_plugin.reset();
			redcraft::host::HostLog::Close();
		}
		break;
	default:
		break;
	}
	return TRUE;
}
