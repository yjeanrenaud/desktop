//
//  FileProviderUtils.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 13/5/22.
//

import Foundation
import FileProvider

class FileProviderUtils {
    
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
    
    static func createOcIdentifierOnFileSystem(metadata: tableMetadata) {
        
        let itemIdentifier = getItemIdentifier(metadata: metadata)

        if metadata.directory {
            NCUtils.getFileProviderDirectoryStorageOcId(ocId: itemIdentifier.rawValue)
        } else {
            NCUtils.getFileProviderDirectoryStorageOcId(ocId: itemIdentifier.rawValue, fileNameView: metadata.fileNameView)
        }
    }
}
