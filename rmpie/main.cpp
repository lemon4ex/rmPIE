//
//    ____  __  __ ____ ___ _____
//   |  _ \|  \/  |  _ \_ _| ____|
//   | |_) | |\/| | |_) | ||  _|
//   |  _ <| |  | |  __/| || |___
//   |_| \_\_|  |_|_|  |___|_____|
//
//  Created by lemon4ex on 16/5/13.
//  Copyright © 2016年 lemon4ex. All rights reserved.
//

#include <iostream>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <string.h>
#include <mach-o/fat.h>

using namespace std;

uint32_t Swap32(uint32_t magic , uint32_t value)
{
    if (magic == FAT_CIGAM || magic == MH_CIGAM || magic == MH_CIGAM_64) {
        return ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) | ((value & 0x0000FF00) << 8) | ((value & 0x000000FF) << 24);
    }
    return value;
}

void SwapMachHeader(uint32_t magic,mach_header *header)
{
    header->cputype = Swap32(magic, header->cputype);
    header->cpusubtype = Swap32(magic, header->cpusubtype);
    header->filetype = Swap32(magic, header->filetype);
    header->magic = Swap32(magic, header->magic);
    header->ncmds = Swap32(magic, header->ncmds);
    header->sizeofcmds = Swap32(magic, header->sizeofcmds);
    header->flags = Swap32(magic, header->flags);
}

void BackupFile(FILE *fi,const char *output_path)
{
    fseeko(fi, 0, SEEK_SET);
    FILE *fo = fopen(output_path, "w");
    char buffer[1024];
    size_t read_len;
    while ((read_len = fread(buffer, sizeof(char), sizeof(buffer), fi)) > 0) {
        fwrite(buffer, sizeof(char), read_len, fo);
    }
    fseeko(fi, 0, SEEK_SET);
    fflush(fo);
    fclose(fo);
}

void fpeek(void *buffer,size_t size,size_t len,FILE *file)
{
    off_t ori_offset = fseeko(file, 0, SEEK_CUR);
    fread(buffer, size, len, file);
    fseeko(file, ori_offset, SEEK_SET);
}

void RemoveArchPIE(FILE *fi,uint32_t offset,uint32_t magic)
{
    fseeko(fi, offset, SEEK_SET);
    printf("载入文件头...\n");
    mach_header mah;
    fread(&mah, sizeof(mah), 1, fi);
    SwapMachHeader(magic, &mah);
    if (mah.filetype != MH_EXECUTE) {
        printf("不是可执行文件，不作处理\n");
        return;
    }
    
    if (!(mah.flags & MH_PIE)) {
        printf("Arch未开启PIE，不做处理\n");
        return;
    }
    
    printf("移除PIE...\n");
    mah.flags &= ~MH_PIE;
    
    printf("写回文件...\n");
    fseeko(fi, offset, SEEK_SET);
    
    SwapMachHeader(magic, &mah);
    
    fwrite(&mah, sizeof(mah), 1, fi);
    
    return;
}

int main(int argc, const char * argv[]) {
    
    if (argc < 2) {
        printf("Usage:\n\trmpie bin_file\n");
        return 0;
    }
    
    FILE *fi = fopen(argv[1], "r+");
    if (!fi) {
        printf("无法打开文件：%s",argv[1]);
        return 0;
    }
    
    const char *bakFile = strncat((char *)argv[1], ".bak", 5);
    printf("备份文件到：%s\n",bakFile);
    BackupFile(fi, bakFile);
    
    uint32_t magic;
    fpeek(&magic, sizeof(magic), 1, fi);
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        fat_header fh;
        fpeek(&fh,sizeof(fh),1,fi);
        uint32_t archs = Swap32(magic, fh.nfat_arch);
        printf("文件为fat Mach-O，共有 %d 个archs",archs);
        for (uint32_t i = 0; i < archs; ++i) {
            fat_arch arch;
            fread(&arch, sizeof(arch), 1, fi);
            RemoveArchPIE(fi,arch.offset,magic);
        }
    }
    else
    {
        RemoveArchPIE(fi,0,magic);
    }
    
    fflush(fi);
    fclose(fi);
    
    printf("完毕\n");
    
    return 0;
}
