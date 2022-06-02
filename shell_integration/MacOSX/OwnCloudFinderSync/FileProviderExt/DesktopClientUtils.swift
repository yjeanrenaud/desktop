//
//  DesktopClientUtils.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 27/5/22.
//

import Foundation

struct NCDesktopClientAccountData {
    let accountId: String
    let username: String
    let password: String
    let serverUrl: String
    let davFilesRootUrl: String
    
    var isNull: Bool {
        return accountId.isEmpty || username.isEmpty
    }
}

class DesktopClientUtils {
    
    static let appName: String = Bundle.main.infoDictionary!["NC Client Application Name"] as! String
    static let orgName: String = Bundle.main.infoDictionary!["NC Client Organization Name"] as! String
    static let appExecutableName: String = Bundle.main.infoDictionary!["NC Client Executable Name"] as! String
    
    static let clientPreferencesPath: URL? = {
        if let libraryPath = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).last {
            let preferencesPath = libraryPath.appendingPathComponent("Preferences")
            
            // NOTE: If application organization name is set in the future on the client side, make sure to update this
            let appPrefPath = preferencesPath.appendingPathComponent(DesktopClientUtils.appName)

            return appPrefPath;
        }
        
        return nil
    }()
    
    static let clientConfigFilePath: URL? = {
        return clientPreferencesPath?.appendingPathComponent(DesktopClientUtils.appExecutableName + ".cfg")
    }()
    
    static func getUserPasswordFromKeychain(accountString: String) -> Data? {
        let query = [
            kSecClass as String       : kSecClassGenericPassword,
            kSecAttrAccount as String : accountString,
            kSecReturnData as String  : kCFBooleanTrue!,
            kSecMatchLimit as String  : kSecMatchLimitOne
        ] as [String : Any]

        var dataTypeRef: AnyObject? = nil

        let status: OSStatus = SecItemCopyMatching(query as CFDictionary, &dataTypeRef)

        if status == noErr {
            return dataTypeRef as! Data?
        } else {
            return nil
        }
    }
    
    static func getAccountDetails(domainDisplayname: String) -> NCDesktopClientAccountData? {
        guard let configPath = clientConfigFilePath?.absoluteString else { return nil }
        
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
                    guard let accountId = splitUserLine.first else { continue }
                    let accountIdString = String(accountId)
                    
                    for innerLine in configFileLines {
                        if innerLine.contains(accountIdString) && innerLine.contains(displayUrlString) {
                            
                            let splitServerUrlLine = line.components(separatedBy: "url=")
                            guard let serverUrl = splitServerUrlLine.last else { continue }
                            let serverUrlString = String(serverUrl)
                            
                            // The client sets the account field in the keychain entry as a colon-separated string consisting of
                            // an account's username, its homeserver url, and the id of the account
                            let keychainAccountString = displayUserString + ":" + serverUrlString + ":" + accountIdString
                            guard let userPassword = self.getUserPasswordFromKeychain(accountString: keychainAccountString),
                                  let userPasswordString = String(data: userPassword, encoding: .utf8)
                            else { continue }
                            
                            let davUrlString = serverUrlString + "/remote.php/dav"
                            let davFilesRootUrlString = davUrlString + "/files/" + displayUserString
                            
                            return NCDesktopClientAccountData(accountId: accountIdString,
                                                              username: displayUserString,
                                                              password: userPasswordString,
                                                              serverUrl: serverUrlString,
                                                              davFilesRootUrl: davFilesRootUrlString)
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
