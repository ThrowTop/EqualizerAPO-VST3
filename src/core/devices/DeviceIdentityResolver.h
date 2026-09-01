#pragma once

#include "StableDeviceSelector.h"

#include <string>
#include <vector>

enum class DeviceResolutionStatus
{
	ExactEndpoint,
	StableContainer,
	RecoveredExactName,
	RecoveredModelTokens,
	Missing,
	Ambiguous,
	InvalidSelector
};

struct DeviceResolution
{
	DeviceResolutionStatus status = DeviceResolutionStatus::Missing;
	const EndpointIdentity* endpoint = nullptr;
	std::vector<const EndpointIdentity*> candidates;
	std::wstring diagnostic;
};

class DeviceIdentityResolver
{
public:
	static DeviceResolution resolve(const StableDeviceSelector& selector, const std::vector<EndpointIdentity>& catalog);
	static std::wstring normalizeName(const std::wstring& value);
	static std::vector<std::wstring> distinctiveModelTokens(const std::wstring& value);
	static const wchar_t* statusName(DeviceResolutionStatus status);
};
