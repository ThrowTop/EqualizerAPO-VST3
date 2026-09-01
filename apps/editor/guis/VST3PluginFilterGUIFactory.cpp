#include "helpers/MemoryHelper.h"
#include "filters/VST3PluginFilter.h"
#include "filters/VST3PluginFilterFactory.h"
#include "VST3PluginFilterGUI.h"
#include "VST3PluginFilterGUIFactory.h"

QList<FilterTemplate> VST3PluginFilterGUIFactory::createFilterTemplates()
{
	return {FilterTemplate(QStringLiteral("VST3 plugin"), QStringLiteral("VST3Plugin:"),
		{QStringLiteral("Plugins")})};
}

IFilterGUI* VST3PluginFilterGUIFactory::createFilterGUI(QString& command, QString& parameters)
{
	if (command != QStringLiteral("VST3Plugin"))
		return nullptr;

	VST3PluginFilterFactory factory;
	std::wstring commandText = command.toStdWString();
	std::wstring parameterText = parameters.toStdWString();
	std::vector<IFilter*> filters = factory.createFilter(L"", commandText, parameterText);
	VST3PluginFilterGUI* result = nullptr;
	if (!filters.empty())
	{
		VST3PluginFilter* filter = static_cast<VST3PluginFilter*>(filters.front());
		result = new VST3PluginFilterGUI(filter->getBundlePath(), filter->getClassId(),
			filter->getProcessorState(), filter->getControllerState());
	}
	else
	{
		result = new VST3PluginFilterGUI(L"", L"", L"", L"");
	}

	for (IFilter* filter : filters)
	{
		filter->~IFilter();
		MemoryHelper::free(filter);
	}
	return result;
}
