/*
    This file is part of EqualizerAPO, a system-wide equalizer.
    Copyright (C) 2015  Jonas Thedering

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "Editor/helpers/GUIHelper.h"
#include "DeviceFilterGUIDialog.h"
#include "ui_DeviceFilterGUIDialog.h"

#include <filters/DeviceFilterFactory.h>
#include <devices/StableDeviceSelector.h>
#include <VoicemeeterAPOInfo.h>
#include <QMessageBox>

using namespace std;

DeviceFilterGUIDialog::DeviceFilterGUIDialog(QWidget* parent, DeviceFilterGUIFactory* factory, const QString& pattern)
	: QDialog(parent),
	ui(new Ui::DeviceFilterGUIDialog),
	factory(factory)
{
	ui->setupUi(this);
	resize(GUIHelper::scale(QSize(500, 350)));

	bool all = pattern.trimmed() == "all";
	ui->allDevicesCheckBox->setChecked(all);

	QStringList labels;
	labels.append(tr("Connection"));
	labels.append(tr("Device"));
	labels.append(tr("State"));
	ui->treeWidget->setHeaderLabels(labels);
	QTreeWidgetItem* outputNode = new QTreeWidgetItem(ui->treeWidget, QStringList(tr("Playback devices")));
	outputNode->setExpanded(true);
	QTreeWidgetItem* inputNode = new QTreeWidgetItem(ui->treeWidget, QStringList(tr("Capture devices")));
	inputNode->setExpanded(true);

	const vector<EndpointIdentity> catalog = factory->getEndpointCatalog();
	QMap<QString, QStringList> stableSelectorsByEndpoint;
	QSet<QString> recoveredEndpoints;
	struct UnresolvedBinding
	{
		QString selector;
		QString label;
		QString state;
	};
	QList<UnresolvedBinding> unresolvedBindings;
	const QStringList alternatives = pattern.split(';', Qt::KeepEmptyParts);
	for (const QString& rawAlternative : alternatives)
	{
		const QString alternative = rawAlternative.trimmed();
		if (!alternative.startsWith(QStringLiteral("stable:v")))
			continue;
		auto parsed = StableDeviceSelectorCodec::parse(alternative.toStdWString());
		if (parsed.status != StableSelectorParseStatus::Success)
		{
			unresolvedBindings.append({alternative, tr("Invalid device binding"), tr("Invalid selector")});
			continue;
		}
		DeviceResolution resolution = DeviceIdentityResolver::resolve(parsed.selector, catalog);
		if (resolution.endpoint != nullptr)
		{
			QString endpointGuid = QString::fromStdWString(resolution.endpoint->endpointGuid).toLower();
			stableSelectorsByEndpoint[endpointGuid].append(alternative);
			if (resolution.status == DeviceResolutionStatus::RecoveredExactName
				|| resolution.status == DeviceResolutionStatus::RecoveredModelTokens)
				recoveredEndpoints.insert(endpointGuid);
		}
		else
		{
			QString connection = QString::fromStdWString(parsed.selector.snapshot.connectionName);
			QString device = QString::fromStdWString(parsed.selector.snapshot.deviceName);
			QString label = connection.isEmpty() ? device : connection + QStringLiteral(": ") + device;
			QString state = resolution.status == DeviceResolutionStatus::Ambiguous
				? tr("Device binding ambiguous")
				: resolution.status == DeviceResolutionStatus::InvalidSelector ? tr("Invalid selector") : tr("Device missing");
			unresolvedBindings.append({alternative, label, state});
		}
	}

	const QList<shared_ptr<AbstractAPOInfo>>& devices = factory->getDevices();
	for (const shared_ptr<AbstractAPOInfo>& apoInfo : devices)
	{
		QStringList values;
		values.append(QString::fromStdWString(apoInfo->getConnectionName()));
		values.append(QString::fromStdWString(apoInfo->getDeviceName()));
		QString state;
		if (apoInfo->isInstalled())
			state = tr("APO installed");
		else
			state = tr("APO not installed");
		VoicemeeterAPOInfo* voicemeeterInfo = dynamic_cast<VoicemeeterAPOInfo*>(apoInfo.get());
		if (voicemeeterInfo != NULL && !voicemeeterInfo->isVoicemeeterInstalled())
			state += ", " + tr("Voicemeeter was uninstalled");
		values.append(state);
		QTreeWidgetItem* item = new QTreeWidgetItem(apoInfo->isInput() ? inputNode : outputNode, values);

		bool matches = !all && DeviceFilterFactory::matchDevice(apoInfo->getEndpointIdentity(), catalog, pattern.toStdWString());
		item->setCheckState(0, matches ? Qt::Checked : Qt::Unchecked);
		item->setData(0, Qt::UserRole, QVariant::fromValue(apoInfo));
		QString endpointGuid = QString::fromStdWString(apoInfo->getEndpointIdentity().endpointGuid).toLower();
		item->setData(0, Qt::UserRole + 1, stableSelectorsByEndpoint.value(endpointGuid));
		item->setData(0, Qt::UserRole + 2, recoveredEndpoints.contains(endpointGuid));
		if (recoveredEndpoints.contains(endpointGuid))
			item->setText(2, state + QStringLiteral(", ") + tr("binding recovered"));
		item->setHidden(!matches && !apoInfo->isInstalled() && ui->showOnlyInstalledCheckBox->isChecked());
		if (!apoInfo->isInstalled())
			for (int i = 0; i < ui->treeWidget->columnCount(); i++)
				item->setForeground(i, QBrush(Qt::gray));
	}

	if (!unresolvedBindings.isEmpty())
	{
		QTreeWidgetItem* unavailableNode = new QTreeWidgetItem(ui->treeWidget, QStringList(tr("Unavailable bindings")));
		unavailableNode->setExpanded(true);
		for (const auto& binding : unresolvedBindings)
		{
			QStringList values;
			values << binding.label << QString() << binding.state;
			QTreeWidgetItem* item = new QTreeWidgetItem(unavailableNode, values);
			item->setCheckState(0, Qt::Checked);
			item->setData(0, Qt::UserRole + 3, binding.selector);
			item->setForeground(2, QBrush(Qt::red));
		}
	}

	for (int i = 0; i < ui->treeWidget->columnCount(); i++)
		ui->treeWidget->resizeColumnToContents(i);
}

DeviceFilterGUIDialog::~DeviceFilterGUIDialog()
{
	delete ui;
}

QString DeviceFilterGUIDialog::getPattern()
{
	if (ui->allDevicesCheckBox->isChecked())
	{
		return "all";
	}
	else
	{
		QString pattern = "";
		bool updateRecovered = false;
		QList<std::wstring> rebindIds;
		for (int i = 0; i < ui->treeWidget->topLevelItemCount() && !updateRecovered; i++)
		{
			QTreeWidgetItem* groupItem = ui->treeWidget->topLevelItem(i);
			for (int j = 0; j < groupItem->childCount(); j++)
			{
				QTreeWidgetItem* item = groupItem->child(j);
				if (item->checkState(0) == Qt::Checked && item->data(0, Qt::UserRole + 2).toBool())
				{
					updateRecovered = QMessageBox::question(this, tr("Update binding"),
						tr("One or more device bindings were recovered by name. Update them to the current endpoint identity?"),
						QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
					break;
				}
			}
		}
		for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* groupItem = ui->treeWidget->topLevelItem(i);
			for (int j = 0; j < groupItem->childCount(); j++)
			{
				QTreeWidgetItem* item = groupItem->child(j);
				QString unresolvedSelector = item->data(0, Qt::UserRole + 3).toString();
				if (!unresolvedSelector.isEmpty() && item->checkState(0) != Qt::Checked)
				{
					auto parsed = StableDeviceSelectorCodec::parse(unresolvedSelector.toStdWString());
					if (parsed.status == StableSelectorParseStatus::Success)
						rebindIds.append(parsed.selector.bindingId);
				}
			}
		}
		auto appendAlternative = [&](const QString& alternative) {
			if (!pattern.isEmpty())
				pattern += QStringLiteral("; ");
			pattern += alternative;
		};
		for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* groupItem = ui->treeWidget->topLevelItem(i);
			for (int j = 0; j < groupItem->childCount(); j++)
			{
				QTreeWidgetItem* item = groupItem->child(j);
				if (item->checkState(0) == Qt::Checked)
				{
					shared_ptr<AbstractAPOInfo> apoInfo = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
					if (apoInfo != nullptr)
					{
						QStringList originalSelectors = item->data(0, Qt::UserRole + 1).toStringList();
						if (!originalSelectors.isEmpty() && !(updateRecovered && item->data(0, Qt::UserRole + 2).toBool()))
						{
							for (const QString& selector : originalSelectors)
								appendAlternative(selector);
						}
						else if (apoInfo->getEndpointIdentity().endpointGuid.empty())
						{
							appendAlternative(QString::fromStdWString(apoInfo->getDeviceString()));
						}
						else
						{
							StableDeviceSelector selector;
							if (!originalSelectors.isEmpty())
							{
								auto parsed = StableDeviceSelectorCodec::parse(originalSelectors.front().toStdWString());
								selector.bindingId = parsed.status == StableSelectorParseStatus::Success
									? parsed.selector.bindingId : StableDeviceSelectorCodec::createBindingId();
							}
							else
							{
								if (!rebindIds.isEmpty())
									selector.bindingId = rebindIds.takeFirst();
								else
									selector.bindingId = StableDeviceSelectorCodec::createBindingId();
							}
							selector.snapshot = apoInfo->getEndpointIdentity();
							selector.allowNameFallback = selector.snapshot.formFactor.has_value();
							appendAlternative(QString::fromStdWString(StableDeviceSelectorCodec::serialize(selector)));
						}
					}
					else
					{
						QString unresolvedSelector = item->data(0, Qt::UserRole + 3).toString();
						if (!unresolvedSelector.isEmpty())
							appendAlternative(unresolvedSelector);
					}
				}
			}
		}

		return pattern;
	}
}

void DeviceFilterGUIDialog::on_allDevicesCheckBox_toggled(bool checked)
{
	ui->treeWidget->setEnabled(!checked);
}

void DeviceFilterGUIDialog::on_showOnlyInstalledCheckBox_toggled(bool checked)
{
	for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* groupItem = ui->treeWidget->topLevelItem(i);
		for (int j = 0; j < groupItem->childCount(); j++)
		{
			QTreeWidgetItem* item = groupItem->child(j);
			shared_ptr<AbstractAPOInfo> apoInfo = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
			if (apoInfo != nullptr)
				item->setHidden(item->checkState(0) != Qt::Checked && !apoInfo->isInstalled() && checked);
		}
	}

	for (int i = 0; i < ui->treeWidget->columnCount(); i++)
		ui->treeWidget->resizeColumnToContents(i);
}
