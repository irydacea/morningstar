//
// codename Morning Star
//
// Copyright (C) 2011 - 2012 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#include "custompalettes.hpp"
#include "ui_custompalettes.h"
#include "util.hpp"

#include "codesnippetdialog.hpp"
#include "paletteitem.hpp"
#include "wesnothrc.hpp"

#include <QColorDialog>
#include <QMessageBox>

// BIG TODO: signal handling in this class is a massive mess!
// I really need to refactor this and regroup widgets according to
// their task so we don't need to have such inconvenient private
// wrappers as the setTaskThingsEnabled() family.

CustomPalettes::CustomPalettes(const QMap< QString, QList<QRgb> >& initialPalettes, QWidget *parent) :
    QDialog(parent),
	ui(new Ui::CustomPalettes),
	palettes_(initialPalettes)
{
    ui->setupUi(this);
	ui->listColors->setItemDelegate(new PaletteItemDelegate(ui->listPals));
	updatePaletteUI();
}

CustomPalettes::~CustomPalettes()
{
    delete ui;
}

void CustomPalettes::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void CustomPalettes::updatePaletteUI()
{
	{
		// Make sure to not emit signals while setting up rows.
		const ObjectLock lock(ui->listPals);

		ui->listPals->clear();

		for(QMap< QString, QList<QRgb> >::const_iterator i = palettes_.constBegin();
			i != palettes_.constEnd(); ++i) {
			addPaletteListEntry(i.key());
		}
	}

	if(palettes_.empty()) {
		setPaletteViewEnabled(false);
	} else {
		// Notify the palette view widget.
		ui->listPals->setCurrentRow(0);
	}
}

void CustomPalettes::addPaletteListEntry(const QString& name)
{
	QListWidgetItem* lwi = new QListWidgetItem(name, ui->listPals);
	lwi->setData(Qt::UserRole, name);
	lwi->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

	const QList<QRgb> palette = palettes_.value(name);

	if(!palette.empty()) {
		lwi->setIcon(createColorIcon(palette.front()));
	}
}

void CustomPalettes::removePaletteListEntry(const QString &name)
{
	QList<QListWidgetItem*> const items = ui->listPals->findItems(name, Qt::MatchFixedString);
	Q_ASSERT(items.size() <= 1);

	foreach(QListWidgetItem* w, items) {
		delete ui->listPals->takeItem(ui->listPals->row(w));
	}
}

void CustomPalettes::on_listPals_currentRowChanged(int currentRow)
{
	QListWidgetItem* const itemw = ui->listPals->item(currentRow);

	Q_ASSERT(itemw);
	if(!itemw)
		return;

	const QString& name = itemw->text();
	QMap< QString, QList<QRgb> >::iterator pal_it = palettes_.find(name);

	if(pal_it == palettes_.end()) {
		QMessageBox::critical(this, tr("Wesnoth RCX"),
			tr("The palette \"%1\" does not exist.").arg(name));
		removePaletteListEntry(name);
		return;
	}

	populatePaletteView(pal_it.value());
}

void CustomPalettes::populatePaletteView(const QList<QRgb> &pal)
{
	QListWidget* const listw = ui->listColors;

	Q_ASSERT(listw);
	if(!listw)
		return;

	{
		// Make sure to not emit signals while setting up rows.
		const ObjectLock lock(listw);

		listw->clear();

		foreach(QRgb rgb, pal) {
			QListWidgetItem* itemw = new QListWidgetItem("", listw);
			itemw->setData(Qt::UserRole, rgb);
			itemw->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
		}
	}

	// The hex editor box needs to be notified now.
	listw->setCurrentRow(0);
}

void CustomPalettes::setColorEditControlsEnabled(bool enabled)
{
	const bool haveColors = ui->listColors->count() != 0;

	ui->cmdDelCol->setEnabled(enabled && haveColors);
	ui->tbEditColor->setEnabled(enabled && haveColors);
	ui->leColor->setEnabled(enabled && haveColors);
}

void CustomPalettes::setPaletteEditControlsEnabled(bool enabled)
{
	ui->listColors->setEnabled(enabled);
	ui->cmdAddCol->setEnabled(enabled);
	ui->cmdWml->setEnabled(enabled);

	setColorEditControlsEnabled(enabled);
}

void CustomPalettes::setPaletteViewEnabled(bool enabled)
{
	ui->cmdDelPal->setEnabled(enabled);
	ui->cmdRenPal->setEnabled(enabled);

	setPaletteEditControlsEnabled(enabled);
}

void CustomPalettes::clearPaletteView()
{
	ui->listColors->clear();
	ui->leColor->clear();
}

QList<QRgb>& CustomPalettes::getCurrentPalette()
{
	QListWidgetItem const* palItem = ui->listPals->currentItem();
	Q_ASSERT(palItem);

	if(!palItem)
		return palettes_[""];

	const QString& palId = palItem->data(Qt::UserRole).toString();
	// Create a palette if necessary.
	return palettes_[palId];
}

void CustomPalettes::updatePaletteIcon()
{
	QListWidgetItem* const palw = ui->listPals->currentItem();
	Q_ASSERT(palw);

	const QList<QRgb>& palette = palettes_.value(palw->data(Qt::UserRole).toString());

	if(palette.empty())
		return;

	palw->setIcon(createColorIcon(palette.front()));
}

QString CustomPalettes::generateNewPaletteName() const
{
	QString name;
	int i = 0;

	do {
		name = tr("New Palette #%1").arg(++i);
	} while(palettes_.find(name) != palettes_.end());

	return name;
}

void CustomPalettes::on_listColors_currentRowChanged(int currentRow)
{
	QLineEdit* const textw = ui->leColor;
	const QColor currentColor =
		ui->listColors->item(currentRow)->data(Qt::UserRole).toInt();

	textw->setText(currentColor.name());
}

void CustomPalettes::on_listColors_itemChanged(QListWidgetItem *item)
{
	QLineEdit* const textw = ui->leColor;
	const QColor currentColor =
		item->data(Qt::UserRole).toInt();

	textw->setText(currentColor.name());

	// Update palette definition.

	QListWidget* const listw = ui->listColors;

	QList<QRgb>& pal = getCurrentPalette();
	const int index = listw->currentRow();
	Q_ASSERT(index < pal.size());
	pal[index] = currentColor.rgb();

	// If this is the first row we might as well update
	// the palette icon.

	if(listw->currentRow() == 0) {
		updatePaletteIcon();
	}
}

void CustomPalettes::on_cmdRenPal_clicked()
{
	QListWidget* const listw = ui->listPals;
	// An edit slot should take care of updating the
	// palette definition afterwards.
	listw->editItem(listw->currentItem());
}

void CustomPalettes::on_listPals_itemChanged(QListWidgetItem *item)
{
	Q_ASSERT(item);

	const QString& newName = item->text();
	const QString& oldName = item->data(Qt::UserRole).toString();

	if(newName == oldName)
		return;

	QMap<QString, QList<QRgb> >::iterator
		oldIt = palettes_.find(oldName),
		newIt = palettes_.find(newName);
	Q_ASSERT(oldIt != palettes_.end());
	Q_ASSERT(oldIt != newIt);

	if(oldIt == palettes_.end())
		return;

	if(newIt != palettes_.end()) {
		if(!JobUi::prompt(this, tr("The palette '%1' already exists. Do you wish to overwrite it?").arg(newName))) {
			item->setText(oldName);
			return;
		}

		for(int i = 0; i < ui->listPals->count(); ++i) {
			QListWidgetItem* const ii = ui->listPals->item(i);
			if(ii->data(Qt::UserRole).toString() == newName) {
				delete ui->listPals->takeItem(i);
				break;
			}
		}
	} else {
		newIt = palettes_.insert(newName, QList<QRgb>());
	}

	newIt.value().swap(oldIt.value());
	palettes_.erase(oldIt);

	item->setData(Qt::UserRole, newName);
}

void CustomPalettes::on_tbEditColor_clicked()
{
	QListWidget* const listw = ui->listColors;
	QListWidgetItem* const lwi = listw->currentItem();

	if(!lwi)
		return;

	QColor color = lwi->data(Qt::UserRole).toInt();
	color = QColorDialog::getColor(color, this);

	if(!color.isValid())
		return;

	const QRgb rgb = color.rgb();
	lwi->setData(Qt::UserRole, rgb);
}

void CustomPalettes::on_cmdAddCol_clicked()
{
	QListWidget* const listw = ui->listColors;

	// Make sure to actually add the item only later, once
	// it is safe to notify other widgets!

	QListWidgetItem* const itemw = new QListWidgetItem("");

	// We might already have a first row with the first
	// color of the palette. If we don't we use pure black.

	QListWidgetItem* const first = listw->item(0);

	const QRgb rgb = first ? first->data(Qt::UserRole).toInt() : qRgb(0,0,0);

	itemw->setData(Qt::UserRole, rgb);
	itemw->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

	// Update palette definition.

	QList<QRgb>& pal = getCurrentPalette();
	pal.push_back(rgb);

	// Notify widgets.

	listw->addItem(itemw);
	listw->setCurrentItem(itemw);

	setColorEditControlsEnabled(true);

	// If this is the first row we might as well update
	// the palette icon.

	if(first == NULL) {
		updatePaletteIcon();
	}
}

void CustomPalettes::on_cmdDelCol_clicked()
{
	QListWidget* const listw = ui->listColors;

	ObjectLock lock(listw);

	const int remaining = listw->count();

	if(remaining == 0)
		return;

	const int index = listw->currentRow();

	delete listw->takeItem(listw->currentRow());

	if(remaining == 1) {
		// No more colors!
		setColorEditControlsEnabled(false);
	}

	// Update palette definition.

	QList<QRgb>& pal = getCurrentPalette();

	Q_ASSERT(index < pal.count());
	pal.removeAt(index);

	// If this was the first row and we still have
	// more colors, update the palette's color icon.
	if(index == 0 && remaining > 1) {
		updatePaletteIcon();
	}
}

void CustomPalettes::on_cmdAddPal_clicked()
{
	QListWidget* const listw = ui->listPals;

	{
		ObjectLock lockPals(listw);

		const QString& palName = generateNewPaletteName();
		palettes_[palName].push_back(qRgb(0,0,0));
		addPaletteListEntry(palName);
	}

	listw->setCurrentRow(listw->count() - 1);
	setPaletteViewEnabled(true);
	listw->editItem(listw->currentItem());
}

void CustomPalettes::on_cmdDelPal_clicked()
{
	QListWidget* const listw = ui->listPals;

	const int remaining = listw->count();

	if(remaining == 0)
		return;

	// If there are at least two remaining items (including the one that's
	// about to be deleted), it's safe to allow some signals to go through.
	// Otherwise, we need to disable them.

	QScopedPointer<ObjectLock> lockPals(remaining == 1 ? new ObjectLock(listw) : NULL);
	QScopedPointer<ObjectLock> lockColors(remaining == 1 ? new ObjectLock(ui->listColors) : NULL);

	QListWidgetItem* const itemw = listw->takeItem(listw->currentRow());
	Q_ASSERT(itemw);
	const QString& palId = itemw->data(Qt::UserRole).toString();
	delete itemw;

	if(remaining == 1) {
		// No more palettes!
		setPaletteViewEnabled(false);
		clearPaletteView();
	}

	// Delete palette definition.

	QMap< QString, QList<QRgb> >::iterator palItem = palettes_.find(palId);

	if(palItem != palettes_.end()) {
		palettes_.erase(palItem);
	}
}

void CustomPalettes::on_cmdWml_clicked()
{
	QListWidget* const listw = ui->listPals;
	QListWidgetItem* const itemw = listw->currentItem();

	if(!itemw)
		return;

	const QString& palName = itemw->data(Qt::UserRole).toString();
	const QList<QRgb>& pal = palettes_.value(palName);
	const QString& wml = generate_color_palette_wml(palName, pal);

	CodeSnippetDialog dlg(wml, this);
	dlg.setWindowTitle(tr("Color Palette WML"));
	dlg.exec();
}

void CustomPalettes::on_leColor_textEdited(const QString &arg1)
{
	QColor color(arg1);

	if(!color.isValid())
		return;

	QListWidgetItem* const itemw = ui->listColors->currentItem();
	Q_ASSERT(itemw);

	const QRgb rgb = color.rgb();
	itemw->setData(Qt::UserRole, rgb);}
