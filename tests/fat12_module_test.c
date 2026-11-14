/* SPDX-License-Identifier: GPL-3.0-or-later
 * FAT12 native Common V2 media and filesystem regression.
 * The linked release engine owns all format and file semantics.
 */
#include "infiltratr/filesystem_module_v2.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
#define SECTOR 512u
#define SECTORS 2880u
static unsigned char media[SECTOR * SECTORS];
static unsigned writes;
static int read_media(void *ctx,uint64_t at,void *buf,uint32_t length)
{
    (void)ctx;
    if (!buf || at>sizeof(media) || length>sizeof(media)-at) return -1;
    memcpy(buf,media+(size_t)at,length);
    return 0;
}
static int write_media(void *ctx,uint64_t at,const void *buf,uint32_t length)
{
    (void)ctx;
    if (!buf || at>sizeof(media) || length>sizeof(media)-at) return -1;
    memcpy(media+(size_t)at,buf,length);
    ++writes;
    return 0;
}
static int flush_media(void *ctx) { (void)ctx;return 0; }
static void *allocate(void *ctx,size_t bytes) { (void)ctx;return malloc(bytes); }
static void release(void *ctx,void *p) { (void)ctx;free(p); }
#define CHECK(expr) do { if(!(expr)) {     fprintf(stderr,"FAT12 V2 regression: %s (%u)\n",#expr,(unsigned)__LINE__);     return 1; } } while(0)

int main(void)
{
    const IfsFsModuleApiV2 *api=infiltrator_filesystem_api_v2();
    CHECK(ifs_fs_api_v2_valid(api,"fat12"));
    CHECK(api->group_mask ==
        (IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|
         IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)));
    const IfsFsCoreOpsV2 *core=
        (const IfsFsCoreOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    const IfsFsVolumeOpsV2 *volume_ops=
        (const IfsFsVolumeOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    CHECK(core && volume_ops);
    CHECK((core->header.capabilities &
          (IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT|IFS_FS_CORE_STAT|
           IFS_FS_CORE_LIST|IFS_FS_CORE_READ|IFS_FS_CORE_WRITE|
           IFS_FS_CORE_CREATE|IFS_FS_CORE_REMOVE|IFS_FS_CORE_RENAME|
           IFS_FS_CORE_TRUNCATE)) == IFS_FS_CORE_ALL);
    CHECK((volume_ops->header.capabilities &
          (IFS_FS_VOLUME_QUERY|IFS_FS_VOLUME_SYNC|IFS_FS_VOLUME_CHECK|
           IFS_FS_VOLUME_REPAIR|IFS_FS_VOLUME_FORMAT)) ==
          (IFS_FS_VOLUME_QUERY|IFS_FS_VOLUME_SYNC|IFS_FS_VOLUME_CHECK|
           IFS_FS_VOLUME_REPAIR|IFS_FS_VOLUME_FORMAT));

    IfsFsHostRuntimeV2 host={0};
    host.abi_version=IFS_FS_HOST_ABI_V2;
    host.structure_bytes=sizeof(host);
    host.capabilities=IFS_FS_HOST_READ_ONLY;
    host.volume_bytes=sizeof(media);
    host.logical_sector_bytes=SECTOR;
    host.physical_sector_bytes=SECTOR;
    host.maximum_transfer_bytes=8192u;
    host.read_at=read_media;
    host.allocate=allocate;
    host.release=release;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(volume_ops->format(&host,12u,NULL,0u)==IFS_FS_ACCESS);
    CHECK(writes==0u);

    host.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
    host.write_at=write_media;
    host.flush=flush_media;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(volume_ops->format(&host,16u,NULL,0u)==IFS_FS_UNSUPPORTED);
    CHECK(volume_ops->format(&host,12u,"NOT-SUPPORTED",0u)==IFS_FS_UNSUPPORTED);
    CHECK(volume_ops->format(&host,12u,NULL,1u)==IFS_FS_UNSUPPORTED);
    CHECK(writes==0u);
    CHECK(volume_ops->format(&host,12u,NULL,0u)==IFS_FS_OK);
    CHECK(writes>0u);

    uint32_t variant=0u;
    CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==12u);
    IfsFsVolumeInfoV2 info={0};
    info.structure_bytes=sizeof(info);
    void *mounted=NULL;
    volatile size_t short_bytes=IFS_FS_HOST_V2_BASE_BYTES;
    IfsFsHostRuntimeV2 *short_host=malloc(short_bytes);
    CHECK(short_host);
    memcpy(short_host,&host,IFS_FS_HOST_V2_BASE_BYTES);
    short_host->structure_bytes=IFS_FS_HOST_V2_BASE_BYTES;
    CHECK(core->mount(short_host,&mounted,&info)==IFS_FS_OK && mounted);
    CHECK(core->unmount(mounted)==IFS_FS_OK);
    free(short_host);
    mounted=NULL;info.structure_bytes=sizeof(info);
    CHECK(core->mount(&host,&mounted,&info)==IFS_FS_OK && mounted);
    CHECK(info.variant==12u && info.block_bytes && info.free_bytes<=info.total_bytes);
    IfsFsVolumeInfoV2 measured={0};
    measured.structure_bytes=sizeof(measured);
    CHECK(volume_ops->query(mounted,&measured)==IFS_FS_OK);
    CHECK(measured.free_bytes==info.free_bytes && measured.total_bytes==info.total_bytes);

    const char payload[]="STRICT-NATIVE-V2-FAT12";
    char data[sizeof(payload)]={0};
    char name[768]={0};
    IfsFsEntryV2 entry={0};
    entry.structure_bytes=sizeof(entry);
    entry.name=name;entry.name_capacity=sizeof(name);
    CHECK(core->create(mounted,"/TEST",IFS_FS_ENTRY_DIRECTORY)==IFS_FS_OK);
    CHECK(core->create(mounted,"/TEST/FILE.TXT",IFS_FS_ENTRY_FILE)==IFS_FS_OK);
    CHECK(core->write(mounted,"/TEST/FILE.TXT",0u,payload,sizeof(payload)-1u)==IFS_FS_OK);
    CHECK(core->read(mounted,"/TEST/FILE.TXT",0u,data,sizeof(payload)-1u)==IFS_FS_OK);
    CHECK(memcmp(data,payload,sizeof(payload)-1u)==0);
    CHECK(core->rename(mounted,"/TEST/FILE.TXT","/TEST/RENAMED.TXT")==IFS_FS_OK);
    CHECK(core->stat(mounted,"/TEST/RENAMED.TXT",&entry)==IFS_FS_OK);
    CHECK(entry.size_bytes==sizeof(payload)-1u);
    CHECK(core->stat(mounted,"/../TEST/RENAMED.TXT",&entry)==IFS_FS_INVALID);
    uint64_t errors=UINT64_MAX;
    CHECK(volume_ops->check(mounted,0u,&errors)==IFS_FS_OK && errors==0u);
    CHECK(volume_ops->sync(mounted)==IFS_FS_OK);
    CHECK(core->unmount(mounted)==IFS_FS_OK);
    CHECK(core->unmount(mounted)==IFS_FS_INVALID);

    host.capabilities=IFS_FS_HOST_READ_ONLY;
    host.write_at=NULL;host.flush=NULL;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(volume_ops->format(&host,12u,NULL,0u)==IFS_FS_ACCESS);
    info.structure_bytes=sizeof(info);
    CHECK(core->mount(&host,&mounted,&info)==IFS_FS_OK);
    CHECK(core->read(mounted,"/TEST/RENAMED.TXT",0u,data,sizeof(payload)-1u)==IFS_FS_OK);
    CHECK(memcmp(data,payload,sizeof(payload)-1u)==0);
    CHECK(core->write(mounted,"/TEST/RENAMED.TXT",0u,"x",1u)==IFS_FS_ACCESS);
    CHECK(volume_ops->repair(mounted,0u,&errors)==IFS_FS_ACCESS);
    CHECK(core->unmount(mounted)==IFS_FS_OK);
    puts("FAT12-STRICT-COMMON-V2-NATIVE-ROUNDTRIP-PASS");
    return 0;
}
