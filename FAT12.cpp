#include "string.h"
#include "FAT12.h"
#include <stdio.h>

#include "PrintfMacros.h"

uint16_t FAT12::GetFAT12_entry(size_t index)
{
     size_t offset_bits = index * 12;
    //size_t bitoffset_intou16 = (index%2)*4;
    size_t bitsintobytes = (index%2)*4;
    size_t offset_bytes = offset_bits/8;
    
    size_t ThisFATSecNum = bpb.BPB_RsvdSecCnt + (offset_bytes / bpb.BPB_BytsPerSec);
    size_t ThisFATEntOffset = offset_bytes % bpb.BPB_BytsPerSec; 

    bool second_sector = (bpb.BPB_BytsPerSec-1) == ThisFATEntOffset;

    uint8_t buffer[(1+second_sector)*bpb.BPB_BytsPerSec]={
        0xFF
    };
    if(ReadSector(ThisFATSecNum,buffer,bpb.BPB_BytsPerSec)!=OK)return 0;

    if(second_sector){
        if(
            ReadSector(
            ThisFATSecNum,
            buffer + bpb.BPB_BytsPerSec,
            bpb.BPB_BytsPerSec
            ) != OK
        )return ERROR;
    } 


    uint16_t number = buffer[ThisFATEntOffset] | buffer[ThisFATEntOffset+1] << 8;

    number >>= bitsintobytes;
    number&=0x0fff;
    return number;
}

int FAT12::SetFAT12_entry(size_t index, uint16_t value)
{
    size_t offset_bits = index * 12;
    //size_t bitoffset_intou16 = (index%2)*4;
    bool odd = (index%2);
    size_t offset_bytes = offset_bits/8;
    
    size_t ThisFATSecNum = bpb.BPB_RsvdSecCnt + (offset_bytes / bpb.BPB_BytsPerSec);
    size_t ThisFATEntOffset = offset_bytes % bpb.BPB_BytsPerSec; 

    bool second_sector = (bpb.BPB_BytsPerSec-1) == ThisFATEntOffset;

    uint8_t buffer[(1+second_sector)*bpb.BPB_BytsPerSec]={
        0xFF
    };
    if(ReadSector(ThisFATSecNum,buffer,bpb.BPB_BytsPerSec)!=OK)return ERROR;

    if(second_sector){
        if(
            ReadSector(
            ThisFATSecNum,
            buffer + bpb.BPB_BytsPerSec,
            bpb.BPB_BytsPerSec
            ) != OK
        )return ERROR;
    } 





    //uint16_t number = buffer[ThisFATEntOffset] | buffer[ThisFATEntOffset+1] << 8;
    WriteFAT12EntryToBuffer((buffer+ThisFATEntOffset),value,odd);
    PRINT_X(GetFAT12_entry(index));

    if(WriteSector(ThisFATSecNum,buffer,bpb.BPB_BytsPerSec)!=OK)return ERROR;
    if(second_sector){
        if(
            WriteSector(
            ThisFATSecNum,
            buffer + bpb.BPB_BytsPerSec,
            bpb.BPB_BytsPerSec
            ) != OK
        )return ERROR;
    }



    return OK;
}

int FAT12::ReadFirst512bytes(BPB *out)
{
    if(!out)return NULLPOINTER_PROVIDED;
    uint8_t buffer[512];

    /// this can be changed to implement another disk type.
    memcpy(buffer,disk,512);

    
    memcpy(out,buffer,sizeof(BPB));
    const uint8_t magic_words[2] = {0x55, 0xAA};
    if(memcmp(buffer+510,magic_words,2)!=0){
        return NO_MAGICBYTES;
    };
    return OK;
}

int FAT12::WriteFAT12EntryToBuffer(uint8_t *buffer, uint16_t val, bool odd)
{
    uint16_t number = buffer[0] | buffer[1] << 8;
    PRINT_X(number);
    PRINT_X(val);
    PRINT_X(odd);
    //nullify
    if(odd){
        number &= 0xf;
        val <<= 4;
    }else{
        number &= (0xf << 12);

    }
    number |= val;

    PRINT_X(val);
    PRINT_X(number);
    printf("---\n");
    buffer[0] = number&0xFF;
    buffer[1] = number>>8;
    
    
    uint16_t vt= buffer[0] | buffer[1] << 8;
    PRINT_X(vt);
    return OK;
}

int FAT12::NextSec(SectorIterator *iterator)
{
    uint16_t next = GetFAT12_entry(*iterator);
    if(next == END_OF_FILE ){// some other values may also indicate eof
        return END;
    }
    *iterator = next;
    return OK;
}

int FAT12::ReadSector(size_t index,uint8_t *buffer, size_t buffersize)
{
    if(buffersize!=bpb.BPB_BytsPerSec)return ERROR;
    /// this can be changed to implement another disk type.
    memcpy(buffer,disk+index*(bpb.BPB_BytsPerSec),bpb.BPB_BytsPerSec);
    return OK;
}

int FAT12::WriteSector(size_t index, const uint8_t *buffer, size_t buffersize)
{
    if(buffersize!=bpb.BPB_BytsPerSec)return ERROR;
    memcpy(disk+index*(bpb.BPB_BytsPerSec),buffer,bpb.BPB_BytsPerSec);
    return OK;
}

int FAT12::ClearFAT()
{
    uint8_t buffer[bpb.BPB_FATSz16*bpb.BPB_BytsPerSec]={0};
    
    WriteFAT12EntryToBuffer(buffer,0xFF8,false);
    WriteFAT12EntryToBuffer(buffer+1,0xFFF,true);


    bpb.BPB_RsvdSecCnt;
    for(size_t i = 0; i < bpb.BPB_FATSz16;i++){
        WriteSector(bpb.BPB_RsvdSecCnt+i,buffer+i*bpb.BPB_BytsPerSec,bpb.BPB_BytsPerSec);
    }
    return 0;
    
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

int FAT12::Format(const char *volumename, BytesPerSector bytespersector, uint8_t SectorPerClusters, bool dual_FATs)
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
    
    bootsector_buffer[510] = 0x55;
    bootsector_buffer[511] = 0xAA;


    WriteSector(0,(uint8_t*)bootsector_buffer,bpb.BPB_BytsPerSec);
    ClearFAT();

    return 0;

}

int FAT12::Mount()
{
    
    int result = ReadFirst512bytes(&bpb);
    bool fat12 = IsFAT12(&bpb);
    
    return 0;
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



