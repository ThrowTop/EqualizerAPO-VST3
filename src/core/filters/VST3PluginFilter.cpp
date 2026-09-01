#include "stdafx.h"

#include <cstring>

#include "helpers/LogHelper.h"
#include "helpers/VST3PluginInstance.h"
#include "helpers/VST3PluginModule.h"
#include "VST3PluginFilter.h"

using namespace std;

VST3PluginFilter::VST3PluginFilter(wstring bundlePath, wstring classId,
	wstring processorState, wstring controllerState)
	: bundlePath(move(bundlePath)), classId(move(classId)), processorState(move(processorState)),
	controllerState(move(controllerState))
{
}

VST3PluginFilter::~VST3PluginFilter() = default;

vector<wstring> VST3PluginFilter::initialize(float sampleRate, unsigned maxFrameCount,
	vector<wstring> channelNames)
{
	instance.reset();
	bypassed = true;
	latencySamples = 0;
	channelCount = channelNames.size();
	if (channelCount != 1 && channelCount != 2)
	{
		LogF(L"VST3 plugin %s bypassed: exact mono or stereo channels are required", bundlePath.c_str());
		return channelNames;
	}

	wstring error;
	shared_ptr<VST3PluginModule> module = VST3PluginModule::load(bundlePath, &error);
	if (!module)
	{
		LogF(L"VST3 plugin %s could not be loaded: %s", bundlePath.c_str(), error.c_str());
		return channelNames;
	}

	instance = make_unique<VST3PluginInstance>(module, classId);
	if (!instance->initialize(sampleRate, maxFrameCount, static_cast<unsigned>(channelCount),
		processorState, controllerState, &error))
	{
		LogF(L"VST3 plugin %s could not initialize: %s", bundlePath.c_str(), error.c_str());
		instance.reset();
		return channelNames;
	}
	latencySamples = instance->getLatencySamples();
	bypassed = false;
	return channelNames;
}

#pragma AVRT_CODE_BEGIN
void VST3PluginFilter::process(float** output, float** input, unsigned frameCount)
{
	if (bypassed || !instance)
	{
		passDry(output, input, frameCount);
		return;
	}

	__try
	{
		if (!instance->process(output, input, frameCount))
		{
			bypassed = true;
			passDry(output, input, frameCount);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		bypassed = true;
		passDry(output, input, frameCount);
	}
}

void VST3PluginFilter::passDry(float** output, float** input, unsigned frameCount) const
{
	for (size_t channel = 0; channel < channelCount; ++channel)
		memcpy(output[channel], input[channel], frameCount * sizeof(float));
}
#pragma AVRT_CODE_END

const wstring& VST3PluginFilter::getBundlePath() const { return bundlePath; }
const wstring& VST3PluginFilter::getClassId() const { return classId; }
const wstring& VST3PluginFilter::getProcessorState() const { return processorState; }
const wstring& VST3PluginFilter::getControllerState() const { return controllerState; }
unsigned VST3PluginFilter::getLatencySamples() const { return latencySamples; }
