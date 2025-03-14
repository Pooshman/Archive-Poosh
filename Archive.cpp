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
    Block::Block() : status(0), type(0), blockNumber(0), blockCount(0), fileSize(0), 
                    timeStamp(0), mode(BlockMode::free) {
        memset(filename, 0, sizeof(filename));
        memset(data, 0, sizeof(data));
    }

    // Block copy constructor
    Block::Block(const Block &aBlock) : status(aBlock.status), type(aBlock.type),
                                    blockNumber(aBlock.blockNumber), blockCount(aBlock.blockCount),
                                    fileSize(aBlock.fileSize), timeStamp(aBlock.timeStamp),
                                    mode(aBlock.mode) {
        strncpy(filename, aBlock.filename, sizeof(filename));
        memcpy(data, aBlock.data, sizeof(data));
    }

    // Block assignment operator
    Block& Block::operator=(const Block &aBlock) {
        if (this != &aBlock) {
            status = aBlock.status;
            type = aBlock.type;
            blockNumber = aBlock.blockNumber;
            blockCount = aBlock.blockCount;
            fileSize = aBlock.fileSize;
            timeStamp = aBlock.timeStamp;
            mode = aBlock.mode;
            strncpy(filename, aBlock.filename, sizeof(filename));
            memcpy(data, aBlock.data, sizeof(data));
        }
        return *this;
    }

    // Block destructor
    Block::~Block() {
        // Nothing to clean up (all members are on stack)}
    }

    //READ BLOCK from the archive
    bool Archive::readBlock(Block &aBlock, size_t anIndex) {
        stream.seekg(anIndex * kBlockSize);
        if (!stream) return false;
        
        stream.read(reinterpret_cast<char*>(&aBlock), sizeof(Block));
        return stream.good();
    }

    //WRITE BLOCK to the archive
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
        //look through existing blocks
        Block theBlock;
        size_t theBlockCount = 0;
        while (readBlock(theBlock, theBlockCount)) {
            if (theBlock.mode == BlockMode::inUse && strcmp(theBlock.filename, theName.c_str()) == 0) {
                // File already exists in archive
                notifyObservers(ActionType::added, theName, false);
                return ArchiveStatus<bool>(ArchiveErrors::fileExists);
            }
            theBlockCount++;
        }
        
        //open the source file
        std::fstream sourceFile(aFilename, std::ios::in | std::ios::binary);
        if (!sourceFile) {
            notifyObservers(ActionType::added, theName, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileOpenError);
        }
        
        // get file size
        sourceFile.seekg(0, std::ios::end);
        size_t fileSize = sourceFile.tellg();
        sourceFile.seekg(0, std::ios::beg);
        
        // Calculate how many blocks we need
        size_t blocksNeeded = calculateRequiredBlocks(fileSize);
        
        //Find free blocks
        std::vector<size_t> freeBlocks;
        theBlockCount = 0;
        
        //First pass: count total blocks
        size_t totalBlocks = 0;
        while (readBlock(theBlock, totalBlocks)) {
            totalBlocks++;
        }
        
        //Second pass: find free blocks
        for (size_t i = 0; i < totalBlocks; i++) {
            if (readBlock(theBlock, i) && theBlock.mode == BlockMode::free) {
                freeBlocks.push_back(i);
                if (freeBlocks.size() == blocksNeeded) break;
            }
        }
        
        //if we don't have enough free blocks add new ones at the end
        while (freeBlocks.size() < blocksNeeded) {
            freeBlocks.push_back(totalBlocks++);
        }
        
        //prepare and write blocks
        size_t remainingSize = fileSize;
        time_t currentTime = time(nullptr);
        
        for (size_t i = 0; i < blocksNeeded; i++) {
            Block newBlock;
            newBlock.status = 1;  // In use
            newBlock.type = 0;    // Data block
            newBlock.blockNumber = i;
            newBlock.blockCount = blocksNeeded;
            newBlock.fileSize = fileSize;
            newBlock.timeStamp = currentTime;
            newBlock.mode = BlockMode::inUse;
            strncpy(newBlock.filename, theName.c_str(), sizeof(newBlock.filename) - 1);
            
            //read data from source file
            size_t bytesToRead = std::min(remainingSize, kPayloadSize);
            sourceFile.read(reinterpret_cast<char*>(newBlock.data), bytesToRead);
            remainingSize -= bytesToRead;
            
            //write block to archive
            if (!writeBlock(newBlock, freeBlocks[i])) {
                notifyObservers(ActionType::added, theName, false);
                return ArchiveStatus<bool>(ArchiveErrors::fileWriteError);
            }
        }
        
        notifyObservers(ActionType::added, theName, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //EXTRACT FILE FROM ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<bool> Archive::extract(const std::string &aFilename, const std::string &aFullPath) {
        // Find the file in the archive
        Block theBlock;
        bool fileFound = false;
        size_t blockIndex = 0;
        
        while (readBlock(theBlock, blockIndex)) {
            if (theBlock.mode == BlockMode::inUse && strcmp(theBlock.filename, aFilename.c_str()) == 0) {
                fileFound = true;
                break;
            }
            blockIndex++;
        }
        
        if (!fileFound) {
            notifyObservers(ActionType::extracted, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
        }
        
        //create output file
        std::fstream outputFile(aFullPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!outputFile) {
            notifyObservers(ActionType::extracted, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileOpenError);
        }
        
        //get info about the file
        size_t totalBlocks = theBlock.blockCount;
        size_t fileSize = theBlock.fileSize;
        size_t remainingSize = fileSize;
        
        //Find the first block
        size_t currentBlock = 0;
        while (currentBlock < blockIndex) {
            if (readBlock(theBlock, currentBlock) && 
                theBlock.mode == BlockMode::inUse && 
                strcmp(theBlock.filename, aFilename.c_str()) == 0 && 
                theBlock.blockNumber == 0) {
                break;
            }
            currentBlock++;
        }
        
        //extract all blocks in sequence
        for (size_t i = 0; i < totalBlocks; i++) {
            if (!readBlock(theBlock, currentBlock + i) || 
                theBlock.mode != BlockMode::inUse || 
                strcmp(theBlock.filename, aFilename.c_str()) != 0 ||
                theBlock.blockNumber != i) {
                
                //find next block
                bool foundNextBlock = false;
                for (size_t j = 0; j < blockIndex; j++) {
                    if (readBlock(theBlock, j) && 
                        theBlock.mode == BlockMode::inUse && 
                        strcmp(theBlock.filename, aFilename.c_str()) == 0 && 
                        theBlock.blockNumber == i) {
                        currentBlock = j - i;  //adjust the current block index
                        foundNextBlock = true;
                        break;
                    }
                }
                
                if (!foundNextBlock) {
                    outputFile.close();
                    notifyObservers(ActionType::extracted, aFilename, false);
                    return ArchiveStatus<bool>(ArchiveErrors::badBlock);
                }
            }
            
            //write data to output file
            size_t bytesToWrite = std::min(remainingSize, kPayloadSize);
            outputFile.write(reinterpret_cast<char*>(theBlock.data), bytesToWrite);
            remainingSize -= bytesToWrite;
        }
        
        outputFile.close();
        notifyObservers(ActionType::extracted, aFilename, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //REMOVE FILE FROM ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<bool> Archive::remove(const std::string &aFilename) {
        // Find all blocks for this file
        std::vector<size_t> blockIndices;
        Block theBlock;
        size_t blockIndex = 0;
        
        while (readBlock(theBlock, blockIndex)) {
            if (theBlock.mode == BlockMode::inUse && strcmp(theBlock.filename, aFilename.c_str()) == 0) {
                blockIndices.push_back(blockIndex);
            }
            blockIndex++;
        }
        
        if (blockIndices.empty()) {
            notifyObservers(ActionType::removed, aFilename, false);
            return ArchiveStatus<bool>(ArchiveErrors::fileNotFound);
        }
        
        // Mark all blocks as free
        for (size_t index : blockIndices) {
            if (readBlock(theBlock, index)) {
                theBlock.mode = BlockMode::free;
                theBlock.status = 0;
                memset(theBlock.filename, 0, sizeof(theBlock.filename));
                if (!writeBlock(theBlock, index)) {
                    notifyObservers(ActionType::removed, aFilename, false);
                    return ArchiveStatus<bool>(ArchiveErrors::fileWriteError);
                }
            }
        }
        
        notifyObservers(ActionType::removed, aFilename, true);
        return ArchiveStatus<bool>(true);
    }

    //--------------------------------------------------------------------------------
    //LIST ALL FILES IN ARCHIVE
    //--------------------------------------------------------------------------------
    ArchiveStatus<size_t> Archive::list(std::ostream &aStream) {
        std::map<std::string, std::pair<size_t, time_t>> fileInfo;
        Block theBlock;
        size_t blockIndex = 0;
        
        // Gather information about all files
        while (readBlock(theBlock, blockIndex)) {
            if (theBlock.mode == BlockMode::inUse) {
                // Only process first block of each file
                if (theBlock.blockNumber == 0) {
                    fileInfo[theBlock.filename] = std::make_pair(theBlock.fileSize, theBlock.timeStamp);
                }
            }
            blockIndex++;
        }
        
        // Output header
        aStream << "###  name         size       date added\n";
        aStream << "------------------------------------------------\n";
        
        // Output file information
        size_t fileNumber = 1;
        for (const auto &file : fileInfo) {
            char timeBuffer[32];
            struct tm *timeinfo = localtime(&file.second.second);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
            
            aStream << fileNumber << ".   "
                    << file.first << "    "
                    << file.second.first << "    "
                    << timeBuffer << "\n";
            
            fileNumber++;
        }
        
        notifyObservers(ActionType::listed, "", true);
        return ArchiveStatus<size_t>(fileInfo.size());
    }

    //--------------------------------------------------------------------------------
    //DUMP Block organization for DEBUGGING
    //--------------------------------------------------------------------------------
    ArchiveStatus<size_t> Archive::debugDump(std::ostream &aStream) {
        Block theBlock;
        size_t blockIndex = 0;
        size_t blockCount = 0;
        
        // Output header
        aStream << "###  status   name    \n";
        aStream << "-----------------------\n";
        
        // Examine all blocks
        while (readBlock(theBlock, blockIndex)) {
            blockCount++;
            
            aStream << blockIndex + 1 << ".   ";
            
            if (theBlock.mode == BlockMode::free) {
                aStream << "empty    \n";
            } else {
                aStream << "used     " << theBlock.filename << "\n";
            }
            
            blockIndex++;
        }
        
        notifyObservers(ActionType::dumped, "", true);
        return ArchiveStatus<size_t>(blockCount);
    }

    //--------------------------------------------------------------------------------
    //COMPACT ARCHIVE: TODO (FINAL???)
    //--------------------------------------------------------------------------------


} // namespace ECE141