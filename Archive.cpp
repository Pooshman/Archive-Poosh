//
//  Archive.cpp
//
//
//
//

#include "Archive.hpp"
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace ECE141 {
    // Default implementation for ArchiveObserver
    void ArchiveObserver::operator()(ActionType anAction, const std::string &aName, bool status) {
        // Default implementation does nothing, subclasses will override
    }

    // Archive constructor
    Archive::Archive(const std::string &aFullPath, AccessMode aMode) 
    : aPath(aFullPath), mode(aMode) {
        // Ensure path ends with .arc extension
        if (aPath.substr(aPath.length() - 4) != ".arc") {
            aPath += ".arc";
        }
    }

    // Archive destructor
    Archive::~Archive() {
        if (stream.is_open()) {
            stream.close();
        }
    }

    //--------------------------------------------------------------------------------
    //OPENING/CLOSING ARCHIVES
    //--------------------------------------------------------------------------------
    // Static factory method to create a new archive
    ArchiveStatus<std::shared_ptr<Archive>> Archive::createArchive(const std::string &anArchiveName) {
        std::string theFullPath = anArchiveName + ".arc";
        
        // Create a new archive file (truncate/erase if exists)
        std::fstream theStream(theFullPath, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!theStream.is_open()) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileOpenError);
        }
        theStream.close();
        
        // Create and return a new Archive object
        auto theArchive = std::make_shared<Archive>(anArchiveName, AccessMode::AsNew);
        theArchive->stream.open(theFullPath, std::ios::binary | std::ios::in | std::ios::out);
        
        if (!theArchive->stream.is_open()) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileOpenError);
        }
        
        return ArchiveStatus<std::shared_ptr<Archive>>(theArchive);
    }

    // Static factory method to open an existing archive
    ArchiveStatus<std::shared_ptr<Archive>> Archive::openArchive(const std::string &anArchiveName) {
        std::string theFullPath = anArchiveName + ".arc";
        
        // Check if file exists
        if (!fs::exists(theFullPath)) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileNotFound);
        }
        
        // Open the existing archive
        auto theArchive = std::make_shared<Archive>(anArchiveName, AccessMode::AsExisting);
        theArchive->stream.open(theFullPath, std::ios::binary | std::ios::in | std::ios::out);
        
        if (!theArchive->stream.is_open()) {
            return ArchiveStatus<std::shared_ptr<Archive>>(ArchiveErrors::fileOpenError);
        }
        
        return ArchiveStatus<std::shared_ptr<Archive>>(theArchive);
    }

    // Add an observer to the archive
    Archive& Archive::addObserver(std::shared_ptr<ArchiveObserver> anObserver) {
        observers.push_back(anObserver);
        return *this;
    }

    //GET FULL PATH of Archive
    ArchiveStatus<std::string> Archive::getFullPath() const {
        return ArchiveStatus<std::string>(aPath);
    }

    //EXTRACT filename from a full path
    std::string Archive::extractFilename(const std::string &aFullPath) const {
        fs::path thePath(aFullPath);
        return thePath.filename().string();
    }

    // Calculate number of blocks needed for a file
    size_t Archive::calculateRequiredBlocks(size_t fileSize) const {
        return (fileSize + kPayloadSize - 1) / kPayloadSize; // Ceiling division
    }

    // Notify all observers about an action
    void Archive::notifyObservers(ActionType anAction, const std::string &aName, bool status) {
        for (auto& observer : observers) {
            (*observer)(anAction, aName, status);
        }
    }

//--------------------------------------------------------------------------------
//BLOCK METHODS
//--------------------------------------------------------------------------------
    // Block constructor
    Block::Block() : mode(BlockMode::free), type(BlockType::data), blockNumber(0), blockCount(0), fileSize(0), 
                    timeStamp(0) {
        //to null all bytes for filename/data, must use memset
        memset(filename, 0, sizeof(filename));
        memset(data, 0, sizeof(data));
    }

    // Block copy constructor
    Block::Block(const Block &aBlock) : mode(aBlock.mode), type(aBlock.type),
                                    blockNumber(aBlock.blockNumber), blockCount(aBlock.blockCount),
                                    fileSize(aBlock.fileSize), timeStamp(aBlock.timeStamp) {
        strncpy(filename, aBlock.filename, sizeof(filename));
        memcpy(data, aBlock.data, sizeof(data));
    }

    // Block assignment operator
    //NOTE: can't use initializer list bc objects already initialized!
            //so must overwrite, hence doing it within operator def
    Block& Block::operator=(const Block &aBlock) {
        if (this != &aBlock) {
            mode = aBlock.mode;
            type = aBlock.type;
            blockNumber = aBlock.blockNumber;
            blockCount = aBlock.blockCount;
            fileSize = aBlock.fileSize;
            timeStamp = aBlock.timeStamp;
            strncpy(filename, aBlock.filename, sizeof(filename));
            memcpy(data, aBlock.data, sizeof(data));
        }
        return *this;
    }

    // Block destructor
    Block::~Block() {
        // Nothing to clean up (all members are on stack)}
    }

    //READ BLOCK from the archive (puts data from stream into aBlock)
    bool Archive::readBlock(Block &aBlock, size_t anIndex) {
        stream.seekg(anIndex * kBlockSize);
        if (!stream) return false;
        
        stream.read(reinterpret_cast<char*>(&aBlock), sizeof(Block));
        return stream.good();
    }

    //WRITE BLOCK to the archive (puts data from aBlock into stream)
    bool Archive::writeBlock(Block &aBlock, size_t anIndex) {
        stream.seekp(anIndex * kBlockSize);
        if (!stream) return false;
        
        stream.write(reinterpret_cast<const char*>(&aBlock), sizeof(Block));
        return stream.good();
    }

    //--------------------------------------------------------------------------------
    //ADD FILE TO ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<bool> Archive::add(const std::string &aFilename) {
        // Extract just the filename part from the full path
        std::string theName = extractFilename(aFilename);
        
        //check if file already exists in the archive
        if (blockManager.findFileEntry(theName).isOK()) {
            notifyObservers(ActionType::added, theName, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileExists);
        }
        
        //open the source file
        std::fstream sourceFile(aFilename, std::ios::in | std::ios::binary);
        if (!sourceFile) {
            notifyObservers(ActionType::added, theName, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileOpenError);
        }
        
        //get file size and calculate blocks needed
        sourceFile.seekg(0, std::ios::end);
        size_t fileSize = sourceFile.tellg();
        sourceFile.seekg(0, std::ios::beg);
        size_t blocksNeeded = calculateRequiredBlocks(fileSize);
        
        //Find free blocks
        std::vector<size_t> freeBlocks = blockManager.findFreeBlocks(blocksNeeded);
        
        //if not enough free blocks, make more space
        while (freeBlocks.size() < blocksNeeded) {
            freeBlocks.push_back(blockManager.getTotalBlocks());
            blockManager.markBlocksAsUsed({freeBlocks.back()});
        }

        //mark blocks as used
        blockManager.markBlocksAsUsed(freeBlocks);
        
        //prepare and write blocks
        size_t remainingSize = fileSize;
        time_t currentTime = time(nullptr);
        
        for (size_t i = 0; i < blocksNeeded; i++) {
            Block newBlock;
            newBlock.initializeBlock(theName, i, blocksNeeded, fileSize, currentTime);
            
            //read data from source file
            size_t bytesToRead = std::min(remainingSize, kPayloadSize);
            sourceFile.read(reinterpret_cast<char*>(newBlock.data), bytesToRead);
            remainingSize -= bytesToRead;
            
            //write blocks to stream
            writeBlock(newBlock, freeBlocks[i]);
        }
        
        //store the file entry
        blockManager.addFileEntry(theName, freeBlocks);
        notifyObservers(ActionType::added, theName, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //EXTRACT FILE FROM ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<bool> Archive::extract(const std::string &aFilename, const std::string &aFullPath) {
        // find file in archive
        auto fileBlocks = blockManager.findFileEntry(aFilename);
        if (!fileBlocks.isOK()) {
            notifyObservers(ActionType::extracted, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
        }
        
        //get file blocks (check if empty)
        std::vector<size_t> blocks = fileBlocks.getValue();
        if (blocks.empty()) {
            notifyObservers(ActionType::extracted, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::badBlock);
        }

        //open output file
        std::fstream outputFile(aFullPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!outputFile) {
            notifyObservers(ActionType::extracted, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileOpenError);
        }

        Block theBlock;
        size_t remainingSize = 0;

        //read blocks and write to output file
        for (size_t blockIndex : blocks) {
            if (!readBlock(theBlock, blockIndex)) {
                notifyObservers(ActionType::extracted, aFilename, false);
                return ArchiveStatus<bool>(ArchiveErrors::badBlock);
            }

            size_t bytesToWrite = std::min(theBlock.fileSize - remainingSize, kPayloadSize);
            outputFile.write(reinterpret_cast<char*>(theBlock.data), bytesToWrite);
            remainingSize += bytesToWrite;
        }
        outputFile.close();
        notifyObservers(ActionType::extracted, aFilename, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //REMOVE FILE FROM ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<bool> Archive::remove(const std::string &aFilename) {
        //find all blocks for this file
        auto fileBlocks = blockManager.findFileEntry(aFilename);
        if (!fileBlocks.isOK()) {
            notifyObservers(ActionType::removed, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
        }
        //mark blocks free and remove file entry
        blockManager.markBlocksAsFree(fileBlocks.getValue());
        blockManager.removeFileEntry(aFilename);

        notifyObservers(ActionType::removed, aFilename, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //LIST ALL FILES IN ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<size_t> Archive::list(std::ostream &aStream) {
        auto fileEntries = blockManager.getAllFileEntries();    
        
        //output header with NAME/SIZE/TIMESTAMP
        aStream << "###  name         size       date added\n";
        aStream << "------------------------------------------------\n";
        
        // Output file information
        size_t fileNumber = 1;
        for (const auto &file : fileEntries) {
            Block theBlock;

            //read first block to get file info
            if (!readBlock(theBlock, file.second[0])) {
                continue;
            }

            char timeBuffer[32];
            struct tm *timeinfo = localtime(&theBlock.timeStamp);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
            
            aStream << fileNumber << ".   "
                    << file.first << "    "
                    << theBlock.fileSize << "    "
                    << timeBuffer << "\n";
            
            fileNumber++;
        }
        
        notifyObservers(ActionType::listed, "", true);
        return ArchiveStatus<size_t>(fileEntries.size());
    }

    //--------------------------------------------------------------------------------
    //DUMP Block organization for DEBUGGING
    //--------------------------------------------------------------------------------
    ArchiveStatus<size_t> Archive::debugDump(std::ostream &aStream) {
        auto fileEntries = blockManager.getAllFileEntries();
        size_t blockCount = blockManager.getTotalBlocks();
        
        // Output header
        aStream << "###  Block #   Status   Filename\n";
        aStream << "----------------------------------\n";
        
        // Examine all blocks
        for (size_t i = 0; i < blockCount; i++) {
            std::string fileName = "empty";
            for (const auto &file : fileEntries) {
                if (std::find(file.second.begin(), file.second.end(), i) != file.second.end()) {
                    fileName = file.first;
                    break;
                }
            }
            aStream << i << ".   "
                    << (blockManager.findFileEntry(fileName).isOK() ? "in use" : "free") << "   "
                    << fileName << "\n";
        }
        
        notifyObservers(ActionType::dumped, "", true);
        return ArchiveStatus<size_t>(blockCount);
    }

    //--------------------------------------------------------------------------------
    //BLOCK MANAGER FUNCTIONS
    //--------------------------------------------------------------------------------

    std::vector<size_t> BlockManager::findFreeBlocks(size_t blockCount) {
        //Find free blocks
        std::vector<size_t> freeBlocks;
        for (size_t i=0;i<<blockStatus.size(); i++) {
            if (blockStatus[i] == BlockMode::free) {
                freeBlocks.push_back(i);
                if (freeBlocks.size() == blockCount) {
                    break;
                }
            }
        }
        return freeBlocks;
    }

    ArchiveStatus<bool> BlockManager::markBlocksAsUsed(const std::vector<size_t>& blocks) {
        for (size_t block : blocks) {
            if (block >= blockStatus.size()) {
                return ArchiveStatus<bool>(ArchiveErrors::badBlockIndex);
            }
            blockStatus[block] = BlockMode::inUse;
        }
        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<bool> BlockManager::markBlocksAsFree(const std::vector<size_t>& blocks) {
        for (size_t block : blocks) {
            if (block >= blockStatus.size()) {
                return ArchiveStatus<bool>(ArchiveErrors::badBlockIndex);
            }
            blockStatus[block] = BlockMode::free;
        }
        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<bool> BlockManager::addFileEntry(const std::string& filename, const std::vector<size_t>& blocks) {
        auto file = fileEntries.find(filename);
        if (file != fileEntries.end()) {
            return ArchiveStatus<bool>(ArchiveErrors::fileExists);
        }

        fileEntries[filename] = blocks;

        //update blockStatus
        for (size_t block : blocks) {
            blockStatus[block] = BlockMode::inUse;
        }

        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<bool> BlockManager::removeFileEntry(const std::string& filename) {
        auto file = fileEntries.find(filename);
        if (file == fileEntries.end()) {
            return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
        }

        //update blockStatus
        for (size_t block : file->second) {
            blockStatus[block] = BlockMode::free;
        }

        fileEntries.erase(filename);
        return ArchiveStatus<bool>(true);
    }

    ArchiveStatus<std::vector<size_t>> BlockManager::findFileEntry(const std::string& filename) {
        auto file = fileEntries.find(filename);
        if (file != fileEntries.end()) {
            return ArchiveStatus<std::vector<size_t>>(file->second);
        }
        return ArchiveStatus<std::vector<size_t>>(ArchiveErrors::fileNotFound);
    }
        
    std::map<std::string, std::vector<size_t>> BlockManager::getAllFileEntries() const {
        return fileEntries;
    }

    //--------------------------------------------------------------------------------
    //COMPACT ARCHIVE: TODO (FINAL???)
    //--------------------------------------------------------------------------------
    ArchiveStatus<size_t> Archive::compact() {
        auto fileEntries = blockManager.getAllFileEntries();
        std::vector<Block> newBlocks;
        std::map<std::string, std::vector<size_t>> newFileEntries;
    
        size_t newBlockIndex = 0;
    
        for (const auto& file : fileEntries) {
            std::vector<size_t> oldBlocks = file.second;
            std::vector<size_t> newBlockList;
            for (size_t oldBlock : oldBlocks) {
                Block tempBlock;
                if (readBlock(tempBlock, oldBlock)) {
                    newBlocks.push_back(tempBlock);
                    newBlockList.push_back(newBlockIndex++);
                }
            }
            newFileEntries[file.first] = newBlockList;
        }

        //rewrite archive
        stream.close();
        stream.open(aPath, std::ios::binary | std::ios::out | std::ios::trunc);
        for (size_t i=0;i<newBlocks.size();i++) {
            writeBlock(newBlocks[i], i);
        }

        //update blockManager
        blockManager = BlockManager(); //reset

        //add file entries
        for (const auto& file : newFileEntries) {
            blockManager.addFileEntry(file.first, file.second);
        }

        notifyObservers(ActionType::compacted, "", true);
        return ArchiveStatus<size_t>(newBlocks.size());
    }

} // namespace ECE141