//
// codename Morning Star
//
// Copyright (C) 2010 - 2024 by Iris Morelle <iris@irydacea.me>
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

#include "recentfiles.hpp"
#include "wesnothrc.hpp"

#include <QSize>

namespace MosConfig {

class Manager
{
public:
	/**
	 * Retrieves the current config instance.
	 */
	static Manager& instance()
	{
		static Manager configManager;
		return configManager;
	}

	// original character do not steal
	Manager(const Manager&) = delete;
	Manager(Manager&&) = delete;
	Manager& operator=(const Manager&) = delete;

	/**
	 * Retrieves the current recent files.
	 */
	const MruList& recentFiles() const
	{
		return imageFilesMru_;
	}

	/**
	 * Adds a new recent file entry.
	 *
	 * @param filePath       File path.
	 * @param image          Image contents of the file which will be used for
	 *                       generating a thumbnail.
	 */
	void addRecentFile(const QString& filePath, const QImage& image);

	/**
	 * Clears the recent files list.
	 */
	void clearRecentFiles();

	// TODO
	//void removeRecentFile(const QString& filePath);

	/**
	 * Retrieves the list of custom color ranges.
	 */
	const QMap<QString, ColorRange>& customColorRanges() const
	{
		return customColorRanges_;
	}

	/**
	 * Sets the list of custom color ranges.
	 */
	void setCustomColorRanges(const QMap<QString, ColorRange>& colorRanges);

	//void addCustomColorRange(const QString& name, const ColorRange& colorRange);

	//void deleteCustomColorRange(const QString& name);

	/**
	 * Retrieves the list of custom palettes.
	 */
	const QMap<QString, ColorList>& customPalettes() const
	{
		return customPalettes_;
	}

	/**
	 * Sets the list of custom palettes.
	 */
	void setCustomPalettes(const QMap<QString, ColorList>& palettes);

	//void addCustomPalette(const QString& name, const ColorList& palette);

	//void deleteCustomPalette(const QString& palette);

	/**
	 * Retrieves the saved main window size.
	 */
	const QSize& mainWindowSize() const
	{
		return mainWindowSize_;
	}

	/**
	 * Sets the saved main window size.
	 */
	void setMainWindowSize(const QSize& size);

	/**
	 * Retrieves the preview background color.
	 */
	const QString& previewBackgroundColor() const
	{
		return previewBackgroundColor_;
	}

	/**
	 * Sets the preview background color.
	 */
	void setPreviewBackgroundColor(const QString& previewBackgroundColor);

private:
	Manager();

	MruList imageFilesMru_;
	QMap<QString, ColorRange> customColorRanges_;
	QMap<QString, ColorList> customPalettes_;
	QSize mainWindowSize_;
	QString previewBackgroundColor_;
};

inline Manager& current()
{
	return Manager::instance();
}

} // end namespace MosConfig

inline MosConfig::Manager& MosCurrentConfig()
{
	return MosConfig::Manager::instance();
}
