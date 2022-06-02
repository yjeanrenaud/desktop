//
//  FileProviderData.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 25/5/22.
//

import Foundation
import NCCommunication
import FileProvider

class FileProviderData: NSObject {
    static let shared: FileProviderData = {
        let instance = FileProviderData()
        return instance
    }()
    
    var domain: NSFileProviderDomain?
    var fileProviderManager: NSFileProviderManager?
    
    // Max item for page
    let itemForPage = 100

    // Anchor
    var currentAnchor: UInt64 = 0

    // Rank favorite
    var listFavoriteIdentifierRank: [String: NSNumber] = [:]

    // Item for signalEnumerator
    var fileProviderSignalDeleteContainerItemIdentifier: [NSFileProviderItemIdentifier: NSFileProviderItemIdentifier] = [:]
    var fileProviderSignalUpdateContainerItem: [NSFileProviderItemIdentifier: FileProviderItem] = [:]
    var fileProviderSignalDeleteWorkingSetItemIdentifier: [NSFileProviderItemIdentifier: NSFileProviderItemIdentifier] = [:]
    var fileProviderSignalUpdateWorkingSetItem: [NSFileProviderItemIdentifier: FileProviderItem] = [:]
    
    // Error
    enum FileProviderError: Error {
        case downloadError
        case uploadError
    }
    
    /*@discardableResult
    func signalEnumerator(ocId: String, delete: Bool = false, update: Bool = false) -> FileProviderItem? {

        guard let metadata = NCManageDatabase.shared.getMetadataFromOcId(ocId) else { return nil }

        guard let parentItemIdentifier = FileProviderUtils.getParentItemIdentifier(metadata: metadata) else { return nil }

        let item = FileProviderItem(metadata: metadata, parentItemIdentifier: parentItemIdentifier)

        if delete {
            FileProviderData.shared.fileProviderSignalDeleteContainerItemIdentifier[item.itemIdentifier] = item.itemIdentifier
            FileProviderData.shared.fileProviderSignalDeleteWorkingSetItemIdentifier[item.itemIdentifier] = item.itemIdentifier
        }

        if update {
            FileProviderData.shared.fileProviderSignalUpdateContainerItem[item.itemIdentifier] = item
            FileProviderData.shared.fileProviderSignalUpdateWorkingSetItem[item.itemIdentifier] = item
        }

        if !update && !delete {
            FileProviderData.shared.fileProviderSignalUpdateWorkingSetItem[item.itemIdentifier] = item
        }

        if update || delete {
            currentAnchor += 1
            fileProviderManager?.signalEnumerator(for: parentItemIdentifier) { _ in }
        }

        fileProviderManager?.signalEnumerator(for: .workingSet) { _ in }

        return item
    }*/
    
}
