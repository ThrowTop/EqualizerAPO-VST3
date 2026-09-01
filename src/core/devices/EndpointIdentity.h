#pragma once

#include <optional>
#include <string>

enum class AudioDataFlow
{
	Unknown,
	Render,
	Capture
};

struct EndpointIdentity
{
	std::wstring mmDeviceId;
	std::wstring endpointGuid;
	std::wstring containerId;
	AudioDataFlow flow = AudioDataFlow::Unknown;
	std::optional<unsigned> formFactor;
	std::wstring association;
	std::wstring connectionName;
	std::wstring deviceName;
	unsigned channelCount = 0;
	unsigned long channelMask = 0;
};

std::wstring audioDataFlowToString(AudioDataFlow flow);
AudioDataFlow audioDataFlowFromString(const std::wstring& value);
bool equalGuid(const std::wstring& left, const std::wstring& right);
bool isCanonicalGuid(const std::wstring& value);
