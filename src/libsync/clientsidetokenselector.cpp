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

#define OPENSSL_SUPPRESS_DEPRECATED

#include "clientsidetokenselector.h"

#include "account.h"

#include <QLoggingCategory>

#include <libp11.h>

namespace OCC
{

Q_LOGGING_CATEGORY(lcCseSelector, "nextcloud.sync.clientsideencryption.selector", QtInfoMsg)

ClientSideTokenSelector::ClientSideTokenSelector(QObject *parent)
    : QObject{parent}
{

}

bool ClientSideTokenSelector::isSetup() const
{
    return false;
}

QVariantList ClientSideTokenSelector::discoveredTokens() const
{
    return _discoveredTokens;
}

QVariantList ClientSideTokenSelector::discoveredKeys() const
{
    return _discoveredPrivateKeys;
}

QString ClientSideTokenSelector::slotManufacturer() const
{
    return _slotManufacturer;
}

QString ClientSideTokenSelector::tokenManufacturer() const
{
    return _tokenManufacturer;
}

QString ClientSideTokenSelector::tokenModel() const
{
    return _tokenModel;
}

QString ClientSideTokenSelector::tokenSerialNumber() const
{
    return _tokenSerialNumber;
}

int ClientSideTokenSelector::keyIndex() const
{
    return _keyIndex;
}

void ClientSideTokenSelector::searchForToken(const AccountPtr &account)
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

    _discoveredTokens.clear();
    _discoveredPrivateKeys.clear();
    auto currentSlot = static_cast<PKCS11_SLOT*>(nullptr);
    for(auto i = 0u; i < tokensCount; ++i) {
        currentSlot = PKCS11_find_next_token(ctx, tokenSlots, tokensCount, currentSlot);
        if (currentSlot == nullptr || currentSlot->token == nullptr) {
            break;
        }

        qCInfo(lcCseSelector()) << "Slot manufacturer......:" << currentSlot->manufacturer;
        qCInfo(lcCseSelector()) << "Slot description.......:" << currentSlot->description;
        qCInfo(lcCseSelector()) << "Slot token label.......:" << currentSlot->token->label;
        qCInfo(lcCseSelector()) << "Slot token manufacturer:" << currentSlot->token->manufacturer;
        qCInfo(lcCseSelector()) << "Slot token model.......:" << currentSlot->token->model;
        qCInfo(lcCseSelector()) << "Slot token serialnr....:" << currentSlot->token->serialnr;

        _discoveredTokens.push_back(QVariantMap{
                                                {QStringLiteral("slotManufacturer"), QString::fromLatin1(currentSlot->manufacturer)},
                                                {QStringLiteral("slotDescription"), QString::fromLatin1(currentSlot->description)},
                                                {QStringLiteral("tokenLabel"), QString::fromLatin1(currentSlot->token->label)},
                                                {QStringLiteral("tokenManufacturer"), QString::fromLatin1(currentSlot->token->manufacturer)},
                                                {QStringLiteral("tokenModel"), QString::fromLatin1(currentSlot->token->model)},
                                                {QStringLiteral("tokenSerialNumber"), QString::fromLatin1(currentSlot->token->serialnr)},
                                                });

        auto keysCount = 0u;
        auto tokenKeys = static_cast<PKCS11_KEY*>(nullptr);
        if (PKCS11_enumerate_public_keys(currentSlot->token, &tokenKeys, &keysCount)) {
            qCWarning(lcCseSelector()) << "PKCS11_enumerate_public_keys failed" << ERR_reason_error_string(ERR_get_error());

            Q_EMIT failedToInitialize(account);
            return;
        }

        for (auto keyIndex = 0u; keyIndex < keysCount; ++keyIndex) {
            auto currentPrivateKey = &tokenKeys[0];
            qCInfo(lcCseSelector()) << "key metadata:"
                            << "type:" << (currentPrivateKey->isPrivate ? "is private" : "is public")
                            << "label:" << currentPrivateKey->label
                            << "need login:" << (currentPrivateKey->needLogin ? "true" : "false");

            _discoveredPrivateKeys.push_back(QVariantMap{
                                                         {QStringLiteral("label"), QString::fromLatin1(currentPrivateKey->label)},
                                                         {QStringLiteral("needLogin"), QVariant::fromValue(currentPrivateKey->needLogin)},
                                                         });
        }
    }
    Q_EMIT discoveredTokensChanged();
    Q_EMIT discoveredKeysChanged();
}

void ClientSideTokenSelector::setSlotManufacturer(const QString &slotManufacturer)
{
    if (_slotManufacturer == slotManufacturer) {
        return;
    }

    _slotManufacturer = slotManufacturer;
    Q_EMIT slotManufacturerChanged();
}

void ClientSideTokenSelector::setTokenManufacturer(const QString &tokenManufacturer)
{
    if (_tokenManufacturer == tokenManufacturer) {
        return;
    }

    _tokenManufacturer = tokenManufacturer;
    Q_EMIT tokenManufacturerChanged();
}

void ClientSideTokenSelector::setTokenModel(const QString &tokenModel)
{
    if (_tokenModel == tokenModel) {
        return;
    }

    _tokenModel = tokenModel;
    Q_EMIT tokenModelChanged();
}

void ClientSideTokenSelector::setTokenSerialNumber(const QString &tokenSerialNumber)
{
    if (_tokenSerialNumber == tokenSerialNumber) {
        return;
    }

    _tokenSerialNumber = tokenSerialNumber;
    Q_EMIT tokenSerialNumberChanged();
}

void ClientSideTokenSelector::setKeyIndex(int keyIndex)
{
    if (_keyIndex == keyIndex) {
        return;
    }

    _keyIndex = keyIndex;
    Q_EMIT keyIndexChanged();
}

}
