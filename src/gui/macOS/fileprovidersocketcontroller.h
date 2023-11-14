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

#pragma once

#include <QPointer>

#include "accountstate.h"

class QLocalSocket;

namespace OCC {

namespace Mac {

class FileProviderSocketController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AccountStatePtr accountState READ accountState NOTIFY accountStateChanged)

public:
    explicit FileProviderSocketController(QLocalSocket * const socket, QObject * const parent = nullptr);

    AccountStatePtr accountState() const;

signals:
    void socketDestroyed(const QLocalSocket * const socket);
    void accountStateChanged();

public slots:
    void sendMessage(const QString &message) const;
    void start();

private slots:
    void slotOnDisconnected();
    void slotSocketDestroyed(const QObject * const object);
    void slotReadyRead();

    void slotAccountStateChanged(const OCC::AccountState::State state);

    void parseReceivedLine(const QString &receivedLine);
    void requestFileProviderDomainInfo() const;
    void sendAccountDetails() const;
    void sendNotAuthenticated() const;

private:
    QPointer<QLocalSocket> _socket;
    AccountStatePtr _accountState;
};

} // namespace Mac

} // namespace OCC
