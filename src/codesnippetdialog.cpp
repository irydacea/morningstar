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

#include "codesnippetdialog.hpp"
#include "ui_codesnippetdialog.h"

#include "util.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QPushButton>
#include <QStringBuilder>
#include <QStyle>
#include <QTextStream>

CodeSnippetDialog::CodeSnippetDialog(const QString& contents, QWidget *parent) :
    QDialog(parent),
	ui(new Ui::CodeSnippetDialog)
{
	ui->setupUi(this);
	ui->teContents->setPlainText(contents);

	auto* const closeButton = ui->buttonBox->button(QDialogButtonBox::Close);
	auto* const saveButton = ui->buttonBox->button(QDialogButtonBox::Save);
	auto* const copyButton = ui->buttonBox->button(QDialogButtonBox::Apply);

	Q_ASSERT(closeButton && saveButton && copyButton);

	copyButton->setText(tr("Copy"));
	copyButton->setDefault(true);

	// Primitive check to see whether the current style likes assigning icons
	// to dialog buttons.
	if (!closeButton->icon().isNull()) {
		auto copyIcon = QIcon::fromTheme("edit-copy", QIcon(":/edit-copy-16.png"));
		copyButton->setIcon(copyIcon);
	}

	connect(copyButton, SIGNAL(clicked()), this, SLOT(handleCopy()));
	connect(saveButton, SIGNAL(clicked()), this, SLOT(handleSave()));

	auto okIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
	ui->successIcon->setPixmap(okIcon.pixmap(22));
	ui->boxClipboardMessage->setVisible(false);

	const auto& monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	ui->teContents->setFont(monoFont);
}

CodeSnippetDialog::~CodeSnippetDialog()
{
	delete ui;
}

void CodeSnippetDialog::changeEvent(QEvent *e)
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

void CodeSnippetDialog::handleCopy()
{
	QApplication::clipboard()->setText(ui->teContents->toPlainText());
	ui->boxClipboardMessage->setVisible(true);
	ui->teContents->selectAll();
}

void CodeSnippetDialog::handleSave()
{
	const auto& filePath =
			QFileDialog::getSaveFileName(
				this,
				tr("Save WML"),
				{},
				tr("WML document") % " (*.cfg);;" %
				tr("All files") % " (*)");

	if (filePath.isNull()) {
		return;
	}

	QFile f{filePath};

	if (f.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
		QTextStream out{&f};
		out << ui->teContents->toPlainText();
		f.close();
	}

	if (f.error()) {
		MosUi::error(this, tr("The file could not be saved"), filePath);
	} else {
		MosUi::message(this, tr("The file was saved successfully."), filePath);
	}
}
