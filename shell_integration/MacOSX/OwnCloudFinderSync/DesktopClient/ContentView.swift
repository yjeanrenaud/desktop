//
//  ContentView.swift
//  DesktopClient
//
//  Created by Claudio Cambra on 2/6/22.
//

import SwiftUI

struct ContentView: View {
    let account: String = {
        ActiveAccount.shared.setAccountFromDomainName(domainDisplayName: "claudio@cloud.nextcloud.com")
        print(ActiveAccount.shared.davFilesRootUrl)
        // print(NCUtils.fileProviderStoragePath)
        return ActiveAccount.shared.user
    }()
    
    var body: some View {
        Text(account)
            .padding()
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
