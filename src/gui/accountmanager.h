/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "account.h"
#include "accountstate.h"

namespace OCC {

/**
   @brief The AccountManager class
   @ingroup gui
*/
class AccountManager : public QObject
{
    Q_OBJECT
public:
    static AccountManager *instance();
    ~AccountManager() override = default;

    /**
     * Saves the accounts to a given settings file
     */
    void save(bool saveCredentials = true);

    /**
     * Creates account objects from a given settings file.
     *
     * Returns false if there was an error reading the settings,
     * but note that settings not existing is not an error.
     */
    bool restore(bool alsoRestoreLegacySettings = true);

    /**
     * Add this account in the list of saved accounts.
     * Typically called from the wizard
     */
    AccountState *addAccount(const AccountPtr &newAccount);

    /**
     * remove all accounts
     */
    void shutdown();

    /**
     * Return a list of all accounts.
     * (this is a list of QSharedPointer for internal reasons, one should normally not keep a copy of them)
     */
    [[nodiscard]] QList<AccountStatePtr> accounts() const;

    /**
     * Return the account state pointer for an account identified by its display name
     */
    AccountStatePtr account(const QString &name);

    /**
     * Return the account state pointer for an account from its id
     */

    [[nodiscard]] AccountStatePtr accountFromUserId(const QString &id) const;

    /**
     * Delete the AccountState
     */
    void deleteAccount(AccountState *account);

    /**
     * Creates an account and sets up some basic handlers.
     * Does *not* add the account to the account manager just yet.
     */
    static AccountPtr createAccount();

    /**
     * Returns the list of settings keys that can't be read because
     * they are from the future.
     */
    static void backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys);

private:
    // saving and loading Account to settings
    void saveAccountHelper(Account *account, QSettings &settings, bool saveCredentials = true);
    AccountPtr loadAccountHelper(QSettings &settings);

    bool restoreFromLegacySettings();

    [[nodiscard]] bool isAccountIdAvailable(const QString &id) const;
    [[nodiscard]] QString generateFreeAccountId() const;

    // Adds an account to the tracked list, emitting accountAdded()
    void addAccountState(AccountState *accountState);

    AccountManager() = default;
    QList<AccountStatePtr> _accounts;
    /// Account ids from settings that weren't read
    QSet<QString> _additionalBlockedAccountIds;

public slots:
    /// Saves account data, not including the credentials
    void saveAccount(OCC::Account *a);

    /// Saves account state data, not including the account
    void saveAccountState(OCC::AccountState *a);


Q_SIGNALS:
    void accountAdded(OCC::AccountState *account);
    void accountRemoved(OCC::AccountState *account);
    void accountSyncConnectionRemoved(OCC::AccountState *account);
    void removeAccountFolders(OCC::AccountState *account);
    void accountCredentialsSaved(OCC::Account *account);
};
}
