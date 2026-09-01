#include "stdafx.h"

#include <filesystem>

#include "helpers/MemoryHelper.h"
#include "helpers/StringHelper.h"
#include "helpers/VST3PluginModule.h"
#include "VST3PluginFilter.h"
#include "VST3PluginFilterFactory.h"

using namespace std;

vector<IFilter*> VST3PluginFilterFactory::createFilter(const wstring& configPath,
	wstring& command, wstring& parameters)
{
	if (command != L"VST3Plugin")
		return {};

	wstring bundlePath;
	wstring classId;
	wstring processorState;
	wstring controllerState;
	vector<wstring> parts = StringHelper::splitQuoted(parameters, L' ');
	for (size_t i = 0; i + 1 < parts.size(); i += 2)
	{
		const wstring& key = parts[i];
		const wstring& value = parts[i + 1];
		if (key == L"Bundle")
			bundlePath = value;
		else if (key == L"ClassID")
			classId = value;
		else if (key == L"ProcessorState")
			processorState = value;
		else if (key == L"ControllerState")
			controllerState = value;
	}
	if (bundlePath.empty() || classId.empty())
		return {};

	filesystem::path path(bundlePath);
	if (path.is_relative())
		path = filesystem::path(VST3PluginModule::getDefaultPluginPath()) / path;
	bundlePath = path.lexically_normal().wstring();

	void* memory = MemoryHelper::alloc(sizeof(VST3PluginFilter));
	VST3PluginFilter* filter = new(memory) VST3PluginFilter(
		move(bundlePath), move(classId), move(processorState), move(controllerState));
	return {filter};
}
