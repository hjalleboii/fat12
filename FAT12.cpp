#include "string.h"
#include "FAT12.h"
#include <stdio.h>

#include "PrintfMacros.h"

Result<uint16_t> FAT12::GetFAT12_entry(size_t index)
{

    if(!(index < GetNumberOfValidFatEntries()))return {INDEX_OUT_OF_RANGE};
    size_t offset_bits = index * 12;
    //size_t bitoffset_intou16 = (index%2)*4;
    size_t bitsintobytes = offset_bits%8;
    size_t offset_bytes = offset_bits/8;
    
    size_t disk_offset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec) + offset_bytes;


    
    uint16_t number;
    memcpy(&number,disk+disk_offset,sizeof(number));

    number >>= bitsintobytes;
    number&=0x0fff;
    return {OK,number};
}

Result<none> FAT12::SetFAT12_entry(size_t index, uint16_t value)
{

    if(!(index < GetNumberOfValidFatEntries()))return {INDEX_OUT_OF_RANGE};

    size_t offset_bits = index * 12;
    size_t offset_bytes = offset_bits/8;
    
    bool odd = index%2;
    size_t disk_offset = (bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec) + offset_bytes;

    
    uint16_t twobytes;
    memcpy(&twobytes,disk+disk_offset,sizeof(twobytes));

    if(odd){
        value <<= 4;
        twobytes &= 0x000f;
    }else{
        twobytes &= 0xf000;
    }
    twobytes |= value;

    memcpy(disk+disk_offset,&twobytes,sizeof(twobytes));

    return {OK};
}

Result<none> FAT12::ReadFirst512bytes(BPB *out)
{
    if(!out)return {NULLPOINTER_PROVIDED};
    uint8_t buffer[512];

    /// this can be changed to implement another disk type.
    memcpy(buffer,disk,512);

    
    memcpy(out,buffer,sizeof(BPB));
    const uint8_t magic_words[2] = {0x55, 0xAA};
    if(memcmp(buffer+510,magic_words,2)!=0){
        return {NO_MAGICBYTES};
    };
    return {OK};
}




Result<none> FAT12::ClearFAT()
{
    uint8_t buffer[bpb.BPB_FATSz16*bpb.BPB_BytsPerSec]={0};
    
    memset(disk+(bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec),0,(bpb.BPB_FATSz16*bpb.BPB_BytsPerSec));
    
    SetFAT12_entry(0,0xFF8);
    SetFAT12_entry(1,0xFFF);

    return {OK};
    
}

FatIterator FAT12::IterateFat(FatIterator *it)
{
    FatIterator newval;
    Result<uint16_t> result = GetFAT12_entry(*it);
    if(result.Ok()){
        newval = result.val;
    };
    
    *it = newval;

    return *it;
}

Result<none> FAT12::InitRootDir()
{
    FileEntry volumelabel;
    volumelabel.DIR_Attr = 0x08;
    volumelabel.DIR_NTRes =0x00;//SHould always be 0
    volumelabel.DIR_CrtTimeTenth = 0x00; // 0 <= && <= 199
    volumelabel.DIR_CrtTime = 0x00; // Granularity 2 secs
    volumelabel.DIR_CrtDate = 0x00;
    volumelabel.DIR_LstAccDate = 0x00;
    volumelabel.DIR_FstClusHI = 0x00;
    volumelabel.DIR_WrtTime = 0x00;
    volumelabel.DIR_WrtDate = 0x00;
    volumelabel.DIR_FstClusLO = 0x00;
    volumelabel.DIR_FileSize = 0x0;

    
    memset(
        disk + OffsetToFirstCluster(),
        0,
        bpb.BPB_RootEntCnt * sizeof(FileEntry)
    );


    memcpy(volumelabel.DIR_Name,bpb.BS_VolLab,sizeof(bpb.BS_VolLab));


    memcpy(
        disk+(bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec) + (bpb.BPB_FATSz16*bpb.BPB_NumFATs*bpb.BPB_BytsPerSec),
        &volumelabel,
        sizeof(volumelabel)
    );

    return {OK};
}

Result<uint16_t> FAT12::GetNextFreeCluster()
{
    for(uint16_t i =0; i < GetNumberOfValidFatEntries(); i++){
        auto fat12entry = GetFAT12_entry(i);
        if(fat12entry.Ok()){
            if(fat12entry.val == 0){
                PRINT_i(GetFAT12_entry(i));
                PRINT_i(i);
                return {OK,i};
            }
        }
    }
    return {OUT_OF_SPACE};
}

Result<none> FAT12::ClearCluster(uint16_t index)
{
    auto offsettocluster_res = OffsetToCluster(index);
    if(!offsettocluster_res.Ok()){
        return {INDEX_OUT_OF_RANGE};
    }
    memset(disk + offsettocluster_res.val,0,bpb.BPB_BytsPerSec*bpb.BPB_SecPerClus);
    return {OK};
}

Result<size_t> FAT12::OffsetToCluster(uint16_t index)
{   
    size_t RootDirSectors = (bpb.BPB_RootEntCnt*sizeof(FileEntry) / bpb.BPB_BytsPerSec);
    size_t RootDirOffset = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz16);
    size_t FirstDataSector = RootDirOffset + RootDirSectors;
    size_t FirstSectorofCluster = ((index - 2) * bpb.BPB_SecPerClus) + FirstDataSector;

    size_t offset = FirstSectorofCluster * bpb.BPB_BytsPerSec;

    if (index < 2){
        offset = RootDirOffset * bpb.BPB_BytsPerSec;
    }
    //size_t offset = (bpb.BPB_RsvdSecCnt + (bpb.BPB_FATSz16 * bpb.BPB_NumFATs) + index*bpb.BPB_SecPerClus) * bpb.BPB_BytsPerSec;
    printf("Fat Index %i -> offset %i\n",index,offset);
    return {OK,offset};
}

Result<size_t> FAT12::OffsetToFileHandle(FileHandle filehandle)
{
    size_t offsettocluster;
    auto offsettocluster_res = OffsetToCluster(filehandle.direntry);
    if(offsettocluster_res.Ok()){
        offsettocluster = offsettocluster_res.val;
    }else{
        return {offsettocluster_res.status};
    };

    return {OK, offsettocluster + filehandle.dirindex*sizeof(FileEntry)};
}



bool FAT12::IsFAT12(const BPB *bpb)
{
    size_t RootDirSectors = ((bpb->BPB_RootEntCnt * 32) + (bpb->BPB_BytsPerSec - 1)) / bpb->BPB_BytsPerSec;

    size_t FATSz;
    if(bpb->BPB_FATSz16 != 0){
        FATSz = bpb->BPB_FATSz16;
    }else{
        return false;
        //FATSz = BPB_FATSz32;
    }
    
    size_t TotSec;
    if(bpb->BPB_TotSec16 != 0){
        TotSec = bpb->BPB_TotSec16;
    }else{
        return false;
        //TotSec = BPB_TotSec32;
    }

    size_t DataSec = TotSec - (bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * FATSz) + RootDirSectors);

    size_t CountofClusters = DataSec / bpb->BPB_SecPerClus; 

    if(CountofClusters < 4085) {
        return true; // FAT12
    } else if(CountofClusters < 65525) {
        /* Volume is FAT16 */
        return false;
    } else {
        /* Volume is FAT32 */
        return false;
    } 
    return false;
}

FAT12::FAT12(uint8_t *disk, size_t disk_size):disk(disk),disk_size(disk_size)
{
    
}

Result<FileEntry *> FAT12::GetFileEntryFromHanlde(FileHandle filehandle, FileEntry *fileentryout)
{
    auto offset = OffsetToFileHandle(filehandle);
    if(!offset.Ok())return {offset.status};
    memcpy(fileentryout,disk + offset.val, sizeof(FileEntry));
    return {OK,fileentryout};
}


bool FAT12::FatIteratorOK(FatIterator it)
{
    return it < 0xff8;
}

bool FAT12::DirIsDotOrDotDot(FileEntry *fileentry)
{   
    char dot[13] = {'.',0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
    if(memcmp(dot,fileentry->DIR_Name,13) == 0){
        return true;
    }
    dot[1] = '.';
    if(memcmp(dot,fileentry->DIR_Name,13) == 0){
        return true;
    }
    return false;
}

uint32_t FAT12::GetFreeDiskSpaceAmount()
{   
    uint32_t freeclusters = 0;
    size_t imax = GetNumberOfValidFatEntries();
    for(unsigned i =0; i < imax; i++){
        auto fatentry_res = GetFAT12_entry(i);
        
        if(fatentry_res.Ok() && fatentry_res.val == 0){
            freeclusters ++;
        }
    }
    return freeclusters*bpb.BPB_SecPerClus * bpb.BPB_BytsPerSec;
}

Result<none> FAT12::AllocateNewEntryInDir(Directory dir, FileHandle *out_entry)
{
    FatIterator lastent;
    for(FatIterator ent = dir.fat_entry; ent<0xff8; IterateFat(&ent)){
        printf("ent %u\n",ent);
        uint8_t fbyte;
        auto offsettocluster_res = OffsetToCluster(ent);
        if(offsettocluster_res.Ok()){
            for(uint16_t i = 0;
            i < bpb.BPB_SecPerClus*bpb.BPB_BytsPerSec/sizeof(FileEntry);
            i++){
                
                memcpy(&fbyte,disk + offsettocluster_res.val + i * sizeof(FileEntry),sizeof(fbyte) );
                printf("Fbyte: %u\n",fbyte);

                if(fbyte ==  0xE5 || fbyte == 0x00){
                    *out_entry = FileHandle{ent, i};
                    
                    return {OK};
                }
            }
        
            lastent = ent;
        }
    }
    auto newsector_res = GetNextFreeCluster();

    if(!newsector_res.Ok())return {ERROR};

    SetFAT12_entry(lastent,newsector_res.val);
    SetFAT12_entry(newsector_res.val,0xfff);
    *out_entry = FileHandle{newsector_res.val,0};
    return {OK};

}

Result<none> FAT12::Format(const char *volumename, BytesPerSector bytespersector, uint8_t SectorPerClusters, bool dual_FATs)
{
    bpb = BPB();
    bpb.BS_jmpBoot[0] = 0xEB;
    bpb.BS_jmpBoot[1] = 0x00;
    bpb.BS_jmpBoot[2] = 0x90;
    char OEMName[9] = "SFAT1.0\0";
    memcpy(bpb.BS_OEMName,OEMName,8);
    bpb.BPB_BytsPerSec = bytespersector;
    bpb.BPB_SecPerClus = SectorPerClusters;
    bpb.BPB_RsvdSecCnt = 0x01;
    
    if(dual_FATs){
        bpb.BPB_NumFATs = 2;
    }else{
        bpb.BPB_NumFATs = 1;
    }

    bpb.BPB_RootEntCnt= bytespersector/32; // This might need a change
    bpb.BPB_TotSec16 = disk_size/bytespersector;

    bpb.BPB_Media = 0xF0; // this is also not written in stone

    size_t sectorsavalible = bpb.BPB_TotSec16;
    sectorsavalible -= 1; // Bootsector

    size_t entriescount = sectorsavalible/SectorPerClusters;

    size_t sectorsrequiredforFAT =   (((1 + entriescount * 3) / 2) / bpb.BPB_BytsPerSec) +1;


    bpb.BPB_FATSz16 = sectorsrequiredforFAT;

    bpb.BPB_SecPerTrk = 0x01;
    bpb.BPB_NumHeads = 0x01;
    bpb.BPB_HiddSec = 0x00;

    bpb.BPB_TotSec32 = 0x00;
    bpb.BS_DrvNum = 0x80;
    bpb.BS_Reserved1 = 0x00;

    bpb.BS_BootSig = 0x29;

    bpb.BS_VolID = 0x00001234;
    memcpy(bpb.BS_VolLab,volumename,MIN(strlen(volumename),11));
    memcpy(bpb.BS_FilSysType,"FAT12\0\0",8);

    
    char bootsector_buffer[bpb.BPB_BytsPerSec]={0};

    memcpy(bootsector_buffer,&bpb,sizeof(bpb));
    
    memset(disk,0,bpb.BPB_BytsPerSec);

    memcpy(disk,&bpb,sizeof(bpb));

    uint8_t magic_bytes[2]={0x55,0xAA};
    memcpy(disk+510,magic_bytes,sizeof(magic_bytes));


    ClearFAT();
    InitRootDir();
    return {OK};

}
Result<none> FAT12::CreateFile(const char name[8], const char extension[3], Directory parent,FileHandle* filehandle)
{
    FileEntry file;
    size_t namelen = strnlen(name,8);
    size_t extensionlen = strnlen(extension,3);
    memset(file.DIR_Name,0x20,8);
    memcpy(file.DIR_Name,name,namelen);

    memset(file.DIR_Name+8,0x20,3);
    memcpy(file.DIR_Name+8,extension,extensionlen);
    file.DIR_NTRes = 0;
    file.DIR_CrtTimeTenth = 0;
    file.DIR_CrtTime = 0;
    file.DIR_CrtDate = 0;
    file.DIR_LstAccDate = 0;
    file.DIR_FstClusHI = 0;
    file.DIR_Attr = 0;
    

    file.DIR_FstClusLO = 0x00;
    file.DIR_FileSize = 0;
    
    FileHandle newfilehandle;
    if(!AllocateNewEntryInDir(parent,&newfilehandle).Ok())return {ERROR};

    auto  firstcluster = GetNextFreeCluster();

    if(!firstcluster.Ok())return {ERROR};

    file.DIR_FstClusLO = firstcluster.val;
    if(!SetFAT12_entry(firstcluster.val,END_OF_FILE).Ok())return {ERROR};
    

    auto offsettofilehandle_res = OffsetToFileHandle(newfilehandle);
    if(!offsettofilehandle_res.Ok()) return {ERROR};

    memcpy(disk +offsettofilehandle_res.val
        ,
        &file,
        sizeof(file)
    );
    *filehandle = newfilehandle;
    return {OK};
}
Result<none> FAT12::DeleteFile(FileHandle *filehandle)
{
    
    FileEntry fentry;
    printf("Filehandle offset: %i\n",OffsetToFileHandle(*filehandle));
    auto offset_to_filehandle = OffsetToFileHandle(*filehandle);
    if(!offset_to_filehandle.Ok()){return {ERROR};}

    memcpy(&fentry,disk +offset_to_filehandle.val ,sizeof(fentry));
    if(fentry.DIR_Attr & ATTR_DIRECTORY){
        auto empty = DirectoryEmpty({fentry.DIR_FstClusLO});
        if(!(empty.Ok() && empty.val)){
            return {DIRECTORY_NOT_EMPTY};
        }
    };

    memset(disk + offset_to_filehandle.val,0xE5,sizeof(fentry));


    uint16_t fat = fentry.DIR_FstClusLO;
    
    while(fat > 0 && fat < 0xff8){
        auto nextfat = GetFAT12_entry(fat);
        if(nextfat.Ok()){
            SetFAT12_entry(fat,0);
            auto offsettocluster = OffsetToCluster(fat);
            if(!offsettocluster.Ok())return {ERROR};

            memset(disk + offsettocluster.val,0,bpb.BPB_BytsPerSec*bpb.BPB_SecPerClus);
            fat = nextfat.val;
        }
    }
   
    return {OK};
}

Result<FileIOHandle> FAT12::Open(FileHandle file, uint8_t mode)
{
    FileIOHandle fileio;
    
    fileio.handle = file;
    FileEntry entry;
    auto entry_res = GetFileEntryFromHanlde(fileio.handle,&entry);
    if(!entry_res.Ok()){return {ERROR};};
    PRINT_X(mode);
    if(mode & FILE_MODE_WRITE){
        fileio.mode = FILE_IO_WRITE;
        if(mode & FILE_MODE_APP){
            panic("you are gay");
        }else{
            fileio.currentoffset = 0;
            fileio.currentAU = entry.DIR_FstClusLO;
            entry.DIR_FileSize = 0;
            FatIterator it = fileio.currentAU;
            ClearCluster(it);
            IterateFat(&it);
            for(;FatIteratorOK(it);){
                FatIterator Cur = it;
                ClearCluster(it);
                IterateFat(&it);
                SetFAT12_entry(Cur,0x00);
            }

            SetFAT12_entry(fileio.currentAU,END_OF_FILE);
        }
       
        
    }else{
        fileio.mode = FILE_IO_READ;
        fileio.currentoffset = 0;
        fileio.currentAU = entry.DIR_FstClusLO;
    }

    return {OK,fileio};
}

Result<none> FAT12::Close(FileIOHandle *file)
{
    return Result<none>();
}

int FAT12::Read(FileIOHandle &file, uint8_t *buffer, size_t buffersize)
{
    size_t read = 0;
    FileEntry entry;
    if(!GetFileEntryFromHanlde(file.handle,&entry).Ok()){return {ERROR};};

    PRINT_i(file.currentAU);
    PRINT_i(file.currentoffset);
    while(read < buffersize && file.currentoffset < entry.DIR_FileSize){
        size_t offsetintosector = file.currentoffset%GetAllocationUnitSize();
        

        size_t possible_read_buffer = buffersize-read;
        
        size_t possible_read_file = entry.DIR_FileSize - file.currentoffset;
        size_t possible_read_sector = GetAllocationUnitSize()-offsetintosector;

        size_t readsize = MIN(MIN(possible_read_buffer,possible_read_file),possible_read_sector);
        
        auto clusteroffset = OffsetToCluster(file.currentAU);
        if(!clusteroffset.Ok()){return 0;};
        PRINT_i(clusteroffset.val);
        memcpy(buffer+read,disk+clusteroffset.val +offsetintosector, readsize);
        PRINT_i(read);
        PRINT_i(readsize);
        PRINT_i(clusteroffset.val + offsetintosector);
        file.currentoffset += readsize;
        read += readsize;

        if(file.currentoffset % GetAllocationUnitSize()==0){
            IterateFat(&file.currentAU);
        }
    }
    return read;
}

Result<none> FAT12::CreateDir(const char name[8],const char extension[3], Directory parent,FileHandle* filehandle)
{
    FileEntry dir;
    size_t namelen = strnlen(name,8);
    size_t extensionlen = strnlen(extension,3);
    memset(dir.DIR_Name,0x20,8);
    memcpy(dir.DIR_Name,name,namelen);

    memset(dir.DIR_Name+8,0x20,3);
    memcpy(dir.DIR_Name+8,extension,extensionlen);
    dir.DIR_NTRes = 0;
    dir.DIR_CrtTimeTenth = 0;
    dir.DIR_CrtTime = 0;
    dir.DIR_CrtDate = 0;
    dir.DIR_LstAccDate = 0;
    dir.DIR_FstClusHI = 0;
    dir.DIR_Attr = ATTR_DIRECTORY;
    auto firstcluster = GetNextFreeCluster();
    if(!firstcluster.Ok())return {firstcluster.status};

    SetFAT12_entry(firstcluster.val,0xfff);
    if(firstcluster.val == 0)return {ERROR};

    dir.DIR_FstClusLO = firstcluster.val;
    dir.DIR_FileSize = 0;

    FileHandle newdirhandle;
    if(!AllocateNewEntryInDir(parent,&newdirhandle).Ok())return {ERROR};

    *filehandle = newdirhandle;

    
    auto offsettofilehandle_res = OffsetToFileHandle(newdirhandle);

    if(!offsettofilehandle_res.Ok())return {ERROR};
    
    memcpy(disk +
        offsettofilehandle_res.val,
        &dir,
        sizeof(dir)
    );

    FileHandle df{dir.DIR_FstClusLO,0};
    FileHandle ddf{dir.DIR_FstClusLO,1};

    FileEntry dot = dir;
    memset(dot.DIR_Name,0x20,sizeof(dot.DIR_Name));

    memcpy(dot.DIR_Name,".",1);

    auto offset_df = OffsetToFileHandle(df);
    if(!offset_df.Ok())return {ERROR};

    memcpy(disk +
        offset_df.val,
        &dot,
        sizeof(dot)
    );

    FileEntry dotdot = dot;
    memset(dotdot.DIR_Name,0x20,sizeof(dot.DIR_Name));
    memcpy(dotdot.DIR_Name,"..",2);

    dotdot.DIR_FstClusLO = parent.fat_entry;

    FileHandle dotdotf{};
    
    OffsetToFileHandle(ddf);

    auto offset_ddf = OffsetToFileHandle(ddf);
    if(!offset_ddf.Ok())return {ERROR};

    memcpy(disk +
        offset_ddf.val,
        &dotdot,
        sizeof(dotdot)
    );



    return {OK};


}

Result<bool> FAT12::DirectoryEmpty(Directory directory)
{
    for(FatIterator ent = directory.fat_entry; FatIteratorOK(ent); IterateFat(&ent)){ // root directory can be bigger than the standard allocation unit. So i need to implement a check for that.
        for(  uint16_t i = 0; i < (bpb.BPB_BytsPerSec*bpb.BPB_SecPerClus / sizeof(FileEntry)); i++){
            FileHandle f{ent,i};
            FileEntry entry;//continue hereeeeeeeeee
            auto result = GetFileEntryFromHanlde(f,&entry);
            if(result.Ok()){
                if(entry.DIR_Name[0] == 0xE5 || entry.DIR_Name[0] == 0x00){
                    continue;
                }
                if(DirIsDotOrDotDot(&entry)){
                    continue;
                }
                return {OK,false};
            }else{
                return {ERROR};
            }
        }
    }
    return {OK,false};
}

int FAT12::SectorSerialDump(size_t index)
{   
    if(!(index < bpb.BPB_TotSec16))return ERROR;
    uint8_t buf[bpb.BPB_BytsPerSec];

    memcpy(buf,disk+(index*bpb.BPB_BytsPerSec),bpb.BPB_BytsPerSec);
    
    
    for(unsigned int i = 0; i < bpb.BPB_BytsPerSec; i++){
        printf(" %02X",buf[i]);
        if((i+1)%32 == 0){
            printf("\n");
        }
    }
    printf("\n");
    return OK;
}

Result<none> FAT12::Mount()
{
    
    auto result = ReadFirst512bytes(&bpb);
    bool fat12 = IsFAT12(&bpb);
    if(fat12 && (result.Ok()))return {OK};
    return {ERROR};
}


void BPB::print()
{
    PRINT_X(BPB_BytsPerSec);
    PRINT_X(BPB_SecPerClus);
    PRINT_X(BPB_RsvdSecCnt);
    PRINT_X(BPB_NumFATs);
    PRINT_X(BPB_RootEntCnt);
    PRINT_X(BPB_TotSec16);
    PRINT_X(BPB_Media);
    PRINT_X(BPB_FATSz16);
    PRINT_X(BPB_SecPerTrk);
    PRINT_X(BPB_NumHeads);
    PRINT_X(BPB_HiddSec);
    PRINT_X(BPB_TotSec32);
    PRINT_X(BS_DrvNum);
    PRINT_X(BS_Reserved1);
    PRINT_X(BS_BootSig);
    PRINT_X(BS_VolID);
}



