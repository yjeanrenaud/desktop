//
//  FileProviderUtils.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 13/5/22.
//

import Foundation

struct NCAccountDetails {
    let account_id: String
    let username: String
    let server_url: URL
}

class FileProviderUtils: NSObject {
    static let shared: FileProviderUtils = {
        let instance = FileProviderUtils()
        return instance
    }()
        
    func clientConfigPath() -> String? {
        // Upon compiling this extension through CMake we set OC_APPLICATION_EXECUTABLE_NAME in the Info.plist
        // Qt's standard location for writableLocations( QStandardPaths::AppConfigLocation ) internally runs:
        //
        // [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) lastObject] + "/Preferences" +
        // "/" + applicationOrganizationName + "/" + applicationName
        //
        // In the client we then append the name of the app executable and ".cfg" as the name of the config. Most of these
        // values can be found in the ConfigFile and the Theme classes for the client.
        //
        // We are just replicating that here so we can get the client's config file.
        // NOTE: with the notable exception of the applicationOrganizationName, which isn't being set in the client yet
        
        if let libraryPath = NSSearchPathForDirectoriesInDomains(.libraryDirectory, .userDomainMask, true).last {
            let preferencesPath = libraryPath + "/Preferences/"
            let infoPlistDictionary = Bundle.main.infoDictionary
            
            guard let appName = infoPlistDictionary?["NC Client Application Name"] as? String else { return nil }
            //let orgName = infoPlistDictionary?["NC Client Organization Name"] as? String
            guard let appExecName = infoPlistDictionary?["NC Client Executable Name"] as? String else { return nil }
            
            // NOTE: If application organization name is set in the future on the client side, make sure to update this
            let configPath = preferencesPath + appName + "/" + appExecName + ".cfg"
            return configPath
        }

        return nil
    }
    
    func accountDetails(domainDisplayname: String) -> NCAccountDetails? {
        guard let configPath = clientConfigPath() else { return nil }
        
        let separatedDisplayString = domainDisplayname.split(separator: "@", maxSplits: 1)
        guard let displayUrl = separatedDisplayString.last,
              let displayUser = separatedDisplayString.first
        else { return nil }
        
        let displayUrlString = String(displayUrl)
        let displayUserString = String(displayUser)
        
        do {
            let configFileString = try String(contentsOfFile: configPath, encoding: .utf8)
            let configFileLines = configFileString.components(separatedBy: .newlines)
            
            // We have the name of the account and the "display" string of the URL, but we want more information.
            // We have what we need to find it in the displaystring.
            //
            // The config stores account data in the following format:
            //
            //      1\authType=webflow
            //      1\dav_user=claudio
            //      1\serverVersion=24.0.0.11
            //      1\url=https://mycloud.mynextcloud.com
            //      1\user=@Invalid()
            //      1\version=1
            //      1\webflow_user=claudio
            //      2\authType=webflow
            //      2\dav_user=claudio
            //      etc.
            //
            // First, we iterate over the config line by line to find the account ID of the account name we have in the
            // display string. However, since multiple servers might have an account with the same name, we need to make
            // sure that the ID we acquire from checking the line is the ID of the account at the server we actually want.
            // So in the ID-finding loop, we start another loop where we find the server URL. We check for the ID (i.e. "0")
            // and also make sure that this line contains the displaystring version of the server URL. If we match, great!
            // If not, we go back to the account ID loop and redo it all over again.
            
            // TODO: Avoid iterating over the entire config and limit to the relevant [Accounts] section
            // TODO: Even better, just find the matches in the configFileLines for W/user=XYZ and W/url=XYZ and check those
            for line in configFileLines {
                if line.contains(displayUserString) {
                    let splitUserLine = line.split(separator: "\\", maxSplits: 1) // Have to escape the escape character
                    guard let userId = splitUserLine.first else { continue }
                    let userIdString = String(userId)
                    
                    for innerLine in configFileLines {
                        if innerLine.contains(userIdString) && innerLine.contains(displayUrlString) {
                            let splitServerUrlLine = line.components(separatedBy: "url=")
                            guard let serverUrl = splitServerUrlLine.last,
                                  let serverUrlUrl = URL(string: String(serverUrl))
                            else { continue }
                            
                            return NCAccountDetails(account_id: userIdString,
                                                    username: displayUserString,
                                                    server_url: serverUrlUrl)
                        }
                    }
                }
            }
        } catch let error {
            print("Error reading client config file: \(error)")
            return nil;
        }
        
        return nil
    }
    
}
