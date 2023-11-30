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
#include "owncloudlib.h"

#include <QObject>

namespace OCC
{

class OWNCLOUDSYNC_EXPORT ClientSideEncryptionTokenSelector : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isSetup READ isSetup NOTIFY isSetupChanged)

    Q_PROPERTY(QVariantList discoveredCertificates READ discoveredCertificates NOTIFY discoveredCertificatesChanged)

    Q_PROPERTY(QString serialNumber READ serialNumber WRITE setSerialNumber NOTIFY serialNumberChanged)

    Q_PROPERTY(QString issuer READ issuer WRITE setIssuer NOTIFY issuerChanged)

public:
    explicit ClientSideEncryptionTokenSelector(QObject *parent = nullptr);

    [[nodiscard]] bool isSetup() const;

    [[nodiscard]] QVariantList discoveredCertificates() const;

    [[nodiscard]] QString serialNumber() const;

    [[nodiscard]] QString issuer() const;

public slots:
    void searchForCertificates(const OCC::AccountPtr &account);

    void setSerialNumber(const QString &serialNumber);

    void setIssuer(const QString &issuer);

signals:

    void isSetupChanged();

    void discoveredCertificatesChanged();

    void certificateIndexChanged();

    void serialNumberChanged();

    void issuerChanged();

    void failedToInitialize(const OCC::AccountPtr &account);

private:
    void processDiscoveredCertificates();

    QVariantList _discoveredCertificates;

    QString _serialNumber;

    QString _issuer;
};

}

#endif // CLIENTSIDETOKENSELECTOR_H
