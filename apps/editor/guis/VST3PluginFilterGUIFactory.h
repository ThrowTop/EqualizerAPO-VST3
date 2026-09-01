#pragma once

#include "Editor/IFilterGUIFactory.h"

class VST3PluginFilterGUIFactory : public IFilterGUIFactory
{
	Q_OBJECT

public:
	QList<FilterTemplate> createFilterTemplates() override;
	IFilterGUI* createFilterGUI(QString& command, QString& parameters) override;
};
