#include <Windows.h>
#include <Shlwapi.h>
#include <tchar.h>

#include <vector>
#include <fstream>
#include <iostream>
#include <io.h>
#include <fcntl.h>     /* for _O_TEXT and _O_BINARY */

HINSTANCE hLThis = 0;
FARPROC p[329];
HINSTANCE hL = 0;
bool isSkyrimSE = false;
std::string logName = "preloader.log";

PVOID origFunc = 0;

std::string skyrimPath;
bool alreadyLoaded = false;
bool alreadyLoadedDLLPlugins = false;
std::vector<HINSTANCE> loadedLib;

int LoadDLLPlugin(const char * path)
{
	int state = -1;
	__try
	{
		HINSTANCE lib = LoadLibrary(path);
		if (lib == NULL)
			return 0;

		int ok = 1;
		FARPROC funcAddr = GetProcAddress(lib, "Initialize");
		if (funcAddr != 0)
		{
			state = -2;
			((void(__cdecl *)())funcAddr)();
			ok = 2;
		}

		loadedLib.push_back(lib);
		return ok;
	}
	__except (1)
	{

	}

	return state;
}

std::string GetMehLoaderDLLPath()
{
    std::string pluginPath = skyrimPath;
    auto pos = pluginPath.rfind('\\');
    if (pos != std::string::npos)
        pluginPath = pluginPath.substr(0, pos);

    return pluginPath + "\\binkw64_.dll";
}

std::string GetPluginsDirectory()
{
	std::string pluginPath = skyrimPath;
	auto pos = pluginPath.rfind('\\');
	if (pos != std::string::npos)
		pluginPath = pluginPath.substr(0, pos);

	return pluginPath + "\\Data\\SKSE\\Plugins\\";
}

std::string GetDLLPluginsDirectory()
{
    std::string pluginPath = skyrimPath;
    auto pos = pluginPath.rfind('\\');
    if (pos != std::string::npos)
        pluginPath = pluginPath.substr(0, pos);

    return pluginPath + "\\Data\\DLLPlugins\\";
}

void LoadDLLPlugins()
{
    if (alreadyLoadedDLLPlugins)
        return;

    alreadyLoadedDLLPlugins = true;

    if (PathFileExists(GetMehLoaderDLLPath().c_str()))
        return;


    std::ofstream logFile(logName, std::ios_base::out | std::ios_base::app);

    logFile << "hook triggered, loading dll plugins (meh's loader)" << std::endl;

    WIN32_FIND_DATA wfd;

    std::string dir = GetDLLPluginsDirectory();
    std::string search_dir = dir + "*.dll";
    HANDLE hFind = FindFirstFile(search_dir.c_str(), &wfd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            std::string fileName = wfd.cFileName;
            fileName = dir + fileName;

            logFile << "attempting to load \"" << fileName.c_str() << "\"" << std::endl;

            int result = LoadDLLPlugin(fileName.c_str());

            switch (result)
            {
            case 2:
                logFile << "loaded successfully and Initialize() called" << std::endl;
                break;
            case 1:
                logFile << "loaded successfully" << std::endl;
                break;
            case 0:
                logFile << "LoadLibrary failed" << std::endl;
                break;
            case -1:
                logFile << "LoadLibrary crashed, contact the plugin author" << std::endl;
                break;
            case -2:
                logFile << "Initialize() crashed, contact the plugin author" << std::endl;
                break;
            }


        } while (FindNextFile(hFind, &wfd));
        FindClose(hFind);
    }
    else
    {
        logFile << "failed to search dll plugin directory" << std::endl;
    }

    logFile << "DLLPlugin loader finished" << std::endl;
}

void LoadSKSEPlugins()
{
	if (alreadyLoaded)
		return;

	alreadyLoaded = true;

	std::ofstream logFile(logName, std::ios_base::out | std::ios_base::app);

	logFile << "hook triggered, loading skse plugins" << std::endl;
	
	std::vector<std::string> filesToLoad;
	WIN32_FIND_DATA wfd;
	std::string dir = GetPluginsDirectory();
	std::string search_dir = dir + "*.txt";

	std::cout << "searching \"" << search_dir.c_str() << "\" for preloading" << std::endl;
	logFile << "searching \"" << search_dir.c_str() << "\" for preloading" << std::endl;
	HANDLE hFind = FindFirstFile(search_dir.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				continue;

			std::string fileName = wfd.cFileName;

			if (fileName.length() < 4)
				continue;

			std::string ext = fileName.substr(fileName.length() - 4, 4);

			if (_stricmp(ext.c_str(), ".txt") != 0)
				continue;

			fileName = fileName.substr(0, fileName.length() - 4);

			if (fileName.length() < 8)
				continue;

			std::string end = fileName.substr(fileName.length() - 8, 8);

			if (_stricmp(end.c_str(), "_preload") != 0)
				continue;

			fileName = fileName.substr(0, fileName.length() - 8);

			filesToLoad.push_back(fileName);

			logFile << "found plugin \"" << fileName.c_str() << "\" for preloading" << std::endl;

		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	else
	{
		logFile << "failed to search skse plugin directory" << std::endl;
	}

	logFile << "attempting to load found plugins" << std::endl;

	char plugin_path[MAX_PATH];

	for (auto file : filesToLoad)
	{
		_snprintf_s(plugin_path, MAX_PATH, "%s%s.dll", dir.c_str(), file.c_str());

		logFile << "attempting to load \"" << plugin_path << "\"" << std::endl;

		int result = LoadDLLPlugin(plugin_path);

		switch(result)
		{
		case 2:
			logFile << "loaded successfully and Initialize() called" << std::endl;
			break;
		case 1:
			logFile << "loaded successfully" << std::endl;
			break;
		case 0:
			logFile << "LoadLibrary failed" << std::endl;
			break;
		case -1:
			logFile << "LoadLibrary crashed, contact the plugin author" << std::endl;
			break;
		case -2:
			logFile << "Initialize() crashed, contact the plugin author" << std::endl;
			break;
		}
	}

	logFile << "loader finished" << std::endl;
}

extern "C" {
	
	// Use __declspec(dllexport) to export the function
	__declspec(dllexport) void Init()
	{
		// Setup console.
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		FILE* pCout;
		freopen_s(&pCout, "CON", "w", stdout);

		LoadDLLPlugins();
		LoadSKSEPlugins();
	}
}

BOOL WINAPI DllMain(HINSTANCE hInst,DWORD reason,LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		std::ofstream logFile(logName);
		logFile << "skse64 plugin preloader - d3dx9_42" << std::endl;
		hLThis = hInst;

		/*
		 * Check if we're loaded by SkyrimSE.exe (or VR)
		 * Purpose two-fold:
		 *	1) Don't load skse plugins into things that aren't Skyrim (Creation Kit)
		 *	2) SkyrimSE uses a very limited subset of d3dx9_42, with implementations provided in d3dx9_impl, so we don't need to load the actual dll in this case
		 */
		TCHAR exePath[MAX_PATH];
		GetModuleFileName(nullptr, exePath, MAX_PATH);
		logFile << "exe path: " << exePath << std::endl;

		skyrimPath = exePath;
		isSkyrimSE = true;
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		if (isSkyrimSE)
		{
			if (!loadedLib.empty())
			{
				for (auto lib : loadedLib)
					FreeLibrary(lib);
				loadedLib.clear();
			}
		}
		else
		{
			FreeLibrary(hL);
		}
		return 1;
	}

	return 1;
}
