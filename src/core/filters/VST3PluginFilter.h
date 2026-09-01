#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "IFilter.h"

class VST3PluginInstance;

#pragma AVRT_VTABLES_BEGIN
class VST3PluginFilter : public IFilter
{
public:
	VST3PluginFilter(std::wstring bundlePath, std::wstring classId,
		std::wstring processorState, std::wstring controllerState);
	~VST3PluginFilter() override;

	bool getInPlace() override { return false; }
	std::vector<std::wstring> initialize(float sampleRate, unsigned maxFrameCount,
		std::vector<std::wstring> channelNames) override;
	void process(float** output, float** input, unsigned frameCount) override;

	const std::wstring& getBundlePath() const;
	const std::wstring& getClassId() const;
	const std::wstring& getProcessorState() const;
	const std::wstring& getControllerState() const;
	unsigned getLatencySamples() const;

private:
	void passDry(float** output, float** input, unsigned frameCount) const;

	std::wstring bundlePath;
	std::wstring classId;
	std::wstring processorState;
	std::wstring controllerState;
	std::unique_ptr<VST3PluginInstance> instance;
	size_t channelCount = 0;
	std::atomic<bool> bypassed {true};
	unsigned latencySamples = 0;
};
#pragma AVRT_VTABLES_END
