/* SPDX-License-Identifier: GPL-3.0-or-later
 * FAT12 provider-owned redo journal. Standard FAT data, directories and
 * allocation tables remain unchanged in format. Native formatting reserves
 * 64 sectors, identifies that reservation with an OEM tag, and initializes
 * two independent commit headers. No external journal, device pathname or
 * retired worker ABI is used. The host supplies bounded storage and durable
 * barriers only. Foreign FAT media can still use the same CORE engine.
 *
 * Protocol: write and flush every replacement metadata sector; publish
 * both checksummed commit headers; flush before checkpointing; apply all
 * replacements; flush; clear both headers. Replay is idempotent. A single
 * damaged/torn header cannot discard a valid committed transaction. Any
 * ambiguous geometry, conflicting header or invalid payload fails closed.
 * This implementation requires equal filesystem/logical/physical sector
 * sizes for mutations; cross-sector journal atomicity is not assumed.
 */
#ifndef IFS_FAT12_NATIVE_JOURNAL_H
#define IFS_FAT12_NATIVE_JOURNAL_H
#define FJ_RESERVED 64u
#define FJ_FIRST_DATA 3u
#define FJ_MAX_PATCH 60u
#define FJ_RECORD 32u
#define FJ_OEM "IFSJFAT1"
#define FJ_MAGIC "IFSJ12V1"

typedef struct {
    uint32_t sector;
    const uint8_t *data;
} FjPatch;
static uint32_t fj_u16(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static uint32_t fj_u32(const uint8_t *p)
{return fj_u16(p)|(fj_u16(p+2)<<16);}
static void fj_p16(uint8_t *p,uint32_t n)
{p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void fj_p32(uint8_t *p,uint32_t n)
{fj_p16(p,n);fj_p16(p+2,n>>16);}
static uint32_t fj_crc(const uint8_t *p,uint32_t n)
{
    uint32_t crc=UINT32_MAX;
    for(uint32_t i=0u;i<n;++i){
        crc^=p[i];
        for(unsigned j=0u;j<8u;++j)
            crc=(crc>>1)^((crc&1u)?UINT32_C(0xedb88320):0u);
    }
    return ~crc;
}
static int fj_read(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                   uint32_t sector,void *data)
{
    if(sector>=g->total_sectors)return IFS_FS_CORRUPT;
    return ifs_fs_host_read_bytes_v2(h,(uint64_t)sector*g->bytes_per_sector,
                                     data,g->bytes_per_sector);
}
static int fj_write(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                    uint32_t sector,const void *data)
{
    if(sector>=g->total_sectors)return IFS_FS_CORRUPT;
    return ifs_fs_host_write_bytes_v2(h,(uint64_t)sector*g->bytes_per_sector,
                                      data,g->bytes_per_sector);
}
static int fj_flush(const IfsFsHostRuntimeV2 *h)
{return h->flush(h->context)?IFS_FS_IO_ERROR:IFS_FS_OK;}
static int fj_writable(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g)
{
    return ifs_fs_host_v2_valid(h) && !(h->capabilities&IFS_FS_HOST_READ_ONLY) &&
           h->logical_sector_bytes==g->bytes_per_sector &&
           h->physical_sector_bytes==g->bytes_per_sector;
}
/* Bind headers to the BPB's stable allocation geometry and volume serial.
 * Root directory labels and ordinary file edits do not alter this identity. */
static int fj_identity(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                       uint32_t *identity)
{
    uint8_t boot[512];
    int rc=ifs_fs_host_read_bytes_v2(h,0u,boot,sizeof boot);
    if(rc!=IFS_FS_OK)return rc;
    if(memcmp(boot+3,FJ_OEM,8u))return IFS_FS_NOT_FOUND;
    if(g->fat_offset<(uint64_t)FJ_RESERVED*g->bytes_per_sector ||
       g->fat_copies!=2u || g->root_entries>512u ||
       g->fat_offset%g->bytes_per_sector ||
       g->root_offset%g->bytes_per_sector ||
       g->data_offset%g->bytes_per_sector)
        return IFS_FS_CORRUPT;
    *identity=fj_crc(boot+11,32u);
    return IFS_FS_OK;
}
static void fj_make_header(uint8_t *b,const IfsClassicFatVolume *g,
                           uint32_t identity,const FjPatch *p,uint32_t count)
{
    const uint32_t sector=g->bytes_per_sector;
    memset(b,0,sector);
    memcpy(b,FJ_MAGIC,8u);fj_p32(b+8,identity);fj_p32(b+12,count);
    fj_p32(b+16,sector);
    for(uint32_t i=0u;i<count;++i){
        fj_p32(b+FJ_RECORD+i*8u,p[i].sector);
        fj_p32(b+FJ_RECORD+i*8u+4u,fj_crc(p[i].data,sector));
    }
    fj_p32(b+20,fj_crc(b,sector));
}
static int fj_header_valid(uint8_t *b,const IfsClassicFatVolume *g,uint32_t identity)
{
    const uint32_t sector=g->bytes_per_sector,count=fj_u32(b+12);
    if(memcmp(b,FJ_MAGIC,8u) || fj_u32(b+8)!=identity ||
       fj_u32(b+16)!=sector || fj_u32(b+24) || fj_u32(b+28) ||
       count>FJ_MAX_PATCH || FJ_RECORD+8u*count>sector ||
       FJ_FIRST_DATA+count>FJ_RESERVED)return 0;
    const uint32_t stored=fj_u32(b+20);
    fj_p32(b+20,0u);uint32_t calculated=fj_crc(b,sector);fj_p32(b+20,stored);
    return stored==calculated;
}
/* Prefer any valid committed header over a clean header. Both represent
 * the same transaction if both are committed; no generation guessing. */
static int fj_select(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                     uint32_t identity,uint8_t *out)
{
    uint8_t other[4096];const uint32_t sector=g->bytes_per_sector;
    int rc=fj_read(h,g,1u,out);if(rc!=IFS_FS_OK)return rc;
    rc=fj_read(h,g,2u,other);if(rc!=IFS_FS_OK)return rc;
    const int a=fj_header_valid(out,g,identity),b=fj_header_valid(other,g,identity);
    if(!a&&!b)return IFS_FS_CORRUPT;
    if(a&&b&&fj_u32(out+12)&&fj_u32(other+12)&&memcmp(out,other,sector))
        return IFS_FS_CORRUPT;
    if(!a || (b&&!fj_u32(out+12)&&fj_u32(other+12)))memcpy(out,other,sector);
    return IFS_FS_OK;
}
/* The first implementation relocates regular files in the fixed FAT12
 * root. Replay can target only corresponding sectors in both FAT copies
 * and at most one root-directory sector. Never replay into arbitrary
 * file data or reserved sectors. */
static int fj_targets(const IfsClassicFatVolume *g,const FjPatch *p,uint32_t count)
{
    const uint32_t sector=g->bytes_per_sector;
    const uint32_t fat=(uint32_t)(g->fat_offset/sector),width=g->sectors_per_fat;
    const uint32_t root=(uint32_t)(g->root_offset/sector),end=(uint32_t)(g->data_offset/sector);
    uint32_t fats=0u,dirs=0u;
    if(!count || count>FJ_MAX_PATCH || FJ_RECORD+8u*count>sector ||
       root!=fat+2u*width)return IFS_FS_CORRUPT;
    for(uint32_t i=0u;i<count;++i){
        const uint32_t n=p[i].sector;
        if(!p[i].data || n<fat || n>=end)return IFS_FS_CORRUPT;
        for(uint32_t j=0u;j<i;++j)if(p[j].sector==n)return IFS_FS_CORRUPT;
        if(n>=root){if(++dirs>1u)return IFS_FS_CORRUPT;continue;}
        ++fats;
        const uint32_t twin=n<fat+width?n+width:n-width;
        int found=0;
        for(uint32_t j=0u;j<count;++j)
            if(p[j].sector==twin && !memcmp(p[i].data,p[j].data,sector))found=1;
        if(!found)return IFS_FS_CORRUPT;
    }
    return fats?IFS_FS_OK:IFS_FS_CORRUPT;
}
static int fj_publish(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                      const uint8_t *header)
{
    int rc=fj_write(h,g,1u,header);if(rc!=IFS_FS_OK)return rc;
    rc=fj_flush(h);if(rc!=IFS_FS_OK)return rc;
    rc=fj_write(h,g,2u,header);if(rc!=IFS_FS_OK)return rc;
    return fj_flush(h);
}
static int fj_clear(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                    uint32_t identity)
{
    uint8_t header[4096];fj_make_header(header,g,identity,NULL,0u);
    return fj_publish(h,g,header);
}
static int fj_checkpoint(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                         uint32_t identity,const FjPatch *p,uint32_t count)
{
    for(uint32_t i=0u;i<count;++i){
        int rc=fj_write(h,g,p[i].sector,p[i].data);if(rc!=IFS_FS_OK)return rc;
    }
    int rc=fj_flush(h);if(rc!=IFS_FS_OK)return rc;
    return fj_clear(h,g,identity);
}
static int fj_commit(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                     uint32_t identity,const FjPatch *p,uint32_t count)
{
    if(!fj_writable(h,g))return IFS_FS_ACCESS;
    int rc=fj_targets(g,p,count);if(rc!=IFS_FS_OK)return rc;
    for(uint32_t i=0u;i<count;++i){
        rc=fj_write(h,g,FJ_FIRST_DATA+i,p[i].data);if(rc!=IFS_FS_OK)return rc;
    }
    rc=fj_flush(h);if(rc!=IFS_FS_OK)return rc;
    uint8_t header[4096];fj_make_header(header,g,identity,p,count);
    rc=fj_publish(h,g,header);if(rc!=IFS_FS_OK)return rc;
    /* Cancellation is intentionally not observed after commit publication.
     * Finish checkpointing or retain a recoverable transaction. */
    return fj_checkpoint(h,g,identity,p,count);
}
static int fj_mount(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g,
                    int *present,uint64_t *recovered)
{
    uint32_t identity=0u;*present=0;*recovered=0u;
    int rc=fj_identity(h,g,&identity);
    if(rc==IFS_FS_NOT_FOUND)return IFS_FS_OK;
    if(rc!=IFS_FS_OK)return rc;
    *present=1;
    uint8_t header[4096];
    rc=fj_select(h,g,identity,header);if(rc!=IFS_FS_OK)return rc;
    const uint32_t count=fj_u32(header+12),sector=g->bytes_per_sector;
    if(!count)return IFS_FS_OK;
    if(!fj_writable(h,g))return IFS_FS_ACCESS;
    uint8_t *data=h->allocate(h->context,(size_t)count*sector);
    if(!data)return IFS_FS_NO_MEMORY;
    FjPatch patch[FJ_MAX_PATCH];
    for(uint32_t i=0u;i<count;++i){
        uint8_t *p=data+(size_t)i*sector;
        rc=fj_read(h,g,FJ_FIRST_DATA+i,p);if(rc!=IFS_FS_OK)goto done;
        if(fj_crc(p,sector)!=fj_u32(header+FJ_RECORD+i*8u+4u)){
            rc=IFS_FS_CORRUPT;goto done;
        }
        patch[i]=(FjPatch){fj_u32(header+FJ_RECORD+i*8u),p};
    }
    rc=fj_targets(g,patch,count);
    if(rc==IFS_FS_OK)rc=fj_checkpoint(h,g,identity,patch,count);
    if(rc==IFS_FS_OK)*recovered=1u;
done:
    h->release(h->context,data);return rc;
}
static int fj_initialize(const IfsFsHostRuntimeV2 *h,const IfsClassicFatVolume *g)
{
    uint32_t identity=0u;
    int rc=fj_identity(h,g,&identity);if(rc!=IFS_FS_OK)return rc;
    /* Initialization only writes reserved sectors on newly formatted media.
     * Clean headers can also be installed on a larger physical-sector host;
     * relocation itself remains gated until its atomicity is qualified. */
    return fj_clear(h,g,identity);
}
#endif
