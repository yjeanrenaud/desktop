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

#include "clientstatusreportingcommon.h"
#include <QDebug>

namespace OCC {
QByteArray clientStatusstatusStringFromNumber(const ClientStatusReportingStatus status)
{
    Q_ASSERT(status >= 0 && status < Count);
    if (status < 0 || status >= ClientStatusReportingStatus::Count) {
        qDebug() << "Invalid status:" << status;
        return {};
    }

    switch (status) {
    case DownloadError_Cannot_Create_File:
        return QByteArrayLiteral("DownloadResult.CANNOT_CREATE_FILE");
    case DownloadError_Conflict:
        return QByteArrayLiteral("DownloadResult.CONFLICT");
    case DownloadError_ConflictCaseClash:
        return QByteArrayLiteral("DownloadResult.CONFLICT_CASECLASH");
    case DownloadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("DownloadResult.CONFLICT_INVALID_CHARACTERS");
    case DownloadError_No_Free_Space:
        return QByteArrayLiteral("DownloadResult.NO_FREE_SPACE");
    case DownloadError_ServerError:
        return QByteArrayLiteral("DownloadResult.SERVER_ERROR");
    case DownloadError_Virtual_File_Hydration_Failure:
        return QByteArrayLiteral("DownloadResult.VIRTUAL_FILE_HYDRATION_FAILURE");
    case E2EeError_GeneralError:
        return QByteArrayLiteral("E2EeError.General");
    case UploadError_Conflict:
        return QByteArrayLiteral("UploadResult.CONFLICT_CASECLASH");
    case UploadError_ConflictInvalidCharacters:
        return QByteArrayLiteral("UploadResult.CONFLICT_INVALID_CHARACTERS");
    case UploadError_No_Free_Space:
        return QByteArrayLiteral("UploadResult.NO_FREE_SPACE");
    case UploadError_No_Write_Permissions:
        return QByteArrayLiteral("UploadResult.NO_WRITE_PERMISSIONS");
    case UploadError_ServerError:
        return QByteArrayLiteral("UploadResult.SERVER_ERROR");
    case UploadError_Virus_Detected:
        return QByteArrayLiteral("UploadResult.VIRUS_DETECTED");
    case Count:
        return {};
    };
    return {};
}
}
