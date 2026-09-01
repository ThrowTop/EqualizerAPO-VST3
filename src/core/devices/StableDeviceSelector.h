#pragma once

#include "EndpointIdentity.h"

#include <string>
#include <utility>
#include <vector>

struct StableDeviceSelector
{
	unsigned version = 1;
	std::wstring bindingId;
	EndpointIdentity snapshot;
	bool allowNameFallback = true;
	std::vector<std::pair<std::wstring, std::wstring>> unknownFields;
};

enum class StableSelectorParseStatus
{
	Success,
	NotStable,
	Invalid,
	UnsupportedVersion
};

struct StableSelectorParseResult
{
	StableSelectorParseStatus status = StableSelectorParseStatus::Invalid;
	StableDeviceSelector selector;
	std::wstring diagnostic;
};

class StableDeviceSelectorCodec
{
public:
	static constexpr size_t MaxSelectorLength = 8192;
	static constexpr size_t MaxDecodedValueLength = 1024;

	static StableSelectorParseResult parse(const std::wstring& text);
	static std::wstring serialize(const StableDeviceSelector& selector);
	static std::wstring createBindingId();
	static bool isStable(const std::wstring& text);
};
