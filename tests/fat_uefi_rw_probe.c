/* SPDX-License-Identifier: GPL-3.0-or-later */
/* QEMU-only probe of the exact FAT12 or FAT16 EFI Driver Binding instance.
 * The test calls that instance directly after disconnecting firmware's FAT
 * driver, so an OVMF fallback cannot pass in place of the image under test. */
#include <efi.h>
#include <efilib.h>

#ifndef PROBE_FAT12_BLOCKS
#define PROBE_FAT12_BLOCKS 2880ULL
#endif
#if PROBE_FAT_BITS == 12
#define DRIVER_PATH L"\\FAT12.efi"
#define OWN_BLOCKS PROBE_FAT12_BLOCKS
#define OTHER_BLOCKS 16384ULL
#elif PROBE_FAT_BITS == 16
#define DRIVER_PATH L"\\FAT16.efi"
#define OWN_BLOCKS 16384ULL
#define OTHER_BLOCKS PROBE_FAT12_BLOCKS
#else
#error "PROBE_FAT_BITS must be 12 or 16"
#endif

static EFI_GUID loaded_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID block_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
static EFI_GUID disk_guid = EFI_DISK_IO_PROTOCOL_GUID;
static EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID binding_guid = EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID fileinfo_guid = EFI_FILE_INFO_ID;

static void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
static unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void uart(const char *s) {
    while (*s) {
        unsigned int n=0;
        while ((inb(0x3fdU)&0x20U)==0 && ++n<100000U) ;
        outb(0x3f8U,(unsigned char)*s++);
    }
}
static void print_status(EFI_STATUS status) {
    static const char hex[]="0123456789ABCDEF";
    char text[17];
    for (unsigned i=0;i<16u;++i)
        text[i]=hex[((UINT64)status >> ((15u-i)*4u)) & 15u];
    text[16]='\0';
    uart(" EFI_STATUS=0x"); uart(text); uart("\r\n");
}
static void finish(const char *s,unsigned char code) {
    uart(s);
    outb(0xf4U,code);
    for (;;) __asm__ volatile("pause");
}
static void require(EFI_STATUS status,const char *message) {
    if (EFI_ERROR(status)) {uart(message);print_status(status);finish("",0x11U);}
}
static void disconnect_existing(EFI_HANDLE target) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;
    EFI_DISK_IO *disk=NULL;
    EFI_STATUS status;
    UINTN count=0;
    /* Connecting may install OVMF's FAT driver as well as DiskIo. Identify
     * and disconnect only drivers that own DiskIo, leaving its producer in
     * place. Disconnecting the whole controller also removes DiskIo itself. */
    (void)BS->ConnectController(target,NULL,NULL,FALSE);
    require(BS->HandleProtocol(target,&disk_guid,(VOID **)&disk),
            "FAT_EFI_FAIL missing DiskIo\r\n");
    status=BS->OpenProtocolInformation(target,&disk_guid,&entries,&count);
    if (!EFI_ERROR(status)) {
        for(UINTN i=0;i<count;++i)
            if(entries[i].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER)
                require(BS->DisconnectController(target,entries[i].AgentHandle,NULL),
                        "FAT_EFI_FAIL disconnect firmware driver\r\n");
        BS->FreePool(entries);
    } else if (status!=EFI_NOT_FOUND) {
        require(status,"FAT_EFI_FAIL enumerate DiskIo owners\r\n");
    }
    if (!EFI_ERROR(BS->HandleProtocol(target,&fs_guid,(VOID **)&fs)))
        finish("FAT_EFI_FAIL firmware filesystem still bound\r\n",0x11U);
    require(BS->HandleProtocol(target,&disk_guid,(VOID **)&disk),
            "FAT_EFI_FAIL DiskIo removed\r\n");
}
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *get_fs(EFI_HANDLE target) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    require(BS->HandleProtocol(target,&fs_guid,(VOID **)&fs),
            "FAT_EFI_FAIL filesystem protocol missing\r\n");
    if (!fs) finish("FAT_EFI_FAIL null filesystem\r\n",0x11U);
    return fs;
}
static void verify_data(EFI_FILE_PROTOCOL *root) {
    EFI_FILE_PROTOCOL *f=NULL;
    UINT8 data[4097];
    UINTN length=sizeof data;
    require(root->Open(root,&f,L"\\RESULT\\Renamed Long Name.bin",
                       EFI_FILE_MODE_READ,0),"FAT_EFI_FAIL reopen\r\n");
    require(f->Read(f,&length,data),"FAT_EFI_FAIL read\r\n");
    if (length!=sizeof data) finish("FAT_EFI_FAIL size\r\n",0x11U);
    for (UINTN i=0;i<sizeof data;++i)
        if (data[i]!=(UINT8)((i*17U+23U)&255U))
            finish("FAT_EFI_FAIL contents\r\n",0x11U);
    require(f->Close(f),"FAT_EFI_FAIL close\r\n");
}

EFI_STATUS efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table) {
    EFI_LOADED_IMAGE *loaded=NULL;
    EFI_DEVICE_PATH *path;
    EFI_HANDLE driver=NULL,*handles=NULL,own=NULL,other=NULL,fat32=NULL;
    EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    EFI_FILE_PROTOCOL *root=NULL,*dir=NULL,*file=NULL,*tmp=NULL;
    EFI_BLOCK_IO *block=NULL;
    EFI_STATUS status;
    UINTN count=0,info_size;
    UINT8 data[4097],info_buf[1024];
    EFI_FILE_INFO *info=(EFI_FILE_INFO *)info_buf;
    InitializeLib(image,table);
    uart("FAT_EFI begin\r\n");
    require(BS->HandleProtocol(image,&loaded_guid,(VOID **)&loaded),"FAT_EFI_FAIL loaded image\r\n");
    path=FileDevicePath(loaded->DeviceHandle,DRIVER_PATH);
    if (!path) finish("FAT_EFI_FAIL path\r\n",0x11U);
    require(BS->LoadImage(FALSE,image,path,NULL,0,&driver),"FAT_EFI_FAIL load\r\n");
    require(BS->StartImage(driver,NULL,NULL),"FAT_EFI_FAIL start image\r\n");
    require(BS->HandleProtocol(driver,&binding_guid,(VOID **)&binding),"FAT_EFI_FAIL binding protocol\r\n");
    if (!binding) finish("FAT_EFI_FAIL null binding\r\n",0x11U);
    require(BS->LocateHandleBuffer(ByProtocol,&block_guid,NULL,&count,&handles),"FAT_EFI_FAIL enumerate\r\n");
    for (UINTN i=0;i<count;++i) {
        block=NULL;
        if (EFI_ERROR(BS->HandleProtocol(handles[i],&block_guid,(VOID **)&block)) ||
            !block || !block->Media || !block->Media->MediaPresent ||
            !block->Media->LogicalPartition || block->Media->BlockSize!=512U)
            continue;
        if (block->Media->LastBlock+1==OWN_BLOCKS) own=handles[i];
        if (block->Media->LastBlock+1==OTHER_BLOCKS) other=handles[i];
        if (block->Media->LastBlock+1==131072ULL) fat32=handles[i];
    }
    BS->FreePool(handles);
    if (!own || !other || !fat32) finish("FAT_EFI_FAIL target partitions\r\n",0x11U);
    uart("FAT_EFI reject other format\r\n");
    disconnect_existing(other);
    status=binding->Start(binding,other,NULL);
    print_status(status);
    if (!EFI_ERROR(status)) finish("FAT_EFI_FAIL other variant accepted\r\n",0x11U);
    if (!EFI_ERROR(BS->HandleProtocol(other,&fs_guid,(VOID **)&fs)))
        finish("FAT_EFI_FAIL other variant installed filesystem\r\n",0x11U);
    uart("FAT_EFI reject FAT32\r\n");
    disconnect_existing(fat32);
    status=binding->Start(binding,fat32,NULL);
    if (!EFI_ERROR(status)) finish("FAT_EFI_FAIL FAT32 accepted\r\n",0x11U);
    if (!EFI_ERROR(BS->HandleProtocol(fat32,&fs_guid,(VOID **)&fs)))
        finish("FAT_EFI_FAIL FAT32 installed filesystem\r\n",0x11U);
    uart("FAT_EFI bind own format\r\n");
    disconnect_existing(own);
    require(binding->Supported(binding,own,NULL),"FAT_EFI_FAIL own supported\r\n");
    require(binding->Start(binding,own,NULL),"FAT_EFI_FAIL own binding\r\n");
    fs=get_fs(own);
    require(fs->OpenVolume(fs,&root),"FAT_EFI_FAIL root\r\n");
    require(root->Open(root,&dir,L"RESULT",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY),"FAT_EFI_FAIL mkdir\r\n");
    require(dir->Open(dir,&file,L"Initial Long Name.bin",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0),"FAT_EFI_FAIL create\r\n");
    for (UINTN i=0;i<sizeof data;++i) data[i]=(UINT8)((i*17U+23U)&255U);
    count=sizeof data;
    require(file->Write(file,&count,data),"FAT_EFI_FAIL write\r\n");
    if(count!=sizeof data) finish("FAT_EFI_FAIL short write\r\n",0x11U);
    require(file->Flush(file),"FAT_EFI_FAIL flush\r\n");
    info_size=sizeof info_buf;
    require(file->GetInfo(file,&fileinfo_guid,&info_size,info),"FAT_EFI_FAIL get info\r\n");
    {
        const CHAR16 name[]=L"Renamed Long Name.bin";
        const UINTN name_bytes=sizeof name;
        const UINTN bytes=SIZE_OF_EFI_FILE_INFO+name_bytes;
        if (bytes>sizeof info_buf) finish("FAT_EFI_FAIL name buffer\r\n",0x11U);
        CopyMem(info->FileName,(VOID *)name,name_bytes);
        info->Size=bytes;
        require(file->SetInfo(file,&fileinfo_guid,bytes,info),"FAT_EFI_FAIL rename\r\n");
    }
    require(file->Close(file),"FAT_EFI_FAIL close after rename\r\n");
    require(dir->Open(dir,&tmp,L"DELETE.TXT",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0),"FAT_EFI_FAIL create delete target\r\n");
    require(tmp->Delete(tmp),"FAT_EFI_FAIL delete\r\n");
    require(dir->Close(dir),"FAT_EFI_FAIL close dir\r\n");
    verify_data(root);
    require(root->Close(root),"FAT_EFI_FAIL close root\r\n");
    uart("FAT_EFI stop and rebind\r\n");
    require(binding->Stop(binding,own,0,NULL),"FAT_EFI_FAIL stop\r\n");
    if (!EFI_ERROR(BS->HandleProtocol(own,&fs_guid,(VOID **)&fs)))
        finish("FAT_EFI_FAIL filesystem survived stop\r\n",0x11U);
    require(binding->Start(binding,own,NULL),"FAT_EFI_FAIL rebind\r\n");
    fs=get_fs(own);
    require(fs->OpenVolume(fs,&root),"FAT_EFI_FAIL remount root\r\n");
    verify_data(root);
    require(root->Close(root),"FAT_EFI_FAIL final close\r\n");
    require(binding->Stop(binding,own,0,NULL),"FAT_EFI_FAIL final stop\r\n");
    finish("FAT_EFI_PASS read write rename delete flush rebind reject\r\n",0x10U);
    return EFI_SUCCESS;
}
