#include <stdint.h>
#include <pico/stdlib.h>
#include <string>

#define END_OF_FILE 0xfff

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20


#define LAST_LONG_NAME 0x40


#define FILE_IO_READ 0x01
#define FILE_IO_WRITE 0x02


#define FILE_MODE_READ  0x00
#define FILE_MODE_WRITE 0x01
#define FILE_MODE_TRUNC 0x00
#define FILE_MODE_APP   0x02


#pragma pack(1)
struct BPB{
    uint8_t  BS_jmpBoot[3];
    char     BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint32_t BS_VolID;
    char     BS_VolLab[11];
    char     BS_FilSysType[8];

    void print();
};


struct FileEntry{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes =0;//SHould always be 0
    uint8_t DIR_CrtTimeTenth; // 0 <= && <= 199
    uint16_t DIR_CrtTime; // Granularity 2 secs
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;

  
};



#pragma pop
static_assert(sizeof(BPB)==62);
static_assert(sizeof(FileEntry)==32);

const uint8_t Signature_word[2] = {0x55,0xAA};



struct Directory{
    uint16_t fat_entry;
};

struct FileHandle{
    uint16_t direntry;
    uint16_t dirindex;
    
};

struct FileIOHandle{

    FileHandle handle;
    uint16_t currentoffset;
    uint16_t currentAU;
    uint8_t mode;
};

typedef  uint16_t FatIterator;

enum Status{
    OK,
    ERROR,
    NO_MAGICBYTES,
    NULLPOINTER_PROVIDED,
    OUT_OF_SPACE,
    INDEX_OUT_OF_RANGE,
    DIRECTORY_NOT_EMPTY,
    END
};

enum BytesPerSector{
    B512 =512,
    B1024=1024,
    B2048=2048,
    B4096=4096
};

template<typename T>
struct Result {
    int status;
    T val;
    inline bool Ok(){
        return status == 0;
    }
};
struct none{};

class FAT12{
    public:
    uint8_t* disk;
    size_t disk_size;
    BPB bpb;
    Result<uint16_t> GetFAT12_entry(size_t index);
    Result<none> SetFAT12_entry(size_t index,uint16_t value);
    Result<none> ReadFirst512bytes(BPB*out);
    static bool IsFAT12(const BPB*bpb);
    Result<none> ClearFAT();
    FatIterator IterateFat(FatIterator* it);
    Result<none> InitRootDir();
    Result<uint16_t> GetNextFreeCluster();
    Result<none> ClearCluster(uint16_t index);
    Result<size_t> OffsetToCluster(uint16_t index);
    Result<size_t> OffsetToFileHandle(FileHandle filehandle);
    inline size_t OffsetToFat()const;
    inline size_t OffsetToRootDir()const;
    inline size_t OffsetToFirstCluster()const;
    inline size_t RootDirSize()const;
    inline size_t FatSize()const;
    inline size_t GetNumberOfValidFatEntries()const;
    inline size_t GetAllocationUnitSize()const;
    Result<FileEntry*> GetFileEntryFromHanlde(FileHandle filehandle, FileEntry * fileentryout);
    bool FatIteratorOK(FatIterator it);
    bool DirIsDotOrDotDot(FileEntry *fileentry);
public:
    FAT12(uint8_t* disk,size_t disk_size);
    
    uint32_t GetFreeDiskSpaceAmount();
    Result<none> AllocateNewEntryInDir(Directory dir, FileHandle* out_entry);
    Result<none> Format(const char* volumename, BytesPerSector bytespersector,uint8_t SectorPerClusters, bool dual_FATs);
    Result<none> CreateDir(const char name[8],const char extension[3],Directory parent,FileHandle* filehandle);
    Result<bool> DirectoryEmpty(Directory directory);
    Result<none> CreateFile(const char name[8],const char extension[3],Directory parent,FileHandle* filehandle);

    Result<none> DeleteFile(FileHandle* filehandle);
    Result<FileIOHandle> Open(FileHandle file, uint8_t mode);
    Result<none> Close(FileIOHandle* file);
    int Read(FileIOHandle& file,uint8_t * buffer, size_t buffersize);
    //int Write(File& file,uint8_t * buffer, size_t buffersize);
    int SectorSerialDump(size_t index);

    Result<none> Mount();


};
inline size_t FAT12::OffsetToFat()const
{
    return bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec;
}

inline size_t FAT12::OffsetToRootDir()const
{
    return OffsetToFat() + FatSize();
}

inline size_t FAT12::OffsetToFirstCluster()const
{
    return OffsetToRootDir()+RootDirSize();
}

inline size_t FAT12::RootDirSize()const
{
    return bpb.BPB_RootEntCnt*sizeof(FileEntry);
}

inline size_t FAT12::FatSize()const
{
    return bpb.BPB_NumFATs * bpb.BPB_FATSz16 * bpb.BPB_BytsPerSec;
}

inline size_t FAT12::GetNumberOfValidFatEntries()const
{
    return (bpb.BPB_TotSec16*bpb.BPB_BytsPerSec - OffsetToFirstCluster())/(bpb.BPB_SecPerClus*bpb.BPB_BytsPerSec) + 2;
}

inline size_t FAT12::GetAllocationUnitSize() const
{
    return bpb.BPB_BytsPerSec*bpb.BPB_SecPerClus;
}
