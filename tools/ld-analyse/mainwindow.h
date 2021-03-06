/************************************************************************

    mainwindow.h

    ld-analyse - TBC output analysis
    Copyright (C) 2018 Simon Inns

    This file is part of ld-decode-tools.

    ld-dropout-correct is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>

#include "sourcevideo.h"
#include "lddecodemetadata.h"
#include "oscilloscopedialog.h"
#include "aboutdialog.h"
#include "vbidialog.h"
#include "ntscdialog.h"
#include "videometadatadialog.h"
#include "configuration.h"
#include "../ld-comb-pal/palcolour.h"
#include "../ld-comb-ntsc/comb.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QString inputFilenameParam, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionExit_triggered();
    void on_actionOpen_TBC_file_triggered();
    void on_previousPushButton_clicked();
    void on_nextPushButton_clicked();
    void on_frameNumberSpinBox_editingFinished();
    void on_showActiveVideoCheckBox_clicked();
    void on_highlightDropOutsCheckBox_clicked();
    void on_actionLine_scope_triggered();
    void on_actionAbout_ld_analyse_triggered();
    void on_actionVBI_triggered();

    void mousePressEvent(QMouseEvent *event);
    void scanLineChangedSignalHandler(qint32 scanLine);

    void on_actionNTSC_triggered();
    void on_combFilterPushButton_clicked();
    void on_sourcePushButton_clicked();
    void on_frameHorizontalSlider_valueChanged(int value);

    void on_actionVideo_metadata_triggered();

private:
    Ui::MainWindow *ui;

    // Dialogues
    OscilloscopeDialog *oscilloscopeDialog;
    AboutDialog *aboutDialog;
    VbiDialog *vbiDialog;
    NtscDialog *ntscDialog;
    VideoMetadataDialog *videoMetadataDialog;

    // Class globals
    Configuration *configuration;
    QLabel sourceVideoStatus;
    LdDecodeMetaData ldDecodeMetaData;
    SourceVideo sourceVideo;
    qint32 currentFrameNumber;
    qint32 lastScopeLine;
    bool isFileOpen;

    void updateGuiLoaded(void);
    void updateGuiUnloaded(void);

    void showFrame(qint32 frameNumber, bool showOverScan, bool highlightDropOuts);
    void hideFrame(void);

    QImage generateQImage(qint32 firstFieldNumber, qint32 secondFieldNumber);

    void loadTbcFile(QString inputFileName);
    void updateOscilloscopeDialogue(qint32 frameNumber, qint32 scanLine);
};

#endif // MAINWINDOW_H
