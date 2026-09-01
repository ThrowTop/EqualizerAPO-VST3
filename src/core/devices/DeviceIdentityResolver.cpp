#include "stdafx.h"
#include "DeviceIdentityResolver.h"

#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <set>

namespace
{
	bool sameText(const std::wstring& left, const std::wstring& right)
	{
		return DeviceIdentityResolver::normalizeName(left) == DeviceIdentityResolver::normalizeName(right);
	}

	DeviceResolution resolved(DeviceResolutionStatus status, const EndpointIdentity* endpoint)
	{
		DeviceResolution result;
		result.status = status;
		result.endpoint = endpoint;
		if (endpoint != nullptr)
			result.candidates.push_back(endpoint);
		return result;
	}

	DeviceResolution unresolved(DeviceResolutionStatus status, std::vector<const EndpointIdentity*> candidates, const wchar_t* diagnostic)
	{
		DeviceResolution result;
		result.status = status;
		result.candidates = std::move(candidates);
		result.diagnostic = diagnostic;
		return result;
	}

	template<typename Predicate>
	std::vector<const EndpointIdentity*> filtered(const std::vector<const EndpointIdentity*>& values, Predicate predicate)
	{
		std::vector<const EndpointIdentity*> result;
		std::copy_if(values.begin(), values.end(), std::back_inserter(result), predicate);
		return result;
	}

	void prefer(std::vector<const EndpointIdentity*>& values, const std::function<bool(const EndpointIdentity&)>& predicate)
	{
		auto preferred = filtered(values, [&](const EndpointIdentity* endpoint) { return predicate(*endpoint); });
		if (!preferred.empty())
			values = std::move(preferred);
	}
}

std::wstring DeviceIdentityResolver::normalizeName(const std::wstring& value)
{
	if (value.empty())
		return {};
	int normalizedLength = NormalizeString(NormalizationKC, value.data(), static_cast<int>(value.size()), nullptr, 0);
	std::wstring normalized;
	if (normalizedLength > 0)
	{
		normalized.resize(static_cast<size_t>(normalizedLength));
		int written = NormalizeString(NormalizationKC, value.data(), static_cast<int>(value.size()), normalized.data(), normalizedLength);
		if (written <= 0)
			normalized = value;
		else
			normalized.resize(static_cast<size_t>(written));
	}
	else
	{
		normalized = value;
	}

	int lowerLength = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, normalized.data(), static_cast<int>(normalized.size()), nullptr, 0, nullptr, nullptr, 0);
	if (lowerLength > 0)
	{
		std::wstring lower(static_cast<size_t>(lowerLength), L'\0');
		int written = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, normalized.data(), static_cast<int>(normalized.size()), lower.data(), lowerLength, nullptr, nullptr, 0);
		if (written > 0)
		{
			lower.resize(static_cast<size_t>(written));
			normalized = std::move(lower);
		}
	}

	std::wstring result;
	bool pendingSpace = false;
	for (wchar_t character : normalized)
	{
		if (character == 0x2010 || character == 0x2011 || character == 0x2012 || character == 0x2013 || character == 0x2014 || character == 0x2212)
			character = L'-';
		if (iswspace(character))
		{
			pendingSpace = !result.empty();
			continue;
		}
		if (pendingSpace)
			result.push_back(L' ');
		pendingSpace = false;
		result.push_back(character);
	}
	return result;
}

std::vector<std::wstring> DeviceIdentityResolver::distinctiveModelTokens(const std::wstring& value)
{
	static const std::set<std::wstring> generic = {
		L"audio", L"device", L"output", L"input", L"speaker", L"speakers", L"microphone",
		L"headphone", L"headphones", L"headset", L"usb"
	};
	std::set<std::wstring> unique;
	std::wstring token;
	auto finish = [&]() {
		if (token.empty())
			return;
		bool hasDigit = std::any_of(token.begin(), token.end(), [](wchar_t character) { return iswdigit(character) != 0; });
		if (generic.find(token) == generic.end() && (hasDigit || token.size() >= 4))
			unique.insert(token);
		token.clear();
	};
	for (wchar_t character : normalizeName(value))
	{
		if (iswalnum(character))
			token.push_back(character);
		else
			finish();
	}
	finish();
	return {unique.begin(), unique.end()};
}

DeviceResolution DeviceIdentityResolver::resolve(const StableDeviceSelector& selector, const std::vector<EndpointIdentity>& catalog)
{
	if (selector.version != 1 || selector.snapshot.flow == AudioDataFlow::Unknown || !isCanonicalGuid(selector.bindingId) || !isCanonicalGuid(selector.snapshot.endpointGuid))
		return unresolved(DeviceResolutionStatus::InvalidSelector, {}, L"Selector identity is invalid");

	std::vector<const EndpointIdentity*> exact;
	for (const auto& endpoint : catalog)
	{
		if (endpoint.flow == selector.snapshot.flow && equalGuid(endpoint.endpointGuid, selector.snapshot.endpointGuid))
		{
			if (!selector.snapshot.containerId.empty() && !endpoint.containerId.empty() && !equalGuid(selector.snapshot.containerId, endpoint.containerId))
				continue;
			exact.push_back(&endpoint);
		}
	}
	if (exact.size() == 1)
		return resolved(DeviceResolutionStatus::ExactEndpoint, exact.front());
	if (exact.size() > 1)
		return unresolved(DeviceResolutionStatus::Ambiguous, std::move(exact), L"Multiple endpoints have the stored endpoint GUID");

	if (!selector.snapshot.containerId.empty())
	{
		std::vector<const EndpointIdentity*> candidates;
		for (const auto& endpoint : catalog)
		{
			if (endpoint.flow == selector.snapshot.flow && !endpoint.containerId.empty() && equalGuid(endpoint.containerId, selector.snapshot.containerId))
				candidates.push_back(&endpoint);
		}
		if (!selector.snapshot.association.empty())
			prefer(candidates, [&](const EndpointIdentity& endpoint) { return equalGuid(endpoint.association, selector.snapshot.association); });
		if (selector.snapshot.formFactor)
			candidates = filtered(candidates, [&](const EndpointIdentity* endpoint) { return endpoint->formFactor == selector.snapshot.formFactor; });
		if (candidates.size() == 1)
			return resolved(DeviceResolutionStatus::StableContainer, candidates.front());
		if (candidates.size() > 1)
			candidates = filtered(candidates, [&](const EndpointIdentity* endpoint) { return sameText(endpoint->connectionName, selector.snapshot.connectionName); });
		if (candidates.size() > 1)
			candidates = filtered(candidates, [&](const EndpointIdentity* endpoint) { return sameText(endpoint->deviceName, selector.snapshot.deviceName); });
		if (candidates.size() > 1 && selector.snapshot.channelCount != 0 && selector.snapshot.channelMask != 0)
		{
			auto channelMatches = filtered(candidates, [&](const EndpointIdentity* endpoint) {
				return endpoint->channelCount == selector.snapshot.channelCount && endpoint->channelMask == selector.snapshot.channelMask;
			});
			if (channelMatches.size() == 1)
				candidates = std::move(channelMatches);
		}
		if (candidates.size() == 1)
			return resolved(DeviceResolutionStatus::StableContainer, candidates.front());
		if (candidates.size() > 1)
			return unresolved(DeviceResolutionStatus::Ambiguous, std::move(candidates), L"Container identity matches multiple endpoints");
	}

	if (!selector.allowNameFallback || !selector.snapshot.formFactor)
		return unresolved(DeviceResolutionStatus::Missing, {}, L"No endpoint or container match and name fallback is unavailable");

	std::vector<const EndpointIdentity*> nameCandidates;
	for (const auto& endpoint : catalog)
	{
		if (endpoint.flow == selector.snapshot.flow && endpoint.formFactor == selector.snapshot.formFactor
			&& sameText(endpoint.deviceName, selector.snapshot.deviceName))
			nameCandidates.push_back(&endpoint);
	}
	if (!selector.snapshot.association.empty())
		prefer(nameCandidates, [&](const EndpointIdentity& endpoint) { return equalGuid(endpoint.association, selector.snapshot.association); });
	prefer(nameCandidates, [&](const EndpointIdentity& endpoint) { return sameText(endpoint.connectionName, selector.snapshot.connectionName); });
	if (nameCandidates.size() == 1)
		return resolved(DeviceResolutionStatus::RecoveredExactName, nameCandidates.front());
	if (nameCandidates.size() > 1)
		return unresolved(DeviceResolutionStatus::Ambiguous, std::move(nameCandidates), L"Name and endpoint type match multiple endpoints");

	auto storedTokens = distinctiveModelTokens(selector.snapshot.deviceName);
	if (storedTokens.empty())
		return unresolved(DeviceResolutionStatus::Missing, {}, L"Device name has no distinctive model tokens");
	std::vector<const EndpointIdentity*> tokenCandidates;
	for (const auto& endpoint : catalog)
	{
		if (endpoint.flow != selector.snapshot.flow || endpoint.formFactor != selector.snapshot.formFactor)
			continue;
		auto candidateTokens = distinctiveModelTokens(endpoint.deviceName);
		if (std::all_of(storedTokens.begin(), storedTokens.end(), [&](const std::wstring& token) {
			return std::find(candidateTokens.begin(), candidateTokens.end(), token) != candidateTokens.end();
		}))
			tokenCandidates.push_back(&endpoint);
	}
	if (!selector.snapshot.association.empty())
		prefer(tokenCandidates, [&](const EndpointIdentity& endpoint) { return equalGuid(endpoint.association, selector.snapshot.association); });
	prefer(tokenCandidates, [&](const EndpointIdentity& endpoint) { return sameText(endpoint.connectionName, selector.snapshot.connectionName); });
	if (tokenCandidates.size() == 1)
		return resolved(DeviceResolutionStatus::RecoveredModelTokens, tokenCandidates.front());
	if (tokenCandidates.size() > 1)
		return unresolved(DeviceResolutionStatus::Ambiguous, std::move(tokenCandidates), L"Model tokens match multiple endpoints");
	return unresolved(DeviceResolutionStatus::Missing, {}, L"No current endpoint matches the stored identity");
}

const wchar_t* DeviceIdentityResolver::statusName(DeviceResolutionStatus status)
{
	switch (status)
	{
	case DeviceResolutionStatus::ExactEndpoint: return L"exact endpoint";
	case DeviceResolutionStatus::StableContainer: return L"stable container";
	case DeviceResolutionStatus::RecoveredExactName: return L"recovered exact name";
	case DeviceResolutionStatus::RecoveredModelTokens: return L"recovered model tokens";
	case DeviceResolutionStatus::Missing: return L"missing";
	case DeviceResolutionStatus::Ambiguous: return L"ambiguous";
	default: return L"invalid selector";
	}
}
