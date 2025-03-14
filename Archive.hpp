//
//  Archive.hpp
//
//
//
//

#ifndef Archive_hpp
#define Archive_hpp

#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>
#include <functional>
#include <map>
#include <cstring>
#include <ctime>

namespace ECE141 {
    //NOTE: enum is global scope, enum class is local scope (avoids name conflicts)
    //you can change values (but not names) of this enum

    //GLOBAL ENUMS
    enum class ActionType {added, extracted, removed, listed, dumped, compacted}; //actions that can be performed on archive
    enum class AccessMode {AsNew, AsExisting}; //mode to open archive
    enum class BlockMode {free, inUse}; //block status

    /*
    NOTE: If the user called the "list", "compact", or "dump" commands on your archive, there is no specific document. In that case, 
    just pass an empty string as the value for the `aName` argument to the observers, along with the action-type, and the 
    result status.
    */

    //--------------------------------------------------------------------------------
    //ARCHIVE OBSERVER: class to observe the actions of the archive
    //- Kinda like a composite! (storing list of Observers, calling their operator())
    //--------------------------------------------------------------------------------    
    struct ArchiveObserver {
        virtual void operator()(ActionType anAction, const std::string &aName, bool status);
        virtual ~ArchiveObserver() = default;
    };


    //--------------------------------------------------------------------------------
    // IDATA PROCESSOR: Class to process files and reverse processing (i.e. compress, decompress)
    // FOR ZIP FILES
    // Not implementing right now, for next assignment (maybe final?)
    //--------------------------------------------------------------------------------
    class IDataProcessor {
    public:
        virtual std::vector<uint8_t> process(const std::vector<uint8_t>& input) = 0;
        virtual std::vector<uint8_t> reverseProcess(const std::vector<uint8_t>& input) = 0;
    };

    enum class ArchiveErrors {
        noError=0,
        fileNotFound=1, fileExists, fileOpenError, fileReadError, fileWriteError, fileCloseError,
        fileSeekError, fileTellError, fileError, badFilename, badPath, badData, badBlock, badArchive,
        badAction, badMode, badProcessor, badBlockType, badBlockCount, badBlockIndex, badBlockData,
        badBlockHash, badBlockNumber, badBlockLength, badBlockDataLength, badBlockTypeLength
    };

    //--------------------------------------------------------------------------------
    //ARCHIVE STATUS: error-handling wrapper class for Archive
    //--------------------------------------------------------------------------------
    template<typename T>
    class ArchiveStatus {
    public:
        // NOTE: explicit keyword means you can't do ArchiveStatus<bool> status = true; (must be ArchiveStatus<bool> status(true);))

        // Constructor for success case (initializes object without throwing exceptions)
        explicit ArchiveStatus(const T value)
                : value(value), error(ArchiveErrors::noError) {}

        // Constructor for error case
        explicit ArchiveStatus(ArchiveErrors anError)
                : value(std::nullopt), error(anError) {
            if (anError == ArchiveErrors::noError) {
                throw std::logic_error("Cannot use noError with error constructor");
            }
        }

        // NOTE: Delete copy constructor and copy assignment to make ArchiveStatus MOVE-ONLY
        ArchiveStatus(const ArchiveStatus&) = delete;
        ArchiveStatus& operator=(const ArchiveStatus&) = delete;

        // Default move constructor and move assignment
        ArchiveStatus(ArchiveStatus&&) noexcept = default;
        ArchiveStatus& operator=(ArchiveStatus&&) noexcept = default;

        //get returned value (if no error in operation)
        T getValue() const {
            if (!isOK()) {
                throw std::runtime_error("Operation failed with error");
            }
            return *value; //std::optional acts as smart pointer, so we dereference
        }

        //make sure value is not empty and error is noError
        bool isOK() const { return error == ArchiveErrors::noError && value.has_value(); }

        //return current error
        ArchiveErrors getError() const { return error; }

    private:
        std::optional<T> value; //can be T value or std::nullopt
        ArchiveErrors error;
    };

    // Block sizing constants
    constexpr size_t kBlockSize = 1024;
    constexpr size_t kMetaSize = 100;
    constexpr size_t kPayloadSize = kBlockSize - kMetaSize;

    //--------------------------------------------------------------------------------
    //You'll need to define your own classes for Blocks, and other useful types...
    //--------------------------------------------------------------------------------
    struct Block {
        /*
        Each block size = 1024 bytes exactly
        Block header <= 100 bytes
        Block payload >= 924 bytes 
        Block statuses = In Use, Free (stored in header as meta data)
        NOTE: Must fill unused blocks first before appending new blocks
        */

        Block();
        Block(const Block &aBlock); //copy constructor
        Block& operator=(const Block &aBlock); //copy assignment
        ~Block();

        //block header (metadata, should be less than 100 bytes)
        uint8_t status; //0 = free, 1 = in use
        uint8_t type; //0 = data, 1 = meta
        uint8_t blockNumber; //position in a sequence for multi-block file
        uint8_t blockCount; //how many blocks the current file uses

        //file info (part of header) //Q: Why is name 80? how can we fit file info into header along with above data?
        char fileName[80]; //null-terminated string
        uint32_t fileSize; //total size of original file in bytes
        time_t timeStamp; //stores time file was added to archive

        //block data payload (924 bytes)
        uint8_t data[kPayloadSize]; 

        //current Block mode (free or in use)
        BlockMode mode;
    };

    void Archive::notifyObservers(ActionType anAction, const std::string &aName, bool status) {
        for (auto& observer : observers) {
            (*observer)(anAction, aName, status);
        }
    }

    //--------------------------------------------------------------------------------
    //BLOCK MANAGER: Block status class (to keep track of free/occupied blocks)
    //--------------------------------------------------------------------------------
    class BlockManager {
    public:
        BlockManager() = default;
        
        // Find free blocks for file storage
        size_t findFreeBlocks(size_t blockCount);
        
        // Mark blocks as used or free
        bool markBlocksAsUsed(size_t startBlock, size_t count);
        bool markBlocksAsFree(size_t startBlock, size_t count);
        
        // Track file locations
        bool addFileEntry(const std::string& filename, size_t startBlock, size_t blockCount);
        bool removeFileEntry(const std::string& filename);
        std::pair<size_t, size_t> findFileEntry(const std::string& filename);
        
        // Get all file entries for listing
        std::map<std::string, std::pair<size_t, size_t>> getAllFileEntries() const;
        
    private:
        std::vector<bool> blockStatus; // Track free/used blocks
        std::map<std::string, std::pair<size_t, size_t>> fileEntries; // filename -> (startBlock, blockCount)
    };


    //BLOCK VISITOR: function to visit each block
    using BlockVisitor = std::function<bool(Block &aBlock, size_t aPos)>;
    
    //--------------------------------------------------------------------------------
    //CHUNKER: class to chunk file into blocks
    //--------------------------------------------------------------------------------
    class Chunker {

    protected:
        std::fstream &stream;
        size_t streamSize;

    public:
        Chunker(std::fstream &aStream) : stream(aStream) {
            stream.seekg(0, std::ios::end);
            streamSize = stream.tellg();
            stream.seekg(0, std::ios::beg);
        }
        
        bool each(BlockVisitor aVisitor) {
            // Process file in block-sized chunks
            size_t theLen = streamSize;
            size_t theDelta = 0;
            size_t thePos = 0;
            bool theResult = true;
            
            while (theLen && theResult) {
                Block theBlock;
                theLen -= theDelta = std::min(size_t(924), theLen);
                stream.read(reinterpret_cast<char*>(theBlock.data), theDelta);
                theResult = aVisitor(theBlock, thePos++);
            }
            
            return theResult;
        }
    };

    //What other classes/types do we need?
    //example code professor gave for Chunk class
    using VisitChunk = std::function<bool(Block &aBlock, size_t aPos)>;


    class Archive {
    protected:
        
        //constructor
        Archive(const std::string &aFullPath, AccessMode aMode);  //protected for factory pattern

        //read and write to block
        bool readBlock(Block& aBlock, size_t anIndex);
        bool writeBlock(Block& aBlock, size_t anIndex);

        //notify archive observers
        void notifyObservers(ActionType anAction, const std::string &aName, bool status);

        //UTILITY
        std::string extractFilename(const std::string &aFullPath) const; //extracts filename from path
        size_t calculateRequiredBlocks(size_t fileSize) const; //finds num blocks needed for file

        //data members
        std::fstream stream; //file stream
        std::string aPath; //file path
        AccessMode mode; //mode to tell whether it's existing or new archive
        BlockManager blockManager; //block manager to keep track of free/occupied blocks

        //to integrate later (during final?)
        std::vector<std::shared_ptr<IDataProcessor>> processors;
        std::vector<std::shared_ptr<ArchiveObserver>> observers;

    public:

        ~Archive();  

        //static factory methods to create/open archive
        static    ArchiveStatus<std::shared_ptr<Archive>> createArchive(const std::string &anArchiveName);
        static    ArchiveStatus<std::shared_ptr<Archive>> openArchive(const std::string &anArchiveName);

        //adds an observer to vector list
        Archive&  addObserver(std::shared_ptr<ArchiveObserver> anObserver);

        /*CORE METHODS For Interface*/
        ArchiveStatus<bool>      add(const std::string &aFilename); //Add a file
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath); //Extract a file
        ArchiveStatus<bool>      remove(const std::string &aFilename); //Remove a file
        ArchiveStatus<size_t>    list(std::ostream &aStream); //List files
        ArchiveStatus<size_t>    debugDump(std::ostream &aStream); //dumps architecture of block storage for debugging

        //next assignment:
        ArchiveStatus<size_t>    compact(); //compacts archive

        //UTILITY (get a file path)
        ArchiveStatus<std::string> getFullPath() const; //get archive path (including .arc extension)


        // //ROUGH FUNCTION IMPLEMENTATIONS
        // /*
        // We need Archive functions to add, extract, remove, list, debug, 
        // compact (do we need that rn? IDataProcessor is for next assignment), read, write, etc.
        // */
        // Archive createArchive() {
        //     if (mode == AccessMode::AsNew) {
        //         //create the Archive
        //         //initialize stream, opener or something?
        //     }
        //     else {
        //         throw std::runtime_error("Cannot create archive in existing file");
        //     }
        // };

        // bool openArchive() {
        //     if (mode == AccessMode::AsExisting) {
        //         stream.open(aPath, std::ios::in | std::ios::out | std::ios::binary);
        //         if (!stream.is_open()) {
        //             throw std::runtime_error("Failed to open archive");
        //         }
        //         return true;
        //     }
        //     else {
        //         throw std::runtime_error("Cannot open archive in new file");
        //     }
        // }

        // bool readBlock(Block &aBlock, size_t anIndex) {
        //     //move read pointer to block anIndex
        //     stream.seekg(anIndex * blockSize);
        //     //read block
        //     stream.read((char*)&aBlock, sizeof(Block));
        //     //stream error checking
        //     bool theResult{stream};
        //     return theResult;
        // };
        // bool writeBlock(Block &aBlock, size_t anIndex) {
        //     //move write pointer to index
        //     stream.seekp(anIndex * blockSize);
        //     //write to block
        //     stream.write((char*)&aBlock, sizeof(Block));
        //     //stream error checking
        //     bool theResult{stream}; //why {}? How does this work?
        //     return theResult;
        // }
        // bool addFile(const std::string &aPath) {
        //     //check duplicate, make sure path doesn't exist yet
        //     if (verifyPath(aPath)) {
        //         //open file stream
        //         std::fstream theStream(aPath);
        //         if (theStream.good()) {
        //             Block theBlock;
        //             Chunker theChunk;
        //             //
        //         }
        //     }
        //     return false;
        // }
        // bool removeFile(const std::string &aPath) {
        //     if (verifyPath(aPath)) {
        //         //find blocks corresponding to file
        //         //remove blocks, mark them free (BLOCKSTATUS)
        //         //close file stream
        //         return true;
        //     }
        //     return false;
        // }
        // bool List(const std::string &aSequenceName) { //list files?
        //     return false;
        // } 
        // bool Extract(const std::string &aSequenceName) { //extract file data
        //     //is this aSequenceName a file path?

        //     return false;
        // }
    };
}

#endif /* Archive_hpp */
