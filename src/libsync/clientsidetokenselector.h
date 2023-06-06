/*
 * Copyright © 2023, Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#ifndef CLIENTSIDETOKENSELECTOR_H
#define CLIENTSIDETOKENSELECTOR_H

#include "accountfwd.h"

#include <QObject>

namespace OCC
{

class ClientSideTokenSelector : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isSetup READ isSetup NOTIFY isSetupChanged)

    Q_PROPERTY(QVariantList discoveredTokens READ discoveredTokens NOTIFY discoveredTokensChanged)

    Q_PROPERTY(QVariantList discoveredKeys READ discoveredKeys NOTIFY discoveredKeysChanged)

    Q_PROPERTY(QString slotManufacturer READ slotManufacturer WRITE setSlotManufacturer NOTIFY slotManufacturerChanged)

    Q_PROPERTY(QString tokenManufacturer READ tokenManufacturer WRITE setTokenManufacturer NOTIFY tokenManufacturerChanged)

    Q_PROPERTY(QString tokenModel READ tokenModel WRITE setTokenModel NOTIFY tokenModelChanged)

    Q_PROPERTY(QString tokenSerialNumber READ tokenSerialNumber WRITE setTokenSerialNumber NOTIFY tokenSerialNumberChanged)

    Q_PROPERTY(int keyIndex READ keyIndex WRITE setKeyIndex NOTIFY keyIndexChanged)

public:
    explicit ClientSideTokenSelector(QObject *parent = nullptr);

    [[nodiscard]] bool isSetup() const;

    [[nodiscard]] QVariantList discoveredTokens() const;

    [[nodiscard]] QVariantList discoveredKeys() const;

    [[nodiscard]] QString slotManufacturer() const;

    [[nodiscard]] QString tokenManufacturer() const;

    [[nodiscard]] QString tokenModel() const;

    [[nodiscard]] QString tokenSerialNumber() const;

    [[nodiscard]] int keyIndex() const;

public slots:

    void searchForToken(const OCC::AccountPtr &account);

    void setSlotManufacturer(const QString &slotManufacturer);

    void setTokenManufacturer(const QString &tokenManufacturer);

    void setTokenModel(const QString &tokenModel);

    void setTokenSerialNumber(const QString &tokenSerialNumber);

    void setKeyIndex(int keyIndex);

signals:

    void isSetupChanged();

    void discoveredTokensChanged();

    void discoveredKeysChanged();

    void slotManufacturerChanged();

    void tokenManufacturerChanged();

    void tokenModelChanged();

    void tokenSerialNumberChanged();

    void keyIndexChanged();

    void failedToInitialize(const OCC::AccountPtr &account);

private:

    QVariantList _discoveredTokens;

    QVariantList _discoveredPrivateKeys;

    QString _slotManufacturer;

    QString _tokenManufacturer;

    QString _tokenModel;

    QString _tokenSerialNumber;

    int _keyIndex = -1;
};

}

#endif // CLIENTSIDETOKENSELECTOR_H
