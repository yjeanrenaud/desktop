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

#include "configfile.h"
#import <FileProvider/FileProvider.h>

#include <QLoggingCategory>

#include "config.h"
#include "fileproviderdomainmanager.h"
#include "pushnotifications.h"

#include "gui/accountmanager.h"
#include "libsync/account.h"

// Ensure that conversion to/from domain identifiers and display names
// are consistent throughout these classes
namespace {

QString domainIdentifierForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    return account->userIdAtHostWithPort();
}

QString domainIdentifierForAccount(const OCC::AccountPtr account)
{
    return domainIdentifierForAccount(account.get());
}

QString domainDisplayNameForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    return account->displayName();
}

QString domainDisplayNameForAccount(const OCC::AccountPtr account)
{
    return domainDisplayNameForAccount(account.get());
}

QString accountIdFromDomainId(const QString &domainId)
{
    return domainId;
}

QString accountIdFromDomainId(NSString * const domainId)
{
    return accountIdFromDomainId(QString::fromNSString(domainId));
}

API_AVAILABLE(macos(11.0))
QString accountIdFromDomain(NSFileProviderDomain * const domain)
{
    return accountIdFromDomainId(domain.identifier);
}

bool accountFilesPushNotificationsReady(const OCC::AccountPtr &account)
{
    const auto pushNotifications = account->pushNotifications();
    const auto pushNotificationsCapability = account->capabilities().availablePushNotifications() & OCC::PushNotificationType::Files;

    return pushNotificationsCapability && pushNotifications && pushNotifications->isReady();
}

}

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderDomainManager, "nextcloud.gui.macfileproviderdomainmanager", QtInfoMsg)

namespace Mac {

class API_AVAILABLE(macos(11.0)) FileProviderDomainManager::MacImplementation {

  public:
    MacImplementation() = default;
    ~MacImplementation() = default;

    void findExistingFileProviderDomains()
    {
        if (@available(macOS 11.0, *)) {
            // Wait for this to finish
            dispatch_group_t dispatchGroup = dispatch_group_create();
            dispatch_group_enter(dispatchGroup);

            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Could not get existing file provider domains: "
                                                              << error.code
                                                              << error.localizedDescription;
                    dispatch_group_leave(dispatchGroup);
                    return;
                }

                if (domains.count == 0) {
                    qCInfo(lcMacFileProviderDomainManager) << "Found no existing file provider domains";
                    dispatch_group_leave(dispatchGroup);
                    return;
                }

                for (NSFileProviderDomain * const domain in domains) {
                    const auto accountId = accountIdFromDomain(domain);

                    if (const auto accountState = AccountManager::instance()->accountFromUserId(accountId);
                            accountState &&
                            accountState->account() &&
                            domainDisplayNameForAccount(accountState->account()) == QString::fromNSString(domain.displayName)) {

                        qCInfo(lcMacFileProviderDomainManager) << "Found existing file provider domain for account:"
                                                               << accountState->account()->displayName();
                        [domain retain];
                        _registeredDomains.insert(accountId, domain);

                        NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:domain];
                        [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
                            if (error) {
                                qCWarning(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                                          << domain.displayName
                                                                          << error.code
                                                                          << error.localizedDescription;
                                return;
                            }

                            qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                                    << domain.displayName;
                        }];

                    } else {
                        qCInfo(lcMacFileProviderDomainManager) << "Found existing file provider domain with no known configured account:"
                                                               << domain.displayName;
                        [NSFileProviderManager removeDomain:domain completionHandler:^(NSError * const error) {
                            if (error) {
                                qCWarning(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                                          << error.code
                                                                          << error.localizedDescription;
                            }
                        }];
                    }
                }

                dispatch_group_leave(dispatchGroup);
            }];

            dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);
        }
    }

    void addFileProviderDomain(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto domainDisplayName = domainDisplayNameForAccount(account);
            const auto domainId = domainIdentifierForAccount(account);

            qCInfo(lcMacFileProviderDomainManager) << "Adding new file provider domain with id: "
                                                   << domainId;

            if (_registeredDomains.contains(domainId) && _registeredDomains.value(domainId) != nil) {
                qCDebug(lcMacFileProviderDomainManager) << "File provider domain with id already exists: "
                                                        << domainId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = [[NSFileProviderDomain alloc] initWithIdentifier:domainId.toNSString()
                                                                                                   displayName:domainDisplayName.toNSString()];
            [fileProviderDomain retain];

            [NSFileProviderManager addDomain:fileProviderDomain completionHandler:^(NSError * const error) {
                if(error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error adding file provider domain: "
                                                              << error.code
                                                              << error.localizedDescription;
                }

                _registeredDomains.insert(domainId, fileProviderDomain);
            }];
        }
    }

    void removeFileProviderDomain(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto domainId = domainIdentifierForAccount(account);
            qCInfo(lcMacFileProviderDomainManager) << "Removing file provider domain with id: "
                                                   << domainId;

            if (!_registeredDomains.contains(domainId)) {
                qCWarning(lcMacFileProviderDomainManager) << "File provider domain not found for id: "
                                                          << domainId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];

            [NSFileProviderManager removeDomain:fileProviderDomain completionHandler:^(NSError *error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error removing file provider domain: "
                                                              << error.code
                                                              << error.localizedDescription;
                }

                NSFileProviderDomain * const domain = _registeredDomains.take(domainId);
                [domain release];
            }];
        }
    }

    void removeAllFileProviderDomains()
    {
        if (@available(macOS 11.0, *)) {
            qCDebug(lcMacFileProviderDomainManager) << "Removing all file provider domains.";

            [NSFileProviderManager removeAllDomainsWithCompletionHandler:^(NSError * const error) {
                if(error) {
                    qCDebug(lcMacFileProviderDomainManager) << "Error removing all file provider domains: "
                                                            << error.code
                                                            << error.localizedDescription;
                    return;
                }

                const auto registeredDomainPtrs = _registeredDomains.values();
                for (NSFileProviderDomain * const domain : registeredDomainPtrs) {
                    if (domain != nil) {
                        [domain release];
                    }
                }
                _registeredDomains.clear();
            }];
        }
    }

    void wipeAllFileProviderDomains()
    {
        if (@available(macOS 12.0, *)) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing and wiping all file provider domains";

            [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domains: "
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                for (NSFileProviderDomain * const domain in domains) {
                    [NSFileProviderManager removeDomain:domain mode:NSFileProviderDomainRemovalModeRemoveAll completionHandler:^(NSURL * const preservedLocation, NSError * const error) {
                        Q_UNUSED(preservedLocation)

                        if (error) {
                            qCWarning(lcMacFileProviderDomainManager) << "Error removing and wiping file provider domain: "
                                                                      << domain.displayName
                                                                      << error.code
                                                                      << error.localizedDescription;
                            return;
                        }

                        NSFileProviderDomain * const registeredDomainPtr = _registeredDomains.take(QString::fromNSString(domain.identifier));
                        if (registeredDomainPtr != nil) {
                            [domain release];
                        }
                    }];
                }
            }];
        } else if (@available(macOS 11.0, *)) {
            qCInfo(lcMacFileProviderDomainManager) << "Removing all file provider domains, can't specify wipe on macOS 11";
            removeAllFileProviderDomains();
        }
    }

    void disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &message)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto domainId = domainIdentifierForAccount(account);
            qCInfo(lcMacFileProviderDomainManager) << "Disconnecting file provider domain with id: "
                                                   << domainId;

            if(!_registeredDomains.contains(domainId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for id: "
                                                       << domainId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager disconnectWithReason:message.toNSString()
                                    options:NSFileProviderManagerDisconnectionOptionsTemporary
                          completionHandler:^(NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error disconnecting file provider domain: "
                                                              << fileProviderDomain.displayName
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                qCInfo(lcMacFileProviderDomainManager) << "Successfully disconnected file provider domain: "
                                                       << fileProviderDomain.displayName;
            }];
        }
    }

    void reconnectFileProviderDomainForAccount(const AccountState * const accountState)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(accountState);
            const auto account = accountState->account();
            Q_ASSERT(account);

            const auto domainId = domainIdentifierForAccount(account);
            qCInfo(lcMacFileProviderDomainManager) << "Reconnecting file provider domain with id: "
                                                   << domainId;

            if(!_registeredDomains.contains(domainId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for id: "
                                                       << domainId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager reconnectWithCompletionHandler:^(NSError * const error) {
                if (error) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error reconnecting file provider domain: "
                                                              << fileProviderDomain.displayName
                                                              << error.code
                                                              << error.localizedDescription;
                    return;
                }

                qCInfo(lcMacFileProviderDomainManager) << "Successfully reconnected file provider domain: "
                                                       << fileProviderDomain.displayName;

                signalEnumeratorChanged(account.get());
            }];
        }
    }

    void signalEnumeratorChanged(const Account * const account)
    {
        if (@available(macOS 11.0, *)) {
            Q_ASSERT(account);
            const auto domainId = domainIdentifierForAccount(account);

            qCInfo(lcMacFileProviderDomainManager) << "Signalling enumerator changed in file provider domain for account with id: "
                                                   << domainId;

            if(!_registeredDomains.contains(domainId)) {
                qCInfo(lcMacFileProviderDomainManager) << "File provider domain not found for id: "
                                                       << domainId;
                return;
            }

            NSFileProviderDomain * const fileProviderDomain = _registeredDomains[domainId];
            Q_ASSERT(fileProviderDomain != nil);

            NSFileProviderManager * const fpManager = [NSFileProviderManager managerForDomain:fileProviderDomain];
            [fpManager signalEnumeratorForContainerItemIdentifier:NSFileProviderWorkingSetContainerItemIdentifier completionHandler:^(NSError * const error) {
                if (error != nil) {
                    qCWarning(lcMacFileProviderDomainManager) << "Error signalling enumerator changed for working set:"
                                                              << error.localizedDescription;
                }
            }];
        }
    }

    QStringList configuredDomainIds() const {
        return _registeredDomains.keys();
    }

private:
    QHash<QString, NSFileProviderDomain*> _registeredDomains;
};

FileProviderDomainManager::FileProviderDomainManager(QObject * const parent)
    : QObject(parent)
{
    if (@available(macOS 11.0, *)) {
        d.reset(new FileProviderDomainManager::MacImplementation());

        ConfigFile cfg;
        std::chrono::milliseconds polltime = cfg.remotePollInterval();
        _enumeratorSignallingTimer.setInterval(polltime.count());
        connect(&_enumeratorSignallingTimer, &QTimer::timeout,
                this, &FileProviderDomainManager::slotEnumeratorSignallingTimerTimeout);
        _enumeratorSignallingTimer.start();

        setupFileProviderDomains();

        connect(AccountManager::instance(), &AccountManager::accountAdded,
                this, &FileProviderDomainManager::addFileProviderDomainForAccount);
        // If an account is deleted from the client, accountSyncConnectionRemoved will be
        // emitted first. So we treat accountRemoved as only being relevant to client
        // shutdowns.
        connect(AccountManager::instance(), &AccountManager::accountSyncConnectionRemoved,
                this, &FileProviderDomainManager::removeFileProviderDomainForAccount);
        connect(AccountManager::instance(), &AccountManager::accountRemoved,
                this, [this](const AccountState * const accountState) {

            const auto trReason = tr("%1 application has been closed. Reopen to reconnect.").arg(APPLICATION_NAME);
            disconnectFileProviderDomainForAccount(accountState, trReason);
        });
    } else {
        qCWarning(lcMacFileProviderDomainManager()) << "Trying to run File Provider on system that does not support it.";
    }
}

FileProviderDomainManager::~FileProviderDomainManager() = default;

void FileProviderDomainManager::setupFileProviderDomains()
{
    if (!d) {
        return;
    }

    d->findExistingFileProviderDomains();

    for(auto &accountState : AccountManager::instance()->accounts()) {
        addFileProviderDomainForAccount(accountState.data());
    }
}

void FileProviderDomainManager::addFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->addFileProviderDomain(accountState);

    // Disconnect the domain when something changes regarding authentication
    connect(accountState, &AccountState::stateChanged, this, [this, accountState] {
        slotAccountStateChanged(accountState);
    });

    // Setup push notifications
    const auto accountCapabilities = account->capabilities().isValid();
    if (!accountCapabilities) {
        connect(account.get(), &Account::capabilitiesChanged, this, [this, account] {
            trySetupPushNotificationsForAccount(account.get());
        });
        return;
    }

    trySetupPushNotificationsForAccount(account.get());
}

void FileProviderDomainManager::trySetupPushNotificationsForAccount(const Account * const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);

    const auto pushNotifications = account->pushNotifications();
    const auto pushNotificationsCapability = account->capabilities().availablePushNotifications() & PushNotificationType::Files;

    if (pushNotificationsCapability && pushNotifications && pushNotifications->isReady()) {
        qCDebug(lcMacFileProviderDomainManager) << "Push notifications already ready, connecting them to enumerator signalling."
                                                << account->displayName();
        setupPushNotificationsForAccount(account);
    } else if (pushNotificationsCapability) {
        qCDebug(lcMacFileProviderDomainManager) << "Push notifications not yet ready, will connect to signalling when ready."
                                                << account->displayName();
        connect(account, &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
    }
}

void FileProviderDomainManager::setupPushNotificationsForAccount(const Account * const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);

    qCDebug(lcMacFileProviderDomainManager) << "Setting up push notifications for file provider domain for account:"
                                            << account->displayName();

    connect(account->pushNotifications(), &PushNotifications::filesChanged, this, &FileProviderDomainManager::signalEnumeratorChanged);
    disconnect(account, &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
}

void FileProviderDomainManager::signalEnumeratorChanged(const Account * const account)
{
    if (!d) {
        return;
    }

    Q_ASSERT(account);
    d->signalEnumeratorChanged(account);
}

void FileProviderDomainManager::removeFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->removeFileProviderDomain(accountState);

    if (accountFilesPushNotificationsReady(account)) {
        const auto pushNotifications = account->pushNotifications();
        disconnect(pushNotifications, &PushNotifications::filesChanged, this, &FileProviderDomainManager::signalEnumeratorChanged);
    } else if (const auto hasFilesPushNotificationsCapability = account->capabilities().availablePushNotifications() & PushNotificationType::Files) {
        disconnect(account.get(), &Account::pushNotificationsReady, this, &FileProviderDomainManager::setupPushNotificationsForAccount);
    }
}

void FileProviderDomainManager::disconnectFileProviderDomainForAccount(const AccountState * const accountState, const QString &reason)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();
    Q_ASSERT(account);

    d->disconnectFileProviderDomainForAccount(accountState, reason);
}

void FileProviderDomainManager::reconnectFileProviderDomainForAccount(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto account = accountState->account();

    d->reconnectFileProviderDomainForAccount(accountState);
}

void FileProviderDomainManager::slotAccountStateChanged(const AccountState * const accountState)
{
    if (!d) {
        return;
    }

    Q_ASSERT(accountState);
    const auto state = accountState->state();

    qCDebug(lcMacFileProviderDomainManager) << "Account state changed for account:"
                                            << accountState->account()->displayName()
                                            << "changing connection status of file provider domain.";

    switch(state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    {
        // Disconnect File Provider domain while unauthenticated
        const auto trReason = tr("This account is not authenticated. Please check your account state in the %1 application.").arg(APPLICATION_NAME);
        disconnectFileProviderDomainForAccount(accountState, trReason);
        break;
    }
    case AccountState::Connected:
        // Provide credentials
        reconnectFileProviderDomainForAccount(accountState);
        break;
    }
}

void FileProviderDomainManager::slotEnumeratorSignallingTimerTimeout()
{
    if (!d) {
        return;
    }

    qCDebug(lcMacFileProviderDomainManager) << "Enumerator signalling timer timed out, notifying domains for accounts without push notifications";

    const auto registeredDomainIds = d->configuredDomainIds();
    for (const auto &domainId : registeredDomainIds) {
        const auto accountUserId = accountIdFromDomainId(domainId);
        const auto accountState = AccountManager::instance()->accountFromUserId(accountUserId);
        const auto account = accountState->account();

        if (!accountFilesPushNotificationsReady(account)) {
            qCDebug(lcMacFileProviderDomainManager) << "Notifying domain for account:" << account->userIdAtHostWithPort();
            d->signalEnumeratorChanged(account.get());
        }
    }
}

AccountStatePtr FileProviderDomainManager::accountStateFromFileProviderDomainIdentifier(const QString &domainIdentifier)
{
    if (domainIdentifier.isEmpty()) {
        qCWarning(lcMacFileProviderDomainManager) << "Cannot return accountstateptr for empty domain identifier";
        return AccountStatePtr();
    }

    const auto accountUserId = accountIdFromDomainId(domainIdentifier);
    const auto accountForReceivedDomainIdentifier = AccountManager::instance()->accountFromUserId(accountUserId);
    if (!accountForReceivedDomainIdentifier) {
        qCWarning(lcMacFileProviderDomainManager) << "Could not find account matching user id matching file provider domain identifier:"
                                                  << domainIdentifier;
    }

    return accountForReceivedDomainIdentifier;
}

} // namespace Mac

} // namespace OCC
