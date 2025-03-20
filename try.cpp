
//createArchive returns a sharedptr to new Archive, with Error checking (ArchiveStatus)
ArchiveStatus<std::shared_ptr<Archive>> Archive::createArchive(std::string &anArchiveName) {
    //we want to overwrite files if they exist (truncate)
    std::string fullPath = anArchiveName + ".arc";
    //open stream:
    std::fstream theStream(anArchiveName, std::ios::binary | std::ios::out | std::ios::trunc);

    if(!theStream.is_open()) {
        return ArchiveStatus(ArchiveErrors::fileOpenError);
    }
    theStream.close();

    auto myArch = Archive& theArchive(fullPath, AccessMode::AsNew);

    myArch.stream = theStream;
    return ArchiveStatus<std::shared_ptr<Archive>> (myArch);
}

openArchive returns a sharedptr to existing Archive

ArchiveStatus<std::shared_ptr<Archive>> Archive::openArchive(std::string &anArchiveName) {
    //have to check if archive exists
    std::string fullPath = anArchiveName + ".arc";
    auto archExists = std::filesystem::exists(fullPath);

    if (!archExists) {
        return ArchiveStatus(ArchiveErrors::fileNotFound);
    }

    auto myArch = std::make_shared<Archive>(fullPath);
    //myArch is ptr, so must use -> instead of .
    myArch->stream.open(fullPath, std::ios::binary | std::ios::out | std::ios::in);

    if (!myArch->stream.is_open()) {
        return ArchiveStatus(ArchiveErrors::fileOpenError);
    }

    return ArchiveStatus<std::shared_ptr<Archive>> (myArch);
}


Archive& addObserver(std::shared_ptr<ArchiveObserver> anObserver) {
    observers.push_back(anObserver);
    return *this;
}

ArchiveStatus<std::string> Archive::getFullPath() {
    return ArchiveStatus<std::string> (aPath);
}

Block::Block() : status(0), type(0), blockNumber(0), blockCount(0),
            fileSize(0), timeStamp(0), mode(BlockMode::free) {
                memset(filename, 0, sizeof(filename));
                memset(data, 0, sizeof(data));
            }

Block::Block(const Block &aBlock) : status(aBlock.status), type(aBlock.type), 
            blockNumber(aBlock.blockNumber), blockCount(aBlock.blockCount), 
            fileSize(aBlock.fileSize), timeStamp(aBlock.timeStamp),
            mode(aBlock.mode) {
                strncpy(filename, aBlock.filename, sizeof(filename));
                memcpy(data, aBlock.data, sizeof(data));
            }

Block::Block& operator=(const Block &aBlock) {
    if (aBlock != *this) {
        status = aBlock.status;
        and so on...
    }
    return *this;
}


size_t BlockManager::findFreeBlocks(size_t blockCount) {
    //iterate through file entries

}

bool Archive::readBlock(Block& aBlock, size_t anIndex) {
    stream.seekg(anIndex*1024);
    if (!stream) {return false;}

    stream.read(aBlock, sizeof(aBlock));

}


bool Archive::writeBlock(Block& aBlock, size_t anIndex) {

    
}

//need blocks/chunking to add file
ArchiveStatus<bool> Archive::add(const sstd::string &aFilename) {
    thePath = getFullPath(aFilename);

}

ArchiveStatus<bool> Archive::extract(const std::string &aFilename, const std::string &aFullPath) {

}
ArchiveStatus<bool> Archive::remove(const std::string &aFilename) {

}
ArchiveStatus<size_t> Archive::list(std::ostream &aStream) {

}
ArchiveStatus<size_t> Archive::debugDump(std::ostream &aStream) {

}
