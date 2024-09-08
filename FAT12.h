#include <stdint.h>
#include <pico/stdlib.h>


#define END_OF_FILE 0xfff

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

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

typedef  uint16_t FatIterator;

enum Status{
    OK,
    ERROR,
    NO_MAGICBYTES,
    NULLPOINTER_PROVIDED,
    END
};

enum BytesPerSector{
    B512 =512,
    B1024=1024,
    B2048=2048,
    B4096=4096
};



class FAT12{
    public:
    uint8_t* disk;
    size_t disk_size;
    BPB bpb;
    uint16_t GetFAT12_entry(size_t index);
    int SetFAT12_entry(size_t index,uint16_t value);
    int ReadFirst512bytes(BPB*out);
    static bool IsFAT12(const BPB*bpb);
    int ClearFAT();
    FatIterator IterateFat(FatIterator* it);
    int InitRootDir();
    uint16_t GetNextFreeCluster();
    int ClearCluster(uint16_t index);
    size_t OffsetToCluster(uint16_t index);
    size_t OffsetToFileHandle(FileHandle filehandle);

public:
    FAT12(uint8_t* disk,size_t disk_size);
    
    uint32_t GetFreeDiskSpaceAmount();
    int AllocateNewEntryInDir(Directory dir, FileHandle* out_entry);
    int Format(const char* volumename, BytesPerSector bytespersector,uint8_t SectorPerClusters, bool dual_FATs);
    int CreateDir(const char name[8],const char extension[3],Directory parent);
    
    int CreateFile(const char name[8],const char extension[3],Directory parent,FileHandle* filehandle);
    int DeleteFile(FileHandle* filehandle);
    //File Open(const char* filepath,uint8_t mode);
    //int Read(File& file,uint8_t * buffer, size_t buffersize);
    //int Write(File& file,uint8_t * buffer, size_t buffersize);
    int SectorSerialDump(size_t index);

    int Mount();


};



