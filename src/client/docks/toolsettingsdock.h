/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef TOOLSETTINGSDOCK_H
#define TOOLSETTINGSDOCK_H

#include "tools/tool.h"
#include "tools/toolproperties.h"

#include <QDockWidget>

class QStackedWidget;

namespace color_widgets {
	class ColorDialog;
}

namespace tools {
	class ToolSettings;
	class AnnotationSettings;
	class ColorPickerSettings;
	class LaserPointerSettings;
	class FillSettings;
	class SelectionSettings;
}

namespace paintcore {
	class Brush;
}

namespace widgets {
	class ToolSlotButton;
}

namespace docks {

/**
 * @brief Tool settings window
 * A dock widget that displays settings for the currently selected tool.
 */
class ToolSettings : public QDockWidget
{
Q_OBJECT
public:
	//! Number of quick tool change slots
	static const int QUICK_SLOTS = 5;

	ToolSettings(QWidget *parent=0);
	ToolSettings(const ToolSettings& ts) = delete;
	ToolSettings& operator=(const ToolSettings& ts) = delete;

	~ToolSettings();

	//! Get a brush with the current settings
	paintcore::Brush getBrush() const;

	//! Get the annotation settings page
	tools::AnnotationSettings *getAnnotationSettings() { return _textsettings; }

	//! Get the color picker page
	tools::ColorPickerSettings *getColorPickerSettings() { return _pickersettings; }

	//! Get the laser pointer settings page
	tools::LaserPointerSettings  *getLaserPointerSettings() { return _lasersettings; }

	tools::SelectionSettings *getRectSelectionSettings() { return _selectionsettings; }
	tools::SelectionSettings *getPolySelectionSettings() { return _polyselectionsettings; }

	//! Get flood fill settings page
	tools::FillSettings *getFillSettings() { return _fillsettings; }

	//! Get the current foreground color
	QColor foregroundColor() const;

	//! Get the currently selected tool slot
	int currentToolSlot() const;

	//! Get the currently selected tool
	tools::Tool::Type currentTool() const;

	//! Load tool related settings
	void readSettings();

	//! Save tool related settings
	void saveSettings();

public slots:
	//! Set the tool for which settings are shown
	void setTool(tools::Tool::Type tool);

	//! Quick adjust current tool
	void quickAdjustCurrent1(qreal adjustment);

	//! Select the tool previosly set with setTool
	void setPreviousTool();

	//! Set the currently active quick tool slot
	void setToolSlot(int i);

	//! Select the tool slot previously set with setToolSlot
	void setPreviousToolSlot();

	//! Set foreground color
	void setForegroundColor(const QColor& color);

	//! Pop up a dialog for changing the foreground color
	void changeForegroundColor();

	//! Switch tool when eraser is brought near the tablet
	void eraserNear(bool near);

	/**
	 * @brief Change the eraser override tool if it matches the given
	 *
	 * This is used to prevent a disabled tool from being reselected
	 * after the tablet eraser is lifted.
	 *
	 * The chain of events is:
	 * 1. User selects a tool that can be disabled (annotation or laser pointer)
	 * 2. User places the tablet eraser near the tablet surface, triggering eraser override tool
	 * 3. User disables annotations/lasers
	 * 4. User lifts the eraser, causing the previous tool (which is now disabled) to be reselected.
	 *
	 * @param tool the tool to disable
	 */
	void disableEraserOverride(tools::Tool::Type tool);

	//! Query current tool's subpixel mode and emit subpixelModeChanged
	void updateSubpixelMode();

signals:
	//! This signal is emitted when the current tool changes its size
	void sizeChanged(int size);

	//! This signal is emitted when tool subpixel drawing mode is changed
	void subpixelModeChanged(bool subpixel);

	//! Current foreground color selection changed
	void foregroundColorChanged(const QColor &color);

	//! Currently active tool was changed
	void toolChanged(tools::Tool::Type tool);

public:
	tools::ToolSettings *getToolSettingsPage(tools::Tool::Type tool);

private:
	void updateToolSlot(int i, bool typeChanged);

public:
	void selectTool(tools::Tool::Type tool);

private:
	void selectToolSlot(int i);
	void saveCurrentTool();

	tools::ToolSettings *_pensettings;
	tools::ToolSettings *_brushsettings;
	tools::ToolSettings *_smudgesettings;
	tools::ToolSettings *_erasersettings;
	tools::ColorPickerSettings *_pickersettings;
	tools::ToolSettings *_linesettings;
	tools::ToolSettings *_rectsettings;
	tools::ToolSettings *_ellipsesettings;
	tools::FillSettings *_fillsettings;
	tools::AnnotationSettings *_textsettings;
	tools::SelectionSettings *_selectionsettings;
	tools::SelectionSettings *_polyselectionsettings;
	tools::LaserPointerSettings  *_lasersettings;

	tools::ToolSettings *_currenttool;
	QStackedWidget *_widgets;

	widgets::ToolSlotButton *_quickslot[QUICK_SLOTS];
	int _currentQuickslot;
	int _eraserOverride;
	bool _eraserActive;

	QList<tools::ToolsetProperties> _toolprops;

	tools::Tool::Type _previousTool;
	int _previousToolSlot;

	QColor _foreground;
	color_widgets::ColorDialog *_fgdialog;
};

}

#endif

