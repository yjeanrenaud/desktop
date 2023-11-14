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

#include "fileprovidersocketserver.h"

#include <QLocalSocket>
#include <QLoggingCategory>

#include "gui/macOS/fileproviderdomainmanager.h"
#include "gui/macOS/fileprovidersocketcontroller.h"

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSocketServer, "nextcloud.gui.macos.fileprovider.socketserver", QtInfoMsg)

FileProviderSocketServer::FileProviderSocketServer(QObject *parent)
    : QObject{parent}
{
    _socketPath = fileProviderSocketPath();
    startListening();
}

FileProviderSocketControllerPtr FileProviderSocketServer::socketControllerForDomain(const QString &domainIdentifier) const
{
    const auto controllerIt = std::find_if(_socketControllers.cbegin(),
                                           _socketControllers.cend(),
                                           [domainIdentifier](const auto &socketController) {
                                               const auto socketAccountState = socketController->accountState();
                                               const auto socketControllerDomainId = FileProviderDomainManager::fileProviderDomainIdentifierFromAccountState(socketAccountState);
                                               return socketControllerDomainId == domainIdentifier;
                                           });

    if (controllerIt == _socketControllers.cend()) {
        return nullptr;
    }

    return controllerIt.value();
}

void FileProviderSocketServer::startListening()
{
    QLocalServer::removeServer(_socketPath);

    const auto serverStarted = _socketServer.listen(_socketPath);
    if (!serverStarted) {
        qCWarning(lcFileProviderSocketServer) << "Could not start file provider socket server"
                                              << _socketPath;
    } else {
        qCInfo(lcFileProviderSocketServer) << "File provider socket server started, listening"
                                           << _socketPath;
    }

    connect(&_socketServer, &QLocalServer::newConnection,
            this, &FileProviderSocketServer::slotNewConnection);
}

void FileProviderSocketServer::slotNewConnection()
{
    if (!_socketServer.hasPendingConnections()) {
        return;
    }

    qCInfo(lcFileProviderSocketServer) << "New connection in file provider socket server";
    const auto socket = _socketServer.nextPendingConnection();
    if (!socket) {
        return;
    }

    const FileProviderSocketControllerPtr socketController(new FileProviderSocketController(socket, this));
    connect(socketController.data(), &FileProviderSocketController::socketDestroyed,
            this, &FileProviderSocketServer::slotSocketDestroyed);
    _socketControllers.insert(socket, socketController);

    socketController->start();
}

void FileProviderSocketServer::slotSocketDestroyed(const QLocalSocket * const socket)
{
    const auto socketController = _socketControllers.take(socket);

    if (socketController) {
        const auto rawSocketControllerPtr = socketController.data();
        delete rawSocketControllerPtr;
    }
}

} // namespace Mac

} // namespace OCC
