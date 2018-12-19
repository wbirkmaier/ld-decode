/************************************************************************

    mainwindow.cpp

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

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QString inputFilenameParam, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Load the application's configuration
    configuration = new Configuration();

    // Add a status bar to show the state of the source video file
    ui->statusBar->addWidget(&sourceVideoStatus);
    sourceVideoStatus.setText(tr("No source video file loaded"));

    // Set the initial frame number
    currentFrameNumber = 1;
    isFileOpen = false;

    // Add an event filter to the frame viewer label to catch mouse events
    ui->frameViewerLabel->installEventFilter(ui->frameViewerLabel);

    // Set up the oscilloscope dialogue
    oscilloscopeDialog = new OscilloscopeDialog(this);

    // Connect to the scan line changed signal from the oscilloscope dialogue
    connect(oscilloscopeDialog, &OscilloscopeDialog::scanLineChanged, this, &MainWindow::scanLineChangedSignalHandler);
    lastScopeLine = 1;

    // Set up the about dialogue
    aboutDialog = new AboutDialog(this);

    // Set up the VBI dialogue
    vbiDialog = new VbiDialog(this);

    // Load the window geometry from the configuration
    restoreGeometry(configuration->getMainWindowGeometry());
    vbiDialog->restoreGeometry(configuration->getVbiDialogGeometry());
    oscilloscopeDialog->restoreGeometry(configuration->getOscilloscopeDialogGeometry());

    // Set up the GUI
    updateGuiUnloaded();

    // Was a filename specified on the command line?
    if (!inputFilenameParam.isEmpty()) {
        loadTbcFile(inputFilenameParam);
    }
}

MainWindow::~MainWindow()
{
    // Save the window geometry to the configuration
    configuration->setMainWindowGeometry(saveGeometry());
    configuration->setVbiDialogGeometry(vbiDialog->saveGeometry());
    configuration->setOscilloscopeDialogGeometry(oscilloscopeDialog->saveGeometry());
    configuration->writeConfiguration();

    // Close the source video if open
    sourceVideo.close();
    delete ui;
}

// Method to update the GUI when a file is loaded
void MainWindow::updateGuiLoaded(void)
{
    // Disable the frame controls
    ui->frameNumberSpinBox->setEnabled(true);
    ui->previousPushButton->setEnabled(true);
    ui->nextPushButton->setEnabled(true);

    // Disable the option check boxes
    ui->highlightDropOutsCheckBox->setEnabled(true);
    ui->showActiveVideoCheckBox->setEnabled(true);

    // Update the current frame number
    currentFrameNumber = 1;
    ui->frameNumberSpinBox->setMinimum(1);
    ui->frameNumberSpinBox->setMaximum(getAvailableNumberOfFrames());
    ui->frameNumberSpinBox->setValue(currentFrameNumber);

    // Enable the VBI data groupbox
    ui->vbiGroupBox->setEnabled(true);

    // Enable the frame info group box
    ui->frameGroupBox->setEnabled(true);

    // Enable menu options
    ui->actionLine_scope->setEnabled(true);
    ui->actionVBI->setEnabled(true);

    // Show the current frame
    showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());

    isFileOpen = true;
}

// Method to update the GUI when a file is unloaded
void MainWindow::updateGuiUnloaded(void)
{
    // Disable the frame controls
    ui->frameNumberSpinBox->setEnabled(false);
    ui->previousPushButton->setEnabled(false);
    ui->nextPushButton->setEnabled(false);

    // Disable the option check boxes
    ui->highlightDropOutsCheckBox->setEnabled(false);
    ui->showActiveVideoCheckBox->setEnabled(false);

    // Update the current frame number
    currentFrameNumber = 1;
    ui->frameNumberSpinBox->setValue(currentFrameNumber);

    // Allow the next and previous frame buttons to auto-repeat
    ui->previousPushButton->setAutoRepeat(true);
    ui->previousPushButton->setAutoRepeatDelay(500);
    ui->previousPushButton->setAutoRepeatInterval(10);
    ui->nextPushButton->setAutoRepeat(true);
    ui->nextPushButton->setAutoRepeatDelay(500);
    ui->nextPushButton->setAutoRepeatInterval(10);

    // Disable the VBI data groupbox
    ui->vbiGroupBox->setEnabled(false);

    // Disable the frame info group box
    ui->frameGroupBox->setEnabled(false);

    // Set the window title
    this->setWindowTitle(tr("ld-analyse"));

    // Set the status bar text
    sourceVideoStatus.setText(tr("No source video file loaded"));

    // Disable menu options
    ui->actionLine_scope->setEnabled(false);
    ui->actionVBI->setEnabled(false);

    // Hide the displayed frame
    hideFrame();

    isFileOpen = false;
}

// Method to get the available number of frames
qint32 MainWindow::getAvailableNumberOfFrames(void)
{
    // Get the video parameter metadata
    LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();

    // Determine the top and bottom fields for the frame number
    qint32 fieldNumberOffset = 0;

    if (videoParameters.isFieldOrderEvenOdd) {
        // Top frame should be even, so if the current topField is odd, increment it by one
        if (!ldDecodeMetaData.getField(1).isEven) {
            fieldNumberOffset++;
        }
    } else {
        // Top frame should be odd, so if the current topField is even, increment it by one
        if (ldDecodeMetaData.getField(1).isEven) {
            fieldNumberOffset++;
        }
    }

    return (sourceVideo.getNumberOfAvailableFields() - fieldNumberOffset) / 2;
}

// Method to display a sequential frame
void MainWindow::showFrame(qint32 frameNumber, bool showActiveVideoArea, bool highlightDropOuts)
{
    // Get the video parameter metadata
    LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();

    // Determine the top and bottom fields for the frame number
    qint32 topFieldNumber = (frameNumber * 2) - 1;

    if (videoParameters.isFieldOrderEvenOdd) {
        // Top frame should be even, so if the current topField is odd, increment it by one
        if (!ldDecodeMetaData.getField(topFieldNumber).isEven) {
            topFieldNumber++;
            qDebug() << "MainWindow::showFrame(): First field is out of frame order - ignoring";
        }
    } else {
        // Top frame should be odd, so if the current topField is even, increment it by one
        if (ldDecodeMetaData.getField(topFieldNumber).isEven) {
            topFieldNumber++;
            qDebug() << "MainWindow::showFrame(): First field is out of frame order - ignoring";
        }
    }

    // Range check the bottom field number
    if (topFieldNumber + 1 > sourceVideo.getNumberOfAvailableFields()) {
        qDebug() << "MainWindow::showFrame(): Bottom field number exceed the available number of fields!";
        return;
    }

    qDebug() << "MainWindow::showFrame(): Frame number" << frameNumber << "has a top-field of" << topFieldNumber <<
                "and a bottom field of" << topFieldNumber + 1;

    // Get a QImage for the frame
    QImage frameImage = generateQImage(topFieldNumber, topFieldNumber + 1);

    // Get the field metadata
    LdDecodeMetaData::Field topField = ldDecodeMetaData.getField(topFieldNumber);
    LdDecodeMetaData::Field bottomField = ldDecodeMetaData.getField(topFieldNumber + 1);

    // Show the field numbers
    ui->topFieldLabel->setText(QString::number(topFieldNumber));
    ui->bottomFieldLabel->setText(QString::number(topFieldNumber + 1));

    // Show the field order
    if (videoParameters.isFieldOrderValid) {
        if (videoParameters.isFieldOrderEvenOdd) ui->fieldOrderLabel->setText(tr("Even/Odd"));
        else ui->fieldOrderLabel->setText(tr("Odd/Even"));
    } else {
        ui->fieldOrderLabel->setText(tr("Missing metadata"));
    }

    // Show the active video extent?
    if (showActiveVideoArea) {
        // Create a painter object
        QPainter imagePainter;

        imagePainter.begin(&frameImage);
        imagePainter.setPen(Qt::cyan);

        // Determine the first and last active scan line based on the source format
        qint32 firstActiveScanLine;
        qint32 lastActiveScanLine;
        if (videoParameters.isSourcePal) {
            firstActiveScanLine = 44;
            lastActiveScanLine = 617;
        } else {
            firstActiveScanLine = 40;
            lastActiveScanLine = 519;
        }

        // Outline the active video area
        imagePainter.drawLine(videoParameters.activeVideoStart, firstActiveScanLine, videoParameters.activeVideoStart, lastActiveScanLine);
        imagePainter.drawLine(videoParameters.activeVideoEnd, firstActiveScanLine, videoParameters.activeVideoEnd, lastActiveScanLine);
        imagePainter.drawLine(videoParameters.activeVideoStart, firstActiveScanLine, videoParameters.activeVideoEnd, firstActiveScanLine);
        imagePainter.drawLine(videoParameters.activeVideoStart, lastActiveScanLine, videoParameters.activeVideoEnd, lastActiveScanLine);

        // Outline the VP415 Domesday player active video area
        qreal vp415FirstActiveScanLine = firstActiveScanLine + (((videoParameters.fieldHeight * 2) / 100) * 1.0);
        qreal vp415LastActiveScanLine = lastActiveScanLine - (((videoParameters.fieldHeight * 2) / 100) * 1.0);
        qreal vp415VideoStart = videoParameters.activeVideoStart + ((videoParameters.fieldWidth / 100) * 1.0);
        qreal vp415VideoEnd = videoParameters.activeVideoEnd - ((videoParameters.fieldWidth / 100) * 1.0);
        imagePainter.drawLine(static_cast<qint32>(vp415VideoStart), static_cast<qint32>(vp415FirstActiveScanLine),
                              static_cast<qint32>(vp415VideoStart), static_cast<qint32>(vp415LastActiveScanLine));
        imagePainter.drawLine(static_cast<qint32>(vp415VideoEnd), static_cast<qint32>(vp415FirstActiveScanLine),
                              static_cast<qint32>(vp415VideoEnd), static_cast<qint32>(vp415LastActiveScanLine));

        imagePainter.drawLine(static_cast<qint32>(vp415VideoStart), static_cast<qint32>(vp415FirstActiveScanLine),
                              static_cast<qint32>(vp415VideoEnd), static_cast<qint32>(vp415FirstActiveScanLine));
        imagePainter.drawLine(static_cast<qint32>(vp415VideoStart), static_cast<qint32>(vp415LastActiveScanLine),
                              static_cast<qint32>(vp415VideoEnd), static_cast<qint32>(vp415LastActiveScanLine));

        // End the painter object
        imagePainter.end();
    }

    if (highlightDropOuts) {
        // Create a painter object
        QPainter imagePainter;
        imagePainter.begin(&frameImage);

        // Draw the drop out data for the top field
        imagePainter.setPen(Qt::red);
        for (qint32 dropOutIndex = 0; dropOutIndex < topField.dropOuts.startx.size(); dropOutIndex++) {
            qint32 startx = topField.dropOuts.startx[dropOutIndex];
            qint32 endx = topField.dropOuts.endx[dropOutIndex];
            qint32 fieldLine = topField.dropOuts.fieldLine[dropOutIndex];

            if (videoParameters.isSourcePal) imagePainter.drawLine(startx, ((fieldLine - 1) * 2), endx, ((fieldLine - 1) * 2));
            else imagePainter.drawLine(startx, ((fieldLine - 1) * 2) + 1, endx, ((fieldLine - 1) * 2) + 1);
        }

        // Draw the drop out data for the bottom field
        imagePainter.setPen(Qt::blue);
        for (qint32 dropOutIndex = 0; dropOutIndex < bottomField.dropOuts.startx.size(); dropOutIndex++) {
            qint32 startx = bottomField.dropOuts.startx[dropOutIndex];
            qint32 endx = bottomField.dropOuts.endx[dropOutIndex];
            qint32 fieldLine = bottomField.dropOuts.fieldLine[dropOutIndex];

            if (videoParameters.isSourcePal) imagePainter.drawLine(startx, ((fieldLine - 1) * 2) + 1, endx, ((fieldLine - 1) * 2) + 1);
            else imagePainter.drawLine(startx, ((fieldLine - 1) * 2), endx, ((fieldLine - 1) * 2));
        }

        // End the painter object
        imagePainter.end();
    }

    // Add the top field VBI data to the dialogue
    if (topField.vbi.inUse) {
        ui->even0VbiLabel->setText("0x" + QString::number(topField.vbi.vbi16, 16));
        ui->even1VbiLabel->setText("0x" + QString::number(topField.vbi.vbi17, 16));
        ui->even2VbiLabel->setText("0x" + QString::number(topField.vbi.vbi18, 16));
    } else {
        ui->even0VbiLabel->setText("Not present");
        ui->even1VbiLabel->setText("Not present");
        ui->even2VbiLabel->setText("Not present");
    }

    // Add the bottom field VBI data to the dialogue
    if (bottomField.vbi.inUse) {
        ui->odd0VbiLabel->setText("0x" + QString::number(bottomField.vbi.vbi16, 16));
        ui->odd1VbiLabel->setText("0x" + QString::number(bottomField.vbi.vbi17, 16));
        ui->odd2VbiLabel->setText("0x" + QString::number(bottomField.vbi.vbi18, 16));
    } else {
        ui->odd0VbiLabel->setText("Not present");
        ui->odd1VbiLabel->setText("Not present");
        ui->odd2VbiLabel->setText("Not present");
    }

    // Update the VBI dialogue
    vbiDialog->updateVbi(topField, bottomField);

    // Add the QImage to the QLabel in the dialogue
    ui->frameViewerLabel->clear();
    ui->frameViewerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->frameViewerLabel->setAlignment(Qt::AlignCenter);
    ui->frameViewerLabel->setMinimumSize(frameImage.width(), frameImage.height());
    ui->frameViewerLabel->setMaximumSize(frameImage.width(), frameImage.height());
    ui->frameViewerLabel->setScaledContents(false);
    ui->frameViewerLabel->setPixmap(QPixmap::fromImage(frameImage));

    // If the scope window is open, update it too (using the last scope line selected by the user)
    if (oscilloscopeDialog->isVisible()) {
        // Show the oscilloscope dialogue for the selected scan-line
        updateOscilloscopeDialogue(currentFrameNumber, lastScopeLine);
    }
}

// Method to create a QImage for a source video frame
QImage MainWindow::generateQImage(qint32 topFieldNumber, qint32 bottomFieldNumber)
{
    // Generate the QImage for the frame

    // Get the metadata for the video parameters
    LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();

    // Get the raw data for the fields
    QByteArray topFieldData = sourceVideo.getVideoField(topFieldNumber)->getFieldData();
    QByteArray bottomFieldData = sourceVideo.getVideoField(bottomFieldNumber)->getFieldData();

    // Calculate the frame height
    qint32 frameHeight = (videoParameters.fieldHeight * 2) - 1;

    qDebug() << "MainWindow::generateQImage(): Generating a QImage with topField =" << topFieldNumber <<
                "and bottomField =" << bottomFieldNumber << "(" << videoParameters.fieldWidth << "x" <<
                frameHeight << ")";

    // Create a QImage
    QImage frameImage = QImage(videoParameters.fieldWidth, frameHeight, QImage::Format_RGB888);

    // Copy the raw 16-bit grayscale data into the RGB888 QImage
    for (qint32 y = 0; y < frameHeight; y++) {
        // Extract the current scan line data from the frame
        qint32 startPointer = (y / 2) * videoParameters.fieldWidth * 2;
        qint32 length = videoParameters.fieldWidth * 2;

        QByteArray topLineData = topFieldData.mid(startPointer, length);
        QByteArray bottomLineData = bottomFieldData.mid(startPointer, length);

        for (qint32 x = 0; x < videoParameters.fieldWidth; x++) {
            // Take just the MSB of the input data
            qint32 dp = x * 2;
            uchar pixelValue;
            if (y % 2) {
                if (videoParameters.isSourcePal) pixelValue = static_cast<uchar>(bottomLineData[dp + 1]);
                else pixelValue = static_cast<uchar>(topLineData[dp + 1]);
            } else {
                if (videoParameters.isSourcePal) pixelValue = static_cast<uchar>(topLineData[dp + 1]);
                else pixelValue = static_cast<uchar>(bottomLineData[dp + 1]);
            }

            qint32 xpp = x * 3;
            *(frameImage.scanLine(y) + xpp + 0) = static_cast<uchar>(pixelValue); // R
            *(frameImage.scanLine(y) + xpp + 1) = static_cast<uchar>(pixelValue); // G
            *(frameImage.scanLine(y) + xpp + 2) = static_cast<uchar>(pixelValue); // B
        }
    }

    return frameImage;
}

// Method to hide the current frame
void MainWindow::hideFrame(void)
{
    ui->frameViewerLabel->clear();
}

void MainWindow::on_actionExit_triggered()
{
    qDebug() << "MainWindow::on_actionExit_triggered(): Called";
        // Quit the application
    qApp->quit();
}

// Load a TBC file based on the passed file name
void MainWindow::loadTbcFile(QString inputFileName)
{
    qInfo() << "Opening TBC filename =" << inputFileName;

    // Open the TBC metadata file
    if (!ldDecodeMetaData.read(inputFileName + ".json")) {
        // Open failed
        qWarning() << "Open TBC JSON metadata failed for filename" << inputFileName;

        // Show an error to the user
        QMessageBox messageBox;
        messageBox.critical(this, "Error","Could not open TBC JSON metadata file for the TBC input file!");
        messageBox.setFixedSize(500, 200);

        // Update the GUI
        updateGuiUnloaded();
    } else {
        // Opened meta data file, now open TBC source video file
        LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();
        sourceVideo.close();

        if (!sourceVideo.open(inputFileName, videoParameters.fieldWidth * videoParameters.fieldHeight)) {
            // Open failed
            qWarning() << "Open TBC file failed for filename" << inputFileName;

            // Show an error to the user
            QMessageBox messageBox;
            messageBox.critical(this, "Error","Could not open TBC video file!");
            messageBox.setFixedSize(500, 200);

            // Update the GUI
            updateGuiUnloaded();
        } else {
            // Both the video and metadata files are now open

            if (!videoParameters.isFieldOrderValid) {
                // Show a warning to the user
                QMessageBox messageBox;
                messageBox.warning(this, "Warning","TBC Metadata does not contain a valid field order... Frame rendering may be incorrect!");
                messageBox.setFixedSize(500, 200);
            }

            // Update the status bar
            QString statusText;
            if (videoParameters.isSourcePal) statusText += "PAL";
            else statusText += "NTSC";
            statusText += " source loaded with ";
            statusText += QString::number(getAvailableNumberOfFrames());
            statusText += " sequential frames available";
            sourceVideoStatus.setText(statusText);

            // Update the configuration for the source directory
            QFileInfo inFileInfo(inputFileName);
            configuration->setSourceDirectory(inFileInfo.absolutePath());
            qDebug() << "MainWindow::on_actionOpen_TBC_file_triggered(): Setting source directory to:" << inFileInfo.absolutePath();
            configuration->writeConfiguration();

            // Update the GUI
            updateGuiLoaded();

            // Set the window title
            this->setWindowTitle(tr("ld-analyse - ") + inputFileName);
        }
    }
}

// Load a TBC file based on the file selection from the GUI
void MainWindow::on_actionOpen_TBC_file_triggered()
{
    qDebug() << "MainWindow::on_actionOpen_PAL_file_triggered(): Called";

    QString inputFileName = QFileDialog::getOpenFileName(this,
                tr("Open TBC file"),
                configuration->getSourceDirectory()+tr("/ldsample.tbc"),
                tr("TBC output (*.tbc);;All Files (*)"));

    // Was a filename specified?
    if (!inputFileName.isEmpty() && !inputFileName.isNull()) {
        loadTbcFile(inputFileName);
    }
}

// Previous frame button has been clicked
void MainWindow::on_previousPushButton_clicked()
{
    currentFrameNumber--;
    if (currentFrameNumber < 1) {
        currentFrameNumber = 1;
    } else {
        ui->frameNumberSpinBox->setValue(currentFrameNumber);
        showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());
    }
}

// Next frame button has been clicked
void MainWindow::on_nextPushButton_clicked()
{
    currentFrameNumber++;
    if (currentFrameNumber > getAvailableNumberOfFrames()) {
        currentFrameNumber = getAvailableNumberOfFrames();
    } else {
        ui->frameNumberSpinBox->setValue(currentFrameNumber);
        showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());
    }
}

// Frame number spin box editing has finished
void MainWindow::on_frameNumberSpinBox_editingFinished()
{
    if (ui->frameNumberSpinBox->value() != currentFrameNumber) {
        if (ui->frameNumberSpinBox->value() < 1) ui->frameNumberSpinBox->setValue(1);
        if (ui->frameNumberSpinBox->value() > sourceVideo.getNumberOfAvailableFields()) ui->frameNumberSpinBox->setValue(getAvailableNumberOfFrames());
        currentFrameNumber = ui->frameNumberSpinBox->value();
        showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());
    }
}

void MainWindow::on_showActiveVideoCheckBox_clicked()
{
    showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());
}

void MainWindow::on_highlightDropOutsCheckBox_clicked()
{
    showFrame(currentFrameNumber, ui->showActiveVideoCheckBox->isChecked(), ui->highlightDropOutsCheckBox->isChecked());
}

void MainWindow::on_actionLine_scope_triggered()
{
    if (isFileOpen) {
        // Show the oscilloscope dialogue for the selected scan-line
        updateOscilloscopeDialogue(currentFrameNumber, lastScopeLine);
        oscilloscopeDialog->show();
    }
}

void MainWindow::on_actionAbout_ld_analyse_triggered()
{
    aboutDialog->show();
}


void MainWindow::on_actionVBI_triggered()
{
    // Show the VBI dialogue
    vbiDialog->show();
}

// Mouse press event handler
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // Get the mouse position relative to our scene
    QPoint origin = ui->frameViewerLabel->mapFromGlobal(QCursor::pos());

    // Get the metadata for the fields
    LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();

    // Calculate the frame height
    qint32 frameHeight = (videoParameters.fieldHeight * 2) - 1;

    // Check that the mouse click is within bounds of the current picture
    if (origin.x() + 1 >= 0 &&
            origin.y() >= 1 &&
            origin.x() + 1 <= videoParameters.fieldWidth &&
            origin.y() <= frameHeight) {

        qDebug() << "MainWindow::mousePressEvent():" << origin.x() << "x" << origin.y();

        if (isFileOpen) {
            // Show the oscilloscope dialogue for the selected scan-line
            updateOscilloscopeDialogue(currentFrameNumber, origin.y());
            oscilloscopeDialog->show();

            // Remember the last line rendered
            lastScopeLine = origin.y();
        }

        event->accept();
    }
}

void MainWindow::scanLineChangedSignalHandler(qint32 scanLine)
{
    qDebug() << "MainWindow::scanLineChangedSignalHandler(): Called with scanLine =" << scanLine;

    if (isFileOpen) {
        // Show the oscilloscope dialogue for the selected scan-line
        updateOscilloscopeDialogue(currentFrameNumber, scanLine);
        oscilloscopeDialog->show();

        // Remember the last line rendered
        lastScopeLine = scanLine;
    }
}

// Method to update the line oscilloscope based on the frame number and scan line
void MainWindow::updateOscilloscopeDialogue(qint32 frameNumber, qint32 scanLine)
{
    // Get the video parameter metadata
    LdDecodeMetaData::VideoParameters videoParameters = ldDecodeMetaData.getVideoParameters();

    // Determine the top and bottom fields for the frame number
    qint32 topFieldNumber = (frameNumber * 2) - 1;

    if (videoParameters.isFieldOrderEvenOdd) {
        // Top frame should be even, so if the current topField is odd, increment it by one
        if (!ldDecodeMetaData.getField(topFieldNumber).isEven) {
            topFieldNumber++;
            qDebug() << "MainWindow::updateOscilloscopeDialogue(): First field is out of frame order - ignoring";
        }
    } else {
        // Top frame should be odd, so if the current topField is even, increment it by one
        if (ldDecodeMetaData.getField(topFieldNumber).isEven) {
            topFieldNumber++;
            qDebug() << "MainWindow::updateOscilloscopeDialogue(): First field is out of frame order - ignoring";
        }
    }

    // Range check the bottom field number
    if (topFieldNumber + 1 > sourceVideo.getNumberOfAvailableFields()) {
        qDebug() << "MainWindow::updateOscilloscopeDialogue(): Bottom field number exceed the available number of fields!";
        return;
    }

    // Update the oscilloscope dialogue
    oscilloscopeDialog->showTraceImage(sourceVideo.getVideoField(topFieldNumber)->getFieldData(),
                                       sourceVideo.getVideoField(topFieldNumber + 1)->getFieldData(),
                                       videoParameters, scanLine);
}
