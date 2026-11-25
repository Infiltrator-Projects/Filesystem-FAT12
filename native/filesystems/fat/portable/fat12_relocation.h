/* SPDX-License-Identifier: GPL-3.0-or-later
 * FAT12 native cluster relocation. The classic scanner first proves complete
 * allocation ownership; the plan pins reserved/bad clusters and chooses
 * non-overlapping contiguous destinations. A move into an occupied target
 * uses a free scratch cluster, not destructive overwrite. Every individual
 * movement copies and reads back payload before atomically checkpointing
 * both FAT copies and the root directory pointer through the native redo
 * journal. The standard FAT layout remains directly interoperable.
 *
 * This first qualified writer is bounded to journal-enabled, flat FAT12
 * volumes. Foreign/non-journaled layouts and directories are never silently
 * rewritten using a weaker transaction path. Read-only planning is allowed;
 * a new plan is mandatory on an exclusive writable session.
 */
#ifndef IFS_FAT12_NATIVE_RELOCATION_H
#define IFS_FAT12_NATIVE_RELOCATION_H
#define FR_SLOTS 4086u
#define FR_FILES 512u
#define FR_PIN UINT16_MAX
typedef struct {
    uint32_t directory_sector,size;
    uint16_t directory_offset,first,count,target,reserve;
} FatRelocFile;
typedef struct {
    uint16_t cluster,file;
    uint64_t digest;
} FatRelocSlot;
typedef struct {
    IfsClassicFatVolume geometry;
    uint32_t identity,file_count,slot_count,free_count,policy;
    uint64_t fat_digest;
    int executed;
    FatRelocFile files[FR_FILES];
    FatRelocSlot slots[FR_SLOTS];
    uint16_t owner[FR_SLOTS];
} FatRelocPlan;
static uintptr_t fat_reloc_generation;
static uint64_t fr_hash(const void *data,uint32_t n)
{
    const uint8_t *p=data;
    uint64_t h=UINT64_C(14695981039346656037);
    for(uint32_t i=0u;i<n;++i)h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
static uint16_t fr_entry(const uint8_t *fat,uint32_t c)
{
    const uint32_t off=c+c/2u;
    const uint16_t v=(uint16_t)fj_u16(fat+off);
    return (uint16_t)((c&1u)?v>>4:v&0xfffu);
}
static void fr_set(uint8_t *fat,uint32_t c,uint16_t n)
{
    const uint32_t off=c+c/2u;
    uint16_t v=(uint16_t)fj_u16(fat+off);
    v=(uint16_t)((c&1u)?((v&0x000fu)|(n<<4)):((v&0xf000u)|n));
    fj_p16(fat+off,v);
}
static int fr_cancel(const IfsFsHostRuntimeV2 *h)
{return h->cancelled && h->cancelled(h->context);}
static int fr_geometry(const IfsFsHostRuntimeV2 *h,IfsClassicFatVolume *g)
{
    IfsFatV2ProbeIo probe={h,IFS_FS_OK};
    IfsClassicFatIo io={common_probe_read,&probe,h->volume_bytes};
    if(ifs_classic_fat_probe(&io,g)!=0)
        return probe.error==IFS_FS_OK?IFS_FS_CORRUPT:probe.error;
    if(g->cluster_count+2u>FR_SLOTS || !g->cluster_bytes ||
       g->cluster_bytes>65536u || g->root_entries>FR_FILES ||
       g->fat_copies!=2u ||
       h->logical_sector_bytes!=g->bytes_per_sector ||
       h->physical_sector_bytes!=g->bytes_per_sector)
        return IFS_FS_UNSUPPORTED;
    const uint32_t bytes=(uint32_t)g->sectors_per_fat*g->bytes_per_sector;
    const uint32_t last=g->cluster_count+1u;
    if(bytes>65536u || last+last/2u+2u>bytes)return IFS_FS_CORRUPT;
    return IFS_FS_OK;
}
static uint64_t fr_offset(const IfsClassicFatVolume *g,uint32_t cluster)
{return g->data_offset+(uint64_t)(cluster-2u)*g->cluster_bytes;}
static void fat_reloc_discard(void)
{
    if(common_session.reloc_plan)
        common_session.host.release(common_session.host.context,common_session.reloc_plan);
    common_session.reloc_plan=NULL;common_session.reloc_token=0u;
}
static int fr_plan(void *context,const char *path,uint32_t policy,void **out)
{
    if(!out)return IFS_FS_INVALID;
    *out=NULL;
    if(!common_session_valid(context) || !path)return IFS_FS_INVALID;
    if(strcmp(path,"/") || (policy!=0u && policy!=10u) ||
       !common_session.journal_present)return IFS_FS_UNSUPPORTED;
    if(common_session.reloc_plan || fat_reloc_generation==UINTPTR_MAX)return IFS_FS_BUSY;
    const IfsFsHostRuntimeV2 *h=&common_session.host;
    IfsClassicFatVolume g;
    int rc=fr_geometry(h,&g);if(rc!=IFS_FS_OK)return rc;
    uint32_t errors=0u;
    rc=common_check(context,&errors);if(rc!=IFS_FS_OK)return rc;
    if(errors)return IFS_FS_CORRUPT;
    uint32_t identity=0u;
    rc=fj_identity(h,&g,&identity);if(rc!=IFS_FS_OK)return rc;
    uint8_t header[4096];
    rc=fj_select(h,&g,identity,header);if(rc!=IFS_FS_OK)return rc;
    if(fj_u32(header+12))return IFS_FS_CORRUPT;
    const uint32_t sector=g.bytes_per_sector,unit=g.cluster_bytes;
    const uint32_t fat_bytes=(uint32_t)g.sectors_per_fat*sector;
    const uint32_t limit=g.cluster_count+2u;
    FatRelocPlan *p=h->allocate(h->context,sizeof(*p));
    uint8_t *fat=h->allocate(h->context,fat_bytes);
    uint8_t *data=h->allocate(h->context,unit);
    if(!p || !fat || !data){rc=IFS_FS_NO_MEMORY;goto done;}
    memset(p,0,sizeof(*p));p->geometry=g;p->identity=identity;p->policy=policy;
    rc=ifs_fs_host_read_bytes_v2(h,g.fat_offset,fat,fat_bytes);
    if(rc!=IFS_FS_OK)goto done;
    p->fat_digest=fr_hash(fat,fat_bytes);
    /* The native FAT12 fixed root owns no clusters. Resolve every live
     * short entry, including entries preceded by an LFN sequence. The
     * plan retains exact directory entry coordinates, not rewritten names. */
    uint8_t dir[4096];
    uint32_t last_sector=UINT32_MAX;
    for(uint32_t n=0u;n<g.root_entries;++n){
        if(fr_cancel(h)){rc=IFS_FS_BUSY;goto done;}
        const uint32_t at=n*32u;
        const uint32_t ds=(uint32_t)(g.root_offset/sector)+at/sector;
        if(ds!=last_sector){
            rc=fj_read(h,&g,ds,dir);if(rc!=IFS_FS_OK)goto done;
            last_sector=ds;
        }
        const uint8_t *e=dir+at%sector;
        if(!e[0])break;
        if(e[0]==0xe5u || e[11]==0x0fu || (e[11]&0x08u))continue;
        if(e[11]&0x10u){rc=IFS_FS_UNSUPPORTED;goto done;}
        if(fj_u16(e+20) || p->file_count>=FR_FILES){rc=IFS_FS_CORRUPT;goto done;}
        const uint32_t size=fj_u32(e+28),blocks=size/unit+(size%unit!=0u);
        if(blocks>limit-2u || blocks>FR_SLOTS-p->slot_count){
            rc=IFS_FS_CORRUPT;goto done;
        }
        FatRelocFile *f=&p->files[p->file_count];
        f->directory_sector=ds;f->directory_offset=(uint16_t)(at%sector);
        f->size=size;f->first=(uint16_t)p->slot_count;f->count=(uint16_t)blocks;
        uint32_t cluster=fj_u16(e+26);
        if(!blocks && cluster){rc=IFS_FS_CORRUPT;goto done;}
        for(uint32_t j=0u;j<blocks;++j){
            if(fr_cancel(h)){rc=IFS_FS_BUSY;goto done;}
            if(cluster<2u || cluster>=limit || cluster>=0xff0u || p->owner[cluster]){
                rc=IFS_FS_CORRUPT;goto done;
            }
            uint32_t next=fr_entry(fat,cluster);
            if((j+1u==blocks && next<0xff8u) ||
               (j+1u<blocks && (next<2u || next>=limit || next>=0xff0u))){
                rc=IFS_FS_CORRUPT;goto done;
            }
            rc=ifs_fs_host_read_bytes_v2(h,fr_offset(&g,cluster),data,unit);
            if(rc!=IFS_FS_OK)goto done;
            p->slots[p->slot_count]=(FatRelocSlot){
                (uint16_t)cluster,(uint16_t)p->file_count,fr_hash(data,unit)};
            p->owner[cluster]=(uint16_t)(++p->slot_count);
            cluster=next;
        }
        ++p->file_count;
    }
    for(uint32_t c=2u;c<limit;++c){
        const uint16_t value=fr_entry(fat,c);
        if(!value){
            if(p->owner[c]){rc=IFS_FS_CORRUPT;goto done;}
            if(c>=0xff0u)p->owner[c]=FR_PIN;
            else ++p->free_count;
        }else if(!p->owner[c]){
            if(value<0xff0u || value>0xff7u){rc=IFS_FS_CORRUPT;goto done;}
            p->owner[c]=FR_PIN;
        }
    }
    /* Unlike an allocate-new-file strategy, the goal may occupy existing
     * movable file clusters. Execution resolves these overlaps through a
     * single free scratch cluster. Reserved and bad clusters remain pinned. */
    uint32_t cursor=2u;
    for(uint32_t i=0u;i<p->file_count;++i){
        FatRelocFile *f=&p->files[i];
        if(!f->count)continue;
        f->reserve=policy?(uint16_t)((f->count+9u)/10u):0u;
        const uint32_t need=(uint32_t)f->count+f->reserve;
        uint32_t run=0u;
        for(;cursor<limit;++cursor){
            run=p->owner[cursor]==FR_PIN?0u:run+1u;
            if(run==need)break;
        }
        if(cursor==limit){rc=IFS_FS_NO_SPACE;goto done;}
        f->target=(uint16_t)(cursor+1u-need);++cursor;
    }
    if(!p->free_count){
        for(uint32_t i=0u;i<p->file_count;++i){
            const FatRelocFile *f=&p->files[i];
            for(uint32_t j=0u;j<f->count;++j)
                if(p->slots[f->first+j].cluster!=f->target+j){
                    rc=IFS_FS_NO_SPACE;goto done;
                }
        }
    }
    common_session.reloc_plan=p;common_session.reloc_token=++fat_reloc_generation;
    *out=(void *)common_session.reloc_token;p=NULL;rc=IFS_FS_OK;
done:
    if(data)h->release(h->context,data);
    if(fat)h->release(h->context,fat);
    if(p)h->release(h->context,p);
    return rc;
}
static FatRelocPlan *fr_token(void *context,void *token)
{
    if(!common_session_valid(context) || !token ||
       (uintptr_t)token!=common_session.reloc_token)return NULL;
    return common_session.reloc_plan;
}
/* Move exactly one live cluster to an unallocated cluster. All chain
 * references are derived from the validated native ownership snapshot.
 * The FAT's packed 12-bit entries share bytes; fr_set preserves the other
 * nibble and both mirrors are included in one durable metadata transaction. */
static int fr_move(FatRelocPlan *p,uint32_t slot,uint32_t dest,
                   uint8_t *fat,uint8_t *after,uint8_t *data,uint8_t *copy,
                   uint64_t *moved)
{
    const IfsFsHostRuntimeV2 *h=&common_session.host;
    const IfsClassicFatVolume *g=&p->geometry;
    const uint32_t sector=g->bytes_per_sector,unit=g->cluster_bytes;
    const uint32_t fat_bytes=(uint32_t)g->sectors_per_fat*sector;
    if(slot>=p->slot_count || dest<2u || dest>=g->cluster_count+2u ||
       dest>=0xff0u || p->owner[dest] || fr_entry(fat,dest))
        return IFS_FS_CORRUPT;
    FatRelocSlot *s=&p->slots[slot];
    FatRelocFile *f=&p->files[s->file];
    const uint32_t src=s->cluster,logical=slot-f->first;
    if(src<2u || src>=g->cluster_count+2u || logical>=f->count ||
       p->owner[src]!=slot+1u)return IFS_FS_CORRUPT;
    const uint16_t next=fr_entry(fat,src);
    if((logical+1u<f->count && next!=p->slots[slot+1u].cluster) ||
       (logical+1u==f->count && next<0xff8u))return IFS_FS_CORRUPT;
    if(fr_cancel(h))return IFS_FS_BUSY;
    uint8_t dir[4096];
    if(!logical){
        int rc=fj_read(h,g,f->directory_sector,dir);if(rc!=IFS_FS_OK)return rc;
        uint8_t *e=dir+f->directory_offset;
        if(!e[0] || e[0]==0xe5u || (e[11]&0x18u) ||
           fj_u16(e+20) || fj_u16(e+26)!=src || fj_u32(e+28)!=f->size)
            return IFS_FS_CORRUPT;
        fj_p16(e+26,dest);
    }else if(fr_entry(fat,p->slots[slot-1u].cluster)!=src)
        return IFS_FS_CORRUPT;
    int rc=ifs_fs_host_read_bytes_v2(h,fr_offset(g,src),data,unit);
    if(rc!=IFS_FS_OK)return rc;
    if(fr_hash(data,unit)!=s->digest)return IFS_FS_CORRUPT;
    rc=ifs_fs_host_write_bytes_v2(h,fr_offset(g,dest),data,unit);
    if(rc!=IFS_FS_OK)return rc;
    rc=fj_flush(h);if(rc!=IFS_FS_OK)return rc;
    rc=ifs_fs_host_read_bytes_v2(h,fr_offset(g,dest),copy,unit);
    if(rc!=IFS_FS_OK)return rc;
    if(memcmp(data,copy,unit))return IFS_FS_IO_ERROR;
    memcpy(after,fat,fat_bytes);
    fr_set(after,dest,next);
    if(logical)fr_set(after,p->slots[slot-1u].cluster,(uint16_t)dest);
    fr_set(after,src,0u);
    FjPatch patches[FJ_MAX_PATCH];uint32_t count=0u;
    const uint32_t base=(uint32_t)(g->fat_offset/sector);
    for(uint32_t n=0u;n<g->sectors_per_fat;++n){
        uint8_t *block=after+(size_t)n*sector;
        if(!memcmp(fat+(size_t)n*sector,block,sector))continue;
        if(count+2u>FJ_MAX_PATCH-1u)return IFS_FS_CORRUPT;
        patches[count++]=(FjPatch){base+n,block};
        patches[count++]=(FjPatch){base+g->sectors_per_fat+n,block};
    }
    if(!logical)patches[count++]=(FjPatch){f->directory_sector,dir};
    rc=fj_commit(h,g,p->identity,patches,count);
    if(rc!=IFS_FS_OK)return rc;
    memcpy(fat,after,fat_bytes);
    p->owner[src]=0u;p->owner[dest]=(uint16_t)(slot+1u);
    s->cluster=(uint16_t)dest;
    const uint64_t remaining=f->size-(uint64_t)logical*unit;
    *moved+=remaining<unit?remaining:unit;
    return IFS_FS_OK;
}
static int fr_execute(void *context,void *token,uint64_t *moved)
{
    if(!moved)return IFS_FS_INVALID;
    *moved=0u;
    FatRelocPlan *p=fr_token(context,token);
    if(!p)return IFS_FS_INVALID;
    const IfsFsHostRuntimeV2 *h=&common_session.host;
    const IfsClassicFatVolume *g=&p->geometry;
    if(!fj_writable(h,g))return IFS_FS_ACCESS;
    if(p->executed)return IFS_FS_BUSY;
    const uint32_t unit=g->cluster_bytes;
    const uint32_t fat_bytes=(uint32_t)g->sectors_per_fat*g->bytes_per_sector;
    uint8_t *fat=h->allocate(h->context,fat_bytes),*after=h->allocate(h->context,fat_bytes);
    uint8_t *data=h->allocate(h->context,unit),*copy=h->allocate(h->context,unit);
    int rc=IFS_FS_NO_MEMORY;
    if(!fat || !after || !data || !copy)goto done;
    rc=ifs_fs_host_read_bytes_v2(h,g->fat_offset,fat,fat_bytes);
    if(rc!=IFS_FS_OK)goto done;
    if(fr_hash(fat,fat_bytes)!=p->fat_digest){rc=IFS_FS_CORRUPT;goto done;}
    /* FatFs caches cannot coexist with direct provider-owned metadata
     * transactions. Close the native mount, retain its callback transport,
     * and remount it only after all checkpoints have completed. */
    IfsFatfsIo io=device;
    if(detach()!=0){rc=IFS_FS_IO_ERROR;goto done;}
    p->executed=-1;
    uint32_t scratch_cursor=2u;
    for(uint32_t i=0u;i<p->file_count;++i){
        const FatRelocFile *f=&p->files[i];
        for(uint32_t j=0u;j<f->count;++j){
            const uint32_t slot=f->first+j,target=f->target+j;
            if(p->slots[slot].cluster==target)continue;
            if(p->owner[target]){
                if(p->owner[target]==FR_PIN){rc=IFS_FS_CORRUPT;goto fault;}
                const uint32_t limit=g->cluster_count+2u;
                uint32_t scanned=0u;
                for(;scanned<limit-2u;++scanned){
                    if(!p->owner[scratch_cursor])break;
                    if(++scratch_cursor==limit)scratch_cursor=2u;
                }
                if(scanned==limit-2u){rc=IFS_FS_NO_SPACE;goto fault;}
                rc=fr_move(p,p->owner[target]-1u,scratch_cursor,fat,after,data,copy,moved);
                if(rc!=IFS_FS_OK)goto fault;
            }
            rc=fr_move(p,slot,target,fat,after,data,copy,moved);
            if(rc!=IFS_FS_OK)goto fault;
        }
    }
    if(attach(&io)!=0){rc=IFS_FS_IO_ERROR;goto fault;}
    p->executed=1;rc=IFS_FS_OK;goto done;
fault:
    /* A failed checkpoint may still have a committed redo record. Refuse
     * further operations on this session. A fresh writable mount replays
     * the record; already completed moves remain a valid FAT filesystem. */
    common_session.mutation_fault=1;
done:
    if(copy)h->release(h->context,copy);
    if(data)h->release(h->context,data);
    if(after)h->release(h->context,after);
    if(fat)h->release(h->context,fat);
    return rc;
}
static int fr_verify(void *context,void *token)
{
    FatRelocPlan *p=fr_token(context,token);
    if(!p || p->executed!=1)return IFS_FS_INVALID;
    const IfsFsHostRuntimeV2 *h=&common_session.host;
    const IfsClassicFatVolume *g=&p->geometry;
    uint32_t errors=0u;
    int rc=common_check(context,&errors);if(rc!=IFS_FS_OK)return rc;
    if(errors)return IFS_FS_CORRUPT;
    uint8_t *fat=h->allocate(h->context,(size_t)g->sectors_per_fat*g->bytes_per_sector);
    uint8_t *data=h->allocate(h->context,g->cluster_bytes);
    if(!fat || !data){rc=IFS_FS_NO_MEMORY;goto done;}
    rc=ifs_fs_host_read_bytes_v2(h,g->fat_offset,fat,(uint32_t)g->sectors_per_fat*g->bytes_per_sector);
    if(rc!=IFS_FS_OK)goto done;
    uint32_t free_count=0u;
    for(uint32_t c=2u;c<g->cluster_count+2u && c<0xff0u;++c)
        if(!fr_entry(fat,c))++free_count;
    if(free_count!=p->free_count){rc=IFS_FS_CORRUPT;goto done;}
    uint8_t dir[4096];
    for(uint32_t i=0u;i<p->file_count;++i){
        const FatRelocFile *f=&p->files[i];
        rc=fj_read(h,g,f->directory_sector,dir);if(rc!=IFS_FS_OK)goto done;
        const uint8_t *e=dir+f->directory_offset;
        if(!e[0] || e[0]==0xe5u || (e[11]&0x18u) || fj_u16(e+20) ||
           fj_u16(e+26)!=(f->count?f->target:0u) || fj_u32(e+28)!=f->size){
            rc=IFS_FS_CORRUPT;goto done;
        }
        for(uint32_t j=0u;j<f->count;++j){
            if(fr_cancel(h)){rc=IFS_FS_BUSY;goto done;}
            const uint32_t cluster=f->target+j,next=fr_entry(fat,cluster);
            if(p->slots[f->first+j].cluster!=cluster ||
               (j+1u<f->count && next!=cluster+1u) ||
               (j+1u==f->count && next<0xff8u)){
                rc=IFS_FS_CORRUPT;goto done;
            }
            rc=ifs_fs_host_read_bytes_v2(h,fr_offset(g,cluster),data,g->cluster_bytes);
            if(rc!=IFS_FS_OK)goto done;
            if(fr_hash(data,g->cluster_bytes)!=p->slots[f->first+j].digest){
                rc=IFS_FS_CORRUPT;goto done;
            }
        }
        for(uint32_t j=0u;j<f->reserve;++j)
            if(fr_entry(fat,f->target+f->count+j)){rc=IFS_FS_CORRUPT;goto done;}
    }
    rc=IFS_FS_OK;
done:
    if(data)h->release(h->context,data);
    if(fat)h->release(h->context,fat);
    return rc;
}
static int fr_recover(void *context,uint32_t flags,uint64_t *repaired)
{
    if(!repaired)return IFS_FS_INVALID;
    *repaired=0u;
    if(!common_session_valid(context) || flags)return IFS_FS_INVALID;
    if(common_session.host.capabilities&IFS_FS_HOST_READ_ONLY)return IFS_FS_ACCESS;
    if(!common_session.journal_present)return IFS_FS_UNSUPPORTED;
    if(common_session.reloc_plan)return IFS_FS_BUSY;
    IfsClassicFatVolume g;
    int rc=fr_geometry(&common_session.host,&g);if(rc!=IFS_FS_OK)return rc;
    uint32_t identity=0u;uint8_t header[4096];
    rc=fj_identity(&common_session.host,&g,&identity);if(rc!=IFS_FS_OK)return rc;
    rc=fj_select(&common_session.host,&g,identity,header);if(rc!=IFS_FS_OK)return rc;
    if(fj_u32(header+12))return IFS_FS_CORRUPT;
    uint32_t errors=0u;rc=common_check(context,&errors);
    if(rc!=IFS_FS_OK || errors)return rc!=IFS_FS_OK?rc:IFS_FS_CORRUPT;
    *repaired=common_session.mount_recovered;common_session.mount_recovered=0u;
    return IFS_FS_OK;
}
static int guarded_fr_plan(void *v,const char *path,uint32_t policy,void **p)
{
    if(p)*p=NULL;
    if(!v2_enter_operation())return IFS_FS_BUSY;
    return v2_leave_operation(fr_plan(v,path,policy,p));
}
static int guarded_fr_execute(void *v,void *p,uint64_t *moved)
{
    if(moved)*moved=0u;
    if(!v2_enter_operation())return IFS_FS_BUSY;
    return v2_leave_operation(fr_execute(v,p,moved));
}
static int guarded_fr_verify(void *v,void *p)
{
    if(!v2_enter_operation())return IFS_FS_BUSY;
    return v2_leave_operation(fr_verify(v,p));
}
static int guarded_fr_recover(void *v,uint32_t flags,uint64_t *repaired)
{
    if(repaired)*repaired=0u;
    if(!v2_enter_operation())return IFS_FS_BUSY;
    return v2_leave_operation(fr_recover(v,flags,repaired));
}
static void guarded_fr_release(void *v,void *p)
{
    if(!v2_enter_operation())return;
    if(common_session.mounted && v && (uintptr_t)v==common_session.active_handle &&
       p && (uintptr_t)p==common_session.reloc_token)fat_reloc_discard();
    (void)v2_leave_operation(IFS_FS_OK);
}
#endif
