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
    enum class BlockMode : uint8_t {free = 0, inUse = 1}; //block status
    enum class BlockType : uint8_t {data = 0, metaData = 1}; //block type (not really used yet)

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

        //to create new block
        void initializeBlock(const std::string &filename, 
            size_t blockNum, size_t totalBlocks, size_t fileSize, time_t timestamp) {
                mode = BlockMode::inUse;
                blockNumber = blockNum;
                blockCount = totalBlocks;
                this->fileSize = fileSize;
                timeStamp = timestamp;
                strncpy(this->filename, filename.c_str(), sizeof(this->filename) - 1);
                this->filename[sizeof(this->filename) - 1] = '\0'; //nullterm
            }


        //block header (metadata, should be less than 100 bytes)
        BlockMode mode; //current Block mode (free or in use)
        BlockType type; //0 = data, 1 = meta
        uint8_t blockNumber; //position in a sequence for multi-block file
        uint8_t blockCount; //how many blocks the current file uses

        //file info (part of header) //Q: Why is name 80? how can we fit file info into header along with above data?
        char filename[80]; //null-terminated string
        uint32_t fileSize; //total size of original file in bytes
        time_t timeStamp; //stores time file was added to archive

        //block data payload (924 bytes)
        uint8_t data[kPayloadSize]; 
    };

    //--------------------------------------------------------------------------------
    //BLOCK MANAGER: Block status class (to keep track of free/occupied blocks)
    //--------------------------------------------------------------------------------
    class BlockManager {
    public:
        BlockManager() = default;
        
        // Find free blocks for file storage
        std::vector<size_t> findFreeBlocks(size_t blockCount);
        
        // Mark blocks as used or free
        ArchiveStatus<bool> markBlocksAsUsed(const std::vector<size_t>& blocks);
        ArchiveStatus<bool> markBlocksAsFree(const std::vector<size_t>& blocks);
        
        // Track file locations
        ArchiveStatus<bool> addFileEntry(const std::string& filename, const std::vector<size_t>& blocks);
        ArchiveStatus<bool> removeFileEntry(const std::string& filename);
        ArchiveStatus<std::vector<size_t>> findFileEntry(const std::string& filename);
        
        // Get all file entries for listing
        std::map<std::string, std::vector<size_t>> getAllFileEntries() const;
        // return total block count
        size_t getTotalBlocks() const {
            return blockStatus.size();
        }
        
    private:
        //true = used, false = free
        std::vector<BlockMode> blockStatus; // Track free/used blocks
        std::map<std::string, std::vector<size_t>> fileEntries; // filename -> (startBlock, blockCount)
    };


    //BLOCK VISITOR: function to visit each block
    using BlockVisitor = std::function<bool(Block &aBlock, size_t aPos)>;
    
    //--------------------------------------------------------------------------------
    //CHUNKER: class to chunk file into blocks
    //--------------------------------------------------------------------------------
    class Chunker {

    protected:
    //& so we can take ptr to existing stream, not copy
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
                //process file data in at most 924 byte chunks
                theLen -= theDelta = std::min(size_t(924), theLen);
                stream.read(reinterpret_cast<char*>(theBlock.data), theDelta);

                //callback the visitor! to do whatever to each block, i.e. extract
                theResult = aVisitor(theBlock, thePos++);
            }
            
            //if all blocks processed, returns true
            //if we stopped early, returns false
            return theResult;
        }

        ~Chunker(); //not needed!
    };

    //What other classes/types do we need?
    //example code professor gave for Chunk class
    using VisitChunk = std::function<bool(Block &aBlock, size_t aPos)>;


    class Archive {
    protected:
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
    
        Archive(const std::string &aFullPath, AccessMode aMode);
        ~Archive();  
        
        //static factory methods to create/open archive
        static    ArchiveStatus<std::shared_ptr<Archive>> createArchive(const std::string &anArchiveName);
        static    ArchiveStatus<std::shared_ptr<Archive>> openArchive(const std::string &anArchiveName);

        //adds an observer to vector list (returns Archive& for chaining to same arc)
        Archive&  addObserver(std::shared_ptr<ArchiveObserver> anObserver);

        /*CORE METHODS For Interface*/
        ArchiveStatus<bool>      add(const std::string &aFilename); //Add a file
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath); //Extract a file
        ArchiveStatus<bool>      remove(const std::string &aFilename); //Remove a file
        ArchiveStatus<size_t>    list(std::ostream &aStream); //List files
        ArchiveStatus<size_t>    debugDump(std::ostream &aStream); //dumps architecture of block storage for debugging

        //returns new # blocks in compacted archive
        ArchiveStatus<size_t>    compact(); //compacts archive

        //UTILITY (get a file path)
        ArchiveStatus<std::string> getFullPath() const; //get archive path (including .arc extension)
    };
}
#endif /* Archive_hpp */
