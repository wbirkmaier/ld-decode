/************************************************************************

    main.cpp

    ld-process-cav - CAV disc processing for ld-decode
    Copyright (C) 2018 Simon Inns

    This file is part of ld-decode-tools.

    ld-process-cav is free software: you can redistribute it and/or
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

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>
#include <QCommandLineParser>

#include "processcav.h"

// Global for debug output
static bool showDebug = false;

// Qt debug message handler
void debugOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Use:
    // context.file - to show the filename
    // context.line - to show the line number
    // context.function - to show the function name

    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg: // These are debug messages meant for developers
        if (showDebug) {
            // If the code was compiled as 'release' the context.file will be NULL
            if (context.file != nullptr) fprintf(stderr, "Debug: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
            else fprintf(stderr, "Debug: %s\n", localMsg.constData());
        }
        break;
    case QtInfoMsg: // These are information messages meant for end-users
        if (context.file != nullptr) fprintf(stderr, "Info: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Info: %s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        if (context.file != nullptr) fprintf(stderr, "Warning: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        if (context.file != nullptr) fprintf(stderr, "Critical: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        if (context.file != nullptr) fprintf(stderr, "Fatal: [%s:%d] %s\n", context.file, context.line, localMsg.constData());
        else fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        abort();
    }
}

int main(int argc, char *argv[])
{
    // Install the local debug message handler
    qInstallMessageHandler(debugOutputHandler);

    QCoreApplication a(argc, argv);

    // Set application name and version
    QCoreApplication::setApplicationName("ld-process-cav");
    QCoreApplication::setApplicationVersion("1.0");
    QCoreApplication::setOrganizationDomain("domesday86.com");

    // Set up the command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
                "ld-process-cav - CAV disc processing for ld-decode\n"
                "\n"
                "(c)2018 Simon Inns\n"
                "GPLv3 Open-Source - github: https://github.com/happycube/ld-decode");
    parser.addHelpOption();
    parser.addVersionOption();

    // Option to show debug (-d)
    QCommandLineOption showDebugOption(QStringList() << "d" << "debug",
                                       QCoreApplication::translate("main", "Show debug"));
    parser.addOption(showDebugOption);

    // Option to select initial frame number (-i)
    QCommandLineOption initialFrameOption(QStringList() << "i" << "initialFrame",
                                        QCoreApplication::translate("main", "Specify the initial frame number of the input TBC"),
                                        QCoreApplication::translate("main", "number"));
    parser.addOption(initialFrameOption);

    // Positional argument to specify input video file
    parser.addPositionalArgument("input", QCoreApplication::translate("main", "Specify input TBC file"));

    // Positional argument to specify output video file
    parser.addPositionalArgument("output", QCoreApplication::translate("main", "Specify output TBC file (omit for processing only)"));

    // Process the command line options and arguments given by the user
    parser.process(a);

    // Get the options from the parser
    bool isDebugOn = parser.isSet(showDebugOption);

    // Get the arguments from the parser
    QString inputFileName;
    QString outputFileName;
    QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.count() == 1) {
        inputFileName = positionalArguments.at(0);
        outputFileName = "";
    } else if (positionalArguments.count() == 2) {
        inputFileName = positionalArguments.at(0);
        outputFileName = positionalArguments.at(1);
    } else {
        // Quit with error
        qCritical("You must specify an input TBC file");
        return -1;
    }

    if (inputFileName == outputFileName) {
        // Quit with error
        qCritical("Input and output files cannot be the same");
        return -1;
    }

    qint32 initialFrame = -1;
    if (parser.isSet(initialFrameOption)) {
        initialFrame = parser.value(initialFrameOption).toInt();

        if (initialFrame < 1) {
            // Quit with error
            qCritical("Specified initial frame number must be at least 1");
            return -1;
        }
    }

    // Process the command line options
    if (isDebugOn) showDebug = true;

    // Perform the processing
    ProcessCav processCav;
    processCav.process(inputFileName, outputFileName, initialFrame); // -1 = work out the first frame number automatically

    // Quit with success
    return 0;
}