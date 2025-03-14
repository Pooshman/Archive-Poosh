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

namespace ECE141 {
    //NOTE: enum is global scope, enum class is local scope (avoids name conflicts)
    enum class ActionType {added, extracted, removed, listed, dumped, compacted};
    //to tell if we are creating new archive or opening existing one
    enum class AccessMode {AsNew, AsExisting}; //you can change values (but not names) of this enum
    enum class BlockMode {free, inUse}; //block status

    /*
    Observer class to observe the actions of the archive
        - Kinda like a composite! (storing list of Observers, calling their operator())

    NOTE: If the user called the "list", "compact", or "dump" commands on your archive, there is no specific document. In that case, 
    just pass an empty string as the value for the `aName` argument to the observers, along with the action-type, and the 
    result status.
    */
    struct ArchiveObserver {
        void operator()(ActionType anAction, const std::string &aName, bool status);
    };

    /*
    Class to process files and reverse the process (i.e. compress, decompress)
    FOR ZIP FILES
    Not implementing right now, for next assignment (maybe final?)
    */
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

    template<typename T>
    class ArchiveStatus { //what's this class for? Define
    public:
        // Constructor for success case
        /*
        NOTE: explicit keyword means you can't do 
        ArchiveStatus<bool> status = true; (must be ArchiveStatus<bool> status(true);))
        */
        explicit ArchiveStatus(const T value)
                : value(value), error(ArchiveErrors::noError) {}

        // Constructor for error case
        explicit ArchiveStatus(ArchiveErrors anError)
                : value(std::nullopt), error(anError) {
            if (anError == ArchiveErrors::noError) {
                throw std::logic_error("Cannot use noError with error constructor");
            }
        }

        // Deleted copy constructor and copy assignment to make ArchiveStatus move-only
        ArchiveStatus(const ArchiveStatus&) = delete;
        ArchiveStatus& operator=(const ArchiveStatus&) = delete;

        // Default move constructor and move assignment
        ArchiveStatus(ArchiveStatus&&) noexcept = default;
        ArchiveStatus& operator=(ArchiveStatus&&) noexcept = default;

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


    //--------------------------------------------------------------------------------
    //You'll need to define your own classes for Blocks, and other useful types...
    //--------------------------------------------------------------------------------
    struct Block {
        /*
        Each block size = 1024 bytes exactly
        Block header <= 100 bytes
        Block payload >= 924 bytes 

        Block statuses = In Use, Free (stored in header as meta data)
        What else do we need to store in header?
         - Maybe info on where the next block is
         - how many blocks the current file uses

        NOTE: Must fill unused blocks first before appending new blocks
        */
        Block();
        Block(const Block &aBlock);
        Block& operator=(const Block &aBlock);
        ~Block();

        //block header (maybe we should make it a struct, enum?)
        //Should we store header as stream of bytes?
        //maybe some of this should be tracked in BlockStatus
        uint8_t status; //0 = free, 1 = in use
        uint8_t type; //0 = data, 1 = meta
        uint8_t blockNumber; //block number
        uint8_t blockCount; //how many blocks the current file uses
        uint8_t blockHash; //hash of block
        uint8_t blockLength; //length of block
        uint8_t blockDataLength; //length of block data
        uint8_t blockTypeLength; //length of block type

        //block data payload
        uint8_t blockData[924]; 
        BlockMode mode;
    };

    struct BlockStatus { //keep track of which blocks are free/occupied
        BlockStatus();
        BlockStatus(const BlockStatus &aBlockStatus) = delete;
        BlockStatus& operator=(const BlockStatus &aBlockStatus) = delete;
        ~BlockStatus();

        // Default move constructor and move assignment
        BlockStatus(BlockStatus&&) noexcept = default;
        BlockStatus& operator=(BlockStatus&&) noexcept = default;

        //block status(this was autocomplete)

    };

    using BlockVisitor = std::function<bool(Block &aBlock, size_t aPos)>;
    
    struct Chunker { //break up files into blocks
        Chunker(std::fstream &aStream) {
            aStream.seekg(0, std::ios::end);
            streamSize = aStream.tellg();
            aStream.seekg(0, std::ios::beg); //as if we never read
        }
        ~Chunker();
        //we need to split the file into blocks

        bool Chunk() {
        //step 2: divide file size by block size
        size_t theBlockCount = streamSize / 1024;
        //step 3: read file in chunks of block size
        for (size_t i = 0; i < theBlockCount; i++) {
            //read block
            blockData = aStream.read((char*)&theBlock, sizeof(Block));
            //write block
            createBlock(blockData); //writes block data to free block 
            //^updates block status, keeps list of blocks and block number, etc.
        }
        return true;
        }
        bool each(BlockVisitor aVisitor, Block &aBlock) { //function professor showed in class
            
        }  
        protected:
            std::fstream &stream;
            size_t streamSize;
            Block theBlock;
            std::vector<Block> blocks;  
    };

    //What other classes/types do we need?
    //example code professor gave for Chunk class
    using VisitChunk = std::function<bool(Block &aBlock, size_t aPos)>;


struct Chunker {
    Chunker(std::fstream &aStream) : stream(aStream) {
        stream.seekg(0, std::ios::end);
        sSize = stream.tellg();
        stream.seekg(0, std::ios::beg);
    }

    //visitor function
    bool each(VisitChunk aCallback, Block &aBlock) { //takes in lambda and block
        size_t theLen{sSize}, theDelta{0}, thePos{0};
        bool theResult{true};
        while(theLen && theResult) {
            theLen -= theDelta = std::min(kPayloadSize, static_cast<size_t>(theLen));
            stream.read(aBlock.payload, theDelta);
            theResult = aCallback(aBlock, thePos++);
        }
        return theResult;
    }
protected:
    std::fstream& stream;
    size_t sSize;


};
    
    class Archive {
    protected:
        std::vector<std::shared_ptr<IDataProcessor>> processors;
        std::vector<std::shared_ptr<ArchiveObserver>> observers;
        Archive(const std::string &aFullPath, AccessMode aMode);  //protected on purpose
        Block theBlock;
        std::fstream stream; //file stream
        std::string aPath; //do we need this?
        AccessMode mode; //do we need this?

    public:

        ~Archive();  
        static    ArchiveStatus<std::shared_ptr<Archive>> createArchive(const std::string &anArchiveName);
        static    ArchiveStatus<std::shared_ptr<Archive>> openArchive(const std::string &anArchiveName);

        //adds an observer to vector list (should it do anything else?)
        Archive&  addObserver(std::shared_ptr<ArchiveObserver> anObserver);

        /*
        We need Archive functions to add, extract, remove, list, debug, 
        compact (do we need that rn? IDataProcessor is for next assignment), read, write, etc.
        */
        Archive createArchive() {
            if (mode == AccessMode::AsNew) {
                //create the Archive
                //initialize stream, opener or something?
            }
            else {
                throw std::runtime_error("Cannot create archive in existing file");
            }
        };

        bool openArchive() {
            if (mode == AccessMode::AsExisting) {
                stream.open(aPath, std::ios::in | std::ios::out | std::ios::binary);
                if (!stream.is_open()) {
                    throw std::runtime_error("Failed to open archive");
                }
                return true;
            }
            else {
                throw std::runtime_error("Cannot open archive in new file");
            }
        }

        bool readBlock(Block &aBlock, size_t anIndex) {
            //move read pointer to block anIndex
            stream.seekg(anIndex * blockSize);
            //read block
            stream.read((char*)&aBlock, sizeof(Block));
            //stream error checking
            bool theResult{stream};
            return theResult;
        };
        bool writeBlock(Block &aBlock, size_t anIndex) {
            //move write pointer to index
            stream.seekp(anIndex * blockSize);
            //write to block
            stream.write((char*)&aBlock, sizeof(Block));
            //stream error checking
            bool theResult{stream}; //why {}? How does this work?
            return theResult;
        }
        bool addFile(const std::string &aPath) {
            //check duplicate, make sure path doesn't exist yet
            if (verifyPath(aPath)) {
                //open file stream
                std::fstream theStream(aPath);
                if (theStream.good()) {
                    Block theBlock;
                    Chunker theChunk;
                    //
                }
            }
            return false;
        }
        bool removeFile(const std::string &aPath) {
            if (verifyPath(aPath)) {
                //find blocks corresponding to file
                //remove blocks, mark them free (BLOCKSTATUS)
                //close file stream
                return true;
            }
            return false;
        }
        bool List(const std::string &aSequenceName) { //list files?
            return false;
        } 
        bool Extract(const std::string &aSequenceName) { //extract file data
            //is this aSequenceName a file path?

            return false;
        } 
        bool Debug() {return false;} //What does this do??
        bool Dump() {return false;} //What does this do??
        bool Compact() {return false;} //IDataProcessor is for next assignment
        

        ArchiveStatus<bool>      add(const std::string &aFilename);
        ArchiveStatus<bool>      extract(const std::string &aFilename, const std::string &aFullPath);
        ArchiveStatus<bool>      remove(const std::string &aFilename);

        ArchiveStatus<size_t>    list(std::ostream &aStream);
        ArchiveStatus<size_t>    debugDump(std::ostream &aStream);

        ArchiveStatus<size_t>    compact();
        ArchiveStatus<std::string> getFullPath() const; //get archive path (including .arc extension)



        //STUDENT: add anything else you want here, (e.g. blocks?)...
        //Do we want this as private?
    };

}

#endif /* Archive_hpp */
