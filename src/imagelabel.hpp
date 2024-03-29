//
// codename Morning Star
//
// Copyright (C) 2012 - 2024 by Iris Morelle <iris@irydacea.me>
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

#pragma once

#include <QWidget>

class ImageLabel : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
public:
	explicit ImageLabel(QWidget* parent = nullptr);
	
	const QPixmap* pixmap() const {
		return &pixmap_;
	}

	void setPixmap(const QPixmap& pixmap) {
		pixmap_ = pixmap;
		update();
	}

signals:
	
public slots:

protected:
	virtual void paintEvent(QPaintEvent* event);

private:
	QPixmap pixmap_;
};
