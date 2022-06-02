//
//  FileProviderUtils.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 13/5/22.
//

import Foundation
import FileProvider

class FileProviderUtils {
    
    static let userAgent: String = "Mozilla 5.0 (macOS) " + DesktopClientUtils.appName + "FileProviderExt/1.0"
    
    static let certificatesPath: URL? = {
        let certificatesPath = DesktopClientUtils.clientPreferencesPath?.appendingPathComponent("FileProviderCertificates")
        return createPathIfNotExists(path: certificatesPath);
    }()
    
    static let fileProviderStoragePath: URL? = {
        let appGroupIdentifier = Bundle.main.infoDictionary!["NC Client App Group"] as! String
        guard let containerUrl = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroupIdentifier) else { return nil }
        
        return createPathIfNotExists(path: containerUrl.appendingPathComponent("FileProviderStorage"))
    }()
    
    static let fileProviderDatabasePath: URL? = {
        let fileProviderDbPath = DesktopClientUtils.clientPreferencesPath?.appendingPathComponent("FileProviderDatabase")
        return createPathIfNotExists(path: fileProviderDbPath);
    }()
    
    static let fileProviderUserDataPath: URL? = {
        let fileProviderDbPath = DesktopClientUtils.clientPreferencesPath?.appendingPathComponent("FileProviderUserData")
        return createPathIfNotExists(path: fileProviderDbPath);
    }()
    
    // Returns nil if path was not created, returns created path if everything went well
    static func createPathIfNotExists(path: URL?) -> URL? {
        if path != nil && !FileManager.default.fileExists(atPath: path!.absoluteString) {
            do {
                try FileManager.default.createDirectory(atPath: path!.absoluteString, withIntermediateDirectories: true)
                return path
            } catch let error {
                print("Error creating certificates directory: \(error)")
                return nil
            }
        }
        
        return path
    }
    
    static func getFileProviderDirectoryStorageOcId(ocId: String) -> URL? {
        guard let path = fileProviderStoragePath?.appendingPathComponent(ocId) else { return nil }
        return createPathIfNotExists(path: path)
    }
    
    static func getFileProviderDirectoryStorageOcId(ocId: String, fileNameView: String) -> URL? {
        let filePath = getFileProviderDirectoryStorageOcId(ocId: ocId)?.appendingPathComponent(fileNameView)
        guard let filePathString = filePath?.absoluteString else { return nil }
        
        if !FileManager.default.fileExists(atPath: filePathString) {
            FileManager.default.createFile(atPath: filePathString, contents: nil)
        }
        return filePath
    }
    
    static func fileExistsInFileProviderStorage(metadata: tableMetadata) -> Bool {
        guard let fileNamePath = self.getFileProviderDirectoryStorageOcId(ocId: metadata.ocId, fileNameView: metadata.fileNameView) else { return false }
        
        if !FileManager.default.fileExists(atPath: fileNamePath.absoluteString) {
            return false
        }
        
        do {
            let fileAttributes = try FileManager.default.attributesOfItem(atPath: fileNamePath.absoluteString)
            return fileAttributes[.size] as! UInt64 > 0
        } catch {
            return false
        }
    }
    
    static func getFileInStorageSize(ocId: String, fileNameView: String) -> UInt64 {
        guard let fileNamePath = getFileProviderDirectoryStorageOcId(ocId: ocId, fileNameView: fileNameView) else { return 0 }
        
        do {
            let fileAttributes = try FileManager.default.attributesOfItem(atPath: fileNamePath.absoluteString)
            return fileAttributes[.size] as! UInt64
        } catch {
            return 0
        }
    }
    
    static func getItemIdentifier(metadata: tableMetadata) -> NSFileProviderItemIdentifier {
        return NSFileProviderItemIdentifier(metadata.ocId)
    }
    
    static func getParentItemIdentifier(metadata: tableMetadata) -> NSFileProviderItemIdentifier? {
        
        let homeServerUrl = NCUtilityFileSystem.shared.getHomeServer(account: metadata.account)
        if let directory = NCManageDatabase.shared.getTableDirectory(predicate: NSPredicate(format: "account == %@ AND serverUrl == %@", metadata.account, metadata.serverUrl)) {
            if directory.serverUrl == homeServerUrl {
                return NSFileProviderItemIdentifier(NSFileProviderItemIdentifier.rootContainer.rawValue)
            } else {
                // get the metadata.ocId of parent Directory
                if let metadata = NCManageDatabase.shared.getMetadataFromOcId(directory.ocId) {
                    let identifier = getItemIdentifier(metadata: metadata)
                    return identifier
                }
            }
        }

        return nil
    }
    
    static func getTableMetadataFromItemIdentifier(_ itemIdentifier: NSFileProviderItemIdentifier) -> tableMetadata? {
        let ocId = itemIdentifier.rawValue
        return NCManageDatabase.shared.getMetadataFromOcId(ocId)
    }
    
    static func isFolderEncrypted(serverUrl: String, e2eEncrypted: Bool, account: String, urlBase: String) -> Bool {
        let home = NCUtilityFileSystem.shared.getHomeServer(account: account)
        var serverUrl = serverUrl
        
        if e2eEncrypted {
            return true
        } else if serverUrl == home || serverUrl == ".." {
            return false
        } else {
            var directory = NCManageDatabase.shared.getTableDirectory(predicate: NSPredicate(format: "account == \(account) AND serverUrl == \(serverUrl)"))
            
            while directory != nil && directory!.serverUrl != home {
                if directory!.e2eEncrypted {
                    return true
                }
                
                serverUrl = NCUtilityFileSystem.shared.deletingLastPathComponent(account: account, serverUrl: serverUrl)
                directory = NCManageDatabase.shared.getTableDirectory(predicate: NSPredicate(format: "account == \(account) AND serverUrl == \(serverUrl)"))
            }
            
            return false
        }
    }
    
    static func ocIdToFileId(ocId: String?) -> String? {
        
        guard let ocId = ocId else { return nil }

        let items = ocId.components(separatedBy: "oc")
        if items.count < 2 { return nil }
        guard let intFileId = Int(items[0]) else { return nil }
        return String(intFileId)
    }
    
    static func getPathRelativeToServerFilesRoot(serverUrl: String, urlBase: String, account: String) -> String {
        let homeServer = NCUtilityFileSystem.shared.getHomeServer(account: account)
        let path = serverUrl.replacingOccurrences(of: homeServer, with: "")
        
        return path
    }
    
    static func permissionsContainsString(metadataPermissions: String, permissions: String) -> Bool {
        for char in permissions {
            if metadataPermissions.contains(char) == false {
                return false
            }
        }
        return true
    }
    
    static func removeForbiddenCharactersServer(fileName: String) -> String {
        return fileName.replacingOccurrences(of: "/", with: "")
    }
    
    static func removeForbiddenCharacrersFileSystem(fileName: String) -> String {
        let forbiddenCharacters = ["\\", "<", ">", ":", "\"", "|", "?", "*", "/"]
        var sanitisedFileName = fileName
        
        for char in forbiddenCharacters {
            sanitisedFileName = sanitisedFileName.replacingOccurrences(of: char, with: "")
        }
        
        return sanitisedFileName
    }
    
    static func stringAppendServerUrl(serverUrl: String, addFileName: String) -> String {
        if addFileName == "" {
            return serverUrl
        } else if serverUrl == "/" {
            return serverUrl.appending(addFileName)
        } else {
            return serverUrl + "/" + addFileName
        }
    }
    
    static func fileNamePath(metadataFileName: String, serverUrl: String, urlBase: String, account: String) -> String {
        let homeServer = NCUtilityFileSystem.shared.getHomeServer(account: account)
        var fileName = serverUrl.replacingOccurrences(of: homeServer, with: "") + "/" + metadataFileName
        
        if fileName.hasPrefix("/") {
            fileName.remove(at: fileName.index(before: fileName.endIndex))
        }
        
        return fileName
    }
    
    static func createOcIdentifierOnFileSystem(metadata: tableMetadata) {
        
        let itemIdentifier = getItemIdentifier(metadata: metadata)

        if metadata.directory {
            FileProviderUtils.getFileProviderDirectoryStorageOcId(ocId: itemIdentifier.rawValue)
        } else {
            FileProviderUtils.getFileProviderDirectoryStorageOcId(ocId: itemIdentifier.rawValue, fileNameView: metadata.fileNameView)
        }
    }
    
    static func getATime(path: String) -> Date? {
        do {
            let fileAttributes = try FileManager.default.attributesOfItem(atPath: path)
            return fileAttributes[.modificationDate] as! Date
        } catch {
            return nil
        }
    }
}
