#include <devices/DeviceIdentityResolver.h>
#include <devices/StableDeviceSelector.h>
#include <filters/DeviceFilterFactory.h>
#include <FilterEngine.h>

#include <iostream>
#include <stdexcept>

namespace
{
	constexpr const wchar_t* Binding = L"{11111111-1111-1111-1111-111111111111}";
	constexpr const wchar_t* OldEndpoint = L"{22222222-2222-2222-2222-222222222222}";
	constexpr const wchar_t* Container = L"{33333333-3333-3333-3333-333333333333}";

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	EndpointIdentity endpoint(const wchar_t* guid, AudioDataFlow flow, unsigned form, const wchar_t* device,
		const wchar_t* container = L"", const wchar_t* connection = L"Microphone")
	{
		EndpointIdentity result;
		result.endpointGuid = guid;
		result.flow = flow;
		result.formFactor = form;
		result.deviceName = device;
		result.connectionName = connection;
		result.containerId = container;
		result.channelCount = 1;
		result.channelMask = 4;
		return result;
	}

	StableDeviceSelector selector()
	{
		StableDeviceSelector result;
		result.bindingId = Binding;
		result.snapshot = endpoint(OldEndpoint, AudioDataFlow::Capture, 4, L"R\u00d8DE XDM-100", Container);
		result.allowNameFallback = true;
		return result;
	}

	void codecTests()
	{
		auto value = selector();
		value.snapshot.deviceName = L" R\u00d8DE \u4e2d\u6587 % | ; ";
		value.unknownFields.emplace_back(L"future", L"x;y|z");
		auto encoded = StableDeviceSelectorCodec::serialize(value);
		auto parsed = StableDeviceSelectorCodec::parse(encoded);
		require(parsed.status == StableSelectorParseStatus::Success, "selector round trip parses");
		require(parsed.selector.snapshot.deviceName == value.snapshot.deviceName, "Unicode and delimiters round trip");
		require(parsed.selector.unknownFields == value.unknownFields, "unknown fields round trip");
		require(StableDeviceSelectorCodec::parse(L"stable:v2|binding=x").status == StableSelectorParseStatus::UnsupportedVersion, "unsupported version rejected distinctly");
		require(StableDeviceSelectorCodec::parse(L"stable:v1|binding=%GG").status == StableSelectorParseStatus::Invalid, "malformed percent encoding rejected");
		auto malformedGuid = StableDeviceSelectorCodec::serialize(selector());
		size_t endpointField = malformedGuid.find(L"|endpoint=") + wcslen(L"|endpoint=");
		malformedGuid.replace(endpointField, malformedGuid.find(L'|', endpointField) - endpointField, L"not-a-guid");
		require(StableDeviceSelectorCodec::parse(malformedGuid).status == StableSelectorParseStatus::Invalid, "malformed endpoint GUID rejected");
		auto missing = StableDeviceSelectorCodec::serialize(selector());
		size_t deviceField = missing.find(L"|device=");
		missing.erase(deviceField, missing.find(L'|', deviceField + 1) - deviceField);
		require(StableDeviceSelectorCodec::parse(missing).status == StableSelectorParseStatus::Invalid, "missing required field rejected");
		auto duplicate = StableDeviceSelectorCodec::serialize(selector()) + L"|device=again";
		require(StableDeviceSelectorCodec::parse(duplicate).status == StableSelectorParseStatus::Invalid, "duplicate key rejected");
		require(StableDeviceSelectorCodec::parse(L"Speakers Realtek").status == StableSelectorParseStatus::NotStable, "legacy selector remains separate");
	}

	void normalizationTests()
	{
		require(DeviceIdentityResolver::normalizeName(L"  R\u00d8DE\tXDM\u2011100  ") == L"r\u00f8de xdm-100", "normalization is invariant and whitespace/dash stable");
		auto tokens = DeviceIdentityResolver::distinctiveModelTokens(L"R\u00d8DE XDM-100 USB Audio Device");
		require(tokens == std::vector<std::wstring>({L"100", L"r\u00f8de"}), "model tokens retain only distinctive reviewed tokens");
		require(DeviceIdentityResolver::distinctiveModelTokens(L"USB Audio Device").empty(), "generic device has no distinctive token");
	}

	void resolutionTests()
	{
		auto value = selector();
		std::vector<EndpointIdentity> catalog = {endpoint(OldEndpoint, AudioDataFlow::Capture, 4, L"Renamed", Container)};
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::ExactEndpoint, "exact endpoint ignores name changes");

		catalog = {endpoint(L"{44444444-4444-4444-4444-444444444444}", AudioDataFlow::Capture, 4, L"R\u00d8DE XDM-100", Container)};
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::StableContainer, "container recovers changed endpoint");

		catalog.push_back(endpoint(L"{55555555-5555-5555-5555-555555555555}", AudioDataFlow::Render, 3, L"R\u00d8DE XDM-100", Container, L"Headphones"));
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::StableContainer, "capture cannot recover to render");

		value.snapshot.containerId = L"{66666666-6666-6666-6666-666666666666}";
		catalog = {endpoint(L"{77777777-7777-7777-7777-777777777777}", AudioDataFlow::Capture, 4, L"R\u00d8DE XDM-100")};
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::RecoveredExactName, "exact name and type recover");

		catalog[0].deviceName = L"R\u00d8DE XDM-100 USB Audio";
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::RecoveredModelTokens, "model tokens recover modest suffix change");

		catalog[0].deviceName = L"R\u00d8DE XDM-100";
		catalog.push_back(endpoint(L"{88888888-8888-8888-8888-888888888888}", AudioDataFlow::Capture, 4, L"R\u00d8DE XDM-100"));
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::Ambiguous, "identical microphones are ambiguous");

		value.allowNameFallback = false;
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::Missing, "fallback none prevents name recovery");

		value = selector();
		catalog = {endpoint(OldEndpoint, AudioDataFlow::Render, 3, L"R\u00d8DE XDM-100", Container, L"Headphones")};
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::Missing, "wrong-flow exact endpoint rejected");

		value.snapshot.deviceName = L"USB Audio Device";
		value.snapshot.containerId.clear();
		catalog = {endpoint(L"{99999999-9999-9999-9999-999999999999}", AudioDataFlow::Capture, 4, L"USB Audio Device Pro")};
		require(DeviceIdentityResolver::resolve(value, catalog).status == DeviceResolutionStatus::Missing, "generic name cannot model-token recover");
	}

	void matchingTests()
	{
		auto value = selector();
		auto current = endpoint(OldEndpoint, AudioDataFlow::Capture, 4, L"R\u00d8DE XDM-100", Container);
		std::vector<EndpointIdentity> catalog = {current};
		auto stable = StableDeviceSelectorCodec::serialize(value);
		require(DeviceFilterFactory::matchDevice(current, catalog, stable), "stable selector matches current endpoint");
		require(DeviceFilterFactory::matchDevice(current, catalog, L"stable:v2|bad=x; " + stable), "invalid alternative does not override exact alternative");
		require(!DeviceFilterFactory::matchDevice(current, catalog, L"stable:v2|bad=x"), "invalid stable selector fails closed");
		require(DeviceFilterFactory::matchDevice(current, catalog, L"Speakers Missing; Microphone R\u00d8DE"), "legacy alternatives retain OR semantics");

		FilterEngine engine;
		engine.setDeviceInfo(true, current, catalog);
		DeviceFilterFactory factory;
		factory.initialize(&engine);
		factory.startOfConfiguration();
		std::wstring command = L"Device";
		std::wstring parameters = stable;
		factory.createFilter(L"", command, parameters);
		command = L"Preamp";
		parameters = L"-2 dB";
		factory.createFilter(L"", command, parameters);
		require(command == L"Preamp", "matched group keeps its effects active");
		command = L"Device";
		parameters = L"stable:v2|bad=x";
		factory.createFilter(L"", command, parameters);
		command = L"Filter";
		parameters = L"ON PK Fc 1000 Hz Gain 3 dB Q 1";
		factory.createFilter(L"", command, parameters);
		require(command.empty(), "invalid selector disables the entire following group");
	}
}

int wmain()
{
	try
	{
		codecTests();
		normalizationTests();
		resolutionTests();
		matchingTests();
		std::wcout << L"Stable device binding tests passed\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
