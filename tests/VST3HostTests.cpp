#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "filters/VST3PluginFilter.h"
#include "filters/VST3PluginFilterFactory.h"
#include "helpers/MemoryHelper.h"
#include "helpers/VST3PluginInstance.h"
#include "helpers/VST3PluginModule.h"

using namespace std;

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw runtime_error(message);
	}

	void destroyFilter(IFilter* filter)
	{
		if (filter != nullptr)
		{
			filter->~IFilter();
			MemoryHelper::free(filter);
		}
	}

	bool probeBundle(const wstring& path)
	{
		wstring error;
		shared_ptr<VST3PluginModule> module = VST3PluginModule::load(path, &error);
		if (!module)
		{
			wcerr << L"Load failed: " << error << endl;
			return false;
		}

		wcout << L"Bundle: " << module->getBundlePath() << endl;
		if (module->getClasses().empty())
		{
			wcerr << L"No audio effect classes found." << endl;
			return false;
		}

		bool anyConfigurationPassed = false;
		for (const VST3PluginClassInfo& classInfo : module->getClasses())
		{
			wcout << L"Class: " << classInfo.classId << L" | " << classInfo.vendor
				<< L" | " << classInfo.name << L" | " << classInfo.version << endl;
			for (unsigned channels : {1u, 2u})
			{
				VST3PluginInstance instance(module, classInfo.classId);
				error.clear();
				if (!instance.initialize(48000.0, 256, channels, L"", L"", &error))
				{
					wcout << L"  " << channels << L" channel init: rejected (" << error << L")" << endl;
					continue;
				}

				vector<vector<float>> input(channels, vector<float>(256, 0.125f));
				vector<vector<float>> output(channels, vector<float>(256, 0.0f));
				vector<float*> inputPointers(channels);
				vector<float*> outputPointers(channels);
				for (unsigned channel = 0; channel < channels; ++channel)
				{
					inputPointers[channel] = input[channel].data();
					outputPointers[channel] = output[channel].data();
				}
				if (!instance.process(outputPointers.data(), inputPointers.data(), 256))
				{
					wcout << L"  " << channels << L" channel process: failed" << endl;
					continue;
				}

				wstring processorState = instance.captureProcessorState();
				wstring controllerState = instance.captureControllerState();
				unsigned latency = instance.getLatencySamples();
				instance.shutdown();

				bool stateRestored = true;
				if (!processorState.empty() || !controllerState.empty())
				{
					VST3PluginInstance restored(module, classInfo.classId);
					error.clear();
					stateRestored = restored.initialize(48000.0, 256, channels,
						processorState, controllerState, &error);
					if (stateRestored)
						restored.shutdown();
				}

				wcout << L"  " << channels << L" channel process: passed, latency=" << latency
					<< L", processorState=" << processorState.size()
					<< L", controllerState=" << controllerState.size()
					<< L", restore=" << (stateRestored ? L"passed" : L"failed") << endl;
				if (!stateRestored)
					wcout << L"    Restore error: " << error << endl;
				anyConfigurationPassed = anyConfigurationPassed || stateRestored;
			}
		}
		return anyConfigurationPassed;
	}
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
		if (argc > 1)
		{
			bool passed = true;
			for (int i = 1; i < argc; ++i)
				passed = probeBundle(argv[i]) && passed;
			return passed ? 0 : 1;
		}

		wstring error;
		shared_ptr<VST3PluginModule> module = VST3PluginModule::load(EAPO_VST3_TEST_BUNDLE, &error);
		require(module != nullptr, "The deterministic VST3 test bundle did not load.");
		require(module->getClasses().size() == 1, "The test bundle should expose one audio effect.");

		const VST3PluginClassInfo& classInfo = module->getClasses().front();
		require(classInfo.classId.size() == 32, "The VST3 class ID was not serialized correctly.");

		VST3PluginInstance instance(module, classInfo.classId);
		require(instance.initialize(48000.0, 256, 2, L"", L"", &error),
			"The VST3 test processor did not initialize for stereo processing.");

		vector<float> leftIn(256, 0.25f);
		vector<float> rightIn(256, -0.25f);
		vector<float> leftOut(256, 0.0f);
		vector<float> rightOut(256, 0.0f);
		float* input[] = {leftIn.data(), rightIn.data()};
		float* output[] = {leftOut.data(), rightOut.data()};
		require(instance.process(output, input, 256), "The VST3 process call failed.");
		for (size_t i = 0; i < leftOut.size(); ++i)
			require(isfinite(leftOut[i]) && isfinite(rightOut[i]), "VST3 output contained non-finite samples.");

		wstring processorState = instance.captureProcessorState();
		wstring controllerState = instance.captureControllerState();
		require(!processorState.empty(), "The VST3 processor state was not captured.");
		instance.shutdown();

		VST3PluginInstance restored(module, classInfo.classId);
		require(restored.initialize(48000.0, 256, 2, processorState, controllerState, &error),
			"The captured VST3 state could not be restored.");
		restored.shutdown();

		wstring command = L"VST3Plugin";
		wstring parameters = L"Bundle \"" + module->getBundlePath() + L"\" ClassID \"" +
			classInfo.classId + L"\" ProcessorState \"" + processorState + L"\"";
		VST3PluginFilterFactory factory;
		vector<IFilter*> filters = factory.createFilter(L"", command, parameters);
		require(filters.size() == 1, "The VST3 configuration directive was not parsed.");
		VST3PluginFilter* filter = static_cast<VST3PluginFilter*>(filters.front());
		require(filter->getClassId() == classInfo.classId, "The parsed VST3 class ID changed.");
		destroyFilter(filter);

		command = L"VST3Plugin";
		parameters = L"Bundle \"missing.vst3\"";
		require(factory.createFilter(L"", command, parameters).empty(),
			"A VST3 directive without a class ID must fail closed.");

		wcout << L"VST3 host tests passed for " << module->getBundlePath()
			<< L" class " << classInfo.classId << endl;
		return 0;
	}
	catch (const exception& exception)
	{
		cerr << exception.what() << endl;
		return 1;
	}
}
