#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "AbstractAPOInfo.h"
#include "DeviceAPOInfo.h"
#include "VoicemeeterAPOInfo.h"
#include "helpers/RegistryHelper.h"
#include "helpers/ServiceHelper.h"
#include "helpers/StringHelper.h"
#include "devices/DeviceIdentityResolver.h"
#include "devices/EndpointIdentityProvider.h"
#include "devices/StableDeviceSelector.h"

using namespace std;

namespace
{
struct DeviceRequest
{
	bool input;
	wstring identity;
	StableDeviceSelector selector;
	bool stable;
	bool install;
};

const wstring bindingsPath = APP_REGPATH L"\\DeviceBindings";

int fail(const wstring& message)
{
	MessageBoxW(NULL, message.c_str(), L"EqualizerAPO-VST3 Device Management", MB_OK | MB_ICONERROR);
	return 1;
}

vector<shared_ptr<AbstractAPOInfo>> loadDevices()
{
	vector<shared_ptr<AbstractAPOInfo>> devices = DeviceAPOInfo::loadAllInfos(false);
	vector<shared_ptr<AbstractAPOInfo>> inputs = DeviceAPOInfo::loadAllInfos(true);
	devices.insert(devices.end(), inputs.begin(), inputs.end());
	return devices;
}

vector<DeviceRequest> readManifest(const filesystem::path& path)
{
	ifstream stream(path);
	if (!stream)
		throw runtime_error("Could not open the device request file.");

	vector<DeviceRequest> requests;
	set<wstring> identities;
	string line;
	while (getline(stream, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			continue;

		size_t firstTab = line.find('\t');
		size_t secondTab = firstTab == string::npos ? string::npos : line.find('\t', firstTab + 1);
		if (firstTab == string::npos || secondTab == string::npos)
			throw runtime_error("The device request file is invalid.");

		string type = line.substr(0, firstTab);
		string identityText = line.substr(firstTab + 1, secondTab - firstTab - 1);
		string installText = line.substr(secondTab + 1);
		if ((type != "render" && type != "capture") || identityText.empty()
			|| (installText != "0" && installText != "1"))
			throw runtime_error("The device request file is invalid.");

		wstring endpointIdentity = StringHelper::toWString(identityText, CP_UTF8);
		bool input = type == "capture";
		DeviceRequest request = {input, endpointIdentity, {}, false, installText == "1"};
		if (StableDeviceSelectorCodec::isStable(endpointIdentity))
		{
			auto parsed = StableDeviceSelectorCodec::parse(endpointIdentity);
			if (parsed.status != StableSelectorParseStatus::Success
				|| (parsed.selector.snapshot.flow == AudioDataFlow::Capture) != input)
				throw runtime_error("The device request contains an invalid stable selector.");
			request.selector = std::move(parsed.selector);
			request.stable = true;
		}
		wstring identityKey = request.stable ? request.selector.bindingId : (input ? L"capture:" : L"render:") + endpointIdentity;
		if (!identities.insert(identityKey).second)
			throw runtime_error("The device request file contains a duplicate endpoint.");
		requests.push_back(std::move(request));
	}
	return requests;
}

bool sameGuid(const wstring& left, const wstring& right)
{
	return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

bool matchesIdentity(const AbstractAPOInfo& info, const wstring& identity)
{
	static const wstring namePrefix = L"name:";
	if (identity.compare(0, namePrefix.size(), namePrefix) == 0)
		return info.getDeviceGuid().empty()
			&& _wcsicmp(info.getDeviceString().c_str(), identity.substr(namePrefix.size()).c_str()) == 0;
	return sameGuid(info.getDeviceGuid(), identity);
}

wstring utcTimestamp()
{
	SYSTEMTIME time = {};
	GetSystemTime(&time);
	wchar_t buffer[32] = {};
	swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02uZ", time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond);
	return buffer;
}

void persistInstallBinding(const DeviceRequest& request, const EndpointIdentity& endpoint)
{
	if (!request.stable)
		return;
	wstring key = bindingsPath + L"\\" + request.selector.bindingId;
	if (!request.install && !RegistryHelper::keyExists(key))
		return;
	if (!RegistryHelper::keyExists(bindingsPath))
		RegistryHelper::createKey(bindingsPath);
	if (!RegistryHelper::keyExists(key))
		RegistryHelper::createKey(key);
	StableDeviceSelector currentSelector = request.selector;
	currentSelector.snapshot = endpoint;
	RegistryHelper::writeValue(key, L"Selector", StableDeviceSelectorCodec::serialize(currentSelector));
	RegistryHelper::writeDWORDValue(key, L"DesiredInstalled", request.install ? 1 : 0);
	RegistryHelper::writeValue(key, L"LastEndpointGuid", endpoint.endpointGuid);
	RegistryHelper::writeValue(key, L"LastSeenUtc", utcTimestamp());
}

void disableBindingsForEndpoint(const wstring& endpointGuid, const vector<EndpointIdentity>& catalog)
{
	if (!RegistryHelper::keyExists(bindingsPath))
		return;
	for (const wstring& bindingId : RegistryHelper::enumSubKeys(bindingsPath))
	{
		wstring key = bindingsPath + L"\\" + bindingId;
		if (!RegistryHelper::valueExists(key, L"Selector"))
			continue;
		auto parsed = StableDeviceSelectorCodec::parse(RegistryHelper::readValue(key, L"Selector"));
		if (parsed.status != StableSelectorParseStatus::Success)
			continue;
		DeviceResolution resolution = DeviceIdentityResolver::resolve(parsed.selector, catalog);
		if (resolution.endpoint != nullptr && equalGuid(resolution.endpoint->endpointGuid, endpointGuid))
			RegistryHelper::writeDWORDValue(key, L"DesiredInstalled", 0);
	}
}

bool repairRegistration()
{
	if (DeviceAPOInfo::checkAPORegistration(false))
		return false;

	wchar_t executablePath[MAX_PATH];
	if (GetModuleFileNameW(NULL, executablePath, MAX_PATH) == 0)
		throw runtime_error("Could not locate DeviceControl.exe.");
	filesystem::path dllPath = filesystem::path(executablePath).parent_path() / L"EqualizerAPO.dll";
	if (!filesystem::is_regular_file(dllPath))
		throw runtime_error("EqualizerAPO.dll is missing from the application directory.");

	wstring parameters = L"/s \"" + dllPath.wstring() + L"\"";
	SHELLEXECUTEINFOW executeInfo = {};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	executeInfo.lpVerb = L"open";
	executeInfo.lpFile = L"regsvr32.exe";
	executeInfo.lpParameters = parameters.c_str();
	executeInfo.nShow = SW_HIDE;
	if (!ShellExecuteExW(&executeInfo))
		throw runtime_error("Could not start regsvr32.exe.");
	WaitForSingleObject(executeInfo.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(executeInfo.hProcess, &exitCode);
	CloseHandle(executeInfo.hProcess);
	if (exitCode != 0 || !DeviceAPOInfo::checkAPORegistration(false))
		throw runtime_error("EqualizerAPO.dll registration failed.");
	return true;
}

int applyRequests(const vector<DeviceRequest>& requests)
{
	vector<shared_ptr<AbstractAPOInfo>> devices = loadDevices();
	vector<EndpointIdentity> catalog = EndpointIdentityProvider::enumerate();
	struct Operation
	{
		shared_ptr<AbstractAPOInfo> info;
		const DeviceRequest* request;
	};
	vector<Operation> operations;
	set<pair<bool, wstring>> resolvedEndpoints;
	for (const DeviceRequest& request : requests)
	{
		wstring resolvedGuid;
		if (request.stable)
		{
			DeviceResolution resolution = DeviceIdentityResolver::resolve(request.selector, catalog);
			if (resolution.status == DeviceResolutionStatus::Ambiguous)
				return fail(L"An audio endpoint binding is ambiguous. No changes were applied:\n\n" + request.selector.bindingId);
			if (resolution.endpoint == nullptr)
				return fail(L"An audio endpoint binding could not be resolved. No changes were applied:\n\n" + request.selector.bindingId);
			resolvedGuid = resolution.endpoint->endpointGuid;
		}
		auto it = find_if(devices.begin(), devices.end(), [&](const shared_ptr<AbstractAPOInfo>& info) {
			return info->isInput() == request.input && (request.stable
				? equalGuid(info->getDeviceGuid(), resolvedGuid) : matchesIdentity(*info, request.identity));
		});
		if (it == devices.end())
			return fail(L"An audio endpoint in the request no longer exists:\n\n" + request.identity);
		if (!resolvedEndpoints.insert({request.input, (*it)->getDeviceGuid()}).second)
			return fail(L"The device request resolves more than once to the same endpoint. No changes were applied.");
		operations.push_back({*it, &request});
	}

	bool changed = !DeviceAPOInfo::checkProtectedAudioDG(true);
	changed = repairRegistration() || changed;
	for (const auto& operation : operations)
	{
		const shared_ptr<AbstractAPOInfo>& info = operation.info;
		bool install = operation.request->install;
		if (install && !info->isInstalled())
		{
			info->install();
			changed = true;
		}
		else if (!install && info->isInstalled())
		{
			info->uninstall();
			changed = true;
		}
		else if (install && (info->canBeUpgraded() || info->hasChanges() || info->isEnhancementsDisabled()))
		{
			info->reinstall();
			changed = true;
		}
		if (!install)
			disableBindingsForEndpoint(info->getDeviceGuid(), catalog);
		persistInstallBinding(*operation.request, info->getEndpointIdentity());
	}

	VoicemeeterAPOInfo::ensureVoicemeeterClientRunning();
	if (changed)
		ServiceHelper::restartService(L"AudioSrv");
	return 0;
}

int uninstallAll()
{
	vector<shared_ptr<AbstractAPOInfo>> devices = loadDevices();
	bool changed = false;
	for (const shared_ptr<AbstractAPOInfo>& info : devices)
	{
		if (info->isInstalled())
		{
			info->uninstall();
			changed = true;
		}
	}
	if (changed)
		ServiceHelper::restartService(L"AudioSrv");
	if (RegistryHelper::keyExists(bindingsPath))
	{
		for (const wstring& bindingId : RegistryHelper::enumSubKeys(bindingsPath))
		{
			wstring key = bindingsPath + L"\\" + bindingId;
			if (RegistryHelper::valueExists(key, L"DesiredInstalled"))
				RegistryHelper::writeDWORDValue(key, L"DesiredInstalled", 0);
		}
	}
	return 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	int argumentCount = 0;
	wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (arguments == NULL)
		return fail(L"Could not read the command line.");

	HRESULT comResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	int result = 1;
	try
	{
		if (argumentCount == 3 && wcscmp(arguments[1], L"--apply") == 0)
		{
			filesystem::path manifest(arguments[2]);
			if (!manifest.is_absolute() || !filesystem::is_regular_file(manifest))
				result = fail(L"The device request file does not exist.");
			else
				result = applyRequests(readManifest(manifest));
		}
		else if (argumentCount == 2 && wcscmp(arguments[1], L"--uninstall-all") == 0)
		{
			result = uninstallAll();
		}
		else
		{
			result = fail(L"Expected --apply <request-file> or --uninstall-all.");
		}
	}
	catch (RegistryException e)
	{
		result = fail(e.getMessage());
	}
	catch (ServiceException e)
	{
		result = fail(L"Windows Audio could not be restarted:\n\n" + e.getMessage());
	}
	catch (DeviceException e)
	{
		result = fail(e.getMessage());
	}
	catch (const exception& e)
	{
		string message = e.what();
		result = fail(wstring(message.begin(), message.end()));
	}

	LocalFree(arguments);
	if (SUCCEEDED(comResult))
		CoUninitialize();
	return result;
}
