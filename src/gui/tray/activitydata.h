/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef ACTIVITYDATA_H
#define ACTIVITYDATA_H

#include "syncfileitem.h"
#include "folder.h"
#include "account.h"

#include <QtCore>
#include <QIcon>
#include <QJsonObject>
#include <QVariantMap>

namespace OCC {

// Return true if the action was handled, false if not
using ActivityActionFunction = std::function<bool()>;

/**
 * @brief The ActivityLink class describes actions of an activity
 *
 * These are part of notifications which are mapped into activities.
 */

class ActivityAction
{
    Q_GADGET

    Q_PROPERTY(bool primary READ primary CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)

public:
    explicit ActivityAction() = default;
    explicit ActivityAction(const QString &label, const bool primary, ActivityActionFunction action = nullptr);

    bool primary() const;
    QString label() const;

    ActivityActionFunction action() const;

protected:
    bool _primary = false;
    QString _label;
    ActivityActionFunction _action = nullptr;

private:
    ActivityActionFunction _fallbackAction = [] {
        return false;
    };
};

class ActivityLink : public ActivityAction
{
    Q_GADGET

    Q_PROPERTY(QString imageSource READ imageSource CONSTANT)
    Q_PROPERTY(QString imageSourceHovered READ imageSourceHovered CONSTANT)
    Q_PROPERTY(QString link READ link CONSTANT)
    Q_PROPERTY(QByteArray verb READ verb CONSTANT)

public:
    explicit ActivityLink() = default;
    explicit ActivityLink(const QString &label,
                          const bool primary,
                          const QString &link,
                          const QByteArray &verb = {},
                          const QString &imageSource = {},
                          const QString &imageSourceHovered = {});
    static ActivityLink createFromJsonObject(const QJsonObject &obj);

    QString imageSource() const;
    QString imageSourceHovered() const;
    QString link() const;
    QByteArray verb() const;

private:
    bool linkAction();

    QString _imageSource;
    QString _imageSourceHovered;
    QString _link;
    QByteArray _verb;
};

/**
 * @brief The PreviewData class describes the data about a file's preview.
 */

class PreviewData
{
    Q_GADGET

    Q_PROPERTY(QString source MEMBER _source)
    Q_PROPERTY(QString link MEMBER _link)
    Q_PROPERTY(QString mimeType MEMBER _mimeType)
    Q_PROPERTY(int fileId MEMBER _fileId)
    Q_PROPERTY(QString view MEMBER _view)
    Q_PROPERTY(bool isMimeTypeIcon MEMBER _isMimeTypeIcon)
    Q_PROPERTY(QString filename MEMBER _filename)

public:
    QString _source;
    QString _link;
    QString _mimeType;
    int _fileId = 0;
    QString _view;
    bool _isMimeTypeIcon = false;
    QString _filename;
};

struct RichSubjectParameter {
    Q_GADGET
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(QUrl link MEMBER link)

public:
    QString type;    // Required
    QString id;      // Required
    QString name;    // Required
    QString path;    // Required (for files only)
    QUrl link;    // Optional (files only)
};

struct TalkNotificationData {
    Q_GADGET
    Q_PROPERTY(QString conversationToken MEMBER conversationToken)
    Q_PROPERTY(QString messageId MEMBER messageId)
    Q_PROPERTY(QString messageSent MEMBER messageSent)
    Q_PROPERTY(QString userAvatar MEMBER userAvatar)

public:
    QString conversationToken;
    QString messageId;
    QString messageSent;
    QString userAvatar;

    [[nodiscard]] bool operator==(const TalkNotificationData &other) const
    {
        return conversationToken == other.conversationToken &&
            messageId == other.messageId &&
            messageSent == other.messageSent &&
            userAvatar == other.userAvatar;
    }

    [[nodiscard]] bool operator!=(const TalkNotificationData &other) const
    {
        return conversationToken != other.conversationToken ||
            messageId != other.messageId ||
            messageSent != other.messageSent ||
            userAvatar != other.userAvatar;
    }
};

/* ==================================================================== */
/**
 * @brief Activity Structure
 * @ingroup gui
 *
 * contains all the information describing a single activity.
 */
class Activity
{
    Q_GADGET
    Q_PROPERTY(OCC::Activity::Type type MEMBER _type)
    Q_PROPERTY(OCC::TalkNotificationData talkNotificationData MEMBER _talkNotificationData)
    Q_PROPERTY(QVariantMap subjectRichParameters MEMBER _subjectRichParameters)

public:
    // Note that these are in the order we want to present them in the model!
    enum Type {
        DummyFetchingActivityType,
        NotificationType,
        SyncResultType,
        SyncFileItemType,
        ActivityType,
        DummyMoreActivitiesAvailableType,
    };

    Q_ENUM(Type)

    static Activity fromActivityJson(const QJsonObject &json, const AccountPtr account);

    static QString relativeServerFileTypeIconPath(const QMimeType &mimeType);
    static QString localFilePathForActivity(const Activity &activity, const AccountPtr account);

    using RichSubjectParameter = OCC::RichSubjectParameter;
    using TalkNotificationData = OCC::TalkNotificationData;

    Type _type;
    qlonglong _id = 0LL;
    QString _fileAction;
    int _objectId = 0;
    TalkNotificationData _talkNotificationData;
    QString _objectType;
    QString _objectName;
    QString _subject;
    QString _subjectRich;
    QVariantMap _subjectRichParameters;
    QString _subjectDisplay;
    QString _message;
    QString _folder;
    QString _file;
    QString _renamedFile;
    bool _isMultiObjectActivity = false;
    QUrl _link;
    QDateTime _dateTime;
    qint64 _expireAtMsecs = -1;
    QString _accName;
    QString _icon;
    bool _isCurrentUserFileActivity = false;
    QVector<PreviewData> _previews;

    // Stores information about the error
    SyncFileItem::Status _syncFileItemStatus = SyncFileItem::Status::NoStatus;
    SyncResult::Status _syncResultStatus = SyncResult::Status::Undefined;

    QVector<ActivityLink> _links;
    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */

    bool _shouldNotify = true;
};

bool operator==(const Activity &rhs, const Activity &lhs);
bool operator!=(const Activity &rhs, const Activity &lhs);
bool operator<(const Activity &rhs, const Activity &lhs);
bool operator>(const Activity &rhs, const Activity &lhs);

/* ==================================================================== */
/**
 * @brief The ActivityList
 * @ingroup gui
 *
 * A QList based list of Activities
 */
using ActivityList = QList<Activity>;
}

Q_DECLARE_METATYPE(OCC::Activity)
Q_DECLARE_METATYPE(OCC::ActivityList)
Q_DECLARE_METATYPE(OCC::Activity::Type)
Q_DECLARE_METATYPE(OCC::Activity::RichSubjectParameter)
Q_DECLARE_METATYPE(OCC::ActivityLink)
Q_DECLARE_METATYPE(OCC::PreviewData)

#endif // ACTIVITYDATA_H
