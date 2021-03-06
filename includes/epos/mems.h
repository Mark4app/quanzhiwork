/*
*********************************************************************************************************
*                                                    ePOS
*                                   the Easy Portable/Player Operation System
*                                               mems sub-system
*
*                          (c) Copyright 1992-2006, Jean J. Labrosse, Weston, FL
*                                             All Rights Reserved
*
* File    : mems.h
* By      : Steven.ZGJ
* Version : V1.00
*********************************************************************************************************
*/
#ifndef _MEMS_H_
#define _MEMS_H_

#include "typedef.h"
#include "cfgs.h"

#ifdef EPDK_USED_BY_KRNL
#define __swi(x)
#endif
//**********************************************************************************************************
//* define level 0( system level)
//************************************************
// 常数定义

#define MEMS_normaldomain   0
#define MEMS_domain_user    0x0f
#define MEMS_domain_system  0x0f

// 定义内存低地址的对齐方式,1k/2k/4k/8k/16k/32k...
#define MEMS_PALLOC_MODE_BND_NONE   0x00
#define MEMS_PALLOC_MODE_BND_2K     0x01
#define MEMS_PALLOC_MODE_BND_4K     0x02
#define MEMS_PALLOC_MODE_BND_8K     0x03
#define MEMS_PALLOC_MODE_BND_16K    0x04
#define MEMS_PALLOC_MODE_BND_32K    0x05
#define MEMS_PALLOC_MODE_BND_MSK    0x0f

// 定义分配内存块的边界属性，目标块只能在对齐
//的内存块内分配,2M/4M/8M/16M/32M/64M...
#define MEMS_PALLOC_MODE_BNK_NONE   0x00
#define MEMS_PALLOC_MODE_BNK_2M     0x10
#define MEMS_PALLOC_MODE_BNK_4M     0x20
#define MEMS_PALLOC_MODE_BNK_8M     0x30
#define MEMS_PALLOC_MODE_BNK_16M    0x40
#define MEMS_PALLOC_MODE_BNK_32M    0x50
#define MEMS_PALLOC_MODE_BNK_64M    0x60
#define MEMS_PALLOC_MODE_BNK_128M   0x70
#define MEMS_PALLOC_MODE_BNK_256M   0x80
#define MEMS_PALLOC_MODE_BNK_512M   0x90
#define MEMS_PALLOC_MODE_BNK_1G     0xa0
#define MEMS_PALLOC_MODE_BNK_2G     0xb0
#define MEMS_PALLOC_MODE_BNK_4G     0xc0
#define MEMS_PALLOC_MODE_BNK_8G     0xd0
#define MEMS_PALLOC_MODE_BNK_16G    0xe0
#define MEMS_PALLOC_MODE_BNK_32G    0xf0
#define MEMS_PALLOC_MODE_BNK_MSK    0xf0

// 定义分配内存块的位置属性，目标块必须在该地址块
// 范围内进行分配,2M/4M/8M/16M/32M/64M...
#define MEMS_PALLOC_MODE_AREA_NONE  0x0000
#define MEMS_PALLOC_MODE_AREA_2M    0x0100
#define MEMS_PALLOC_MODE_AREA_4M    0x0200
#define MEMS_PALLOC_MODE_AREA_8M    0x0300
#define MEMS_PALLOC_MODE_AREA_16M   0x0400
#define MEMS_PALLOC_MODE_AREA_32M   0x0500
#define MEMS_PALLOC_MODE_AREA_64M   0x0500
#define MEMS_PALLOC_MODE_AREA_128M  0x0600
#define MEMS_PALLOC_MODE_AREA_256M  0x0700
#define MEMS_PALLOC_MODE_AREA_MSK   0x0f00


// 定义DRAM的size，由DRAM驱动实际扫描得到
extern __epos_para_t start_para;
#define DRAM_SIZE       (start_para.dram_para.size<<20)


// 系统堆位于SDRAM下边界以下,系统栈位于SDRAM顶边界(0xc0000000-0xc1ffffff)以上
// 系统堆和栈都需要通过内存管理模块创造出来
// 系统堆和栈大小可以动态变化
#define SYSHEAP_INIT_SIZE   (1*1024*1024)
#define SYSHEAP_MAX_SIZE    (4*1024*1024)
#define SYSHEAP_SIZE    (1*1024*1024)
#define esSysheap       ((__mems_heap_t *)(DRAM_vBASE + DRAM_SIZE))
#define esVmheap        ((__u32)esSysheap + DRAM_SIZE)
#define esSysstack      DRAM_vBASE
// 系统TLB物理地址
#define esTLBp          ((__u32 *)DRAM_pBASE)
// 系统TLB虚拟地址
#define esTLBv          ((__u32 *)DRAM_vBASE)

#define MEMS_maxfreelist    6
#define MEMS_nulllistmask   31
#define MEMS_errlist        ((__mems_mblk_t *)~31)

//************************************************
// 数据类型定义
typedef struct __MEMS_MBLK
{
    struct __MEMS_MBLK  *next;      //next free block
    __u32                len;       //current block size(bytes)/在balloc里，这个位置将存放链里freeblock数
} __mems_mblk_t;

typedef struct __MEMS_PAGE
{
    __u8    byte[1024];
} __mems_page_t;

typedef struct __MEMS_PGPOOL
{
    //当前操作的地方，这个参数用于指定分配空间时的起始查找页，每次分配都是从这个位置开始，当找到结尾后再从头开始找
    __u32       startindex;     //start find index in the bitmap table
    __u32       tblsize;        //table size

    __u32       npage;          //此池中一共有多少页
    __u32       nfree;          //free pages

    __s32       page_idx[8];    //1M/2M/4M/...物理页面在系统页池中的索引号, 如果该值为-1,表示页池中没有此页

    __u32       *bmptbl_v;      //只是bitmaptable的地址,当表中bit为1，表示空闲
    __mems_page_t       *pool_v;//指示页池的虚拟地址
    __mems_page_t    *pool_p;//页池的物理地址
} __mems_pgpool_t;

typedef struct __MEMS_HEAP
{
    __mems_mblk_t   *free;      //free blocks link
    __u32           size;       //heap size
    //    __u8            heap[size];
} __mems_heap_t;


typedef struct __MEMS_BLKGRP
{
    struct __MEMS_BLKGRP    *next;      //指向下一个group的地址, 由于编程方便，此单一必须放在开始的位置
    __u16                   nfree;      //当前group的free的block数
    __u16                   nlistfree;  //此grp总共还有多少个freelist没有挂接1page的链
    __mems_mblk_t           *freelist[MEMS_maxfreelist];    //前27位为地址， 后5位为free counter
} __mems_blkgrp_t;          //32byte一个， 一个group里可以供分配的block大小的总和为6Kbytes，若需求超过，
//需要另外再添加新的group，相同大小block的group将链接为一个group list
//以后若由于需要，要扩大此数据结构，也只能是64/128/256/512/...
#define VHEAP_BMP_SIZE  (16)
typedef struct __VIRTUAL_HEAP
{
    __mems_page_t   *head;      //虚拟堆的起始地址

    __u32           npage;      //虚拟堆总页数，虚拟堆的一个页占1M空间
    __u32           nfree;      //虚拟堆的空闲页数

    __u32           bmptbl[VHEAP_BMP_SIZE];     //虚拟堆页面的状态图，虚拟堆最大为32*16M=512Mbyte

} __virtual_heap_t;


typedef struct __MEMS_INFO
{
    __u32   pagepool_npage;
    __u32   pagepool_nfree;
    __u32   sysheap_size;
    __u32   sysheap_free;
} __mems_info_t;

//************************************************
// 变量定义
/* GLOBAL VARIABLES */

//************************************************
// 函数定义
/* system call table */
typedef struct
{
    __pSWI_t esMEMS_Palloc             ;
    __pSWI_t esMEMS_Pfree              ;
    __pSWI_t esMEMS_VA2PA              ;
    __pSWI_t esMEMS_VMCreate           ;
    __pSWI_t esMEMS_VMDelete           ;
    __pSWI_t esMEMS_HeapCreate         ;
    __pSWI_t esMEMS_HeapDel            ;
    __pSWI_t esMEMS_Malloc             ;
    __pSWI_t esMEMS_Mfree              ;
    __pSWI_t esMEMS_Realloc            ;
    __pSWI_t esMEMS_Calloc             ;
    __pSWI_t esMEMS_Balloc             ;
    __pSWI_t esMEMS_Bfree              ;
    __pSWI_t esMEMS_Info               ;

    __pSWI_t esMEMS_CleanDCache        ;
    __pSWI_t esMEMS_CleanFlushDCache   ;
    __pSWI_t esMEMS_CleanFlushCache    ;
    __pSWI_t esMEMS_FlushDCache        ;
    __pSWI_t esMEMS_FlushICache        ;
    __pSWI_t esMEMS_FlushCache         ;
    __pSWI_t esMEMS_CleanDCacheRegion  ;
    __pSWI_t esMEMS_CleanFlushDCacheRegion ;
    __pSWI_t esMEMS_CleanFlushCacheRegion  ;
    __pSWI_t esMEMS_FlushDCacheRegion  ;
    __pSWI_t esMEMS_FlushICacheRegion  ;
    __pSWI_t esMEMS_FlushCacheRegion   ;

    __pSWI_t esMEMS_VMalloc            ;
    __pSWI_t esMEMS_VMfree             ;
    __pSWI_t esMEMS_FreeMemSize        ;
    __pSWI_t esMEMS_TotalMemSize       ;

    __pSWI_t esMEMS_LockICache         ;
    __pSWI_t esMEMS_LockDCache         ;
    __pSWI_t esMEMS_UnlockICache       ;
    __pSWI_t esMEMS_UnlockDCache       ;
    __pSWI_t esMEMS_GetIoVAByPA        ;
    __pSWI_t esMEMS_PhyAddrConti       ;

} SWIHandler_MEMS_t;

#ifndef SIM_PC_WIN
#define SYSCALL_MEMS(x,y) __swi(SWINO(SWINO_MEMS, SWIHandler_MEMS_t, y))x y
#else
#define SYSCALL_MEMS(x,y) x y
#endif

/* system pages management */
SYSCALL_MEMS(void *, esMEMS_Palloc   )(__u32 /*npage*/, __u32 /*mode*/);
SYSCALL_MEMS(void,  esMEMS_Pfree    )(void */*pblk*/, __u32 /*npage*/);
SYSCALL_MEMS(void *, esMEMS_VA2PA    )(void */*addr_v*/);
SYSCALL_MEMS(__s32, esMEMS_VMCreate )(__mems_page_t */*pblk*/, __u32 /*size*/, __u8 /*domain*/);
SYSCALL_MEMS(void,  esMEMS_VMDelete )(__mems_page_t */*pblk*/, __u32 /*npage*/);
/* system malloc/free management */
SYSCALL_MEMS(__s32, esMEMS_HeapCreate)(void * /*heapaddr*/, __u32 /*initnpage*/);
SYSCALL_MEMS(void,  esMEMS_HeapDel  )(__mems_heap_t * /*heap*/);
SYSCALL_MEMS(void *, esMEMS_Malloc   )(__mems_heap_t *heap, __u32 size);
SYSCALL_MEMS(void,  esMEMS_Mfree    )(__mems_heap_t *heap, void *f);
SYSCALL_MEMS(void *, esMEMS_Realloc  )(__mems_heap_t *heap, void *f, __u32 size);
SYSCALL_MEMS(void *, esMEMS_Calloc   )(__mems_heap_t *heap, __u32 n, __u32 m);
/* system bmalloc/bfree management */
SYSCALL_MEMS(void *, esMEMS_Balloc   )(__u32 size);
SYSCALL_MEMS(void,  esMEMS_Bfree    )(void *mblk, __u32 size);
SYSCALL_MEMS(__s32, esMEMS_Info     )(void);
/*cache operations*/
SYSCALL_MEMS(void,  esMEMS_CleanDCache)(void);
SYSCALL_MEMS(void,  esMEMS_CleanFlushDCache)(void);
SYSCALL_MEMS(void,  esMEMS_CleanFlushCache )(void);
SYSCALL_MEMS(void,  esMEMS_FlushDCache)(void);
SYSCALL_MEMS(void,  esMEMS_FlushICache)(void);
SYSCALL_MEMS(void,  esMEMS_FlushCache )(void);
SYSCALL_MEMS(void,  esMEMS_CleanDCacheRegion)(void *adr, __u32 bytes);
SYSCALL_MEMS(void,  esMEMS_CleanFlushDCacheRegion)(void *adr, __u32 bytes);
SYSCALL_MEMS(void,  esMEMS_CleanFlushCacheRegion )(void *adr, __u32 bytes);
SYSCALL_MEMS(void,  esMEMS_FlushDCacheRegion)(void *adr, __u32 bytes);
SYSCALL_MEMS(void,  esMEMS_FlushICacheRegion)(void *adr, __u32 bytes);
SYSCALL_MEMS(void,  esMEMS_FlushCacheRegion )(void *adr, __u32 bytes);

SYSCALL_MEMS(void *, esMEMS_VMalloc    )(__u32 size);
SYSCALL_MEMS(void,  esMEMS_VMfree     )(void *ptr);
SYSCALL_MEMS(__u32, esMEMS_FreeMemSize)(void);
SYSCALL_MEMS(__u32, esMEMS_TotalMemSize)(void);
SYSCALL_MEMS(__s32, esMEMS_PhyAddrConti)(void *mem, __u32 size);
SYSCALL_MEMS(__u32, esMEMS_GetIoVAByPA )(__u32 phyaddr, __u32 size);

SYSCALL_MEMS(__s32, esMEMS_LockICache  )(void *addr, __u32 size);
SYSCALL_MEMS(__s32, esMEMS_UnlockICache)(void *addr);
SYSCALL_MEMS(__s32, esMEMS_LockDCache  )(void *addr, __u32 size);
SYSCALL_MEMS(__s32, esMEMS_UnlockDCache)(void *addr);


static __inline void esMEMS_MMUFlushTLB(void)
{
    __u32 c8format = 0;
#ifndef SIM_PC_WIN
    __asm {mcr p15, 0, c8format, c8, c7, 0};
#endif
}
//**********************************************************************************************************

//**********************************************************************************************************
//* define level 1( system level)
//************************************************
// 常数定义

//************************************************
// 数据类型定义

//************************************************
// 变量定义
extern SWIHandler_MEMS_t SWIHandler_MEMS;
//************************************************
// 函数定义
__s32 MEMS_Init  (void);
__s32 MEMS_Exit  (void);
__s32 MEMS_PagePoolInfo(void);
void  esMEMS_set_fcse(__u8 id);
__u8  esMEMS_get_fcse(void);


//**********************************************************************************************************

#endif  /* _KRNL_H_ */

