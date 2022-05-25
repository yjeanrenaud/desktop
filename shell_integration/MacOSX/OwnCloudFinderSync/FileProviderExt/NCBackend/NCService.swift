//
//  NCService.swift
//  Nextcloud
//
//  Created by Marino Faggiana on 14/03/18.
//  Copyright © 2018 Marino Faggiana. All rights reserved.
//
//  Author Marino Faggiana <marino.faggiana@nextcloud.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

import Foundation
import NCCommunication

class NCService: NSObject {
    @objc static let shared: NCService = {
        let instance = NCService()
        return instance
    }()

    // MARK: -

    @objc public func startRequestServicesServer() {
        if FileProviderUtils.shared.accountDetails == nil { return }

        self.requestServerStatus()
    }


    private func requestServerStatus() {
        guard let accountDetails = FileProviderUtils.shared.accountDetails else { return }
        
        NCCommunication.shared.getServerStatus(serverUrl: accountDetails.serverUrl, queue: NCCommunicationCommon.shared.backgroundQueue) { serverProductName, _, versionMajor, _, _, extendedSupport, errorCode, _ in

            if errorCode == 0 && extendedSupport == false {

                if serverProductName == "owncloud" {
                    //NCContentPresenter.shared.messageNotification("_warning_", description: "_warning_owncloud_", delay: NCGlobal.shared.dismissAfterSecond, type: NCContentPresenter.messageType.info, errorCode: NCGlobal.shared.errorInternalError, priority: .max)
                } /*else if versionMajor <=  NCGlobal.shared.nextcloud_unsupported_version {
                    NCContentPresenter.shared.messageNotification("_warning_", description: "_warning_unsupported_", delay: NCGlobal.shared.dismissAfterSecond, type: NCContentPresenter.messageType.info, errorCode: NCGlobal.shared.errorInternalError, priority: .max)
                }*/
            }
        }
    }
}
