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

#include <openssl/pem.h>
#define OPENSSL_SUPPRESS_DEPRECATED

#include "clientsideencryptiontokenselector.h"

#include "account.h"

#include <QLoggingCategory>

#include <libp11.h>

namespace {

class Bio {
public:
    Bio()
        : _bio(BIO_new(BIO_s_mem()))
    {
    }

    ~Bio()
    {
        BIO_free_all(_bio);
    }

    operator const BIO*() const
    {
        return _bio;
    }

    operator BIO*()
    {
        return _bio;
    }

private:
    Q_DISABLE_COPY(Bio)

    BIO* _bio;
};

static unsigned char* unsignedData(QByteArray& array)
{
    return (unsigned char*)array.data();
}

static QByteArray BIO2ByteArray(Bio &b) {
    auto pending = static_cast<int>(BIO_ctrl_pending(b));
    QByteArray res(pending, '\0');
    BIO_read(b, unsignedData(res), pending);
    return res;
}

}

namespace OCC
{

Q_LOGGING_CATEGORY(lcCseSelector, "nextcloud.sync.clientsideencryption.selector", QtInfoMsg)

ClientSideEncryptionTokenSelector::ClientSideEncryptionTokenSelector(QObject *parent)
    : QObject{parent}
{

}

bool ClientSideEncryptionTokenSelector::isSetup() const
{
    return !_issuer.isEmpty() && !_serialNumber.isEmpty();
}

QVariantList ClientSideEncryptionTokenSelector::discoveredCertificates() const
{
    return _discoveredCertificates;
}

QString ClientSideEncryptionTokenSelector::serialNumber() const
{
    return _serialNumber;
}

QString ClientSideEncryptionTokenSelector::issuer() const
{
    return _issuer;
}

void ClientSideEncryptionTokenSelector::searchForCertificates(const AccountPtr &account)
{
    auto ctx = PKCS11_CTX_new();

    auto rc = PKCS11_CTX_load(ctx, account->encryptionHardwareTokenDriverPath().toLatin1().constData());
    if (rc) {
        qCWarning(lcCseSelector()) << "loading pkcs11 engine failed:" << ERR_reason_error_string(ERR_get_error());

        Q_EMIT failedToInitialize(account);
        return;
    }

    auto tokensCount = 0u;
    PKCS11_SLOT *tokenSlots = nullptr;
    /* get information on all slots */
    if (PKCS11_enumerate_slots(ctx, &tokenSlots, &tokensCount) < 0) {
        qCWarning(lcCseSelector()) << "no slots available" << ERR_reason_error_string(ERR_get_error());

        Q_EMIT failedToInitialize(account);
        return;
    }

    _discoveredCertificates.clear();
    auto currentSlot = static_cast<PKCS11_SLOT*>(nullptr);
    for(auto i = 0u; i < tokensCount; ++i) {
        currentSlot = PKCS11_find_next_token(ctx, tokenSlots, tokensCount, currentSlot);
        if (currentSlot == nullptr || currentSlot->token == nullptr) {
            break;
        }

        qCDebug(lcCseSelector()) << "Slot manufacturer......:" << currentSlot->manufacturer;
        qCDebug(lcCseSelector()) << "Slot description.......:" << currentSlot->description;
        qCDebug(lcCseSelector()) << "Slot token label.......:" << currentSlot->token->label;
        qCDebug(lcCseSelector()) << "Slot token manufacturer:" << currentSlot->token->manufacturer;
        qCDebug(lcCseSelector()) << "Slot token model.......:" << currentSlot->token->model;
        qCDebug(lcCseSelector()) << "Slot token serialnr....:" << currentSlot->token->serialnr;

        auto keysCount = 0u;
        auto certificatesFromToken = static_cast<PKCS11_CERT*>(nullptr);
        if (PKCS11_enumerate_certs(currentSlot->token, &certificatesFromToken, &keysCount)) {
            qCWarning(lcCseSelector()) << "PKCS11_enumerate_certs failed" << ERR_reason_error_string(ERR_get_error());

            Q_EMIT failedToInitialize(account);
            return;
        }

        for (auto certificateIndex = 0u; certificateIndex < keysCount; ++certificateIndex) {
            const auto currentCertificate = &certificatesFromToken[certificateIndex];
            qCInfo(lcCseSelector()) << "certificate metadata:"
                                    << "label:" << currentCertificate->label;

            const auto certificateId = QByteArray{reinterpret_cast<char*>(currentCertificate->id), static_cast<int>(currentCertificate->id_len)};
            qCInfo(lcCseSelector()) << "new certificate ID:" << certificateId.toBase64();

            const auto certificateSubjectName = X509_get_subject_name(currentCertificate->x509);
            if (!certificateSubjectName) {
                qCWarning(lcCseSelector()) << "X509_get_subject_name failed" << ERR_reason_error_string(ERR_get_error());

                Q_EMIT failedToInitialize(account);
                return;
            }

            Bio out;
            const auto ret = PEM_write_bio_X509(out, currentCertificate->x509);
            if (ret <= 0){
                qCWarning(lcCseSelector()) << "PEM_write_bio_X509 failed" << ERR_reason_error_string(ERR_get_error());

                Q_EMIT failedToInitialize(account);
                return;
            }

            const auto result = BIO2ByteArray(out);
            const auto sslCertificate = QSslCertificate{result, QSsl::Pem};

            qCInfo(lcCseSelector()) << "newly found certificate"
                                    << "subject:" << sslCertificate.subjectDisplayName()
                                    << "issuer:" << sslCertificate.issuerDisplayName()
                                    << "valid since:" << sslCertificate.effectiveDate()
                                    << "valid until:" << sslCertificate.expiryDate()
                                    << "serial number:" << sslCertificate.serialNumber();

            if (sslCertificate.isSelfSigned()) {
                qCInfo(lcCseSelector()) << "newly found certificate is self signed: goint to ignore it";
                continue;
            }

            _discoveredCertificates.push_back(QVariantMap{
                                                          {QStringLiteral("label"), QString::fromLatin1(currentCertificate->label)},
                                                          {QStringLiteral("subject"), sslCertificate.subjectDisplayName()},
                                                          {QStringLiteral("issuer"), sslCertificate.issuerDisplayName()},
                                                          {QStringLiteral("serialNumber"), sslCertificate.serialNumber()},
                                                          {QStringLiteral("validSince"), sslCertificate.effectiveDate()},
                                                          {QStringLiteral("validUntil"), sslCertificate.expiryDate()},
                                                          {QStringLiteral("certificate"), QVariant::fromValue(sslCertificate)},
                                                          });
        }
    }

    Q_EMIT discoveredCertificatesChanged();
    processDiscoveredCertificates();
}

void ClientSideEncryptionTokenSelector::setSerialNumber(const QString &serialNumber)
{
    if (_serialNumber == serialNumber) {
        return;
    }

    _serialNumber = serialNumber;
    Q_EMIT serialNumberChanged();
}

void ClientSideEncryptionTokenSelector::setIssuer(const QString &issuer)
{
    if (_issuer == issuer) {
        return;
    }

    _issuer = issuer;
    Q_EMIT issuerChanged();
}

void ClientSideEncryptionTokenSelector::processDiscoveredCertificates()
{
    for (const auto &oneCertificate : discoveredCertificates()) {
        const auto certificateData = oneCertificate.toMap();
        const auto sslCertificate = certificateData[QStringLiteral("certificate")].value<QSslCertificate>();
        if (sslCertificate.isNull()) {
            qCDebug(lcCseSelector()) << "null certificate";
            continue;
        }
        const auto sslErrors = QSslCertificate::verify({sslCertificate});
        for (const auto &oneError : sslErrors) {
            qCInfo(lcCseSelector()) << oneError;
        }
        qCInfo(lcCseSelector()) << "certificate is valid" << certificateData[QStringLiteral("serialNumber")] << certificateData[QStringLiteral("issuer")];

        setIssuer(certificateData[QStringLiteral("issuer")].toString());
        setSerialNumber(certificateData[QStringLiteral("serialNumber")].toString());
        Q_EMIT isSetupChanged();
        return;
    }
}

}
