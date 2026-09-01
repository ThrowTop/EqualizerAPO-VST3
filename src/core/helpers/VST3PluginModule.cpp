#include "stdafx.h"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#include "RegistryHelper.h"
#include "StringHelper.h"
#include "VST3PluginModule.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/hosting/module.h"

using namespace std;

namespace
{
	mutex moduleMutex;
	unordered_map<wstring, weak_ptr<VST3PluginModule>> moduleCache;
	wstring defaultPluginPath;
}

shared_ptr<VST3PluginModule> VST3PluginModule::load(const wstring& bundlePath, wstring* error)
{
	error_code ec;
	wstring canonicalPath = filesystem::weakly_canonical(bundlePath, ec).wstring();
	if (ec)
		canonicalPath = filesystem::path(bundlePath).lexically_normal().wstring();

	lock_guard<mutex> lock(moduleMutex);
	auto cached = moduleCache.find(canonicalPath);
	if (cached != moduleCache.end())
	{
		if (shared_ptr<VST3PluginModule> existing = cached->second.lock())
			return existing;
	}

	string loadError;
	shared_ptr<VST3::Hosting::Module> sdkModule = VST3::Hosting::Module::create(
		StringHelper::toString(canonicalPath, CP_UTF8), loadError);
	if (!sdkModule)
	{
		if (error != nullptr)
			*error = StringHelper::toWString(loadError, CP_UTF8);
		return nullptr;
	}

	shared_ptr<VST3PluginModule> result(new VST3PluginModule(canonicalPath, sdkModule));
	moduleCache[canonicalPath] = result;
	return result;
}

wstring VST3PluginModule::getDefaultPluginPath()
{
	if (defaultPluginPath.empty())
	{
		wstring installPath = RegistryHelper::readValue(APP_REGPATH, L"InstallPath");
		defaultPluginPath = installPath + L"\\VST3";
	}
	return defaultPluginPath;
}

vector<wstring> VST3PluginModule::getInstalledModulePaths()
{
	vector<wstring> result;
	for (const string& path : VST3::Hosting::Module::getModulePaths())
		result.push_back(StringHelper::toWString(path, CP_UTF8));

	error_code ec;
	filesystem::path applicationPath(getDefaultPluginPath());
	if (filesystem::exists(applicationPath, ec))
	{
		for (filesystem::recursive_directory_iterator it(applicationPath,
			filesystem::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec))
		{
			if (it->is_directory(ec) && it->path().extension() == L".vst3")
			{
				result.push_back(it->path().wstring());
				it.disable_recursion_pending();
			}
		}
	}
	sort(result.begin(), result.end());
	result.erase(unique(result.begin(), result.end()), result.end());
	return result;
}

const wstring& VST3PluginModule::getBundlePath() const
{
	return bundlePath;
}

const vector<VST3PluginClassInfo>& VST3PluginModule::getClasses() const
{
	return classes;
}

shared_ptr<VST3::Hosting::Module> VST3PluginModule::getModule() const
{
	return module;
}

VST3PluginModule::VST3PluginModule(wstring bundlePath, shared_ptr<VST3::Hosting::Module> module)
	: bundlePath(move(bundlePath)), module(move(module))
{
	for (const auto& classInfo : this->module->getFactory().classInfos())
	{
		if (classInfo.category() != kVstAudioEffectClass)
			continue;

		VST3PluginClassInfo result;
		result.classId = StringHelper::toWString(classInfo.ID().toString(), CP_UTF8);
		result.name = StringHelper::toWString(classInfo.name(), CP_UTF8);
		result.vendor = StringHelper::toWString(classInfo.vendor(), CP_UTF8);
		result.version = StringHelper::toWString(classInfo.version(), CP_UTF8);
		classes.push_back(move(result));
	}
}
