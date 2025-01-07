#ifndef FAT12_H
#define FAT12_H

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


#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define LAST_LONG_ENTRY 0x40


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




const char SHORTNAME_LEGAL_CHARACHTERS[]="ABCDEFGHIJKLMNOPQRSTUVXYZ$%'-_@~`!(){}^#&0123456789";

#define SHORTNAME_LEN 11

struct FileEntry{
    char DIR_Name[SHORTNAME_LEN];
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


struct LongNameEntry{
    uint8_t LDIR_ord;
    char LDIR_Name1[10];
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;// zero
    uint8_t LDIR_Chksum;
    char LDIR_Name2[12];
    uint16_t LDIR_FstClusLO; // zero
    char LDIR_Name3[4];
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
    LONGFILEENTRY_IS_CORRUPTED,
    FILE_DOES_NOT_EXIST,
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

#define HANDLE_ERROR(outtype,out,func,error) outtype out;{auto result = func;if(!result.Ok()){error;}out = result.val;}


class FAT12{
    public:
    uint8_t* disk;
    size_t disk_size;
    BPB bpb;
    Result<uint16_t> GetFAT12_entry(size_t index);
    Result<uint16_t> GetFAT12_reverse_entry(size_t value);
    Result<none> SetFAT12_entry(size_t index,uint16_t value);
    Result<none> ReadFirst512bytes(BPB*out);
    static bool IsFAT12(const BPB*bpb);
    Result<none> InitFAT();
    FatIterator IterateFat(FatIterator* it);
    Result<none> InitRootDir();
    Result<uint16_t> GetNextFreeCluster();
    Result<none> ClearCluster(uint16_t index);
    Result<size_t> OffsetToCluster(uint16_t index);
    Result<size_t> OffsetToFileHandle(FileHandle filehandle);
    Result<FileHandle> GetNextEntryInDir(FileHandle fh);
    Result<FileHandle> GetPreviousEntryInDir(FileHandle fh);

    inline size_t GetSizeOfCluster(uint16_t cluster)const;
    inline size_t OffsetToFat()const;
    inline size_t OffsetToRootDir()const;
    inline size_t OffsetToFirstCluster()const;
    inline size_t RootDirSize()const;
    inline size_t FatSize()const;
    inline size_t GetNumberOfValidFatEntries()const;
    inline size_t GetAllocationUnitSize()const;
    inline size_t GetNumberOfFileEntriesPerCluster(size_t cluster) const;

    Result<FileEntry*> GetFileEntryFromHanlde(FileHandle filehandle, FileEntry * fileentryout);
    bool FatIteratorOK(FatIterator it);
    bool DirIsDotOrDotDot(FileEntry *fileentry);


    Result<none> CreateShortNameFromLongName(char* shortname_out, const char* longname, size_t longname_len,Directory dir);
    Result<none> ShortNameifyIfValid(char* shortname_out,const char* longname,size_t longname_len);
     
    uint8_t LongNameChecksum(const char shortname[SHORTNAME_LEN]);

    Result<none> CreateLongFileNameEntry(const char* name, size_t len, Directory dir, FileHandle* filehandle);
    Result<none> AllocateMultipleEntriesInDir(Directory dir,size_t count,FileHandle* first, FileHandle* last);

    Result<FileHandle> GetShortNameInDir(Directory dir, const char* shortname, size_t shortname_len);
    Result<FileHandle> GetLongNameInDir(Directory dir, const char* longname, size_t longname_len);
public:
    FAT12(uint8_t* disk,size_t disk_size);
    
    uint32_t GetFreeDiskSpaceAmount();
    Result<none> AllocateNewEntryInDir(Directory dir, FileHandle* out_entry);
    Result<none> Format(const char* volumename, BytesPerSector bytespersector,uint8_t SectorPerClusters, bool dual_FATs);
    Result<none> CreateDir(const char name[8],const char extension[3],Directory parent,FileHandle* filehandle);
    Result<bool> DirectoryEmpty(Directory directory);
    Result<none> CreateFile(const char name[8],const char extension[3],Directory parent,FileHandle* filehandle);

    Result<none> DeleteFile(FileHandle filehandle);
    Result<FileIOHandle> Open(FileHandle file, uint8_t mode);
    Result<none> Close(FileIOHandle* file);
    Result<size_t> Read(FileIOHandle& file,uint8_t * buffer, size_t buffersize);
    Result<size_t> Write(FileIOHandle& file,const uint8_t * buffer, size_t buffersize);
    int SectorSerialDump(size_t index);

    Result<none> Mount();


};
inline size_t FAT12::GetSizeOfCluster(uint16_t cluster)const
{
    if(cluster == 0){
        return RootDirSize();
    }
    return GetAllocationUnitSize() ;
}
inline size_t FAT12::OffsetToFat() const
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

inline size_t FAT12::GetNumberOfFileEntriesPerCluster(size_t cluster) const
{
    return GetSizeOfCluster(cluster)/sizeof(FileEntry);
}

#endif
