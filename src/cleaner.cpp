/****************************************************************************
**
** SVG Cleaner could help you to clean up your SVG files
** from unnecessary data.
** Copyright (C) 2012-2018 Evgeniy Reizner
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**
****************************************************************************/

#include <QDir>

#include "utils.h"
#include "cleaner.h"
#include "process.h"

Task::Output Task::cleanFile(const Task::Config &config)
{
    Q_ASSERT(config.inputPath.isEmpty() == false);
    Q_ASSERT(config.outputPath.isEmpty() == false);
    Q_ASSERT(config.treeItem != nullptr);

    // We do not rethrow exception to the main thread,
    // because we depend on TreeItem pointer.

    try {
        return _cleanFile(config);
    } catch (const QString &s) {
        return Output::error(s, config.treeItem);
    }
}

Task::Output Task::_cleanFile(const Task::Config &config)
{
    // TODO: create dir structure before running threads
    const QString outFolder = QFileInfo(config.outputPath).absolutePath();
    if (!QFileInfo().exists(outFolder)) {
        const bool flag = QDir().mkpath(outFolder);
        if (!flag) {
            throw tr("Failed to create an output folder:\n'%1'.").arg(outFolder);
        }
    }

    // take before cleaning in case of an overwrite mode
    const auto inSize = QFile(config.inputPath).size();

    // unzip svgz
    QString inputFile = config.inputPath;
    const QString inSuffix = QFileInfo(config.inputPath).suffix().toLower();
    const bool isInputFileCompressed = inSuffix == "svgz";
    if (isInputFileCompressed) {
        inputFile = config.outputPath;
        Compressor::unzip(config.inputPath, inputFile);
    }

    // clean file
    QStringList args;
    args.reserve(config.args.size() + 3);
    args << config.args << "--quiet" << inputFile << config.outputPath;

    // TODO: make timeout optional
    QString cleanerMsg = Process::run(Cleaner::Name, args, 300000, true);
    cleanerMsg = cleanerMsg.trimmed();

    // process output
    if (cleanerMsg.contains("Error:")) {
        // NOTE: have to keep it in sync with CLI
        if (isInputFileCompressed) {
            // remove decompressed file
            QFile().remove(inputFile);
        }

        return Output::error(cleanerMsg, config.treeItem);
    }

    // compress file
    QString outPath = config.outputPath;

    bool shouldCompress = false;
    if (config.compressorType != Compressor::None) {
        // compressor is set
        if (config.compressOnlySvgz) {
            // check that input file was SVGZ
            if (isInputFileCompressed) {
                shouldCompress = true;
            }
        } else {
            shouldCompress = true;
        }
    }

    if (shouldCompress) {
        outPath += "z";
        Compressor(config.compressorType).zip(config.compressionLevel,
                                              config.outputPath, outPath);
    }

    Output::OkData okData;
    okData.outSize = QFile(outPath).size();
    okData.ratio = Utils::cleanerRatio(inSize, okData.outSize);
    okData.outputPath = outPath;

    if (cleanerMsg.contains("Warning:")) {
        return Output::warning(okData, cleanerMsg, config.treeItem);
    }

    return Output::ok(okData, config.treeItem);
}
