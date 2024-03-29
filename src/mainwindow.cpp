//
// codename Morning Star
//
// Copyright (C) 2008 - 2024 by Iris Morelle <iris@irydacea.me>
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

#include "appconfig.hpp"
#include "defs.hpp"
#include "custompalettes.hpp"
#include "customranges.hpp"
#include "mainwindow.hpp"
#include "paletteitem.hpp"
#include "ui_mainwindow.h"
#include "util.hpp"

#include <QActionGroup>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDrag>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QStyle>
#include <QUrl>
#include <QWhatsThis>

namespace {

struct no_initial_file {};
struct canceled_job    {};

static const QSize colorIconSize{16, 16};

} // end unnamed namespace

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)

	, ui(new Ui::MainWindow)

	, colorRanges_()
	, palettes_()

	, userColorRanges_(MosCurrentConfig().customColorRanges())
	, userPalettes_(MosCurrentConfig().customPalettes())

	, imagePath_()

	, originalImage_()
	, transformedImage_()

	, zoom_(1.0)
	, zoomFactors_({ 0.5, 1.0, 2.0, 4.0, 8.0 })

	, ignoreDrops_(false)
	, dragUseRecolored_(false)
	, dragStart_(false)
	, dragStartPos_()

	, recentFileActions_()

	, supportedImageFileFormats_(MosPlatform::supportedImageFileFormats())
{
	ui->setupUi(this);

	const auto& lastWindowSize = MosCurrentConfig().mainWindowSize();

	if (lastWindowSize.isValid()) {
		resize(lastWindowSize);
	}

	ui->cbxKeyPal->setIconSize(colorIconSize);
	ui->cbxNewPal->setIconSize(colorIconSize);
	ui->listRanges->setIconSize(colorIconSize);

	generateMergedRcDefinitions();
	processRcDefinitions();

	QAction* const act_whatsthis = QWhatsThis::createAction(this);
	ui->menu_Help->insertAction(ui->actionAbout_Morning_Star, act_whatsthis);
	ui->menu_Help->insertSeparator(ui->actionAbout_Morning_Star);

	QPushButton* const save = ui->buttonBox->button(QDialogButtonBox::Save);

	save->setWhatsThis(tr("Saves the current recolor job."));

#ifdef Q_OS_MACOS
	ui->action_Reload->setShortcut(QKeySequence::Refresh);
#endif

	ui->action_Open->setIcon(this->style()->standardIcon(QStyle::SP_DialogOpenButton, nullptr, dynamic_cast<QWidget*>(ui->action_Open)));
	ui->action_Save->setIcon(this->style()->standardIcon(QStyle::SP_DialogSaveButton, nullptr, dynamic_cast<QWidget*>(ui->action_Save)));
	ui->action_Reload->setIcon(this->style()->standardIcon(QStyle::SP_BrowserReload, nullptr, dynamic_cast<QWidget*>(ui->action_Reload)));
	ui->action_Quit->setIcon(this->style()->standardIcon(QStyle::SP_DialogCloseButton, nullptr, dynamic_cast<QWidget*>(ui->action_Quit)));

	// Use theme icons for some buttons on X11. This is not
	// in the .ui file for Qt 4.6 - 4.7 compatibility.

	ui->tbZoomIn->setIcon(QIcon::fromTheme("zoom-in", QIcon(":/zoom-in-16.png")));
	ui->tbZoomOut->setIcon(QIcon::fromTheme("zoom-out", QIcon(":/zoom-out-16.png")));

	auto maxMruEntries = MosCurrentConfig().recentFiles().max();

	recentFileActions_.reserve(maxMruEntries);

	for (unsigned k = 0; k < maxMruEntries; ++k) {
		QAction* act = new QAction(this);

		act->setEnabled(false);
		act->setVisible(false);
		act->setIconVisibleInMenu(true);

		connect(act, SIGNAL(triggered()), this, SLOT(handleRecent()));

		ui->menuMru->insertAction(ui->action_MruPlaceholder, act);
		recentFileActions_.push_back(act);
	}

	ui->action_MruPlaceholder->setVisible(false);

	ui->listMru->setIconSize(MosConfig::MruEntry::thumbnailSize() * 0.66);
	ui->listMru->setWordWrap(true);
	ui->listMru->setWrapping(false);

	connect(ui->listMru, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(handleRecent()));

	updateRecentFilesMenu();

	const QString& bgColorName = MosCurrentConfig().previewBackgroundColor();
	QActionGroup* bgColorActs = new QActionGroup(this);

	// The Custom Color entry goes into the list first so that it is preemptively
	// selected first on the next loop in case the color saved in preferences isn't
	// any of the predefined ones.
	QList<QPair<QAction*, QString>> bgColorActsItems = {
		{ ui->actionPreviewBgBlack,   QColor(Qt::black).name()     },
		{ ui->actionPreviewBgDark,    QColor(Qt::darkGray).name()  },
		{ ui->actionPreviewBgDefault, {}                           },
		{ ui->actionPreviewBgLight,   QColor(Qt::lightGray).name() },
		{ ui->actionPreviewBgWhite,   QColor(Qt::white).name()     },
		{ ui->actionPreviewBgCustom,  bgColorName                  },
	};

	bool stockColorSelected = false;

	for (const auto& [action, data] : bgColorActsItems)
	{
		bgColorActs->addAction(action);
		action->setData(data);

		connect(action, SIGNAL(triggered(bool)), this, SLOT(handlePreviewBgOption(bool)));

		// We must find the menu item for the color we read from the app
		// config and mark it as checked, and update the preview background
		// color manually since setChecked() won't raise the triggered()
		// signal.
		if (data == bgColorName) {
			// Avoid marking both Custom and a stock color
			if (!stockColorSelected)
				action->setChecked(true);
			stockColorSelected = true;
			setPreviewBackgroundColor(bgColorName);
		}
	}

	// Make custom bg color icon always visible even on macOS
	ui->actionPreviewBgCustom->setIconVisibleInMenu(true);
	updateCustomPreviewBgIcon();

	ui->radRc->setChecked(true);
	ui->staFunctionOpts->setCurrentIndex(0);
	togglePage2(false);
	togglePage1(true);

	ui->zoomSlider->setMinimum(0);
	ui->zoomSlider->setMaximum(zoomFactors_.size() - 1);
	ui->zoomSlider->setValue(1);

	ui->previewOriginalContainer->viewport()->setBackgroundRole(QPalette::Dark);
	ui->previewRcContainer->viewport()->setBackgroundRole(QPalette::Dark);

	//
	// FIXME: hack to prevent Oxygen stealing our drag events when dragging
	// windows from empty areas is enabled.
	//
	// http://lists.kde.org/?l=kde-devel&m=130530904703913&w=2
	//
	// We should probably figure out a better way to do this later, as well
	// as the preview panels themselves; the proper way according to the
	// Oxygen dev is to prevent (at the widget level) propagation of the event
	// to the window widget.
	//

	ui->previewOriginalContainer->setProperty("_kde_no_window_grab", true);
	ui->previewRcContainer->setProperty("_kde_no_window_grab", true);

	connect(
		ui->previewOriginalContainer->horizontalScrollBar(), SIGNAL(valueChanged(int)),
		ui->previewRcContainer->horizontalScrollBar(), SLOT(setValue(int)));
	connect(
		ui->previewRcContainer->horizontalScrollBar(), SIGNAL(valueChanged(int)),
		ui->previewOriginalContainer->horizontalScrollBar(), SLOT(setValue(int)));
	connect(
		ui->previewOriginalContainer->verticalScrollBar(), SIGNAL(valueChanged(int)),
		ui->previewRcContainer->verticalScrollBar(), SLOT(setValue(int)));
	connect(
		ui->previewRcContainer->verticalScrollBar(), SIGNAL(valueChanged(int)),
		ui->previewOriginalContainer->verticalScrollBar(), SLOT(setValue(int)));

	enableWorkArea(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateWindowTitle(bool hasImage, const QString& filename)
{
	QString appTitle = tr("Wespal");
	QString displayString;

	if (hasImage) {
		if (filename.isEmpty()) {
			this->setWindowFilePath({});
			displayString = tr("Dropped file");
		} else {
			this->setWindowFilePath(filename);
			displayString = QFileInfo(filename).fileName();
		}

		this->setWindowTitle(displayString % QString::fromUtf8(" \342\200\224 ") % appTitle);
	} else {
		this->setWindowTitle(appTitle);
	}
}

void MainWindow::generateMergedRcDefinitions()
{
	colorRanges_ = wesnoth::builtinColorRanges.objects();
	colorRanges_.insert(userColorRanges_);
	palettes_ = wesnoth::builtinPalettes.objects();
	palettes_.insert(userPalettes_);
}

void MainWindow::insertRangeListItem(const QString &id, const QString &display_name, const QColor& color)
{
	QListWidgetItem* lwi = new QListWidgetItem(ui->listRanges);
	auto colorIcon = createColorIcon(color, colorIconSize, ui->listRanges);

	lwi->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
	lwi->setCheckState(Qt::Checked);
	lwi->setText(display_name);
	lwi->setIcon(colorIcon);
	lwi->setData(Qt::UserRole, id);
}

void MainWindow::processRcDefinitions()
{
	QComboBox* cbOldPals = ui->cbxKeyPal;
	QComboBox* cbNewPals = ui->cbxNewPal;

	cbOldPals->clear();
	cbNewPals->clear();
	ui->listRanges->clear();

	//
	// We allow built-in definitions to be overridden by the user
	// by defining their own ranges/palettes with mainline ids; when
	// that happens, their data is internally merged, but these
	// overridden definitions continue to be displayed before regular
	// user-defined items.
	//
	// This makes the process of entering those definitions for the
	// UI less trivial than it should be. Additionally, we keep only
	// ids for user-defined data and use our own translatable names
	// for displaying built-ins, so we must attach extra data to item
	// widget entries to ensure things look good in the front-end.
	//

	//
	// Add built-in palettes.
	//

	for (int k = 0; k < wesnoth::builtinPalettes.objectCount(); ++k)
	{
		const auto& palName = wesnoth::builtinPalettes.orderedNames()[k];
		const auto& uiName = wesnoth::builtinPalettes.orderedTranslatableNames()[k];
		auto color = wesnoth::builtinPalettes[palName].empty()
					 ? 0U
					 : wesnoth::builtinPalettes[palName].front();
		auto colorIcon = createColorIcon(color, colorIconSize, cbOldPals);
		cbOldPals->addItem(colorIcon, uiName, palName);
		cbNewPals->addItem(colorIcon, uiName, palName);
	}

	//
	// User-defined palettes.
	//

	for (const auto& [palName, palette] : userPalettes_.asKeyValueRange())
	{
		if (wesnoth::builtinPalettes.hasName(palName)) {
			// Skip redefinitions of built-in palettes, we only care about
			// ids and names at this point.
			continue;
		}
		auto color = palette.empty()
					 ? 0U
					 : palette.front();
		auto colorIcon = createColorIcon(color, colorIconSize, cbOldPals);
		cbOldPals->addItem(colorIcon, capitalize(palName), palName);
		cbNewPals->addItem(colorIcon, capitalize(palName), palName);
	}

	//
	// Built-in color ranges.
	//

	// It is paramount to ensure built-in ranges are displayed in a specific order,
	// since Wesnoth associates digits from 1 to 9 to items in the sequence and we
	// don't want to break that convention here, for consistency's sake.

	for (int k = 0; k < wesnoth::builtinColorRanges.objectCount(); ++k)
	{
		auto& rangeName = wesnoth::builtinColorRanges.orderedNames()[k];
		auto& uiName = wesnoth::builtinColorRanges.orderedTranslatableNames()[k];
		auto color = wesnoth::builtinColorRanges[rangeName].mid();
		insertRangeListItem(rangeName, uiName, color);
	}

	//
	// User-defined color ranges
	//

	for (const auto& [id, colorRange] : userColorRanges_.asKeyValueRange())
	{
		if (wesnoth::builtinColorRanges.hasName(id)) {
			// Skip redefinitions of built-in ranges, we only care about
			// ids and names at this point.
			continue;
		}
		auto color = colorRange.mid();
		insertRangeListItem(id, capitalize(id), color);
	}

	ui->listRanges->setCurrentRow(0);
	ui->staWorkAreaParent->setCurrentIndex(1);
}

void MainWindow::handleRecent()
{
	if (auto* act = qobject_cast<QAction*>(sender()); act) {
		openFile(act->data().toString());
		return;
	}

	if (auto* listMru = qobject_cast<QListWidget*>(sender()); listMru) {
		openFile(listMru->currentItem()->data(Qt::UserRole).toString());
		//return;
	}
}

void MainWindow::updateRecentFilesMenu()
{
	int k = 0;

	ui->listMru->clear();

	for (const auto& entry : MosCurrentConfig().recentFiles())
	{
		if (k >= recentFileActions_.size())
			break;

		auto& act = *recentFileActions_[k];

		const auto& filePath = entry.filePath();
		const auto& fileName = QFileInfo(filePath).fileName();
		const auto& label =
				QString("&%1 %2").arg(k + 1).arg(fileName);
		const auto& thumbnail =
				QPixmap::fromImage(entry.thumbnail());
		const auto& miniThumbnail =
				QPixmap::fromImage(entry.miniThumbnail());

		act.setText(label);
		act.setIcon(miniThumbnail);
		act.setData(filePath);

		act.setEnabled(true);
		act.setVisible(true);

		auto* listItem = new QListWidgetItem(ui->listMru);

		listItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		listItem->setText(fileName);
		listItem->setToolTip(filePath);
		listItem->setIcon(thumbnail);
		listItem->setData(Qt::UserRole, filePath);

		++k;
	}

	// Disable and hide any extra menu items (e.g. after MruList has coalesced
	// duplicates).

	for (; k < recentFileActions_.size(); ++k)
	{
		auto& act = *recentFileActions_[k];

		act.setEnabled(false);
		act.setVisible(false);
	}

	// Disable the menu entirely if there are no MRU items.

	ui->menuMru->setEnabled(k != 0);

	// The MRU panel gets fully hidden to avoid confusion due to its styling.

	ui->panelMru->setVisible(k != 0);
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);

	switch (e->type()) {
		case QEvent::LanguageChange:
			ui->retranslateUi(this);
			break;
		default:
			break;
    }
}

void MainWindow::closeEvent(QCloseEvent * /*e*/)
{
	MosCurrentConfig().setMainWindowSize(size());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->matches(QKeySequence::ZoomIn)) {
		on_tbZoomIn_clicked();
	} else if (event->matches(QKeySequence::ZoomOut)) {
		on_tbZoomOut_clicked();
	} else {
		QMainWindow::keyPressEvent(event);
	}
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
	if (event->angleDelta().x() == 0 && event->modifiers() & Qt::ControlModifier)
	{
		if (event->angleDelta().y() > 0) {
			on_tbZoomIn_clicked();
		} else if (event->angleDelta().y() < 0) {
			on_tbZoomOut_clicked();
		}
		event->accept();
		return;
	}
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && !originalImage_.isNull() && !transformedImage_.isNull()) {
		this->dragStartPos_ = event->pos();
		this->dragUseRecolored_ = ui->previewRcContainer->geometry().contains(event->pos());
		this->dragStart_ = dragUseRecolored_ || ui->previewOriginalContainer->geometry().contains(event->pos());
	}
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
	if (dragStart_ && (event->buttons() & Qt::LeftButton) && (event->pos() - dragStartPos_).manhattanLength() >= QApplication::startDragDistance()) {

		QDrag *d = new QDrag(this);
		QMimeData *m = new QMimeData();

		if (dragUseRecolored_)
			m->setImageData(this->transformedImage_);
		else
			m->setImageData(this->originalImage_);

		d->setMimeData(m);

		ignoreDrops_ = true;
		d->exec(Qt::CopyAction);
		ignoreDrops_ = dragStart_ = false;
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
	if (e->mimeData()->hasImage() || e->mimeData()->hasUrls())
		e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
	if (ignoreDrops_)
		return;

	QImage newimg;
	QString newpath;

	e->acceptProposedAction();

	if (e->mimeData()->hasImage()) {
		newimg = qvariant_cast<QImage>(e->mimeData()->imageData());
	}
	else if (e->mimeData()->hasUrls()) {
		newpath = e->mimeData()->urls().front().path();
		newimg.load(newpath);
	}

	if (newimg.isNull()) {
		return;
	}

	originalImage_ = newimg.convertToFormat(QImage::Format_ARGB32);

	// Refresh UI
	if (newpath.isEmpty() != true) {
		imagePath_ = newpath;
		updateWindowTitle(true, imagePath_);
	}
	else {
		updateWindowTitle(true, {});
	}

	refreshPreviews();
	enableWorkArea(true);
}

void MainWindow::on_radRc_clicked()
{
	ui->staFunctionOpts->setCurrentIndex(0);
	togglePage1(true);
	togglePage2(false);
	refreshPreviews();
}

void MainWindow::on_radPal_clicked()
{
	ui->staFunctionOpts->setCurrentIndex(1);
	togglePage1(false);
	togglePage2(true);
	refreshPreviews();
}

void MainWindow::togglePage1(bool newstate)
{
	ui->listRanges->setEnabled(newstate);
}

void MainWindow::togglePage2(bool newstate)
{
	ui->cbxNewPal->setEnabled(newstate);
	ui->lblNewPal->setEnabled(newstate);
}

void MainWindow::on_actionAbout_Morning_Star_triggered()
{
	doAboutDialog();
}

void MainWindow::on_buttonBox_clicked(QAbstractButton* button)
{
	QDialogButtonBox::StandardButton btype = ui->buttonBox->standardButton(button);

	switch (btype) {
		case QDialogButtonBox::Save:
			doSaveFile();
			break;
		case QDialogButtonBox::Close:
			if (originalImage_.isNull()) {
				this->close();
			} else {
				doCloseFile();
			}
			break;
		default:
			;
	}
}

void MainWindow::on_action_Open_triggered()
{
	openFile();
}

void MainWindow::on_action_Quit_triggered()
{
	doCloseFile();
	this->close();
}

void MainWindow::on_action_Reload_triggered()
{
	doReloadFile();
}

void MainWindow::openFile(const QString& fileName)
{
	QString selectedPath;
	QString startingDir;

	QStringList pictureLocations =
			QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);

	if (!fileName.isEmpty()) {
		selectedPath = fileName;
	} else {
		if (imagePath_.isEmpty()) {
			if (!pictureLocations.empty()) {
				startingDir = pictureLocations.first();
			} else {
				startingDir = ".";
			}
		} else {
			startingDir = QFileInfo(imagePath_).absolutePath();
		}

		selectedPath = QFileDialog::getOpenFileName(
			this,
			tr("Choose source image"),
			startingDir,
			supportedImageFileFormats_
		);
	}

	if (selectedPath.isEmpty() && originalImage_.isNull()) {
		return;
	}

	QImage selectedImage{selectedPath};

	if (selectedImage.isNull()) {
		if (selectedPath.isNull() != true) {
			MosUi::error(
				this, tr("Could not load %1.").arg(selectedPath));
		}

		// TODO
		//MosCurrentConfig().removeRecentFile(path_temp);

		return;
	}

	imagePath_ = selectedPath;
	// We want to work on actual ARGB data
	originalImage_ = selectedImage.convertToFormat(QImage::Format_ARGB32);

	// Refresh UI
	MosCurrentConfig().addRecentFile(imagePath_, originalImage_);
	updateRecentFilesMenu();
	updateWindowTitle(true, imagePath_);
	refreshPreviews();

	enableWorkArea(true);
}

void MainWindow::doReloadFile()
{
	QImage img{imagePath_};
	if (img.isNull()) {
		MosUi::error(this, tr("Could not reload %1.").arg(imagePath_));
		return;
	}

	originalImage_ = img.convertToFormat(QImage::Format_ARGB32);

	// Refresh UI
	refreshPreviews();
}

void MainWindow::refreshPreviews()
{
	if (this->originalImage_.isNull() || this->signalsBlocked())
		return;

	ColorMap cvtMap;
	const auto& palData = currentPalette();

	if (ui->staFunctionOpts->currentIndex()) {
		const auto& targetPalData = currentPalette(true);
		cvtMap = generateColorMap(palData, targetPalData);
	} else {
		const auto& colorRange = colorRanges_.value(ui->listRanges->currentIndex().data(Qt::UserRole).toString());
		cvtMap = colorRange.applyToPalette(palData);
	}

	transformedImage_ = recolorImage(originalImage_, cvtMap);

	const QSize& scaled_size = originalImage_.size() * zoom_;

	ui->previewOriginal->setPixmap(QPixmap::fromImage(originalImage_));
	ui->previewRc->setPixmap(QPixmap::fromImage(transformedImage_));

	ui->previewOriginal->resize(scaled_size);
	ui->previewRc->resize(scaled_size);

	ui->previewOriginal->parentWidget()->adjustSize();
	ui->previewRc->parentWidget()->adjustSize();

	centerScrollArea(ui->previewOriginalContainer);
	centerScrollArea(ui->previewRcContainer);

	ui->staWorkAreaParent->setCurrentIndex(0);
}

void MainWindow::centerScrollArea(QScrollArea *scrollArea)
{
	if (!scrollArea || !scrollArea->widget())
		return;

	const QSize& childSize = scrollArea->widget()->size();
	const QSize& viewSize = scrollArea->viewport()->size();

	scrollArea->ensureVisible(childSize.width()/2, childSize.height()/2, viewSize.width()/2, viewSize.height()/2);
}

void MainWindow::doSaveFile()
{
	QString base = QFileDialog::getExistingDirectory(
					   this,
					   tr("Choose an output directory"),
					   QFileInfo(imagePath_).absolutePath(),
					   QFileDialog::ShowDirsOnly);

	if (base.isNull())
		return;

	QStringList succeeded;

	try {
		if (ui->staFunctionOpts->currentIndex()) {
			succeeded = doSaveSingleRecolor(base);
		}
		else {
			succeeded = doSaveColorRanges(base);
		}

		MosUi::message(this, tr("The output files have been saved successfully."), succeeded);
	} catch (const canceled_job&) {
		;
	} catch (const QStringList& failed) {
		MosUi::error(this, tr("Some files could not be saved correctly."), failed);
	}
}

void MainWindow::doCloseFile()
{
	enableWorkArea(false);
	this->originalImage_ = QImage{};
	this->transformedImage_ = QImage{};
}

void MainWindow::doAboutDialog()
{
	MosUi::about(this);
}

void MainWindow::enableWorkArea(bool enable)
{
	if (!enable) {
		updateWindowTitle(false);
	}

	auto elements = std::make_tuple(
		ui->action_Reload,
		ui->action_Save,
		ui->radPal, ui->radRc,
		ui->lblKeyPal, ui->cbxKeyPal,
		ui->lblNewPal, ui->cbxNewPal,
		ui->listRanges,
		ui->zoomSlider, ui->tbZoomIn, ui->tbZoomOut,
		ui->buttonBox->button(QDialogButtonBox::Save));

	std::apply([enable](auto&&... widget) {
		(widget->setEnabled(enable), ...);
	}, elements);

	ui->staWorkAreaParent->setCurrentIndex(enable ? 0 : 1);

	auto* closeButton = ui->buttonBox->button(QDialogButtonBox::Close);

	if (enable) {
		closeButton->setText(tr("Close"));
		closeButton->setWhatsThis(tr("Closes the current image."));
	} else {
		closeButton->setText(tr("Quit"));
		closeButton->setWhatsThis(tr("Quits Wespal."));
	}
}

QString MainWindow::currentPaletteName(bool paletteSwitchMode) const
{
	auto* combow = paletteSwitchMode
				   ? ui->cbxNewPal
				   : ui->cbxKeyPal;

	const int choice = combow->currentIndex();
	const auto& palette_name = combow->itemData(choice).toString();

	Q_ASSERT(!palette_name.isEmpty());

	return palette_name;
}

ColorList MainWindow::currentPalette(bool paletteSwitchMode) const
{
	return palettes_.value(currentPaletteName(paletteSwitchMode));
}

bool MainWindow::confirmFileOverwrite(const QStringList& paths)
{
	return MosUi::prompt(this, tr("The chosen directory already contains files with the same names required for output. Do you wish to overwrite them and continue?"), paths);
}

QStringList MainWindow::doSaveSingleRecolor(QString &base)
{
	QMap<QString, ColorMap> jobs;

	const auto& palId = currentPaletteName();
	const auto& palData = currentPalette();

	const auto& targetPalId = currentPaletteName(true);
	const auto& targetPalData = currentPalette(true);

	const QString& filePath = base + "/" + QFileInfo(imagePath_).completeBaseName() +
			"-PAL-" + palId + "-" + targetPalId + ".png";

	jobs[filePath] = generateColorMap(palData, targetPalData);

	if (QFileInfo::exists(filePath) &&
		!confirmFileOverwrite({cleanFileName(filePath)}))
	{
		throw canceled_job();
	}

	return doRunJobs(jobs);
}

QStringList MainWindow::doSaveColorRanges(QString &base)
{
	QMap<QString, ColorMap> jobs;

	const auto& palId = currentPaletteName();
	const auto& palData = currentPalette();

	QStringList needOverwriteFiles;

	for (int k = 0; k < ui->listRanges->count(); ++k)
	{
		QListWidgetItem* itemw = ui->listRanges->item(k);
		Q_ASSERT(itemw);

		if (itemw->checkState() == Qt::Checked) {
			const QString& rangeId = itemw->data(Qt::UserRole).toString();

			const QString& filePath = base + "/" + QFileInfo(imagePath_).completeBaseName() +
					"-RC-" + palId + "-" + QString::number(k + 1) +
					"-" + rangeId + ".png";

			const auto& colorRange = colorRanges_.value(rangeId);
			jobs[filePath] = colorRange.applyToPalette(palData);

			if (QFileInfo::exists(filePath)) {
				needOverwriteFiles.push_back(cleanFileName(filePath));
			}
		}
	}

	if (!needOverwriteFiles.isEmpty() && !confirmFileOverwrite(needOverwriteFiles)) {
		throw canceled_job();
	}

	return doRunJobs(jobs);
}

QStringList MainWindow::doRunJobs(const QMap<QString, ColorMap>& jobs)
{
	QStringList failed, succeeded;

	ScopedCursor sc{*this, {Qt::WaitCursor}};

	setEnabled(false);

	for (const auto& [fileName, colorMap] : jobs.asKeyValueRange())
	{
		const auto& plainName = cleanFileName(fileName);
		auto rc = recolorImage(originalImage_, colorMap);

		if (MosIO::writePng(rc, fileName)) {
			succeeded.push_back(plainName);
		} else {
			failed.push_back(plainName);
		}
	}

	setEnabled(true);

	if (failed.isEmpty() != true) {
		throw failed;
	}

	return succeeded;
}

void MainWindow::on_action_Save_triggered()
{
	doSaveFile();
}

void MainWindow::on_cbxKeyPal_currentIndexChanged(int /*index*/)
{
	refreshPreviews();
}

void MainWindow::on_cbxNewPal_currentIndexChanged(int /*index*/)
{
	refreshPreviews();
}

void MainWindow::on_listRanges_currentRowChanged(int /*currentRow*/)
{
	refreshPreviews();
}

void MainWindow::on_zoomSlider_valueChanged(int value)
{
	Q_ASSERT(value >= 0 && value < zoomFactors_.size());
	zoom_ = zoomFactors_[value];

	updateZoomButtons();
	refreshPreviews();
}

void MainWindow::on_tbZoomIn_clicked()
{
	ui->zoomSlider->setValue(ui->zoomSlider->value() + 1);
}

void MainWindow::on_tbZoomOut_clicked()
{
	ui->zoomSlider->setValue(ui->zoomSlider->value() - 1);
}

void MainWindow::updateZoomButtons()
{
	QSlider& zoomSlider= *ui->zoomSlider;

	ui->tbZoomOut->setEnabled(zoomSlider.value() != zoomSlider.minimum());
	ui->tbZoomIn->setEnabled(zoomSlider.value() != zoomSlider.maximum());
}

void MainWindow::on_actionColor_ranges_triggered()
{
	CustomRanges dlg{userColorRanges_, this};
	dlg.exec();

	if (dlg.result() == QDialog::Rejected)
		return;

	userColorRanges_ = dlg.ranges();

	{
		ObjectLock l{this};
		generateMergedRcDefinitions();
		processRcDefinitions();
	}

	refreshPreviews();

	MosCurrentConfig().setCustomColorRanges(userColorRanges_);
}

void MainWindow::on_action_Palettes_triggered()
{
	CustomPalettes dlg{userPalettes_, colorRanges_, this};
	dlg.exec();

	if (dlg.result() == QDialog::Rejected)
		return;

	userPalettes_ = dlg.getPalettes();

	{
		ObjectLock l{this};
		generateMergedRcDefinitions();
		processRcDefinitions();
	}

	refreshPreviews();

	MosCurrentConfig().setCustomPalettes(userPalettes_);
}

void MainWindow::handlePreviewBgOption(bool checked)
{
	if (!checked)
		return;

	QAction* const act = qobject_cast<QAction*>(sender());

	if (!act)
		return;

	if (act == ui->actionPreviewBgCustom) {
		// We want to give the user the option to customize the preview color
		// first.
		doCustomPreviewBgSelect();
	}

	setPreviewBackgroundColor(act->data().toString());
}

void MainWindow::doCustomPreviewBgSelect()
{
	auto* act = ui->actionPreviewBgCustom;
	QColor userColor = QColorDialog::getColor(QColor(act->data().toString()), this);
	if (userColor.isValid()) {
		act->setData(userColor.name());
		updateCustomPreviewBgIcon();
	}
}

void MainWindow::updateCustomPreviewBgIcon()
{
	auto* act = ui->actionPreviewBgCustom;
	act->setIcon(createColorIcon({act->data().toString()}, this));
}

void MainWindow::setPreviewBackgroundColor(const QString& colorName)
{
	if (!colorName.isEmpty()) {
		const QString ss = "* { background-color: " % colorName % "; }";
		ui->previewOriginalContainer->viewport()->setStyleSheet(ss);
		ui->previewRcContainer->viewport()->setStyleSheet(ss);
	} else {
		ui->previewOriginalContainer->viewport()->setStyleSheet({});
		ui->previewRcContainer->viewport()->setStyleSheet({});
	}

	MosCurrentConfig().setPreviewBackgroundColor(colorName);
}

void MainWindow::on_cmdOpen_clicked()
{
	openFile();
}


void MainWindow::on_action_ClearMru_triggered()
{
	MosCurrentConfig().clearRecentFiles();
	updateRecentFilesMenu();
}
