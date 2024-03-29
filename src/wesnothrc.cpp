//
// codename Morning Star
// src/color_range.cpp - Recoloring mechanism (implementation)
//
// Copyright (C) 2003 - 2024 by the Battle for Wesnoth Project <www.wesnoth.org>
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

/** @file color_range.cpp
 * Generate ranges of colors, and color palettes.
 * Used e.g. to color HP, XP.
 */

#include "wesnothrc.hpp"

#include "version.hpp"

#include <QColorSpace>
#include <QImageWriter>
#include <QRegularExpression>
#include <QStringBuilder>

namespace {

const QString WML_INDENT = QStringLiteral("    ");

const QString WML_COLOR_RANGE_DESC = QStringLiteral(
	"# This code defines a Wesnoth color range. You may use it\n"
	"# at global level (e.g. within the add-on's _main.cfg #ifdef)\n"
	"# or in specific situations by providing the contents of the\n"
	"# rgb= attribute (e.g. in [side] color= attributes or in ~RC()\n"
	"# image path function specifications).\n");

const QString WML_COLOR_PALETTE_DESC = QStringLiteral(
	"# This code defines a Wesnoth color palette. You may use it\n"
	"# at global level (e.g. within the add-on's _main.cfg #ifdef)\n"
	"# or in specific situations by providing the comma-separated\n"
	"# color list (e.g. in ~RC() image path function\n"
	"# specifications).\n");

QString makeIdentifier(const QString& name)
{
	if (name.isEmpty())
		return name;

	QString ret = name.toLower();
	// Characters that may confuse the WML parser or
	// aren't conventionally used in identifiers are
	// replaced by underscores.
	static QRegularExpression invalid_wml_id{"[#=\"\\s]"};
	ret.replace(invalid_wml_id, "_");

	return ret;
}

QString makeWmlColor(QRgb rgb)
{
	QString ret = QColor(rgb).name().toUpper();
	Q_ASSERT(!ret.isEmpty());
	return ret.right(ret.length() - 1);
}

} // end unnamed namespace #1

ColorMap ColorRange::applyToPalette(const ColorList& palette) const
{
	ColorMap mapRgb;

	auto midR = qRed(mid_),
		 midG = qGreen(mid_),
		 midB = qBlue(mid_);

	auto maxR = qRed(max_),
		 maxG = qGreen(max_),
		 maxB = qBlue(max_);

	auto minR = qRed(min_),
		 minG = qGreen(min_),
		 minB = qBlue(min_);

	// Map first color in vector to exact new color
	QRgb tempRgb = palette.empty() ? 0 : palette.front();
	auto oldR = qRed(tempRgb),
		 oldG = qGreen(tempRgb),
		 oldB = qBlue(tempRgb);
	const auto referenceAvg = (oldR + oldG + oldB) / 3;

	for (auto color : palette)
	{
		auto r = qRed(color),
			 g = qGreen(color),
			 b = qBlue(color);
		const auto oldAvg = (r + g + b) / 3;

		// Calculate new color
		if (referenceAvg && oldAvg <= referenceAvg) {
			float oldRatio = float(oldAvg) / float(referenceAvg);
			r = oldRatio * midR + (1 - oldRatio) * minR;
			g = oldRatio * midG + (1 - oldRatio) * minG;
			b = oldRatio * midB + (1 - oldRatio) * minB;
		} else if (255 - referenceAvg) {
			float oldRatio = (255.0f - float(oldAvg)) / (255.0f - float(referenceAvg));
			r = oldRatio * midR + (1 - oldRatio) * maxR;
			g = oldRatio * midG + (1 - oldRatio) * maxG;
			b = oldRatio * midB + (1 - oldRatio) * maxB;
		} else {
			r = g = b = 0;
			Q_ASSERT(false);
			// Should never get here.
			// Would imply oldAvg > referenceAvg = 255
		}

		mapRgb[color] = qRgb(qBound(0, r, 255),
							 qBound(0, g, 255),
							 qBound(0, b, 255));
	}

	return mapRgb;
}

ColorMap generateColorMap(const ColorList& srcPalette,
						  const ColorList& newPalette)
{
	ColorMap mapRgb;

	auto shortestEnd = qMin(srcPalette.count(), newPalette.count());

	for (qsizetype i = 0; i < shortestEnd; ++i)
	{
		mapRgb[srcPalette[i]] = newPalette[i];
	}

	return mapRgb;
}

QString wmlFromColorRange(const QString& name,
						  const ColorRange& range)
{
	QString code = WML_COLOR_RANGE_DESC
				   % "\n"
				   % "[color_range]\n"
				   % WML_INDENT % "id=\"" % makeIdentifier(name) % "\"\n"
				   % WML_INDENT % "name= _ \"" % name % "\"\n"
				   % WML_INDENT % "rgb=\"";

	const auto& strAvg = makeWmlColor(range.mid());
	const auto& strMax = makeWmlColor(range.max());
	const auto& strMin = makeWmlColor(range.min());
	// TODO: restore map marker color support in the color_range type and
	//       allow users to pick one, or just warn them about it?
	const auto& strMap = strAvg;

	code += strAvg % "," % strMax % "," % strMin % "," % strMap % "\"\n"
			% "[/color_range]\n";

	return code;
}

QString wmlFromColorList(const QString& name,
						 const ColorList& palette)
{
	QString code = WML_COLOR_PALETTE_DESC
				   % "\n"
				   % "[color_palette]\n"
				   % WML_INDENT % makeIdentifier(name) % "=\"";

	bool first = true;

	for (auto color : palette) {
		if (!first) {
			code += ',';
		} else {
			first = false;
		}

		code += makeWmlColor(color);
	}

	code += "\"\n";
	code += "[/color_palette]\n";

	return code;
}

QImage recolorImage(const QImage& input,
					const ColorMap& colorMap)
{
	QImage output;

	// Copy input to output first. We force ARGB32 since that's the only
	// format we (and Wesnoth) currently understand.
	output = input.convertToFormat(QImage::Format_ARGB32);

	// Create a version of the color map without alpha values for faster
	// lookups.
	ColorMap plainRgbMap;

	for (auto i = colorMap.begin(); i != colorMap.end(); ++i)
		plainRgbMap[i.key() & 0xFFFFFFU] = i.value() & 0xFFFFFFU;

	auto maxY = output.height(), maxX = output.width();

	for (int y = 0; y < maxY; ++y)
	{
		auto* line = reinterpret_cast<QRgb*>(output.scanLine(y));
		for (int x = 0; x < maxX; ++x)
		{
			auto i = plainRgbMap.find(line[x] & 0xFFFFFFU);

			if (i == plainRgbMap.end())
				continue;

			// Match found, replace everything except alpha
			line[x] = (line[x] & 0xFF000000U) + i.value();
		}
	}

	return output;
}

namespace MosIO {

bool writePng(QImage& input, const QString& fileName)
{
	static QString stamp = QString{"Generated by Wespal v%1"}.arg(MOS_VERSION);

	QImageWriter out{fileName, "PNG"};

	out.setText("", stamp);
	out.setCompression(100);

	// Images produced by reading GIMP XCFs can end up with a color space
	// set that looks like the following:
	//
	//   QColorSpace(
	//     QColorSpace::SRgbLinear,
	//     QColorSpace::Primaries::SRgb,
	//     QColorSpace::TransferFunction::Linear,
	//     gamma=1)
	//
	// This appears to be wrong in some way (?), resulting in both macOS
	// and Windows apps including the GIMP itself displaying output with
	// a seriously washed-out palette. We work around this by making sure
	// that our output never has a color space transform attached.

	input.setColorSpace({});

	return out.write(input);
}

} // end namespace MosIO
