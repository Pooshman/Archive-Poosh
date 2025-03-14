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

} // namespace ECE141