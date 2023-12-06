/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include "clientstatusreporting.h"

#include "account.h"
#include "clientstatusreportingdatabase.h"
#include "clientstatusreportingnetwork.h"
#include "clientstatusreportingrecord.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReporting, "nextcloud.sync.clientstatusreporting", QtInfoMsg)

ClientStatusReporting::ClientStatusReporting(Account *account)
    : _account(account)
{
    for (int i = 0; i < ClientStatusReportingStatus::Count; ++i) {
        const auto statusString = clientStatusstatusStringFromNumber(static_cast<ClientStatusReportingStatus>(i));
        _statusStrings[i] = statusString;
    }

    _database = QSharedPointer<ClientStatusReportingDatabase>::create(account);
    _reporter = std::make_unique<ClientStatusReportingNetwork>(account, _database);

    _isInitialized = _database->isInitialized() && _reporter->isInitialized();
}

ClientStatusReporting::~ClientStatusReporting()
{
    // the sole purpose of this desrtuctor is to make unique_ptr work with forward declaration, but let's clearn the initialized flag too
    _isInitialized = false;
}

void ClientStatusReporting::reportClientStatus(const ClientStatusReportingStatus status) const
{
    if (!_isInitialized) {
        return;
    }

    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= ClientStatusReportingStatus::Count) {
        qCDebug(lcClientStatusReporting) << "Trying to report invalid status:" << status;
        return;
    }

    ClientStatusReportingRecord record;
    record._name = _statusStrings[status];
    record._status = status;
    record._lastOccurence = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const auto result = _database->setClientStatusReportingRecord(record);
    if (!result.isValid()) {
        qCDebug(lcClientStatusReporting) << "Could not report client status:" << result.error();
    }
}
}
