#include "Core/Features/PluginManager/PluginManager.hpp"
#include "Core/Features/Console/Console.hpp"
#include "Core/Features/API/Internal.hpp"
#include "Core/Utils/Logging/Logging.hpp"
#include "Core/Utils/WinAPI/WinAPI.hpp"
#include "Core/Hooks/Hooks.hpp"
#if _WIN64
//#error Don't compile in x64! // What if I do anyway
#endif

LONG WINAPI WhenIMakeAFuckyWucky(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	if (IsDebuggerPresent())
	{
		Utils::Logging::Error(__FILE__, __LINE__, "Exception 0x%X occured at %p - DEBUGGER ATTACHED, NOT EXITING.",
			apExceptionInfo->ExceptionRecord->ExceptionCode,
			apExceptionInfo->ExceptionRecord->ExceptionAddress
		);

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	Utils::Logging::Critical(__FILE__, __LINE__,
		"Exception 0x%X occured at %p",
		apExceptionInfo->ExceptionRecord->ExceptionCode,
		apExceptionInfo->ExceptionRecord->ExceptionAddress
	);

	return EXCEPTION_CONTINUE_SEARCH;
}

void __stdcall Main(HINSTANCE g_hDLL)
{
	using namespace API;

	SetUnhandledExceptionFilter(WhenIMakeAFuckyWucky);

	// Tell the API which module we are in memory
	gAPIVars.Globals.g_hMainModule = g_hDLL;

	// Open the console, write the version number
	Internal::__InitializeConsole__();

	Utils::Logging::Message(CLR_GRAY, "Attempting to load Steam...");
	HMODULE steamApi = LoadLibraryA("steam_api64.dll");

	if (!steamApi)
	{
		steamApi = LoadLibraryA("steam_api.dll");
	}

	if (!steamApi)
	{
		Utils::Logging::Message(CLR_GRAY, "Failed to load Steam API, assuming not present or non-Steam game.");
	}
	else
	{
		typedef bool (*SteamAPI_Init_t)();
		typedef bool (*SteamAPI_IsStreamRunning_t)();

		SteamAPI_Init_t init = (SteamAPI_Init_t)GetProcAddress(steamApi, "SteamAPI_Init");
		SteamAPI_IsStreamRunning_t isSteamRunning = (SteamAPI_IsStreamRunning_t)GetProcAddress(steamApi, "SteamAPI_IsSteamRunning");

		if (!init || !isSteamRunning)
		{
			Utils::Logging::Message(CLR_YELLOW, "Failed to resolve SteamAPI functions!");
		}
		else
		{
			if (!init())
			{
				Utils::Logging::Message(CLR_YELLOW, "Failed to initialize Steam API!");
			}
			else if (!isSteamRunning())
			{
				// The game will restart. Let's wait for that.
				Utils::Logging::Message(CLR_BLUE, "Game will be relaunched under Steam, suspending execution...");
				return;
			}
			else
			{
				Utils::Logging::Message(CLR_GRAY, "Running under Steam!");
			}
		}
	}

	// Map all the auto-executed plugins to memory, don't run any functions though
	PluginManager::Initialize();

	// If we're using Early Launch
	if (Utils::WinAPI::IsPreloaded())
	{
		// Run PluginPreload() on all loaded plugins
		PluginManager::RunPluginPreloads();

		// Resume the game process and note that we preloaded.
		Utils::WinAPI::ResumeGameProcess();
		gAPIVars.Globals.g_bWasPreloaded = true;
	}

	// Runs PluginEntry() on all loaded plugins
	// This function doesn't return until the runner finishes initialization.
	Internal::__Initialize__(g_hDLL);

	// Hook functions like Code_Execute
	Hooks::Initialize();

	Utils::Logging::Message(CLR_LIGHTBLUE, "Initialization done!");

	// Loop
	while (!GetAsyncKeyState(VK_END)) 
	{
		if (GetAsyncKeyState(VK_F10) & 1)
			Console::DoCommand();

		Sleep(5); 
	}

	// Unhook
	Hooks::Uninitialize();

	// Unload all plugins
	Internal::__Unload__();

	SetUnhandledExceptionFilter(NULL);

	// Actually unload the library
	FreeLibraryAndExitThread(g_hDLL, 0);
}

int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Main, hinstDLL, 0, 0));
	}
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL MiniDumpReadDumpStream(PVOID, ULONG, void*, void*, void*) { return FALSE; }
extern "C" __declspec(dllexport) BOOL MiniDumpWriteDump(HANDLE, DWORD, HANDLE, DWORD, void*, void*, void*) { return FALSE; }