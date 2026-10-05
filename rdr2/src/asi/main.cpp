// CraftV_RDR2.asi entry point. ScriptHookRDR2 loads the .asi, calls DllMain, then runs ScriptMain on
// its script thread; WAIT(0) yields one game frame (SDK readme, NativeTrainer sample).
//
// Safety (brief §6): a C++ exception in a tick is caught inside HostPlugin; a hardware fault (access
// violation) is caught here by SEH. Either way CraftV switches itself off and the game keeps running.
#include "../core/config.h"
#include "../core/host_log.h"
#include "../core/host_plugin.h"
#include "rdr2_game.h"

#include "craftv/clock.h"

#pragma warning(push, 0)
#include "main.h"
#pragma warning(pop)

#include <memory>
#include <string>

namespace
{
	HMODULE                                     g_module = nullptr;
	craftv::host::Rdr2Game                    g_game;
	std::unique_ptr<craftv::host::HostPlugin> g_plugin;
	bool                                        g_faulted = false;

	// Folder of the game executable: CraftV_RDR2.log goes there (brief §6), next to RDR2.exe.
	std::wstring GameDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(nullptr, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	// Folder of this .asi: CraftV_RDR2.ini sits next to it.
	std::wstring ModuleDir()
	{
		wchar_t path[MAX_PATH] = {};
		::GetModuleFileNameW(g_module, path, MAX_PATH);
		std::wstring p(path);
		return p.substr(0, p.find_last_of(L"\\/") + 1);
	}

	__declspec(noinline) void TickOnce()
	{
		g_plugin->Tick(craftv::clock::NowMs(), craftv::clock::NowUs());
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
		craftv::host::HostLog::Open(GameDir() + L"CraftV_RDR2.log");
		craftv::host::Config config;
		const std::wstring     ini = ModuleDir() + L"CraftV_RDR2.ini";
		const bool             found = config.Load(ini);
		craftv::host::HostLog::Info("CraftV %s loaded (protocol %u.%u); config %s", craftv::host::kPluginVersion, craftv::proto::kVersionMajor,
			craftv::proto::kVersionMinor, found ? "CraftV_RDR2.ini" : "defaults (CraftV_RDR2.ini not found)");
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
		break;
	case DLL_PROCESS_DETACH:
		scriptUnregister(a_module);
		// On a normal unload (Ctrl+R reload) detach cleanly. During process exit (a_reserved != null)
		// other threads are already gone; Minecraft notices the stopped heartbeat instead.
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
