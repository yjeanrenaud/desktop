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

#include "updatee2eefolderusersmetadatajob.h"
#include "foldermetadata.h"
#include "common/syncjournalfilerecord.h"
#include "common/syncjournaldb.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcUpdateE2eeFolderUsersMetadataJob, "nextcloud.gui.updatee2eefolderusersmetadatajob", QtInfoMsg)

UpdateE2eeFolderUsersMetadataJob::UpdateE2eeFolderUsersMetadataJob(const AccountPtr &account,
                                                       SyncJournalDb *journalDb,
                                                       const QString &syncFolderRemotePath,
                                                       const Operation operation,
                                                       const QString &path,
                                                       const QString &folderUserId,
                                                       QSslCertificate certificate,
                                                       QObject *parent)
    : QObject(parent)
    , _account(account)
    , _journalDb(journalDb)
    , _syncFolderRemotePath(syncFolderRemotePath)
    , _operation(operation)
    , _path(path)
    , _folderUserId(folderUserId)
    , _folderUserCertificate(certificate)
{
    connect(this, &UpdateE2eeFolderUsersMetadataJob::finished, this, &UpdateE2eeFolderUsersMetadataJob::deleteLater);
}

void UpdateE2eeFolderUsersMetadataJob::start()
{
    if (_operation != Operation::Add && _operation != Operation::Remove && _operation != Operation::ReEncrypt) {
        emit finished(-1, tr("Wrong operation for folder %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    if (_operation == Operation::Add) {
        connect(this, &UpdateE2eeFolderUsersMetadataJob::certificateReady, this, &UpdateE2eeFolderUsersMetadataJob::slotStartE2eeMetadataJobs);
        if (!_folderUserCertificate.isNull()) {
            emit certificateReady();
            return;
        }
        connect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain,
            this, &UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain);
        _account->e2e()->fetchFromKeyChain(_account, _folderUserId);
        return;
    }
    slotStartE2eeMetadataJobs();
}

void UpdateE2eeFolderUsersMetadataJob::slotStartE2eeMetadataJobs()
{
    if (_operation == Operation::Add && _folderUserCertificate.isNull()) {
        emit finished(404, tr("Could not fetch publicKey for user %1").arg(_folderUserId));
        return;
    }

    if (!_journalDb) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    SyncJournalFileRecord rec;
    if (!_journalDb->getRootE2eFolderRecord(_path, &rec) || !rec.isValid()) {
        emit finished(-1, tr("Could not find top level E2EE folder for %1").arg(_path));
        return;
    }
    const auto pathSanitized = _path.startsWith(QLatin1Char('/')) ? _path.mid(1) : _path;
    const FolderMetadata::RootEncryptedFolderInfo topLevelInitData(FolderMetadata::RootEncryptedFolderInfo::createRootPath(rec.path(), pathSanitized),
                                                                   _metadataKeyForEncryption,
                                                                   _metadataKeyForDecryption,
                                                                   _keyChecksums);

    _fetchAndUploadE2eeFolderMetadataJob.reset(new FetchAndUploadE2eeFolderMetadataJob(_account, _syncFolderRemotePath + pathSanitized, _journalDb, rec.path(), pathSanitized));
    _fetchAndUploadE2eeFolderMetadataJob->setFolderToken(_folderToken);

    connect(_fetchAndUploadE2eeFolderMetadataJob.data(), &FetchAndUploadE2eeFolderMetadataJob::fetchFinished,
            this, &UpdateE2eeFolderUsersMetadataJob::slotFetchMetadataJobFinished);
    _fetchAndUploadE2eeFolderMetadataJob->fetchMetadata(true);
}

void UpdateE2eeFolderUsersMetadataJob::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Metadata Received, Preparing it for the new file." << message;

    if (statusCode != 0) {
        qCritical() << "fetch metadata finished with error" << statusCode << message;
        emit finished(SyncFileItem::Status::FatalError);
        return;
    }

    if (!_fetchAndUploadE2eeFolderMetadataJob->folderMetadata()->isValid()) {
        emit finished(403, tr("Could not add or remove a folder user %1, for folder %2").arg(_folderUserId).arg(_path));
        return;
    }
    startUpdate();
}

void UpdateE2eeFolderUsersMetadataJob::startUpdate()
{
    if (_operation != Operation::Add && _operation != Operation::Remove && _operation != Operation::ReEncrypt) {
        emit finished(-1, tr("Wrong operation for folder %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    if (_operation == Operation::Add || _operation == Operation::Remove) {
        const auto result = _operation == Operation::Add
            ? _fetchAndUploadE2eeFolderMetadataJob->folderMetadata()->addUser(_folderUserId, _folderUserCertificate)
            : _fetchAndUploadE2eeFolderMetadataJob->folderMetadata()->removeUser(_folderUserId);

        if (!result) {
            emit finished(-1);
            return;
        }

    }
    _fetchAndUploadE2eeFolderMetadataJob->uploadMetadata(true);

}

void UpdateE2eeFolderUsersMetadataJob::slotUpdateMetadataFinished(int code, const QString &message)
{
    if (code != 200) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Update metadata error for folder" << _fetchAndUploadE2eeFolderMetadataJob->folderId() << "with error"
                                                    << code << message;
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob()) << "Unlocking the folder.";
        if (_operation == Operation::Add || _operation == Operation::Remove) {
            slotUnlockFolder(false);
        } else {
            emit finished(code, tr("Failed to update folder metadata."));
        }
        return;
    }

    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Uploading of the metadata success.";
    if (_operation == Operation::Add || _operation == Operation::Remove) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Trying to schedule more jobs.";
        slotScheduleSubJobs();
        if (_subJobs.isEmpty()) {
            slotUnlockFolder(true);
        } else {
            _subJobs.values().last()->start();
        }
    } else {
        emit finished(200);
    }
}

void UpdateE2eeFolderUsersMetadataJob::slotScheduleSubJobs()
{
    const auto pathInDb = _path.mid(_syncFolderRemotePath.size());

    [[maybe_unused]] const auto result = _journalDb->getFilesBelowPath(pathInDb.toUtf8(), [this](const SyncJournalFileRecord &record) {
        if (record.isDirectory()) {
            const auto subJob = new UpdateE2eeFolderUsersMetadataJob(_account, _journalDb, _syncFolderRemotePath, UpdateE2eeFolderUsersMetadataJob::ReEncrypt, QString::fromUtf8(record._e2eMangledName));
            subJob->setMetadataKeyForEncryption(_folderMetadata->metadataKeyForEncryption());
            subJob->setMetadataKeyForDecryption(_folderMetadata->metadataKeyForDecryption());
            subJob->setKeyChecksums(_folderMetadata->keyChecksums() + _folderMetadata->keyChecksumsRemoved());
            subJob->setParent(this);
            subJob->setFolderToken(_fetchAndUploadE2eeFolderMetadataJob->folderToken());
            _subJobs.insert(subJob);
            connect(subJob, &UpdateE2eeFolderUsersMetadataJob::finished, this, &UpdateE2eeFolderUsersMetadataJob::slotSubJobFinished);
        }
    });
}

void UpdateE2eeFolderUsersMetadataJob::slotUnlockFolder(bool success)
{
    if (!_journalDb) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_fetchAndUploadE2eeFolderMetadataJob->folderId())));
        return;
    }

    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Calling Unlock";
    connect(_fetchAndUploadE2eeFolderMetadataJob.data(), &FetchAndUploadE2eeFolderMetadataJob::folderUnlocked, this, &UpdateE2eeFolderUsersMetadataJob::slotFolderUnlocked);
    _fetchAndUploadE2eeFolderMetadataJob->unlockFolder(success);
}

void UpdateE2eeFolderUsersMetadataJob::slotFolderUnlocked(const QByteArray &folderId, int httpStatus)
{
    const QString message = httpStatus != 200 ? tr("Failed to unlock a folder.") : QString{};
    emit finished(httpStatus, message);
}

void UpdateE2eeFolderUsersMetadataJob::slotSubJobsFinished()
{
    slotUnlockFolder(true);
}

void UpdateE2eeFolderUsersMetadataJob::slotSubJobFinished(int code, const QString &message)
{
    Q_UNUSED(message);
    if (code != 200) {
        slotUnlockFolder(false);
        return;
    }
    const auto job = qobject_cast<UpdateE2eeFolderUsersMetadataJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        slotUnlockFolder(false);
        return;
    }

    {
        QMutexLocker locker(&_subjobItemsMutex);
        const auto foundInHash = _subJobItems.constFind(job->path());
        if (foundInHash != _subJobItems.constEnd() && foundInHash.value()) {
            foundInHash.value()->_e2eEncryptionStatus = job->encryptionStatus();
            foundInHash.value()->_e2eEncryptionStatusRemote = job->encryptionStatus();
            foundInHash.value()->_e2eEncryptionMaximumAvailableStatus = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
            _subJobItems.erase(foundInHash);
        }
    }

    _subJobs.remove(job);
    job->deleteLater();

    if (_subJobs.isEmpty()) {
        slotSubJobsFinished();
    } else {
        _subJobs.values().last()->start();
    }
}

void UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain(const QSslCertificate certificate)
{
    disconnect(_account->e2e(),
               &ClientSideEncryption::certificateFetchedFromKeychain,
               this,
               &UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain);
    if (certificate.isNull()) {
        // get folder user's public key
        _account->e2e()->getUsersPublicKeyFromServer(_account, {_folderUserId});
        connect(_account->e2e(),
                &ClientSideEncryption::certificatesFetchedFromServer,
                this,
                &UpdateE2eeFolderUsersMetadataJob::slotCertificatesFetchedFromServer);
        return;
    }
    _folderUserCertificate = certificate;
    emit certificateReady();
}

void UpdateE2eeFolderUsersMetadataJob::slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results)
{
    const auto certificate = results.isEmpty() ? QSslCertificate{} : results.value(_folderUserId);
    _folderUserCertificate = certificate;
    if (certificate.isNull()) {
        emit certificateReady();
        return;
    }
    _account->e2e()->writeCertificate(_account, _folderUserId, certificate);
    connect(_account->e2e(), &ClientSideEncryption::certificateWriteComplete, this, &UpdateE2eeFolderUsersMetadataJob::certificateReady);
}

void UpdateE2eeFolderUsersMetadataJob::setUserData(const UserData &userData)
{
    _userData = userData;
}

void UpdateE2eeFolderUsersMetadataJob::setFolderToken(const QByteArray &folderToken)
{
    _folderToken = folderToken;
}

void UpdateE2eeFolderUsersMetadataJob::setMetadataKeyForEncryption(const QByteArray &metadataKey)
{
    _metadataKeyForEncryption = metadataKey;
}

void UpdateE2eeFolderUsersMetadataJob::setMetadataKeyForDecryption(const QByteArray &metadataKey)
{
    _metadataKeyForDecryption = metadataKey;
}

void UpdateE2eeFolderUsersMetadataJob::setKeyChecksums(const QSet<QByteArray> &keyChecksums)
{
    _keyChecksums = keyChecksums;
}

void UpdateE2eeFolderUsersMetadataJob::setJubJobItems(const QHash<QString, SyncFileItemPtr> &subJobItems)
{
    _subJobItems = subJobItems;
}

QString UpdateE2eeFolderUsersMetadataJob::path() const
{
    return _path;
}

UpdateE2eeFolderUsersMetadataJob::UserData UpdateE2eeFolderUsersMetadataJob::userData() const
{
    return _userData;
}

SyncFileItem::EncryptionStatus UpdateE2eeFolderUsersMetadataJob::encryptionStatus() const
{
    return _folderMetadata->encryptedMetadataEncryptionStatus();
}

}
