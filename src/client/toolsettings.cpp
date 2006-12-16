/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006 Calle Laakkonen

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software Foundation,
	Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QSettings>
#include "toolsettings.h"
#include "brushpreview.h"
using widgets::BrushPreview; // qt designer doesn't know about namespaces
#include "ui_brushsettings.h"

namespace tools {

BrushSettings::BrushSettings(QString name, QString title, bool swapcolors)
	: ToolSettings(name,title), swapcolors_(swapcolors)
{
	ui_ = new Ui_BrushSettings();
}

BrushSettings::~BrushSettings()
{
	if(ui_) {
		// Remember settings
		QSettings cfg;
		cfg.beginGroup("tools");
		cfg.beginGroup(getName());
		cfg.setValue("size", ui_->brushsize->value());
		cfg.setValue("opacity", ui_->brushopacity->value());
		cfg.setValue("hardness", ui_->brushhardness->value());
		cfg.setValue("pressuresize", ui_->pressuresize->isChecked());
		cfg.setValue("pressureopacity", ui_->pressureopacity->isChecked());
		cfg.setValue("pressurehardness", ui_->pressurehardness->isChecked());
		cfg.setValue("pressurecolor", ui_->pressurecolor->isChecked());
		delete ui_;
	}
}

QWidget *BrushSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);

	// Load previous settings
	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.beginGroup(getName());
	ui_->brushsize->setValue(cfg.value("size", 0).toInt());
	ui_->brushsizebox->setValue(ui_->brushsize->value());
	ui_->preview->setSize(ui_->brushsize->value());

	ui_->brushopacity->setValue(cfg.value("opacity", 100).toInt());
	ui_->brushopacitybox->setValue(ui_->brushopacity->value());
	ui_->preview->setOpacity(ui_->brushopacity->value());

	ui_->brushhardness->setValue(cfg.value("hardness", 50).toInt());
	ui_->brushhardnessbox->setValue(ui_->brushhardness->value());
	ui_->preview->setHardness(ui_->brushhardness->value());

	ui_->pressuresize->setChecked(cfg.value("pressuresize",false).toBool());
	ui_->preview->setSizePressure(ui_->pressuresize->isChecked());

	ui_->pressureopacity->setChecked(cfg.value("pressureopacity",false).toBool());
	ui_->preview->setOpacityPressure(ui_->pressureopacity->isChecked());

	ui_->pressurehardness->setChecked(cfg.value("pressurehardness",false).toBool());
	ui_->preview->setHardnessPressure(ui_->pressurehardness->isChecked());

	ui_->pressurecolor->setChecked(cfg.value("pressurecolor",false).toBool());
	ui_->preview->setColorPressure(ui_->pressurecolor->isChecked());

	// Connect size change signal
	parent->connect(ui_->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	return widget;
}

void BrushSettings::setForeground(const QColor& color)
{
	fg_ = color;
	ui_->preview->setColor1(color);
}

void BrushSettings::setBackground(const QColor& color)
{
	bg_ = color;
	ui_->preview->setColor2(color);
}

drawingboard::Brush BrushSettings::getBrush() const
{
	int radius = ui_->brushsize->value();
	qreal hardness = ui_->brushhardness->value()/qreal(ui_->brushhardness->maximum());
	qreal opacity = ui_->brushopacity->value()/qreal(ui_->brushopacity->maximum());
	int radius2 = radius;
	qreal hardness2 = hardness,opacity2 = opacity;
	QColor color2 = swapcolors_?bg_:fg_;
	if(ui_->pressuresize->isChecked())
		radius2 = 0;
	if(ui_->pressurehardness->isChecked())
		hardness2 = 0;
	if(ui_->pressureopacity->isChecked())
		opacity2 = 0;
	if(ui_->pressurecolor->isChecked())
		color2 = swapcolors_?fg_:bg_;

	drawingboard::Brush brush(radius,hardness,opacity, swapcolors_?bg_:fg_);

	brush.setRadius2(radius2);
	brush.setHardness2(hardness2);
	brush.setOpacity2(opacity2);
	brush.setColor2(color2);
	return brush;
}

int BrushSettings::getSize() const
{
	return ui_->brushsize->value();
}

NoSettings::NoSettings(const QString& name, const QString& title)
	: ToolSettings(name, title)
{
}

QWidget *NoSettings::createUi(QWidget *parent)
{
	QLabel *ui = new QLabel(QApplication::tr("This tool has no settings"),
			parent);
	setUiWidget(ui);
	return ui;
}

void NoSettings::setForeground(const QColor&)
{
}

void NoSettings::setBackground(const QColor&)
{
}

drawingboard::Brush NoSettings::getBrush() const
{
	// return a default brush
	return drawingboard::Brush(1,1,1,Qt::black);
}

}

