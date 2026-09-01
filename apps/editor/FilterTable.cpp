/*
    This file is part of EqualizerAPO, a system-wide equalizer.
    Copyright (C) 2014  Jonas Thedering

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

#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QFrame>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCursor>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <QDial>
#include <QJsonDocument>
#include <QSettings>
#include <QStyle>
#include <QToolBar>

#include "MainWindow.h"
#include "FilterTableRow.h"
#include "FilterTableMimeData.h"
#include "guis/ExpressionFilterGUIFactory.h"
#include "guis/CommentFilterGUIFactory.h"
#include "guis/DeviceFilterGUIFactory.h"
#include "guis/DeviceFilterGUIDialog.h"
#include "guis/ChannelFilterGUIFactory.h"
#include "guis/StageFilterGUIFactory.h"
#include "guis/PreampFilterGUIFactory.h"
#include "guis/BiQuadFilterGUIFactory.h"
#include "guis/CopyFilterGUIFactory.h"
#include "guis/DelayFilterGUIFactory.h"
#include "guis/IncludeFilterGUIFactory.h"
#include "guis/GraphicEQFilterGUIFactory.h"
#include "guis/ConvolutionFilterGUIFactory.h"
#include "guis/VSTPluginFilterGUIFactory.h"
#include "guis/VST3PluginFilterGUIFactory.h"
#include "guis/LoudnessCorrectionFilterGUIFactory.h"
#include "Editor/helpers/GUIHelper.h"
#include "helpers/StringHelper.h"
#include "helpers/LogHelper.h"
#include "helpers/ChannelHelper.h"
#include "helpers/RegistryHelper.h"
#include "FilterTable.h"
#include <filters/DeviceFilterFactory.h>

using namespace std;

namespace
{
	const QString deviceGroupDisabledPrefix = QStringLiteral("# EAPO-GROUP-OFF ");
	const QString deviceGroupCollapsedPreference = QStringLiteral("deviceGroupCollapsed");
}

FilterTable::FilterTable(MainWindow* mainWindow, QWidget* parent)
	: QWidget(parent), mainWindow(mainWindow)
{
	gridLayout = new QGridLayout(this);

	QIcon icon(QStringLiteral(":/icons/arrow_right.ico"));
	insertArrow = new QLabel(this);
	insertArrow->setPixmap(icon.pixmap(GUIHelper::scale(QSize(24, 15))));
	insertArrow->setVisible(false);

	factories.append(new ExpressionFilterGUIFactory);
	factories.append(new CommentFilterGUIFactory);
	factories.append(new IncludeFilterGUIFactory);
	deviceFilterFactory = new DeviceFilterGUIFactory;
	factories.append(deviceFilterFactory);
	factories.append(new ChannelFilterGUIFactory);
	factories.append(new StageFilterGUIFactory);
	factories.append(new PreampFilterGUIFactory);
	factories.append(new BiQuadFilterGUIFactory);
	factories.append(new DelayFilterGUIFactory);
	factories.append(new CopyFilterGUIFactory);
	factories.append(new GraphicEQFilterGUIFactory);
	factories.append(new ConvolutionFilterGUIFactory);
	factories.append(new VSTPluginFilterGUIFactory);
	factories.append(new VST3PluginFilterGUIFactory);
	factories.append(new LoudnessCorrectionFilterGUIFactory);

	QApplication::instance()->installEventFilter(this);
}

FilterTable::~FilterTable()
{
	for (IFilterGUIFactory* factory : factories)
		delete factory;
	factories.clear();
}

void FilterTable::initialize(QScrollArea* scrollArea, const QList<shared_ptr<AbstractAPOInfo>>& outputDevices, const QList<shared_ptr<AbstractAPOInfo>>& inputDevices)
{
	this->scrollArea = scrollArea;
	this->outputDevices = outputDevices;
	this->inputDevices = inputDevices;

	for (IFilterGUIFactory* factory : factories)
		factory->initialize(this);
}

void FilterTable::updateDeviceAndChannelMask(shared_ptr<AbstractAPOInfo> selectedDevice, int channelMask)
{
	this->selectedDevice = selectedDevice;
	this->selectedChannelMask = channelMask;

	if (!items.empty())
		updateGuis();
}

void FilterTable::updateGuis()
{
	QElapsedTimer timer;
	timer.start();

	for (Item* item : items)
	{
		if (item->gui != NULL)
		{
			item->prefs.clear();
			item->gui->storePreferences(item->prefs);
		}
	}

	delete layout();

	for (QObject* object : children())
	{
		if (object != insertArrow)
		{
			QWidget* widget = qobject_cast<QWidget*>(object);
			if (widget != NULL)
				widget->setVisible(false);
			object->deleteLater();
		}
	}

	qDebug("Delete took %d ms", timer.elapsed());
	timer.start();

	gridLayout = new QGridLayout(this);
	gridLayout->setContentsMargins(0, 0, 0, 0);
	gridLayout->setSpacing(0);
	gridLayout->setColumnStretch(0, 0);
	gridLayout->setColumnStretch(1, 1);

	for (IFilterGUIFactory* factory : factories)
		factory->startOfFile(configPath);

	int row = 0;
	int groupNumber = 0;
	bool insideDeviceGroup = false;
	bool currentGroupEnabled = true;
	bool currentGroupCollapsed = false;
	for (Item* item : items)
	{
		QString devicePattern;
		bool deviceGroupEnabled;
		if (parseDeviceGroupLine(item->text, &devicePattern, &deviceGroupEnabled))
		{
			groupNumber++;
			insideDeviceGroup = true;
			currentGroupEnabled = deviceGroupEnabled;
			currentGroupCollapsed = item->prefs.value(deviceGroupCollapsedPreference, false).toBool();
			int effectCount = 0;
			int groupEnd = deviceGroupEnd(row);
			for (int i = row + 1; i < groupEnd; i++)
			{
				QString commandLine = stripDeviceGroupDisabledPrefix(items[i]->text).trimmed();
				if (commandLine.startsWith('#'))
					commandLine = commandLine.mid(1).trimmed();
				if (commandLine.contains(':'))
					effectCount++;
			}
			QWidget* header = createDeviceGroupHeader(item, groupNumber, devicePattern, deviceGroupEnabled, effectCount);
			gridLayout->addWidget(header, row, 0);
			item->gui = NULL;
			row++;
			continue;
		}

		QString line = currentGroupEnabled ? item->text : stripDeviceGroupDisabledPrefix(item->text);
		if (line.trimmed().isEmpty())
		{
			QWidget* separator = new QWidget(this);
			separator->setFixedHeight(GUIHelper::scale(7));
			gridLayout->addWidget(separator, row, 0);
			separator->setVisible(!currentGroupCollapsed);
			item->gui = NULL;
			row++;
			continue;
		}
		IFilterGUI* gui = NULL;
		int pos = line.indexOf(':');
		if (pos != -1)
		{
			QString key = line.mid(0, pos);
			QString value = line.mid(pos + 1);

			// allow to use indentation
			key = key.trimmed();
			QString factoryKey = key;
			QString factoryValue = value;

			for (IFilterGUIFactory* factory : factories)
			{
				gui = factory->createFilterGUI(factoryKey, factoryValue);

				if (gui != NULL || factoryKey == "")
					break;
			}

			if (gui != NULL)
			{
				for (IFilterGUIFactory* factory : factories)
				{
					gui = factory->decorateFilterGUI(gui);
				}
			}
		}

		FilterTableRow* rowWidget = new FilterTableRow(this, row + 1, item, gui);
		if (insideDeviceGroup)
			rowWidget->setContentsMargins(GUIHelper::scale(14), 0, 0, 0);
		rowWidget->setEnabled(currentGroupEnabled);
		gridLayout->addWidget(rowWidget, row, 0);
		rowWidget->setVisible(!currentGroupCollapsed);

		item->gui = gui;

		if (gui != NULL)
		{
			gui->loadPreferences(item->prefs);

			connect(gui, SIGNAL(updateModel()), this, SLOT(updateModel()));
			connect(gui, SIGNAL(updateChannels()), this, SLOT(updateChannels()));
		}

		row++;
	}

	for (IFilterGUIFactory* factory : factories)
		factory->endOfFile(configPath);

	propagateChannels();

	QFrame* addBar = new QFrame;
	QHBoxLayout* addLayout = new QHBoxLayout(addBar);
	addLayout->setContentsMargins(GUIHelper::scale(25), GUIHelper::scale(8), 0, GUIHelper::scale(4));
	addLayout->setSpacing(GUIHelper::scale(8));
	QPushButton* addGroupButton = new QPushButton(tr("Add device group"), addBar);
	QPushButton* addFilterButton = new QPushButton(tr("Add effect"), addBar);
	connect(addGroupButton, SIGNAL(clicked()), this, SLOT(addDeviceGroup()));
	connect(addFilterButton, SIGNAL(clicked()), this, SLOT(addActionTriggered()));
	addLayout->addWidget(addGroupButton);
	addLayout->addWidget(addFilterButton);
	addLayout->addStretch(1);

	gridLayout->addWidget(addBar, row++, 0, 1, 1, Qt::AlignLeft | Qt::AlignTop);

	QSpacerItem* spacerItem = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
	gridLayout->addItem(spacerItem, row, 0);

	gridLayout->setRowStretch(row, 1);

	disableWheelForWidgets();

	qDebug("Create took %d ms", timer.elapsed());
	update();
}

void FilterTable::propagateChannels()
{
	vector<wstring> channelNames;
	if (selectedDevice != NULL)
		channelNames = ChannelHelper::getChannelNames(selectedDevice->getChannelCount(), selectedChannelMask);

	for (Item* item : items)
	{
		if (item->gui != NULL)
			item->gui->configureChannels(channelNames);
	}
}

QList<QString> FilterTable::getLines()
{
	QList<QString> result;
	for (Item* item : items)
		result.append(item->text);

	return result;
}

void FilterTable::setLines(const QString& configPath, const QList<QString>& lines)
{
	this->configPath = configPath;

	qDeleteAll(items);
	items.clear();

	bool insertedAllDevicesGroup = false;
	for (const QString& line : lines)
	{
		QString trimmed = line.trimmed();
		if (trimmed.isEmpty() || (trimmed.startsWith('#') && !trimmed.startsWith(deviceGroupDisabledPrefix)))
			continue;

		if (!parseDeviceGroupLine(line))
			insertedAllDevicesGroup = true;
		break;
	}
	if (insertedAllDevicesGroup)
		items.append(new Item(QStringLiteral("Device: all")));

	for (QString line : lines)
	{
		items.append(new Item(line));
	}

	QSettings settings(QString::fromWCharArray(EDITOR_PER_FILE_REGPATH), QSettings::NativeFormat);
	settings.beginGroup(QString(configPath).replace('\\', '|'));
	QVariant prefsValue = settings.value("rowPrefs");
	QStringList prefLines;
	if (prefsValue.isValid())
		prefLines = prefsValue.toStringList();
	for (QString prefLine : prefLines)
	{
		int index = prefLine.indexOf(':');
		int lineNumber = 0;
		if (index != -1)
			lineNumber = prefLine.left(index).toInt() + (insertedAllDevicesGroup ? 1 : 0);

		QString prefCommand;
		QString prefString;
		if (lineNumber > 0)
		{
			int index2 = prefLine.indexOf(':', index + 1);
			if (index2 != -1)
			{
				prefCommand = prefLine.mid(index + 1, index2 - index - 1);
				prefString = prefLine.mid(index2 + 1);

				if (lineNumber <= items.size())
				{
					Item* item = items[lineNumber - 1];

					QString command;
					QString preferenceLine = stripDeviceGroupDisabledPrefix(item->text);
					int index = preferenceLine.indexOf(':');
					if (index != -1)
						command = preferenceLine.left(index).trimmed();

					if (command == prefCommand)
						item->prefs = QJsonDocument::fromJson(prefString.toUtf8()).toVariant().toMap();
				}
			}
		}
	}
	setScrollOffsets(settings.value("scrollX", 0).toInt(), settings.value("scrollY", 0).toInt());
	settings.endGroup();

	if (!items.isEmpty())
	{
		focused = items[0];
		selectionStart = items[0];
	}
	else
	{
		focused = NULL;
		selectionStart = NULL;
	}

	updateGuis();
}

FilterTable::Item* FilterTable::addLine(const QString& line, FilterTable::Item* before)
{
	Item* newItem = new Item(line);

	if (before != NULL)
	{
		int index = items.indexOf(before);
		items.insert(index, newItem);
	}
	else
	{
		items.append(newItem);
	}

	normalizeDeviceGroupLines();
	emit linesChanged();

	return newItem;
}

void FilterTable::removeItem(FilterTable::Item* item)
{
	if (parseDeviceGroupLine(item->text))
	{
		removeDeviceGroup(item);
		return;
	}
	items.removeOne(item);
	emit linesChanged();
}

void FilterTable::setDeviceGroupEnabled(FilterTable::Item* item, bool enabled)
{
	int start = items.indexOf(item);
	if (start == -1 || !parseDeviceGroupLine(item->text))
		return;

	int end = deviceGroupEnd(start);
	for (int i = start; i < end; i++)
	{
		if (enabled)
			items[i]->text = stripDeviceGroupDisabledPrefix(items[i]->text);
		else if (!items[i]->text.trimmed().isEmpty()
			&& !items[i]->text.trimmed().startsWith('#')
			&& !items[i]->text.startsWith(deviceGroupDisabledPrefix))
			items[i]->text.prepend(deviceGroupDisabledPrefix);
	}

	emit linesChanged();
	updateGuis();
}

void FilterTable::selectDeviceGroupDevices(FilterTable::Item* item)
{
	QString pattern;
	bool enabled;
	if (!parseDeviceGroupLine(item->text, &pattern, &enabled))
		return;

	DeviceFilterGUIDialog dialog(this, deviceFilterFactory, pattern);
	if (dialog.exec() != QDialog::Accepted)
		return;

	QString line = QStringLiteral("Device: ") + dialog.getPattern();
	item->text = enabled ? line : deviceGroupDisabledPrefix + line;
	emit linesChanged();
	updateGuis();
}

void FilterTable::addFilterToDeviceGroup(FilterTable::Item* item)
{
	int start = items.indexOf(item);
	if (start == -1)
		return;

	QMenu* menu = createAddPopupMenu();
	QAction* action = menu->exec(QCursor::pos());
	if (action != NULL)
	{
		FilterTemplate filterTemplate = action->data().value<FilterTemplate>();
		int end = deviceGroupEnd(start);
		bool enabled;
		parseDeviceGroupLine(item->text, NULL, &enabled);
		Item* newItem = new Item(filterTemplate.getLine());
		if (!enabled && !newItem->text.trimmed().isEmpty() && !newItem->text.trimmed().startsWith('#'))
			newItem->text.prepend(deviceGroupDisabledPrefix);
		items.insert(end, newItem);
		emit linesChanged();
		updateGuis();
	}
	delete menu;
}

void FilterTable::removeDeviceGroup(FilterTable::Item* item)
{
	int start = items.indexOf(item);
	if (start == -1 || !parseDeviceGroupLine(item->text))
		return;

	int end = deviceGroupEnd(start);
	for (int i = end - 1; i >= start; i--)
	{
		Item* removedItem = items.takeAt(i);
		selected.remove(removedItem);
		if (focused == removedItem)
			focused = NULL;
		if (selectionStart == removedItem)
			selectionStart = NULL;
		delete removedItem;
	}

	if (focused == NULL && !items.isEmpty())
	{
		focused = items[qMin(start, items.size() - 1)];
		selectionStart = focused;
	}

	emit linesChanged();
	updateGuis();
}

QMenu* FilterTable::createAddPopupMenu()
{
	QHash<QList<QString>, QMenu*> pathMap;
	QMenu* rootMenu = new QMenu;
	pathMap[QStringList()] = rootMenu;

	for (IFilterGUIFactory* f : factories)
	{
		QList<FilterTemplate> templates = f->createFilterTemplates();
		for (FilterTemplate t : templates)
		{
			QMenu* menu = pathMap.value(t.getPath());
			if (menu == NULL)
			{
				QMenu* parentMenu = rootMenu;
				QStringList currentPath;
				for (QString pathSegment : t.getPath())
				{
					currentPath.append(pathSegment);
					menu = pathMap.value(currentPath);
					if (menu == NULL)
					{
						menu = new QMenu(pathSegment);
						pathMap.insert(currentPath, menu);
						parentMenu->addMenu(menu);
					}
					parentMenu = menu;
				}
			}

			QAction* action = menu->addAction(t.getName());
			action->setData(QVariant::fromValue(t));
		}
	}

	return rootMenu;
}

void FilterTable::cut()
{
	copy();
	deleteSelectedLines();
}

void FilterTable::copy()
{
	QSet<Item*> itemsToCopy = selected;
	for (Item* selectedItem : selected)
	{
		int start = items.indexOf(selectedItem);
		if (start != -1 && parseDeviceGroupLine(selectedItem->text))
		{
			int end = deviceGroupEnd(start);
			for (int i = start; i < end; i++)
				itemsToCopy.insert(items[i]);
		}
	}

	QString text;
	QList<QVariantMap> prefsList;
	bool first = true;
	for (Item* item : items)
	{
		if (itemsToCopy.contains(item))
		{
			if (first)
				first = false;
			else
				text += "\n";
			text += item->text;
			prefsList.append(item->prefs);
		}
	}

	if (itemsToCopy.size() > 0)
	{
		FilterTableMimeData* mimeData = new FilterTableMimeData;
		mimeData->setText(text);
		mimeData->setPrefsList(prefsList);
		QClipboard* clipboard = QApplication::clipboard();
		clipboard->setMimeData(mimeData);
	}
}

void FilterTable::paste()
{
	QClipboard* clipboard = QApplication::clipboard();
	const QMimeData* mimeData = clipboard->mimeData();
	if (mimeData->hasText())
	{
		int dropRow = items.size();
		for (int i = 0; i < items.size(); i++)
		{
			if (selected.contains(items[i]))
			{
				dropRow = i;
				break;
			}
		}

		QString text = mimeData->text();
		QStringList textLines = text.split("\n");
		QList<QVariantMap> prefsList;
		const FilterTableMimeData* filterTableMimeData = qobject_cast<const FilterTableMimeData*>(mimeData);
		if (filterTableMimeData != NULL)
			prefsList = filterTableMimeData->getPrefsList();

		selected.clear();
		focused = NULL;
		selectionStart = NULL;
		for (int i = 0; i < textLines.size(); i++)
		{
			QString line = textLines[i];
			Item* item = new Item(line);
			if (!prefsList.isEmpty())
				item->prefs = prefsList[i];
			selected.insert(item);
			items.insert(dropRow++, item);
			if (focused == NULL)
			{
				focused = item;
				selectionStart = item;
			}
		}

		normalizeDeviceGroupLines();
		emit linesChanged();
		updateGuis();
	}
}

void FilterTable::deleteSelectedLines()
{
	QSet<Item*> itemsToDelete = selected;
	for (Item* selectedItem : selected)
	{
		int start = items.indexOf(selectedItem);
		if (start != -1 && parseDeviceGroupLine(selectedItem->text))
		{
			int end = deviceGroupEnd(start);
			for (int i = start; i < end; i++)
				itemsToDelete.insert(items[i]);
		}
	}

	QList<Item*> newItems;
	for (Item* item : items)
	{
		if (itemsToDelete.contains(item))
		{
			if (item == focused)
				focused = NULL;
			if (item == selectionStart)
				selectionStart = NULL;
			delete item;
		}
		else
		{
			newItems.append(item);
		}
	}
	selected.clear();
	items = newItems;
	normalizeDeviceGroupLines();
	emit linesChanged();
	updateGuis();
}

void FilterTable::selectAll()
{
	selected.clear();
	for (Item* item : items)
		selected.insert(item);
	update();
}

void FilterTable::updateModel()
{
	normalizeDeviceGroupLines();
	emit linesChanged();
}

void FilterTable::updateChannels()
{
	propagateChannels();
}

void FilterTable::addActionTriggered()
{
	QMenu* menu = createAddPopupMenu();
	QAction* action = menu->exec(QCursor::pos());
	if (action != NULL)
	{
		if (items.isEmpty())
			items.append(new Item(QStringLiteral("Device: all")));
		FilterTemplate t = action->data().value<FilterTemplate>();
		QString line = t.getLine();
		addLine(line);
		updateGuis();
	}
	delete menu;
}

void FilterTable::addDeviceGroup()
{
	Item* item = addLine(QStringLiteral("Device: all"));
	focused = item;
	selectionStart = item;
	selected.clear();
	updateGuis();
}

void FilterTable::openConfig(QString path)
{
	mainWindow->load(path);
}

int FilterTable::getPreferredWidth()
{
	if (scrollArea == NULL)
		return width();

	return scrollArea->viewport()->width();
}

void FilterTable::updateSizeHints()
{
	for (int i = 0; i < items.size(); i++)
	{
		FilterTableRow* tableRow = qobject_cast<FilterTableRow*>(gridLayout->itemAtPosition(i, 0)->widget());
		if (tableRow != NULL)
			tableRow->updateGeometry();
	}
}

QSize FilterTable::minimumSizeHint() const
{
	QSize size = QWidget::minimumSizeHint();
	if (size.height() < minimumHeightHint)
		size.setHeight(minimumHeightHint);

	return size;
}

void FilterTable::setMinimumHeightHint(int height)
{
	minimumHeightHint = height;
	updateGeometry();
}

void FilterTable::savePreferences()
{
	if (!configPath.isEmpty())
	{
		QStringList prefLines;

		for (int i = 0; i < items.size(); i++)
		{
			Item* item = items[i];

			if (item->gui != NULL)
			{
				item->prefs.clear();
				item->gui->storePreferences(item->prefs);
			}

			if (!item->prefs.isEmpty())
			{
				QString command;
				QString preferenceLine = stripDeviceGroupDisabledPrefix(item->text);
				int index = preferenceLine.indexOf(':');
				if (index != -1)
					command = preferenceLine.left(index).trimmed();

				QByteArray byteArray = QJsonDocument::fromVariant(item->prefs).toJson(QJsonDocument::Compact);
				QString string = QString("%0:%1:%2").arg(i + 1).arg(command).arg(QString::fromUtf8(byteArray));
				prefLines.append(string);
			}
		}

		QSettings settings(QString::fromWCharArray(EDITOR_PER_FILE_REGPATH), QSettings::NativeFormat);
		settings.beginGroup(QString(configPath).replace('\\', '|'));
		settings.setValue("rowPrefs", prefLines);
		settings.setValue("scrollX", scrollArea->horizontalScrollBar()->value());
		settings.setValue("scrollY", scrollArea->verticalScrollBar()->value());
		settings.endGroup();
	}
}

void FilterTable::setScrollOffsets(int x, int y)
{
	presetScrollX = x;
	presetScrollY = y;
}

void FilterTable::updateAnalysis()
{
	if (isVisible())
		mainWindow->startAnalysis();
}

void FilterTable::mousePressEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton)
	{
		int row = rowForPos(event->pos(), false);
		if (row != -1)
		{
			Item* item = items[row];

			if (event->modifiers() & Qt::ControlModifier)
			{
				if (!selected.remove(item))
					selected.insert(item);
				selectionStart = item;
			}
			else if (event->modifiers() & Qt::ShiftModifier)
			{
				int startRow = items.indexOf(selectionStart);
				if (startRow != -1)
				{
					selected.clear();
					for (int i = min(startRow, row); i <= max(startRow, row); i++)
					{
						selected.insert(items[i]);
					}
				}
			}
			else
			{
				if (!selected.contains(item))
				{
					selected.clear();
					selected.insert(item);
				}
				selectionStart = item;
			}
			focused = item;
			ensureRowVisible(row);
			update();

			dragStartPos = event->pos();
		}
		else
		{
			selected.clear();
			update();
		}
	}
}

void FilterTable::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		int row = rowForPos(event->pos(), false);
		if (row != -1)
		{
			Item* item = items[row];

			if (!(event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier))
			{
				if (selected.contains(item) && selectionStart == item)
				{
					selected.clear();
					selected.insert(item);
				}
			}
			ensureRowVisible(row);
			update();
		}
	}
}

void FilterTable::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton)
	{
		if ((event->pos() - dragStartPos).manhattanLength() >= QApplication::startDragDistance())
		{
			QString text;
			QList<QVariantMap> prefsList;
			bool first = true;
			int i = 0;
			bool dragPosInside = false;
			for (Item* item : items)
			{
				if (selected.contains(item))
				{
					if (first)
						first = false;
					else
						text += "\n";
					text += item->text;
					if (item->gui != NULL)
						item->gui->storePreferences(item->prefs);
					prefsList.append(item->prefs);

					if (!dragPosInside)
					{
						FilterTableRow* tableRow = qobject_cast<FilterTableRow*>(gridLayout->itemAtPosition(i, 0)->widget());
						if (tableRow != NULL)
						{
							QRect rect = tableRow->getHeaderRect().translated(tableRow->pos());
							if (rect.contains(dragStartPos))
								dragPosInside = true;
						}
					}
				}
				i++;
			}

			if (selected.size() > 0 && dragPosInside)
			{
				FilterTableMimeData* mimeData = new FilterTableMimeData;
				mimeData->setText(text);
				mimeData->setPrefsList(prefsList);

				QDrag* drag = new QDrag(this);
				drag->setMimeData(mimeData);
				QSet<Item*> selectedBefore = selected;
				internalDrag = true;
				Qt::DropAction action = drag->exec(Qt::MoveAction | Qt::CopyAction);
				internalDrag = false;
				if (action == Qt::MoveAction)
				{
					for (Item* item : selectedBefore)
					{
						items.removeOne(item);
						if (focused == item)
							focused = NULL;
						if (selectionStart == item)
							selectionStart = NULL;
						selected.remove(item);
						delete item;
					}
					normalizeDeviceGroupLines();
				}

				if (action != Qt::IgnoreAction)
				{
					emit linesChanged();
					updateGuis();
				}
			}
		}
	}

	QWidget::mouseMoveEvent(event);
}

void FilterTable::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasText())
	{
		if (event->keyboardModifiers() & Qt::ControlModifier)
			event->setDropAction(Qt::CopyAction);
		else
			event->setDropAction(Qt::MoveAction);
		event->accept();
	}

	QWidget::dragEnterEvent(event);
}

void FilterTable::dragMoveEvent(QDragMoveEvent* event)
{
	if (event->mimeData()->hasText())
	{
		if (event->keyboardModifiers() & Qt::ControlModifier)
			event->setDropAction(Qt::CopyAction);
		else
			event->setDropAction(Qt::MoveAction);

		int dropRow = rowForPos(event->pos(), true);

		QRect rect = gridLayout->itemAtPosition(dropRow == -1 ? gridLayout->rowCount() - 2 : dropRow, 0)->geometry();
		insertArrow->move(0, rect.top() - insertArrow->height() / 2 - gridLayout->verticalSpacing() / 2);

		insertArrow->raise();
		insertArrow->show();
	}

	QWidget::dragMoveEvent(event);
}

void FilterTable::dragLeaveEvent(QDragLeaveEvent* event)
{
	insertArrow->hide();
}

void FilterTable::dropEvent(QDropEvent* event)
{
	const QMimeData* mimeData = event->mimeData();
	if (mimeData->hasText())
	{
		if (event->keyboardModifiers() & Qt::ControlModifier)
			event->setDropAction(Qt::CopyAction);
		else
			event->setDropAction(Qt::MoveAction);

		QString text = mimeData->text();
		QStringList textLines = text.split("\n");
		QList<QVariantMap> prefsList;
		const FilterTableMimeData* filterTableMimeData = qobject_cast<const FilterTableMimeData*>(mimeData);
		if (filterTableMimeData != NULL)
			prefsList = filterTableMimeData->getPrefsList();

		int dropRow = rowForPos(event->pos(), true);
		if (dropRow == -1)
			dropRow = items.size();

		selected.clear();
		focused = NULL;
		selectionStart = NULL;
		for (int i = 0; i < textLines.size(); i++)
		{
			QString line = textLines[i];
			Item* item = new Item(line);
			if (!prefsList.isEmpty())
				item->prefs = prefsList[i];
			selected.insert(item);
			items.insert(dropRow++, item);
			if (focused == NULL)
			{
				focused = item;
				selectionStart = item;
			}
		}
		event->accept();
		normalizeDeviceGroupLines();

		if (!internalDrag)
		{
			emit linesChanged();
			updateGuis();
		}
	}

	insertArrow->hide();

	QWidget::dropEvent(event);
}

void FilterTable::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up)
	{
		if (focused != NULL)
		{
			int row = items.indexOf(focused);
			if (row != -1)
			{
				int newRow = row;
				if (event->key() == Qt::Key_Down && row + 1 < items.size())
					newRow = row + 1;
				else if (event->key() == Qt::Key_Up && row - 1 >= 0)
					newRow = row - 1;

				if (newRow != row)
				{
					focused = items[newRow];
					if (event->modifiers() & Qt::ControlModifier)
					{
					}
					else if (event->modifiers() & Qt::ShiftModifier)
					{
						int startRow = items.indexOf(selectionStart);
						if (startRow != -1)
						{
							selected.clear();
							for (int i = min(startRow, newRow); i <= max(startRow, newRow); i++)
							{
								selected.insert(items[i]);
							}
						}
					}
					else
					{
						selected.clear();
						selected.insert(focused);
						selectionStart = focused;
					}

					ensureRowVisible(newRow);
					update();
				}
			}
		}
	}

	if (event->key() == Qt::Key_Space)
	{
		if (!(event->modifiers() & Qt::ControlModifier) || !selected.remove(focused))
			selected.insert(focused);
		update();
	}

	if (event->key() == Qt::Key_F2)
	{
		if (focused != NULL)
		{
			int rowIndex = items.indexOf(focused);
			if (rowIndex != -1)
			{
				QLayoutItem* layoutItem = gridLayout->itemAtPosition(rowIndex, 0);
				FilterTableRow* tableRow = qobject_cast<FilterTableRow*>(layoutItem->widget());
				if (tableRow != NULL)
					tableRow->editText();
			}
		}
	}

	if (event->key() == Qt::Key_Delete)
	{
		deleteSelectedLines();
	}
}

void FilterTable::wheelEvent(QWheelEvent* event)
{
	scrollingNow = true;
	scrollStartPoint = event->globalPosition();

	QWidget::wheelEvent(event);
}

bool FilterTable::eventFilter(QObject* obj, QEvent* event)
{
	QEvent::Type type = event->type();
	if (scrollingNow)
	{
		if (type == QEvent::Wheel)
		{
			QWheelEvent* wheelEvent = (QWheelEvent*)event;
			scrollStartPoint = wheelEvent->globalPosition();

			QWidget* widget = qobject_cast<QWidget*>(obj);
			if (widget != NULL)
			{
				if (isAncestorOf(widget))
				{
					QApplication::sendEvent(parent(), event);
					return true;
				}
			}
		}
		else if (type == QEvent::MouseMove)
		{
			QMouseEvent* mouseEvent = (QMouseEvent*)event;

			if ((mouseEvent->globalPos() - scrollStartPoint).manhattanLength() > GUIHelper::scale(30))
				scrollingNow = false;
		}
	}

	if (obj == scrollArea && type == QEvent::Resize)
	{
		updateSizeHints();
	}

	return false;
}

void FilterTable::showEvent(QShowEvent*)
{
	if (presetScrollX != -1)
	{
		scrollArea->horizontalScrollBar()->setValue(presetScrollX);
		presetScrollX = -1;
	}

	if (presetScrollY != -1)
	{
		scrollArea->verticalScrollBar()->setValue(presetScrollY);
		presetScrollY = -1;
	}
}

void FilterTable::ensureRowVisible(int row)
{
	QScrollBar* vScrollBar = scrollArea->verticalScrollBar();
	if (vScrollBar != NULL)
	{
		QRect rect = rowRect(row).toAlignedRect();
		if (rect.top() < vScrollBar->value())
			vScrollBar->setValue(max(0, rect.top()));
		else if (rect.bottom() + 1 > vScrollBar->value() + scrollArea->viewport()->height())
			vScrollBar->setValue(min(vScrollBar->maximum(), rect.bottom() + 1 - scrollArea->viewport()->height()));
	}
}

int FilterTable::rowForPos(QPoint pos, bool insert)
{
	int row = -1;
	for (int i = 0; i < gridLayout->rowCount() - 2; i++)
	{
		QRect rect = gridLayout->itemAtPosition(i, 0)->geometry();
		int y;
		if (insert)
			y = rect.center().y();
		else
			y = rect.bottom();

		if (pos.y() <= y)
		{
			row = i;
			break;
		}
	}

	return row;
}

QRectF FilterTable::rowRect(int row)
{
	QRectF rect = gridLayout->itemAtPosition(row, 0)->geometry();
	rect = rect.marginsAdded(QMarginsF(-1.5, -1.5, -1.5, -0.5));
	return rect;
}

void FilterTable::disableWheelForWidgets()
{
	QList<QWidget*> widgets = findChildren<QWidget*>();
	for (QWidget* widget : widgets)
	{
		if (qobject_cast<QComboBox*>(widget) || qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QDial*>(widget))
		{
			widget->installEventFilter(new DisableWheelFilter(this, widget));
			if (widget->focusPolicy() == Qt::WheelFocus)
				widget->setFocusPolicy(Qt::StrongFocus);
		}
	}
}

bool FilterTable::parseDeviceGroupLine(const QString& line, QString* pattern, bool* enabled) const
{
	QString candidate = line;
	bool isEnabled = true;
	if (candidate.startsWith(deviceGroupDisabledPrefix))
	{
		candidate = candidate.mid(deviceGroupDisabledPrefix.size());
		isEnabled = false;
	}

	int separator = candidate.indexOf(':');
	if (separator == -1 || candidate.left(separator).trimmed() != QStringLiteral("Device"))
		return false;

	if (pattern != NULL)
		*pattern = candidate.mid(separator + 1).trimmed();
	if (enabled != NULL)
		*enabled = isEnabled;
	return true;
}

QString FilterTable::stripDeviceGroupDisabledPrefix(const QString& line) const
{
	if (line.startsWith(deviceGroupDisabledPrefix))
		return line.mid(deviceGroupDisabledPrefix.size());
	return line;
}

int FilterTable::deviceGroupEnd(int start) const
{
	for (int i = start + 1; i < items.size(); i++)
	{
		if (parseDeviceGroupLine(items[i]->text))
			return i;
	}
	return items.size();
}

void FilterTable::normalizeDeviceGroupLines()
{
	for (Item* item : items)
	{
		QString line = stripDeviceGroupDisabledPrefix(item->text).trimmed();
		if (line.isEmpty() || line.startsWith('#'))
			continue;
		if (!parseDeviceGroupLine(item->text))
			items.prepend(new Item(QStringLiteral("Device: all")));
		break;
	}

	bool insideGroup = false;
	bool groupEnabled = true;
	for (Item* item : items)
	{
		bool selectorEnabled;
		if (parseDeviceGroupLine(item->text, NULL, &selectorEnabled))
		{
			insideGroup = true;
			groupEnabled = selectorEnabled;
			continue;
		}

		if (!insideGroup)
			continue;

		if (groupEnabled)
			item->text = stripDeviceGroupDisabledPrefix(item->text);
		else if (!item->text.trimmed().isEmpty()
			&& !item->text.trimmed().startsWith('#')
			&& !item->text.startsWith(deviceGroupDisabledPrefix))
			item->text.prepend(deviceGroupDisabledPrefix);
	}
}

QString FilterTable::deviceGroupSummary(const QString& pattern, DeviceResolutionStatus* status) const
{
	if (status != NULL)
		*status = DeviceResolutionStatus::ExactEndpoint;
	if (pattern.trimmed().compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0)
		return tr("All devices");

	QStringList matches;
	vector<EndpointIdentity> catalog = deviceFilterFactory->getEndpointCatalog();
	vector<DeviceResolution> resolutions;
	for (const shared_ptr<AbstractAPOInfo>& apoInfo : deviceFilterFactory->getDevices())
	{
		if (DeviceFilterFactory::matchDevice(apoInfo->getEndpointIdentity(), catalog, pattern.toStdWString(), &resolutions))
		{
			QString connection = QString::fromStdWString(apoInfo->getConnectionName());
			QString device = QString::fromStdWString(apoInfo->getDeviceName());
			matches.append(connection.isEmpty() ? device : connection + QStringLiteral(": ") + device);
		}
		resolutions.clear();
	}

	bool recovered = false;
	bool ambiguous = false;
	bool invalid = false;
	bool missing = false;
	DeviceFilterFactory::matchDevice(EndpointIdentity(), catalog, pattern.toStdWString(), &resolutions);
	for (const DeviceResolution& resolution : resolutions)
	{
		recovered |= resolution.status == DeviceResolutionStatus::RecoveredExactName
			|| resolution.status == DeviceResolutionStatus::RecoveredModelTokens;
		ambiguous |= resolution.status == DeviceResolutionStatus::Ambiguous;
		invalid |= resolution.status == DeviceResolutionStatus::InvalidSelector;
		missing |= resolution.status == DeviceResolutionStatus::Missing;
	}

	if (matches.isEmpty())
	{
		if (status != NULL)
			*status = ambiguous ? DeviceResolutionStatus::Ambiguous
				: invalid ? DeviceResolutionStatus::InvalidSelector : DeviceResolutionStatus::Missing;
		if (ambiguous)
			return tr("Device binding ambiguous");
		if (missing || invalid)
			return tr("Device missing");
		return pattern.trimmed().isEmpty() ? tr("No devices selected") : tr("No matching devices");
	}
	if (recovered)
	{
		if (status != NULL)
			*status = DeviceResolutionStatus::RecoveredExactName;
		matches[0] += QStringLiteral(" (") + tr("recovered") + QStringLiteral(")");
	}
	if (matches.size() == 1)
		return matches[0];
	return tr("%1 and %2 more").arg(matches[0]).arg(matches.size() - 1);
}

QWidget* FilterTable::createDeviceGroupHeader(Item* item, int groupNumber, const QString& pattern, bool enabled, int effectCount)
{
	QFrame* frame = new QFrame(this);
	frame->setObjectName(QStringLiteral("deviceGroupHeader"));
	frame->setMinimumHeight(GUIHelper::scale(66));
	QString background = GUIHelper::isDarkMode() ? QStringLiteral("#292929") : QStringLiteral("#f3f3f3");
	QString border = GUIHelper::isDarkMode() ? QStringLiteral("#5b5b5b") : QStringLiteral("#b8b8b8");
	QString summary = deviceGroupSummary(pattern);
	frame->setStyleSheet(QStringLiteral(
		"QFrame#deviceGroupHeader { background: %1; border: 1px solid %2; border-radius: 6px; margin-top: 6px; }"
		"QFrame#deviceGroupHeader QLabel { border: none; background: transparent; }"
		"QFrame#deviceGroupHeader QToolBar { border: none; background: transparent; spacing: 2px; }")
		.arg(background, border));

	QHBoxLayout* layout = new QHBoxLayout(frame);
	layout->setContentsMargins(GUIHelper::scale(8), GUIHelper::scale(10), GUIHelper::scale(10), GUIHelper::scale(7));
	layout->setSpacing(GUIHelper::scale(10));

	bool collapsed = item->prefs.value(deviceGroupCollapsedPreference, false).toBool();
	QToolBar* stateToolBar = new QToolBar(frame);
	stateToolBar->setOrientation(Qt::Horizontal);
	stateToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	stateToolBar->setIconSize(GUIHelper::scale(QSize(24, 24)));

	QAction* collapseAction = stateToolBar->addAction(
		frame->style()->standardIcon(collapsed ? QStyle::SP_ArrowRight : QStyle::SP_ArrowDown),
		collapsed ? tr("Expand group") : tr("Collapse group"));
	collapseAction->setEnabled(effectCount > 0);

	QIcon powerIcon;
	powerIcon.addFile(QStringLiteral(":/icons/power_off.svg"), QSize(), QIcon::Normal, QIcon::Off);
	powerIcon.addFile(QStringLiteral(":/icons/power_on.svg"), QSize(), QIcon::Normal, QIcon::On);
	QAction* powerAction = stateToolBar->addAction(powerIcon, tr("Power on"));
	powerAction->setCheckable(true);
	powerAction->setChecked(enabled);
	powerAction->setToolTip(tr("Enable or disable this device and every effect in its group"));
	layout->addWidget(stateToolBar);

	QVBoxLayout* labelLayout = new QVBoxLayout;
	labelLayout->setContentsMargins(0, 0, 0, 0);
	labelLayout->setSpacing(GUIHelper::scale(1));
	QLabel* deviceLabel = new QLabel(summary, frame);
	QFont titleFont = deviceLabel->font();
	titleFont.setBold(true);
	deviceLabel->setFont(titleFont);
	deviceLabel->setToolTip(pattern);
	deviceLabel->setEnabled(enabled);
	QLabel* detailsLabel = new QLabel(
		effectCount == 1
			? tr("Device group %1, 1 effect").arg(groupNumber)
			: tr("Device group %1, %2 effects").arg(groupNumber).arg(effectCount),
		frame);
	QPalette detailsPalette = detailsLabel->palette();
	detailsPalette.setColor(QPalette::Active, QPalette::WindowText, GUIHelper::isDarkMode() ? QColor(180, 180, 180) : QColor(90, 90, 90));
	detailsPalette.setColor(QPalette::Inactive, QPalette::WindowText, GUIHelper::isDarkMode() ? QColor(180, 180, 180) : QColor(90, 90, 90));
	detailsPalette.setColor(QPalette::Disabled, QPalette::WindowText, GUIHelper::isDarkMode() ? QColor(115, 115, 115) : QColor(145, 145, 145));
	detailsLabel->setPalette(detailsPalette);
	detailsLabel->setEnabled(enabled);
	labelLayout->addWidget(deviceLabel);
	labelLayout->addWidget(detailsLabel);
	layout->addLayout(labelLayout);

	QPushButton* addEffectButton = new QPushButton(tr("Add effect"), frame);
	QPushButton* selectButton = new QPushButton(tr("Select devices"), frame);
	QPushButton* removeButton = new QPushButton(tr("Remove group"), frame);
	layout->addWidget(addEffectButton);
	layout->addWidget(selectButton);
	layout->addWidget(removeButton);
	layout->addStretch(1);

	connect(collapseAction, &QAction::triggered, this, [this, item, collapsed]() {
		item->prefs.insert(deviceGroupCollapsedPreference, !collapsed);
		updateGuis();
	});
	connect(powerAction, &QAction::toggled, this, [this, item](bool checked) {
		setDeviceGroupEnabled(item, checked);
	});
	connect(addEffectButton, &QPushButton::clicked, this, [this, item]() {
		addFilterToDeviceGroup(item);
	});
	connect(selectButton, &QPushButton::clicked, this, [this, item]() {
		selectDeviceGroupDevices(item);
	});
	connect(removeButton, &QPushButton::clicked, this, [this, item]() {
		removeDeviceGroup(item);
	});

	return frame;
}

QString FilterTable::getConfigPath() const
{
	return configPath;
}

void FilterTable::setConfigPath(const QString& value)
{
	configPath = value;
}

FilterTable::Item* FilterTable::getFocusedItem() const
{
	return focused;
}

const QSet<FilterTable::Item*>& FilterTable::getSelectedItems() const
{
	return selected;
}

const QList<shared_ptr<AbstractAPOInfo>>& FilterTable::getOutputDevices() const
{
	return outputDevices;
}

const QList<shared_ptr<AbstractAPOInfo>>& FilterTable::getInputDevices() const
{
	return inputDevices;
}

shared_ptr<AbstractAPOInfo> FilterTable::getSelectedDevice() const
{
	return selectedDevice;
}

int FilterTable::getSelectedChannelMask() const
{
	return selectedChannelMask;
}
