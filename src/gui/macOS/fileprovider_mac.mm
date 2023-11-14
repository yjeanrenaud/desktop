/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#include "fileprovider.h"

#include <QLoggingCategory>

#include "libsync/configfile.h"
#include "gui/macOS/fileprovidersocketcontroller.h"

#import <Foundation/Foundation.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProvider, "nextcloud.gui.macfileprovider", QtInfoMsg)

namespace Mac {

FileProvider* FileProvider::_instance = nullptr;

FileProvider::FileProvider(QObject * const parent)
    : QObject(parent)
{
    Q_ASSERT(!_instance);

    if (!fileProviderAvailable()) {
        qCInfo(lcMacFileProvider) << "File provider system is not available on this version of macOS.";
        deleteLater();
        return;
    }

    qCInfo(lcMacFileProvider) << "Initialising file provider domain manager.";
    _domainManager = std::make_unique<FileProviderDomainManager>(new FileProviderDomainManager(this));

    if (_domainManager) {
        qCDebug(lcMacFileProvider()) << "Initialized file provider domain manager";
    }

    qCDebug(lcMacFileProvider) << "Initialising file provider socket server.";
    _socketServer = std::make_unique<FileProviderSocketServer>(new FileProviderSocketServer(this));

    if (_socketServer) {
        qCDebug(lcMacFileProvider) << "Initialised file provider socket server.";
    }
}

FileProvider *FileProvider::instance()
{
    if (!fileProviderAvailable()) {
        qCInfo(lcMacFileProvider) << "File provider system is not available on this version of macOS.";
        return nullptr;
    }

    if (!_instance) {
        _instance = new FileProvider();
    }
    return _instance;
}

FileProvider::~FileProvider()
{
    _instance = nullptr;
}

bool FileProvider::fileProviderAvailable()
{
    if (@available(macOS 11.0, *)) {
        return true;
    }

    return false;
}

void FileProvider::sendMessageToDomain(const QString &domainIdentifier, const QString &message)
{
    const auto domainSocketController = _socketServer->socketControllerForDomain(domainIdentifier);
    if (!domainSocketController) {
        qCWarning(lcMacFileProvider) << "Could not find socket controller for domain identifier" << domainIdentifier;
        return;
    }

    domainSocketController->sendMessage(message);
}

} // namespace Mac
} // namespace OCC
