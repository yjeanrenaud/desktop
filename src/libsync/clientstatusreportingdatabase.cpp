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
#include "clientstatusreportingdatabase.h"

#include "account.h"
#include <configfile.h>

namespace
{
constexpr auto lastSentReportTimestamp = "lastClientStatusReportSentTime";
constexpr auto statusNamesHash = "statusNamesHash";
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcClientStatusReportingDatabase, "nextcloud.sync.clientstatusreportingdatabase", QtInfoMsg)

ClientStatusReportingDatabase::ClientStatusReportingDatabase(const Account *account)
{
    const auto dbPath = makeDbPath(account);
    _database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    _database.setDatabaseName(dbPath);

    if (!_database.open()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client reporting, database connection error.";
        return;
    }

    QSqlQuery query;
    const auto prepareResult =
        query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS clientstatusreporting("
                                     "name VARCHAR(4096) PRIMARY KEY,"
                                     "status INTEGER(8),"
                                     "count INTEGER,"
                                     "lastOccurrence INTEGER(8))"));
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client clientstatusreporting table:" << query.lastError().text();
        return;
    }

    if (!query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS keyvalue(key VARCHAR(4096), value VARCHAR(4096), PRIMARY KEY(key))")) || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not setup client keyvalue table:" << query.lastError().text();
        return;
    }

    updateStatusNamesHash();

    _isInitialized = true;
}

ClientStatusReportingDatabase::~ClientStatusReportingDatabase()
{
    if (_database.isOpen()) {
        _database.close();
    }
}

QVector<ClientStatusReportingRecord> ClientStatusReportingDatabase::getClientStatusReportingRecords() const
{
    QVector<ClientStatusReportingRecord> records;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;
    if (!query.prepare(QStringLiteral("SELECT * FROM clientstatusreporting")) || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get records from clientstatusreporting:" << query.lastError().text();
        return records;
    }

    while (query.next()) {
        ClientStatusReportingRecord record;
        record._status = query.value(query.record().indexOf(QStringLiteral("status"))).toLongLong();
        record._name = query.value(query.record().indexOf(QStringLiteral("name"))).toByteArray();
        record._numOccurences = query.value(query.record().indexOf(QStringLiteral("count"))).toLongLong();
        record._lastOccurence = query.value(query.record().indexOf(QStringLiteral("lastOccurrence"))).toLongLong();
        records.push_back(record);
    }
    return records;
}

void ClientStatusReportingDatabase::deleteClientStatusReportingRecords() const
{
    QSqlQuery query;
    if (!query.prepare(QStringLiteral("DELETE FROM clientstatusreporting")) || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not delete records from clientstatusreporting:" << query.lastError().text();
    }
}

Result<void, QString> ClientStatusReportingDatabase::setClientStatusReportingRecord(const ClientStatusReportingRecord &record) const
{
    Q_ASSERT(record.isValid());
    if (!record.isValid()) {
        qCDebug(lcClientStatusReportingDatabase) << "Failed to set ClientStatusReportingRecord";
        return {QStringLiteral("Invalid parameter")};
    }

    const auto recordCopy = record;

    QMutexLocker locker(&_mutex);

    QSqlQuery query;

    const auto prepareResult = query.prepare(
        QStringLiteral("INSERT OR REPLACE INTO clientstatusreporting (name, status, count, lastOccurrence) VALUES(:name, :status, :count, :lastOccurrence) ON CONFLICT(name) "
        "DO UPDATE SET count = count + 1, lastOccurrence = :lastOccurrence;"));
    query.bindValue(QStringLiteral(":name"), recordCopy._name);
    query.bindValue(QStringLiteral(":status"), recordCopy._status);
    query.bindValue(QStringLiteral(":count"), 1);
    query.bindValue(QStringLiteral(":lastOccurrence"), recordCopy._lastOccurence);

    if (!prepareResult || !query.exec()) {
        const auto errorMessage = query.lastError().text();
        qCDebug(lcClientStatusReportingDatabase) << "Could not report client status:" << errorMessage;
        return errorMessage;
    }

    return {};
}

QString ClientStatusReportingDatabase::makeDbPath(const Account *account) const
{
    if (!dbPathForTesting.isEmpty()) {
        return dbPathForTesting;
    }
    const auto databaseId = QStringLiteral("%1@%2").arg(account->davUser(), account->url().toString());
    const auto databaseIdHash = QCryptographicHash::hash(databaseId.toUtf8(), QCryptographicHash::Md5);

    return ConfigFile().configPath() + QStringLiteral(".userdata_%1.db").arg(QString::fromLatin1(databaseIdHash.left(6).toHex()));
}

void ClientStatusReportingDatabase::updateStatusNamesHash()
{
    QByteArray statusNamesContatenated;
    for (int i = 0; i < ClientStatusReportingStatus::Count; ++i) {
        statusNamesContatenated += clientStatusstatusStringFromNumber(static_cast<ClientStatusReportingStatus>(i));
    }
    statusNamesContatenated += QByteArray::number(ClientStatusReportingStatus::Count);
    const auto statusNamesHashCurrent = QCryptographicHash::hash(statusNamesContatenated, QCryptographicHash::Md5).toHex();
    const auto statusNamesHashFromDb = getStatusNamesHash();

    if (statusNamesHashCurrent != statusNamesHashFromDb) {
        deleteClientStatusReportingRecords();
        setStatusNamesHash(statusNamesHashCurrent);
    }
}

quint64 ClientStatusReportingDatabase::getLastSentReportTimestamp() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return 0;
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get last sent report timestamp from keyvalue table:" << query.lastError().text();
        return 0;
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toULongLong();
}

void ClientStatusReportingDatabase::setStatusNamesHash(const QByteArray &hash) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    query.bindValue(QStringLiteral(":value"), hash);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not set status names hash.";
        return;
    }
}

QByteArray ClientStatusReportingDatabase::getStatusNamesHash() const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("SELECT value FROM keyvalue WHERE key = (:key)"));
    query.bindValue(QStringLiteral(":key"), statusNamesHash);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get status names hash. No such record:" << statusNamesHash;
        return {};
    }
    if (!query.next()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not get status names hash:" << query.lastError().text();
        return {};
    }
    return query.value(query.record().indexOf(QStringLiteral("value"))).toByteArray();
}

bool ClientStatusReportingDatabase::isInitialized() const
{
    return _isInitialized;
}

void ClientStatusReportingDatabase::setLastSentReportTimestamp(const quint64 timestamp) const
{
    QMutexLocker locker(&_mutex);
    QSqlQuery query;
    const auto prepareResult = query.prepare(QStringLiteral("INSERT OR REPLACE INTO keyvalue (key, value) VALUES(:key, :value);"));
    query.bindValue(QStringLiteral(":key"), lastSentReportTimestamp);
    query.bindValue(QStringLiteral(":value"), timestamp);
    if (!prepareResult || !query.exec()) {
        qCDebug(lcClientStatusReportingDatabase) << "Could not set last sent report timestamp from keyvalue table. No such record:" << lastSentReportTimestamp;
        return;
    }
}
QString ClientStatusReportingDatabase::dbPathForTesting;
}
