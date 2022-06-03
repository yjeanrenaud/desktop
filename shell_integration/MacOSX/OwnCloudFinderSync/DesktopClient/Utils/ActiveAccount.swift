//
//  FileProviderAccount.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 30/5/22.
//

import Foundation
import NCCommunication

class ActiveAccount: tableAccount {
    static let shared: ActiveAccount = {
        let instance = ActiveAccount()
        return instance
    }()
    
    var accountSet: Bool = false
    var davFilesRootUrl: String = ""
    
    func setAccountFromDomainName(domainDisplayName: String) {
        guard let clientAccData = DesktopClientUtils.getAccountDetails(domainDisplayname: domainDisplayName) else { return }
        if clientAccData.isNull { return }
        
        account = clientAccData.username + " " + clientAccData.serverUrl
        user = clientAccData.username
        userId = clientAccData.username
        password = clientAccData.password
        urlBase = clientAccData.serverUrl
        davFilesRootUrl = clientAccData.davFilesRootUrl
        
        NCCommunicationCommon.shared.setup(account: account, user: user, userId: userId, password: password, urlBase: urlBase)
        NCCommunicationCommon.shared.setup(webDav: NCUtilityFileSystem.shared.getWebDAV(account: account))
        
        accountSet = true
        
        print("Account set for domain: \(domainDisplayName)")
    }
}
