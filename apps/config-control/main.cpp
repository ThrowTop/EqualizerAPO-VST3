#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

#include "helpers/RegistryHelper.h"
#include "helpers/ServiceHelper.h"

using namespace std;

namespace
{
int fail(const wstring& message)
{
	MessageBoxW(NULL, message.c_str(), L"EqualizerAPO-VST3 Configuration", MB_OK | MB_ICONERROR);
	return 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	int argumentCount = 0;
	wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (arguments == NULL)
		return fail(L"Could not read the command line.");

	if (argumentCount != 2)
	{
		LocalFree(arguments);
		return fail(L"Expected one absolute .txt configuration file path.");
	}

	filesystem::path configFile(arguments[1]);
	LocalFree(arguments);

	if (!configFile.is_absolute())
		return fail(L"The configuration file path must be absolute.");

	error_code pathError;
	configFile = filesystem::absolute(configFile, pathError).lexically_normal();
	if (pathError || !configFile.is_absolute())
		return fail(L"The configuration file path must be absolute.");

	wstring extension = configFile.extension().wstring();
	transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) { return (wchar_t)towlower(value); });
	if (extension != L".txt" || !filesystem::is_regular_file(configFile, pathError) || pathError)
		return fail(L"The selected configuration must be an existing .txt file.");

	try
	{
		if (!RegistryHelper::keyExists(APP_REGPATH))
			RegistryHelper::createKey(APP_REGPATH);
		RegistryHelper::writeValue(APP_REGPATH, L"ConfigFile", configFile.wstring());
		ServiceHelper::restartService(L"AudioSrv");
	}
	catch (RegistryException e)
	{
		return fail(e.getMessage());
	}
	catch (ServiceException e)
	{
		return fail(L"The active file was saved, but Windows Audio could not be restarted:\n\n" + e.getMessage());
	}

	return 0;
}
