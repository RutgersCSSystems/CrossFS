#ifndef _LINUX_DEVFS_H
#define _LINUX_DEVFS_H

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/blockgroup_lock.h>
#include <linux/percpu_counter.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/devfs_def.h>
#include <linux/kthread.h>

#include <linux/interval_tree.h>

typedef __le32  crfss_hash_t;

//#define _DEVFS_KTHREAD_DISABLED

#ifndef _DEVFS_KTHREAD_DISABLED
/*
 * Use per device thread file pointer linked list
 * instead of global file pointer linked list
 */
//#define _DEVFS_PER_THREAD_RD_LIST

/*
 * Use pthread as device thread instead of kthread
 * to avoid double copy as kthread do not recognize
 * a userspace pointer
 */
#define CRFS_SCHED_THREAD

/*
 * Bypass kernel without ioctl
 */
#define CRFS_BYPASS_KERNEL

/*
 * Multi process support
 */
//#define CRFS_MULTI_PROC

/* 
* Perf Tuning flag. Disabling this flag 
* avoids performance tuning related code changes
*/
//#define CRFS_PERF_TUNING


//#define CRFS_THRDLIST_ARR
//#define MAX_RDS 64

#endif

/*
 * Use PMFS_XIP code
 */
#define _DEVFS_XIP_IO

/* Fsync flag */
#define _DEVFS_FSYNC_ENABLE

#define _DEVFS_SCALABILITY

#define _DEVFS_SCHEDULER_RCU

/* Temporary flag for open/close optimization */
#define CRFS_OPENCLOSE_OPT

/*
 * Pre-allocate and reuse fd-queue data buffer instead of 
 * on-demand kmalloc()
 */
//#define CRFS_RD_QUEUE_PREALLOC
//#define CRFS_RD_QUEUE_PREALLOC_FULL
#define PREALLOC_THRESH_SIZE 2*PAGE_SIZE
#define PREALLOC_LARGE_SIZE PAGE_SIZE << 9



/*Allocation bitmap related flags*/
//#define _DEVFS_BITMAP
//#define _DEVFS_DEBUG_BITMAP
//#define _DEVFS_DEBUG_BITMAP_DISK

/* file stats debug*/
//#define _DEVFS_FSTATS        

//#define _DEVFS_JOURN
//#define _DEVFS_DEBUG       
#define _DEVFS_CHECK         
//#define _DEVFS_TESTING     
//#define _DEVFS_HETERO        
//#define _DEVFS_ONDEMAND_QUEUE
//#define _DEVFS_LOCKING
//#define _DEVFS_FREEZEPROT
//#define _DEVFS_MEMGMT
//#define _DEVFS_AGGR_DENTRY_EVICT
//#define _DEVFS_NOVA_LOG
#define _DEVFS_VM
#define _DEVFS_NOVA_BASED
//#define _DEVFS_DENTRY_OFFLOAD
//#define _DEVFS_INODE_OFFLOAD
#define _DEVFS_SLAB_ALLOC
//#define _DEVFS_USE_INODE_PGTREE
#define _DEVFS_FAKE_CLEAN

//Snappy compression from kernel
//#define _DEVFS_SNAPPY_TEST
#define _DEVFS_CONCURR_TEST

//DevFS max per-file queue pages
#define _DEVFS_QUEUE_PAGES 1000
#define _DEVFS_RDQUEUE       
//#define _DEVFS_DEBUG_RDWR
//#define _DEVFS_DIRECTIO
//#define AFTER_DEADLINE

#define _DEVFS_PMFS_BALLOC

#define _DEVFS_READ_DOUBLECOPY
//#define _DEVFS_KTHRD_SEMAPHORE
#define _TEMP_DEBUG
//#define _DEVFS_INODE_BUG
//#define _DEVFS_GLOBAL_LOCK
//#define _DYNAMIC_RD_QUEUE_ALLOC
#define _DEVFS_INITIALIZE 1

/*FIXME:YUJIE NAMES DO NOT MAKE SENSE*/
#define DEVFS_SUBMISSION_TREE_FOUND 0
#define DEVFS_SUBMISSION_TREE_NOTFOUND 1
#define DEVFS_SUBMISSION_TREE_PARFOUND 2

#define _DEVFS_INTERVAL_TREE
//#define _DEVFS_DISABLE_SQ
//#define _DEVFS_KTHRD_WAKEUP
//#define _DEVFS_SCALABILITY_DBG
//#define _DEVFS_SCHEDULER_DBG
#define _DEVFS_STAT

#define HOST_PROCESS_MAX 32
#define DEVICE_THREAD_MAX 8

#define MAX_FP_QSIZE 128

#define FILEJOURNSZ 102400
#define CACHEP_INIT 5

#define DEVFS_INVALID_SLBA -1

#define DEVFS_DEF_BLOCK_SIZE_4K 4096

#define CRED_ID_BYTES 16

#if 0
/* Processor Cache line related macros */
#define CACHELINE_SIZE  (64)
#define CLINE_SHIFT             (6)
#define CACHELINE_MASK  (~(CACHELINE_SIZE - 1))
#define CACHELINE_ALIGN(addr) (((addr)+CACHELINE_SIZE-1) & CACHELINE_MASK)

#define LOGENTRY_SIZE  CACHELINE_SIZE
#define LESIZE_SHIFT   CLINE_SHIFT

#define MAX_INODE_LENTRIES (2)
#define MAX_SB_LENTRIES (2)
/* 1 le for dir entry and 1 le for potentially allocating a new dir block */
#define MAX_DIRENTRY_LENTRIES   (2)
/* 2 le for adding or removing the inode from truncate list. used to log
 * potential changes to inode table's i_next_truncate and i_sum */
#define MAX_TRUNCATE_LENTRIES (2)
#define MAX_DATA_PER_LENTRY  48


/* blocksize * max_btree_height */
#define MAX_METABLOCK_LENTRIES \
        ((DEVFS_DEF_BLOCK_SIZE_4K * 3)/MAX_DATA_PER_LENTRY)

#define MAX_PTRS_PER_LENTRY (MAX_DATA_PER_LENTRY / sizeof(u64))

#define TRANS_RUNNING    1
#define TRANS_COMMITTED  2
#define TRANS_ABORTED    3

#define LE_DATA        0
#define LE_START       1
#define LE_COMMIT      2
#define LE_ABORT       4

#define MAX_GEN_ID  ((uint16_t)-1)
#endif

#define DEVFS_ASSERT(x)                                                 \
        if (!(x)) {                                                     \
                printk(KERN_WARNING "assertion failed %s:%d: %s\n",     \
                       __FILE__, __LINE__, #x);                         \
        }

/* Devfs file pointer state */
#define DEVFS_RD_IDLE	0
#define DEVFS_RD_BUSY	1

/* Devfs io-command state */
#define DEVFS_CMD_FINISH	1
#define DEVFS_CMD_READY		2
#define DEVFS_CMD_BUSY		4

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#define crfss_dbg(s, args ...)           pr_info(s, ## args)
//#define crfss_dbg(s, args ...) 
#define crfss_dbg1(s, args ...)
#define crfss_err(s, args ...)           crfss_error_mng(s, ## args)
#define crfss_warn(s, args ...)          pr_warning(s, ## args)
#define crfss_info(s, args ...)          pr_info(s, ## args)

extern unsigned int crfss_dbgmask;
#define DEVFS_DBGMASK_MMAPHUGE          (0x00000010)
#define DEVFS_DBGMASK_MMAP4K            (0x00000002)
#define DEVFS_DBGMASK_MMAPVERBOSE       (0x00000004)
#define DEVFS_DBGMASK_MMAPVVERBOSE      (0x00000008)
#define DEVFS_DBGMASK_VERBOSE           (0x00000001)
#define DEVFS_DBGMASK_TRANSACTION       (0x00000020)

#define crfss_dbg_verbose(s, args ...)            \
        ((crfss_dbgmask & DEVFS_DBGMASK_VERBOSE) ? crfss_dbg(s, ##args) : 0)
#define crfss_dbgv(s, args ...)  crfss_dbg_verbose(s, ##args)


#define crfss_test_and_clear_bit(nr, addr)      \
        __test_and_clear_bit((nr) ^ 16, (unsigned long *)(addr))

#define crfss_test_and_set_bit(nr, addr)        \
	__test_and_set_bit((nr), (unsigned long *)(addr))
//      __test_and_set_bit((nr) ^ 16, (unsigned long *)(addr))

#define crfss_set_bit(nr, addr) \
        __set_bit((nr) ^ 16, (unsigned long *)(addr))


static inline void crfss_error_mng(const char *fmt, ...)
{
        va_list args;
        printk("devfs error: ");
        va_start(args, fmt);
        vprintk(fmt, args);
        va_end(args);
}


static inline int crfss_test_bit(int nr, const void *vaddr)
{
        const unsigned short *p = vaddr;
        return (p[nr >> 4] & (1U << (nr & 15))) != 0;
}


/*################## DEVFS ##################################*/
/// Common command header (cdw 0-9)
typedef struct _nvme_command_common {
    u8                      opc;        ///< opcode
    u8                      fuse : 2;   ///< fuse
    u8                      rsvd : 6;   ///< reserved
    u16                     cid;        ///< command id
    u32                     nsid;       ///< namespace id
    u32                     cdw2_3[2];  ///< reserved (cdw 2-3)
    u64                     mptr;       ///< metadata pointer
    u64                     prp1;       ///< PRP entry 1
    u64                     prp2;       ///< PRP entry 2
} nvme_command_common_t;

/// NVMe command:  Read & Write
typedef struct _nvme_command_rw {
    nvme_command_common_t   common;     ///< common cdw 0
    u64                     slba;       ///< starting LBA (cdw 10)
    u64                     nlb;        ///< number of logical blocks
    u16                     rsvd12 : 10; ///< reserved (in cdw 12)
    u16                     prinfo : 4; ///< protection information field
    u16                     fua : 1;    ///< force unit access
    u16                     lr  : 1;    ///< limited retry
    u8                      dsm;        ///< dataset management
    u8                      rsvd13[3];  ///< reserved (in cdw 13)
    u32                     eilbrt;     ///< exp initial block reference tag
    u16                     elbat;      ///< exp logical block app tag
    u16                     elbatm;     ///< exp logical block app tag mask
    u64                     cmdtsc;     ///< TSC of the commands
    u64                     blk_addr;	///< log block address of this request
    u8                      kalloc;	///< allocate buffer from kmalloc or not
    u64                     ret;        ///< return value of this command
    u8	                    status;     ///< status of current command
    u8                      cred_id[16];///< 128-bit credential id
} nvme_cmdrw_t;




/// NVMe command op code
enum {
    NVME_CMD_FLUSH          = 0x0,      ///< flush
    NVME_CMD_WRITE          = 0x1,      ///< write
    NVME_CMD_READ           = 0x2,      ///< read
    NVME_CMD_WRITE_UNCOR    = 0x4,      ///< write uncorrectable
    NVME_CMD_COMPARE        = 0x5,      ///< compare
    NVME_CMD_DS_MGMT        = 0x9,      ///< dataset management
};

/// NVMe admin command op code
enum {
    NVME_ACMD_DELETE_SQ     = 0x0,      ///< delete io submission queue
    NVME_ACMD_CREATE_SQ     = 0x1,      ///< create io submission queue
    NVME_ACMD_GET_LOG_PAGE  = 0x2,      ///< get log page
    NVME_ACMD_DELETE_CQ     = 0x4,      ///< delete io completion queue
    NVME_ACMD_CREATE_CQ     = 0x5,      ///< create io completion queue
    NVME_ACMD_IDENTIFY      = 0x6,      ///< identify
    NVME_ACMD_ABORT         = 0x8,      ///< abort
    NVME_ACMD_SET_FEATURES  = 0x9,      ///< set features
    NVME_ACMD_GET_FEATURES  = 0xA,      ///< get features
    NVME_ACMD_ASYNC_EVENT   = 0xC,      ///< asynchronous event
    NVME_ACMD_FW_ACTIVATE   = 0x10,     ///< firmware activate
    NVME_ACMD_FW_DOWNLOAD   = 0x11,     ///< firmware image download
};

/// NVMe feature identifiers
enum {
    NVME_FEATURE_ARBITRATION = 0x1,     ///< arbitration
    NVME_FEATURE_POWER_MGMT = 0x2,      ///< power management
    NVME_FEATURE_LBA_RANGE = 0x3,       ///< LBA range type
    NVME_FEATURE_TEMP_THRESHOLD = 0x4,  ///< temperature threshold
    NVME_FEATURE_ERROR_RECOVERY = 0x5,  ///< error recovery
    NVME_FEATURE_WRITE_CACHE = 0x6,     ///< volatile write cache
    NVME_FEATURE_NUM_QUEUES = 0x7,      ///< number of queues
    NVME_FEATURE_INT_COALESCING = 0x8,  ///< interrupt coalescing
    NVME_FEATURE_INT_VECTOR = 0x9,      ///< interrupt vector config
    NVME_FEATURE_WRITE_ATOMICITY = 0xA, ///< write atomicity
    NVME_FEATURE_ASYNC_EVENT = 0xB,     ///< async event config
};

/// Version
typedef union _nvme_version {
    u32                 val;            ///< whole value
    struct {
        u8              rsvd;           ///< reserved
        u8              mnr;            ///< minor version number
        u16             mjr;            ///< major version number
    };
} nvme_version_t;

/// Admin queue attributes
typedef union _nvme_adminq_attr {
    u32                 val;            ///< whole value
    struct {
        u16             asqs;           ///< admin submission queue size
        u16             acqs;           ///< admin completion queue size
    };
} nvme_adminq_attr_t;

/// Controller capabilities
typedef union _nvme_controller_cap {
    u64                 val;            ///< whole value
    struct {
        u16             mqes;           ///< max queue entries supported
        u8              cqr     : 1;    ///< contiguous queues required
        u8              ams     : 2;    ///< arbitration mechanism supported
        u8              rsvd    : 5;    ///< reserved
        u8              to;             ///< timeout

        u32             dstrd   : 4;    ///< doorbell stride
        u32             nssrs   : 1;    ///< NVM subsystem reset supported
        u32             css     : 8;    ///< command set supported
        u32             rsvd2   : 3;    ///< reserved
        u32             mpsmin  : 4;    ///< memory page size minimum
        u32             mpsmax  : 4;    ///< memory page size maximum
        u32             rsvd3   : 8;    ///< reserved
    };
} nvme_controller_cap_t;

/// Controller configuration register
typedef union _nvme_controller_config {
    u32                 val;            ///< whole value
    struct {
        u32             en      : 1;    ///< enable
        u32             rsvd    : 3;    ///< reserved
        u32             css     : 3;    ///< I/O command set selected
        u32             mps     : 4;    ///< memory page size
        u32             ams     : 3;    ///< arbitration mechanism selected
        u32             shn     : 2;    ///< shutdown notification
        u32             iosqes  : 4;    ///< I/O submission queue entry size
        u32             iocqes  : 4;    ///< I/O completion queue entry size
        u32             rsvd2   : 8;    ///< reserved
    };
} nvme_controller_config_t;

/// Controller status register
typedef union _nvme_controller_status {
    u32                 val;            ///< whole value
    struct {
        u32             rdy     : 1;    ///< ready
        u32             cfs     : 1;    ///< controller fatal status
        u32             shst    : 2;    ///< shutdown status
        u32             rsvd    : 28;   ///< reserved
    };
} nvme_controller_status_t;

/// Controller register (bar 0)
typedef struct _nvme_controller_reg {
    nvme_controller_cap_t   cap;        ///< controller capabilities
    nvme_version_t          vs;         ///< version
    u32                     intms;      ///< interrupt mask set
    u32                     intmc;      ///< interrupt mask clear
    nvme_controller_config_t cc;        ///< controller configuration
    u32                     rsvd;       ///< reserved
    nvme_controller_status_t csts;      ///< controller status
    u32                     nssr;       ///< NVM subsystem reset
    nvme_adminq_attr_t      aqa;        ///< admin queue attributes
    u64                     asq;        ///< admin submission queue base address
    u64                     acq;        ///< admin completion queue base address
    u32                     rcss[1010]; ///< reserved and command set specific
    u32                     sq0tdbl[1024]; ///< sq0 tail doorbell at 0x1000
} nvme_controller_reg_t;


/// Admin command:  Delete I/O Submission & Completion Queue
typedef struct _nvme_acmd_delete_ioq {
    nvme_command_common_t   common;     ///< common cdw 0
    u16                     qid;        ///< queue id (cdw 10)
    u16                     rsvd10;     ///< reserved (in cdw 10)
    u32                     cwd11_15[5]; ///< reserved (cdw 11-15)
} nvme_acmd_delete_ioq_t;

/// Admin command:  Create I/O Submission Queue
typedef struct _nvme_acmd_create_sq {
    nvme_command_common_t   common;     ///< common cdw 0
    u16                     qid;        ///< queue id (cdw 10)
    u16                     qsize;      ///< queue size
    u16                     pc : 1;     ///< physically contiguous
    u16                     qprio : 2;  ///< interrupt enabled
    u16                     rsvd11 : 13; ///< reserved (in cdw 11)
    u16                     cqid;       ///< associated completion queue id
    u32                     cdw12_15[4]; ///< reserved (cdw 12-15)
} nvme_acmd_create_sq_t;

/// Admin command:  Get Log Page
typedef struct _nvme_acmd_get_log_page {
    nvme_command_common_t   common;     ///< common cdw 0
    u8                      lid;        ///< log page id (cdw 10)
    u8                      rsvd10a;    ///< reserved (in cdw 10)
    u16                     numd : 12;  ///< number of dwords
    u16                     rsvd10b : 4; ///< reserved (in cdw 10)
    u32                     rsvd11[5];  ///< reserved (cdw 11-15)
} nvme_acmd_get_log_page_t;

/// Admin command:  Create I/O Completion Queue
typedef struct _nvme_acmd_create_cq {
    nvme_command_common_t   common;     ///< common cdw 0
    u16                     qid;        ///< queue id (cdw 10)
    u16                     qsize;      ///< queue size
    u16                     pc : 1;     ///< physically contiguous
    u16                     ien : 1;    ///< interrupt enabled
    u16                     rsvd11 : 14; ///< reserved (in cdw 11)
    u16                     iv;         ///< interrupt vector
    u32                     cdw12_15[4]; ///< reserved (cdw 12-15)
} nvme_acmd_create_cq_t;

/// Admin command:  Identify
typedef struct _nvme_acmd_identify {
    nvme_command_common_t   common;     ///< common cdw 0
    u32                     cns;        ///< controller or namespace (cdw 10)
    u32                     cdw11_15[5]; ///< reserved (cdw 11-15)
} nvme_acmd_identify_t;

/// Admin command:  Abort
typedef struct _nvme_acmd_abort {
    nvme_command_common_t   common;     ///< common cdw 0
    u16                     sqid;       ///< submission queue id (cdw 10)
    u16                     cid;        ///< command id
    u32                     cdw11_15[5]; ///< reserved (cdw 11-15)
} nvme_acmd_abort_t;

/// Admin data:  Identify Controller Data
typedef struct _nvme_identify_ctlr {
    u16                     vid;        ///< PCI vendor id
    u16                     ssvid;      ///< PCI subsystem vendor id
    char                    sn[20];     ///< serial number
    char                    mn[40];     ///< model number
    char                    fr[8];      ///< firmware revision
    u8                      rab;        ///< recommended arbitration burst
    u8                      ieee[3];    ///< IEEE OUI identifier
    u8                      mic;        ///< multi-interface capabilities
    u8                      mdts;       ///< max data transfer size
    u8                      rsvd78[178]; ///< reserved (78-255)
    u16                     oacs;       ///< optional admin command support
    u8                      acl;        ///< abort command limit
    u8                      aerl;       ///< async event request limit
    u8                      frmw;       ///< firmware updates
    u8                      lpa;        ///< log page attributes
    u8                      elpe;       ///< error log page entries
    u8                      npss;       ///< number of power states support
    u8                      avscc;      ///< admin vendor specific config
    u8                      rsvd265[247]; ///< reserved (265-511)
    u8                      sqes;       ///< submission queue entry size
    u8                      cqes;       ///< completion queue entry size
    u8                      rsvd514[2]; ///< reserved (514-515)
    u32                     nn;         ///< number of namespaces
    u16                     oncs;       ///< optional NVM command support
    u16                     fuses;      ///< fused operation support
    u8                      fna;        ///< format NVM attributes
    u8                      vwc;        ///< volatile write cache
    u16                     awun;       ///< atomic write unit normal
    u16                     awupf;      ///< atomic write unit power fail
    u8                      nvscc;      ///< NVM vendoe specific config
    u8                      rsvd531[173]; ///< reserved (531-703)
    u8                      rsvd704[1344]; ///< reserved (704-2047)
    u8                      psd[1024];  ///< power state 0-31 descriptors
    u8                      vs[1024];   ///< vendor specific
} nvme_identify_ctlr_t;

/// Admin data:  Identify Namespace - LBA Format Data
typedef struct _nvme_lba_format {
    u16                     ms;         ///< metadata size
    u8                      lbads;      ///< LBA data size
    u8                      rp : 2;     ///< relative performance
    u8                      rsvd : 6;   ///< reserved
} nvme_lba_format_t;

/// Admin data:  Identify Namespace Data
typedef struct _nvme_identify_ns {
    u64                     nsze;       ///< namespace size
    u64                     ncap;       ///< namespace capacity
    u64                     nuse;       ///< namespace utilization
    u8                      nsfeat;     ///< namespace features
    u8                      nlbaf;      ///< number of LBA formats
    u8                      flbas;      ///< formatted LBA size
    u8                      mc;         ///< metadata capabilities
    u8                      dpc;        ///< data protection capabilities
    u8                      dps;        ///< data protection settings
    u8                      rsvd30[98]; ///< reserved (30-127)
    nvme_lba_format_t       lbaf[16];   ///< lba format support
    u8                      rsvd192[192]; ///< reserved (383-192)
    u8                      vs[3712];   ///< vendor specific
} nvme_identify_ns_t;

/// Admin data:  Get Log Page - Error Information
typedef struct _nvme_log_page_error {
    u64                     count;      ///< error count
    u16                     sqid;       ///< submission queue id
    u16                     cid;        ///< command id
    u16                     sf;         ///< status field
    u8                      byte;       ///< parameter byte error location
    u8                      bit: 3;     ///< parameter bit error location
    u8                      rsvd : 5;   ///< reserved
    u64                     lba;        ///< logical block address
    u32                     ns;         ///< name space
    u8                      vspec;      ///< vendor specific infomation
    u8                      rsvd29[35]; ///< reserved (29-63)
} nvme_log_page_error_t;

/// Admin data:  Get Log Page - SMART / Health Information
typedef struct _nvme_log_page_health {
    u8                      warn;       ///< critical warning
    u16                     temp;       ///< temperature
    u8                      avspare;     ///< available spare
    u8                      avsparethresh; ///< available spare threshold
    u8                      used;       ///< percentage used
    u8                      rsvd6[26];  ///< reserved (6-31)
    u64                     dur[2];     ///< data units read
    u64                     duw[2];     ///< data units written
    u64                     hrc[2];     ///< number of host read commands
    u64                     hwc[2];     ///< number of host write commands
    u64                     cbt[2];     ///< controller busy time
    u64                     pcycles[2]; ///< number of power cycles
    u64                     phours[2]; ///< power on hours
    u64                     unsafeshut[2]; ///< unsafe shutdowns
    u64                     merrors[2]; ///< media errors
    u64                     errlogs[2]; ///< number of error log entries
    u64                     rsvd192[320]; ///< reserved (192-511)
} nvme_log_page_health_t;

/// Admin data:  Get Log Page - Firmware Slot Information
typedef struct _nvme_log_page_fw {
    u8                      afi;        ///< active firmware info
    u8                      rsvd1[7];   ///< reserved (1-7)
    u64                     fr[7];      ///< firmware revision for slot 1-7
    u8                      rsvd64[448]; ///< reserved (64-511)
} nvme_log_page_fw_t;

/// Admin feature:  Arbitration
typedef struct _nvme_feature_arbitration {
    u8                      ab: 3;      ///< arbitration burst
    u8                      rsvd: 5;    ///< reserved
    u8                      lpw;        ///< low priority weight
    u8                      mpw;        ///< medium priority weight
    u8                      hpw;        ///< high priority weight
} nvme_feature_arbitration_t;

/// Admin feature:  Power Management
typedef struct _nvme_feature_power_mgmt {
    u32                     ps: 5;      ///< power state
    u32                     rsvd: 27;   ///< reserved
} nvme_feature_power_mgmt_t;

/// Admin feature:  LBA Range Type Data
typedef struct _nvme_feature_lba_data {
    struct {
        u8                  type;       ///< type
        u8                  attributes; ///< attributes
        u8                  rsvd[14];   ///< reserved
        u64                 slba;       ///< starting LBA
        u64                 nlb;        ///< number of logical blocks
        u8                  guid[16];   ///< unique id
        u8                  rsvd48[16]; ///< reserved
    } entry[64];                        ///< LBA data entry
} nvme_feature_lba_data_t;

/// Admin feature:  LBA Range Type
typedef struct _nvme_feature_lba_range {
    u32                     num: 6;     ///< number of LBA ranges
    u32                     rsvd: 26;   ///< reserved
} nvme_feature_lba_range_t;

/// Admin feature:  Temperature Threshold
typedef struct _nvme_feature_temp_threshold {
    u16                     tmpth;      ///< temperature threshold
    u16                     rsvd;       ///< reserved
} nvme_feature_temp_threshold_t;

/// Admin feature:  Error Recovery
typedef struct _nvme_feature_error_recovery {
    u16                     tler;       ///< time limited error recovery
    u16                     rsvd;       ///< reserved
} nvme_feature_error_recovery_t;

/// Admin feature:  Volatile Write Cache
typedef struct _nvme_feature_write_cache {
    u32                     wce: 1;     ///< volatile write cache
    u32                     rsvd: 31;   ///< reserved
} nvme_feature_write_cache_t;

/// Admin feature:  Number of Queues
typedef struct _nvme_feature_num_queues {
    u16                     nsq;        ///< numer of submission queues
    u16                     ncq;        ///< numer of completion queues
} nvme_feature_num_queues_t;

/// Admin feature:  Interrupt Coalescing
typedef struct _nvme_feature_int_coalescing {
    u8                      thr;        ///< aggregation threshold
    u8                      time;       ///< aggregation time
    u16                     rsvd;       ///< reserved
} nvme_feature_int_coalescing_t;

/// Admin feature:  Interrupt Vector Configuration
typedef struct _nvme_feature_int_vector {
    u16                     iv;         ///< interrupt vector
    u16                     cd: 1;      ///< coalescing disable
    u16                     rsvd: 15;   ///< reserved
} nvme_feature_int_vector_t;

/// Admin feature:  Write Atomicity
typedef struct _nvme_feature_write_atomicity {
    u32                     dn: 1;      ///< disable normal
    u32                     rsvd: 31;   ///< reserved
} nvme_feature_write_atomicity_t;

/// Admin feature:  Async Event Configuration
typedef struct _nvme_feature_async_event {
    u8                      smart;      ///< SMART / health critical warnings
    u8                      rsvd[3];    ///< reserved
} nvme_feature_async_event_t;

/// Admin command:  Get Feature
typedef struct _nvme_acmd_get_features {
    nvme_command_common_t   common;     ///< common cdw 0
    u8                      fid;        ///< feature id (cdw 10:0-7)
    u8                      rsvd10[3];  ///< reserved (cdw 10:8-31)
} nvme_acmd_get_features_t;

/// Admin command:  Set Feature
typedef struct _nvme_acmd_set_features {
    nvme_command_common_t   common;     ///< common cdw 0
    u8                      fid;        ///< feature id (cdw 10:0-7)
    u8                      rsvd10[3];  ///< reserved (cdw 10:8-31)
    u32                     val;        ///< cdw 11
} nvme_acmd_set_features_t;





/// Submission queue entry
typedef union _nvme_sq_entry {
    nvme_cmdrw_t       rw;         ///< read/write command

    nvme_acmd_abort_t       abort;      ///< admin abort command
    nvme_acmd_create_cq_t   create_cq;  ///< admin create IO completion queue
    nvme_acmd_create_sq_t   create_sq;  ///< admin create IO submission queue
    nvme_acmd_delete_ioq_t  delete_ioq; ///< admin delete IO queue
    nvme_acmd_identify_t    identify;   ///< admin identify command
    nvme_acmd_get_log_page_t get_log_page; ///< get log page command
    nvme_acmd_get_features_t get_features; ///< get feature
    nvme_acmd_set_features_t set_features; ///< set feature
} nvme_sq_entry_t;

/// Completion queue entry
typedef struct _nvme_cq_entry {
    u32                     cs;         ///< command specific
    u32                     rsvd;       ///< reserved
    u16                     sqhd;       ///< submission queue head
    u16                     sqid;       ///< submission queue id
    u16                     cid;        ///< command id
    union {
        u16                 psf;        ///< phase bit and status field
        struct {
            u16             p : 1;      ///< phase tag id
            u16             sc : 8;     ///< status code
            u16             sct : 3;    ///< status code type
            u16             rsvd3 : 2;  ///< reserved
            u16             m : 1;      ///< more
            u16             dnr : 1;    ///< do not retry
        };
    };
} nvme_cq_entry_t;


/// Queue context (a submission-completion queue pair context)
typedef struct _nvme_queue {
    struct _nvme_device*    dev;        ///< device reference
    int                     id;         ///< queue id
    int                     size;       ///< queue size
    nvme_sq_entry_t*        sq;         ///< submission queue
    nvme_cq_entry_t*        cq;         ///< completion queue
    u32*                    sq_doorbell; ///< submission queue doorbell
    u32*                    cq_doorbell; ///< completion queue doorbell
    int                     sq_head;    ///< submission queue head
    int                     sq_tail;    ///< submission queue tail
    int                     cq_head;    ///< completion queue head
    int                     cq_phase;   ///< completion queue phase bit
//#ifdef _USE_DEVFS
    void*                   vsq;    ///< virt submission queue
    size_t                  vsqlsz; ///< virt submission queue curr len
    int 		    vqentry_count;  ///< virt submission queue max len
//#endif
} nvme_queue_t;

/// Device context
typedef struct _nvme_device {
    nvme_controller_reg_t*  reg;        ///< register address map
    int                     dbstride;   ///< doorbell stride (in word size)
    int                     maxqcount;  ///< max queue count
    int                     maxqsize;   ///< max queue size
    int                     pageshift;  ///< pagesize shift
    struct _nvme_queue      adminq;     ///< admin queue reference
} nvme_device_t;




#define NUM_QUEUE_CMDS 1
#define MAX_DEVFS_BLOCKS 512000



/*
 * open.c
 */
#if 1
struct open_flags {
        int open_flag;
        umode_t mode;
        int acc_mode;
        int intent;
};
#endif


struct crfss_fstruct {

   struct circ_buf fifo;
   wait_queue_head_t fifo_event;
   int num_entries;
   int queuesize;
   int entrysize;
   /* fifo access is synchronized on the producer side by
   * struct_mutex held by submit code (otherwise we could
   * end up w/ cmds logged in different order than they
   * were executed).  And read_lock synchronizes the reads
   */
   struct mutex read_lock;

   //TODO:Needs to be part of the superblock
   __u64 **fsblocks;
   __u64 qnumblocks;
   //flag to indicate queue buffer initialization
   __u8  fqinit;

    struct file *fp;	
    int fd;
    loff_t fpos;

    /*DevFS initialization */
    int init_flg;

    /*Credential structure*/
    const struct cred *f_cred;

    /*Queue size for this file*/
    unsigned int qentrycnt; 	

#ifdef _DEVFS_SCALABILITY
	struct list_head list;

	/*Task struct initiating this I/O in host*/
	struct task_struct *task;

	/*Device thread context handling this I/O*/
	struct dev_thread_struct *dev_thread_ctx;

	/*Index in crfss_inode*/
	int index;

	/*Bytes done by each IO*/
	int iobytes;

	/*Request pointer*/
	nvme_cmdrw_t *req;

	/*rd close indicator*/
	volatile long unsigned int closed;

	/*synchronous read indicator*/
	volatile long unsigned int io_done;

	/*rd state in serive or not*/
	volatile long unsigned int state;

	/*Time Stamp Counter for current request*/
	unsigned long tsc;

	/*fsync indicator*/
	volatile int fsyncing;

	/*file name*/
	char fname[NAME_MAX];

#endif

#ifdef CRFS_THRDLIST_ARR
	int dev_thread_slot;
#endif
};

struct dev_thread_struct {
	/* open file pointer list for this device thread */
	struct crfss_fstruct dummy_rd;
	
	/* state of device thread (Idle, Exit or Done) */
	volatile long unsigned int state;

	/* Task struct initiating this I/O in host*/
	struct task_struct *task;

	/* Task struct handling this I/O in device*/
	struct task_struct *kthrd;

	/* Per device thread mutex */
	struct mutex per_thread_mutex;

	/* current serving rd list_head pointer */
	struct list_head *current_rd_list;

	/* current serving rd */
	struct crfss_fstruct *current_rd;

	/* number of active file pointers in this thread */
	int rd_nr;

#ifdef CRFS_PERF_TUNING
	/*number rqsts*/
	int rqsts;
#endif

#ifdef CRFS_THRDLIST_ARR
	int id;
	struct crfss_fstruct *rd_list_array[MAX_RDS];
	int nxt_free_slot;
	int prev_rd;
	int rd_list_hits;
	int rd_list_miss;
#endif
};

/* Per-inode interval tree */
struct req_tree_entry {
	void *blk_addr;
	int size;
	struct crfss_fstruct *rd;
	struct interval_tree_node it;
};


#if 0
/* Fixed size queue*/
#define QUEUESZ()\
	(NUM_QUEUE_CMDS * sizeof(nvme_cmdrw_t))

#define circ_count(circ) \
        (CIRC_CNT((circ)->head, (circ)->tail,  QUEUESZ()))
#define circ_count_to_end(circ) \
        (CIRC_CNT_TO_END((circ)->head, (circ)->tail,  QUEUESZ()))
/* space available: */
#define circ_space(circ) \
        (CIRC_SPACE((circ)->head, (circ)->tail,  QUEUESZ()))
#define circ_space_to_end(circ) \
        (CIRC_SPACE_TO_END((circ)->head, (circ)->tail,  QUEUESZ()))
#endif

/* Variable size queue*/
#define QUEUESZ(nentries)\
	(nentries * sizeof(__u64))

#define circ_count(circ, nentries) \
        (CIRC_CNT((circ)->head, (circ)->tail,  QUEUESZ(nentries)))
#define circ_count_to_end(circ, nentries) \
        (CIRC_CNT_TO_END((circ)->head, (circ)->tail,  QUEUESZ(nentries)))
/* space available: */
#define circ_space(circ, nentries) \
        (CIRC_SPACE((circ)->head, (circ)->tail,  QUEUESZ(nentries)))
#define circ_space_to_end(circ, nentries) \
        (CIRC_SPACE_TO_END((circ)->head, (circ)->tail,  QUEUESZ(nentries)))

#define DEBUGCMD(cmdrw) \
        printk(KERN_ALERT " %s %d slba %llu, nlb %llu opcode %d prp1 %lu\n", \
          __FUNCTION__,__LINE__, cmdrw->slba, \
          cmdrw->nlb, cmdrw->common.opc, cmdrw->common.prp1)


#if 0
/*
 * Structure of the super block in PMFS
 * The fields are partitioned into static and dynamic fields. The static fields
 * never change after file system creation. This was primarily done because
 * pmfs_get_block() returns NULL if the block offset is 0 (helps in catching
 * bugs). So if we modify any field using journaling (for consistency), we
 * will have to modify s_sum which is at offset 0. So journaling code fails.
 * This (static+dynamic fields) is a temporary solution and can be avoided
 * once the file system becomes stable and pmfs_get_block() returns correct
 * pointers even for offset 0.
 */
struct crfss_super_block {
        /* static fields. they never change after file system creation.
         * checksum only validates up to s_start_dynamic field below */
        //__le16          s_sum;              /* checksum of this sb */
        __le16          s_magic;            /* magic signature */
        __le32          s_blocksize;        /* blocksize in bytes */
        __le64          s_size;             /* total size of fs in bytes */
        //char            s_volume_name[16];  /* volume name */
        /* points to the location of pmfs_journal_t */
        //__le64          s_journal_offset;
        /* points to the location of struct pmfs_inode for the inode table */
        __le64          s_inode_table_offset;

        __le64       s_start_dynamic;

        /* all the dynamic fields should go here */
        /* s_mtime and s_wtime should be together and their order should not be
         * changed. we use an 8 byte write to update both of them atomically */
        __le32          s_mtime;            /* mount time */
        __le32          s_wtime;            /* write time */
        /* fields for fast mount support. Always keep them together */
        __le64          s_num_blocknode_allocated;
        __le64          s_num_free_blocks;
        __le32          s_inodes_count;
        __le32          s_free_inodes_count;
        __le32          s_inodes_used_count;
        __le32          s_free_inode_hint;
};
#endif


#ifdef _DEVFS_BITMAP
/*
 * devfs super-block data on disk
 */
struct crfss_super_block {
        __u16 s_ninodes;
        __u16 s_nzones;
        __u16 s_imap_blocks;
        __u16 s_zmap_blocks;
        __u16 s_firstdatazone;
        __u16 s_log_zone_size;
        __u32 s_max_size;
        __u16 s_magic;
        __u16 s_state;
        __u32 s_zones;
};
#endif

#if 1 //def _DEVFS_NOVA_BASED

#define READDIR_END                     (ULONG_MAX)
#define INVALID_CPU                     (-1)
#define SHARED_CPU                      (65536)
#define FREE_BATCH                      (16)


#define INVALID_MASK    4095
#define BLOCK_OFF(p)    ((p) & ~INVALID_MASK)

#define ENTRY_LOC(p)    ((p) & INVALID_MASK)


/* ===================== DevFS inode flags ================== */
enum inode_flags {
        DEVFS_INODE_IN_USE = 0x00000001,
        DEVFS_ATTR_INODE = 0x00000004,
        DEVFS_INODE_LOGGED = 0x00000008,
        HOST_INODE_DELETED = 0x00000010,
        DEVFS_LONG_SYMLINK = 0x00000040,
        DEVFS_PERMANENT_FLAG = 0x0000ffff,
        DEVFS_INODE_NO_CREATE = 0x00010000,
        DEVFS_INODE_WAS_WRITTEN = 0x00020000,
        DEVFS_NO_TRANSACTION = 0x00040000,
};



/* ======================= Log entry ========================= */
/* Inode entry in the log */

#define	INVALID_MASK	4095
#define	BLOCK_OFF(p)	((p) & ~INVALID_MASK)

#define	ENTRY_LOC(p)	((p) & INVALID_MASK)

enum crfss_entry_type {
	FILE_WRITE = 1,
	DIR_LOG,
	SET_ATTR,
	LINK_CHANGE,
	NEXT_PAGE,
};

static inline u8 crfss_get_entry_type(void *p)
{
	return *(u8 *)p;
}

static inline void crfss_set_entry_type(void *p, enum crfss_entry_type type)
{
	*(u8 *)p = type;
}

struct crfss_file_write_entry {
	/* ret of find_nvmm_block, the lowest byte is entry type */
	__le64	block;
	__le64	pgoff;
	__le32	num_pages;
	__le32	invalid_pages;
	/* For both ctime and mtime */
	__le32	mtime;
	__le32	padding;
	__le64	size;
} __attribute((__packed__));

struct crfss_inode_page_tail {
	__le64	padding1;
	__le64	padding2;
	__le64	padding3;
	__le64	next_page;
} __attribute((__packed__));

#define	LAST_ENTRY	4064
#define	PAGE_TAIL(p)	(((p) & ~INVALID_MASK) + LAST_ENTRY)

/* Fit in PAGE_SIZE */
struct	crfss_inode_log_page {
	char padding[LAST_ENTRY];
	struct crfss_inode_page_tail page_tail;
} __attribute((__packed__));

#define	EXTEND_THRESHOLD	256

/*
 * Structure of a directory log entry in NOVA.
 * Update DIR_LOG_REC_LEN if modify this struct!
 */
struct crfss_dentry {
	u8	entry_type;
	u8	name_len;               /* length of the dentry name */
	u8	file_type;              /* file type */
	u8	invalid;		/* Invalid now? */
	__le16	de_len;                 /* length of this dentry */
	__le16	links_count;
	__le32	mtime;			/* For both mtime and ctime */
	__le64	ino;                    /* inode no pointed to by this entry */
	__le64	size;
	char	name[DEVFS_NAME_LEN + 1];	/* File name */
} __attribute((__packed__));

#define DEVFS_DIR_PAD			8	/* Align to 8 bytes boundary */
#define DEVFS_DIR_ROUND			(DEVFS_DIR_PAD - 1)
#define DEVFS_DIR_LOG_REC_LEN(name_len)	(((name_len) + 29 + DEVFS_DIR_ROUND) & \
				      ~DEVFS_DIR_ROUND)

/* Struct of inode attributes change log (setattr) */
struct crfss_setattr_logentry {
	u8	entry_type;
	u8	attr;
	__le16	mode;
	__le32	uid;
	__le32	gid;
	__le32	atime;
	__le32	mtime;
	__le32	ctime;
	__le64	size;
} __attribute((__packed__));

/* Do we need this to be 32 bytes? */
struct crfss_link_change_entry {
	u8	entry_type;
	u8	padding;
	__le16	links;
	__le32	ctime;
	__le32	flags;
	__le32	generation;
	__le64	paddings[2];
} __attribute((__packed__));

enum alloc_type {
	LOG = 1,
	DATA,
};

#define	MMAP_WRITE_BIT	0x20UL	// mmaped for write
#define	IS_MAP_WRITE(p)	((p) & (MMAP_WRITE_BIT))
#define	MMAP_ADDR(p)	((p) & (PAGE_MASK))

static inline void crfss_update_tail(struct devfss_inode *pi, u64 new_tail)
{
	//DEVFS_PERSISTENT_BARRIER();
	//pi->log_tail = new_tail;
	//nova_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 1);
	//DEVFS_END_TIMING(update_tail_t, update_time);
}






#if defined(_DEVFS_SLAB_ALLOC)
/* DevFS Slab allocator definitions */
/*  Implement ceiling, floor functions.  */
#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )
//#define _DEVFS_SLAB_USER
#define _DEVFS_SLAB_KERNEL
//#define _DEVFS_SLAB_TESTING
#define _DEVFS_TEST_ITR 1024
//#define PAGE_SIZE 4096

extern ssize_t slab_pagesize;

struct slab_header {
    struct slab_header *prev, *next;
    uint64_t slots;
    uintptr_t refcount;
    struct slab_header *page;
    uint8_t data[] __attribute__((aligned(sizeof(void *))));
};

struct slab_chain {
    ssize_t itemsize, itemcount;
    ssize_t slabsize, pages_per_alloc;
    uint64_t initial_slotmask, empty_slotmask;
    uintptr_t alignment_mask;
    struct slab_header *partial, *empty, *full;
};
#endif



struct crfss_range_node {
        struct rb_node node;
        unsigned long range_low;
        unsigned long range_high;
};

struct free_list {
        spinlock_t s_lock;
        struct rb_root  block_free_tree;
        struct crfss_range_node *first_node;
        unsigned long   block_start;
        unsigned long   block_end;
        unsigned long   num_free_blocks;
        unsigned long   num_blocknode;

        /* Statistics */
        unsigned long   alloc_log_count;
        unsigned long   alloc_data_count;
        unsigned long   free_log_count;
        unsigned long   free_data_count;
        unsigned long   alloc_log_pages;
        unsigned long   alloc_data_pages;
        unsigned long   freed_log_pages;
        unsigned long   freed_data_pages;

        u64             padding[8];     /* Cache line break */
};
#endif




/*
 * devfs super-block data in memory
 */
struct crfss_sb_info {
        unsigned long s_ninodes;
        unsigned long s_nzones;
        unsigned long s_imap_blocks;
        unsigned long s_zmap_blocks;
        unsigned long s_firstdatazone;
        unsigned long s_log_zone_size;
        unsigned long s_max_size;
        int s_dirsize;
        int s_namelen;
        struct buffer_head ** s_imap;
        struct buffer_head ** s_zmap;
        struct buffer_head * s_sbh;
        struct crfss_super_block * s_ms;
        unsigned short s_mount_state;
        unsigned short s_version;


//#ifdef _DEVFS_DEVFS_BASED
	/* Host dentry address */
	void *virt_addr;
	phys_addr_t phys_addr;
	phys_addr_t d_physaddr;

	/*Enable support for VM*/
	uint8_t vmmode;

	/*Enable dentry offloading*/	
	void *d_host_addr;
	unsigned long d_host_off;
	uint8_t dentry_offload;
	unsigned long dentrysize;
	/*DevFS host inode slab */
        struct slab_chain *d_host_slab;

	/*Enable inode offloading*/
	void *i_host_addr;
	uint8_t inode_offload;
	phys_addr_t inodeoff_addr;
	unsigned long i_host_off;
	/*DevFS host inode slab */
       struct slab_chain *i_host_slab;

	/* Journal related info */
	//TODO:


	unsigned long initsize;
	/*inode offload size*/
	unsigned long inodeoffsz;


	unsigned long   num_blocks;
	unsigned long   blocksize;
        /* inode tracking */
        unsigned long   s_inodes_used_count;
        unsigned long   reserved_blocks;

        struct mutex    s_lock; /* protects the SB's buffer-head */

        int cpus;

	/* ZEROED page for cache page initialized */
        void *zeroed_page;

        /* Per-CPU inode map */
        struct inode_map        *inode_maps;

        /* Decide new inode map id */
        unsigned long map_id;

        /* Per-CPU free block list */
        struct free_list *free_lists;

        /* Shared free block list */
        unsigned long per_list_blocks;
        struct free_list shared_free_list;
//#endif

	
	//PMFS related metadata
	/* temp bitmap space */                  
	unsigned long num_blocknode_allocated;   
	struct list_head block_inuse_head;    
	unsigned long   block_start;          
	unsigned long   block_end;            
	unsigned long   num_free_blocks;      
};


/*
 * The first block contains super blocks and reserved inodes;
 * The second block contains pointers to journal pages.
 * The third block contains pointers to inode tables.
 */
#define RESERVED_BLOCKS 3


struct inode_table {
        __le64 log_head;
};

struct inode_map {
        struct mutex inode_table_mutex;
        struct rb_root  inode_inuse_tree;
        unsigned long   num_range_node_inode;
        struct crfss_range_node *first_inode_range;
        int allocated;
        int freed;
};


static inline struct crfss_sb_info *crfss_sb(struct super_block *sb)
{
        return sb->s_fs_info;
}

static inline unsigned crfss_blocks_needed(unsigned bits, unsigned blocksize)
{
        return DIV_ROUND_UP(bits, blocksize * 8);
}


struct crfss_inotree {
        struct mutex            lock;
        struct rb_root          inode_list;
        bool                    initalize;
};

/*
 * Devfs inode structure similar to Ext2 inode data in memory
 */
struct crfss_inode {
	/* Dummy field to match with pmfs_inode */
	__u32   i_dir_start_lookup;
	struct list_head i_truncated;

#if defined(_DEVFS_INODE_OFFLOAD)
        struct inode *vfs_inode;
#else
        struct inode vfs_inode;
#endif
	/*Pointer to data blocks */
        __le32  i_data[15];
	/* ACL of the file */
        __u32   i_file_acl;
	/* unlinked but open inodes */
	struct list_head i_orphan;      

        __u32   i_dtime;
	__u32   i_ctime;
	__u32   i_mtime;
	__u64   i_size;
	__u32	i_nlink;

        rwlock_t i_meta_lock;

	/* devfs inode info red-black tree*/
        struct rb_node rbnode;

	/* per crfss_inode journal */
	__u8 isjourn;
	void *journal;
	u64 jsize;
	void       *journal_base_addr;
	struct mutex journal_mutex;
	uint32_t    next_transaction_id;

	/* per crfss_inode active journal */
	void *trans;

	struct kmem_cache *trans_cachep;
	__u8 cachep_init;

	 /* radix tree of all pages */
	struct radix_tree_root  page_tree;      

	/* devfs inode number */
	unsigned long i_ino;	

	/* disk physical address */
	unsigned long pi_addr;

	struct radix_tree_root dentry_tree;
	__u8 dentry_tree_init;
	unsigned long log_pages;

	struct dentry *dentry;

#ifdef _DEVFS_SCALABILITY
	/* per file pointer queue */
	unsigned int rd_nr;
	struct crfss_fstruct *per_rd_queue[MAX_FP_QSIZE];

	/* radix tree of pages in submission queue */
	int sq_tree_init;
	struct radix_tree_root sq_tree;
	struct rb_root sq_it_tree;
	spinlock_t sq_tree_lock;
#endif
};



//DevFS Journal declarations. TODO: Seperate header files
typedef struct crfss_journal {
        __le64     base;
        __le32     size;
        __le32     head;
        /* the next three fields must be in the same order and together.
         * tail and gen_id must fall in the same 8-byte quadword */
        __le32     tail;
        __le16     gen_id;   /* generation id of the log */
        __le16     pad;
        __le16     redo_logging;
}  crfss_journal_t;


/* persistent data structure to describe a single log-entry */
/* every log entry is max CACHELINE_SIZE bytes in size */
typedef struct {
        __le64   addr_offset;
        __le32   transaction_id;
        __le16   gen_id;
        u8       type;  /* normal, commit, or abort */
        u8       size;
        char     data[48];
} crfss_logentry_t;

/* volatile data structure to describe a transaction */
typedef struct crfss_transaction {
        u32              transaction_id;
        u16              num_entries;
        u16              num_used;
        u16              gen_id;
        u16              status;
        crfss_journal_t  *t_journal;
        crfss_logentry_t *start_addr;
        struct crfss_transaction *parent;
} crfss_transaction_t;

#define DEVFS_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct crfss_inode)))

#ifdef _DEVFS_SCALABILITY
typedef struct rd_handle {
	struct crfss_fstruct *rd;
	nvme_cmdrw_t *cmdrw;
} rd_handle;
#endif

typedef struct cred_node {
	u8 cred_id[CRED_ID_BYTES];
	struct hlist_node hash_node;
} cred_node_t;

/*
 * Function prototypes
 */
/*
 * Ok, these declarations are also in <linux/kernel.h> but none of the
 * DevFS source programs needs to include it so they are duplicated here.
 */
#if !defined(_DEVFS_INODE_OFFLOAD)
static inline struct crfss_inode *DEVFS_I(struct inode *inode)
{
        return container_of(inode, struct crfss_inode, vfs_inode);
}
#else
static inline struct crfss_inode *DEVFS_I(struct inode *inode)
{
        return container_of(inode, struct crfss_inode, vfs_inode);
}
#endif


#if 1 //def _DEVFS_DEVFS_BASED

extern unsigned int crfss_blk_type_to_shift[DEVFS_BLOCK_TYPE_MAX];
extern unsigned int crfss_blk_type_to_size[DEVFS_BLOCK_TYPE_MAX];

/* assumes the length to be 4-byte aligned */
static inline void crfss_memset_nt(void *dest, uint32_t dword, size_t length)
{
        uint64_t dummy1, dummy2;
        uint64_t qword = ((uint64_t)dword << 32) | dword;

        asm volatile ("movl %%edx,%%ecx\n"
                "andl $63,%%edx\n"
                "shrl $6,%%ecx\n"
                "jz 9f\n"
                "1:      movnti %%rax,(%%rdi)\n"
                "2:      movnti %%rax,1*8(%%rdi)\n"
                "3:      movnti %%rax,2*8(%%rdi)\n"
                "4:      movnti %%rax,3*8(%%rdi)\n"
                "5:      movnti %%rax,4*8(%%rdi)\n"
                "8:      movnti %%rax,5*8(%%rdi)\n"
                "7:      movnti %%rax,6*8(%%rdi)\n"
                "8:      movnti %%rax,7*8(%%rdi)\n"
                "leaq 64(%%rdi),%%rdi\n"
                "decl %%ecx\n"
                "jnz 1b\n"
                "9:     movl %%edx,%%ecx\n"
                "andl $7,%%edx\n"
                "shrl $3,%%ecx\n"
                "jz 11f\n"
                "10:     movnti %%rax,(%%rdi)\n"
                "leaq 8(%%rdi),%%rdi\n"
                "decl %%ecx\n"
                "jnz 10b\n"
                "11:     movl %%edx,%%ecx\n"
                "shrl $2,%%ecx\n"
                "jz 12f\n"
                "movnti %%eax,(%%rdi)\n"
                "12:\n"
                : "=D"(dummy1), "=d" (dummy2) : "D" (dest), "a" (qword), "d" (length) : "memory", "rcx");
}



static inline struct crfss_sb_info *DEVFS_SB(struct super_block *sb)
{
        return sb->s_fs_info;
}



/* If this is part of a read-modify-write of the super block,
 * crfss_memunlock_super() before calling! */
static inline struct crfss_super_block *crfss_get_super(struct super_block *sb)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);

        return (struct crfss_super_block *)sbi->virt_addr;
}


static inline u64
devfss_get_addr_off(struct crfss_sb_info *sbi, void *addr)
{
       	DEVFS_ASSERT((addr >= sbi->virt_addr) &&
                        (addr < (sbi->virt_addr + sbi->initsize)));
        return (u64)(addr - sbi->virt_addr);
}

static inline u64
crfss_get_block_off(struct super_block *sb, unsigned long blocknr,
                    unsigned short btype)
{
        return (u64)blocknr << PAGE_SHIFT;
}

static inline
struct free_list *crfss_get_free_list(struct super_block *sb, int cpu)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);

        if (cpu < sbi->cpus)
                return &sbi->free_lists[cpu];
        else
                return &sbi->shared_free_list;
}

// BKDR String Hash Function
static inline unsigned long BKDRHash(const char *str, int length)
{
        unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
        unsigned long hash = 0;
        int i;

        for (i = 0; i < length; i++) {
                hash = hash * seed + (*str++);
        }

        return hash;
}

#if 0
static inline
struct inode_table *crfss_get_inode_table(struct super_block *sb, int cpu)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);

        if (cpu >= sbi->cpus)
                return NULL;

        return (struct inode_table *)((char *)crfss_get_block(sb,
                DEVFS_DEF_BLOCK_SIZE_4K * 2) + cpu * CACHELINE_SIZE);
}
#endif


static inline unsigned int crfss_inode_blk_shift (struct devfss_inode *pi)
{
        return crfss_blk_type_to_shift[pi->i_blk_type];
}

static inline uint32_t crfss_inode_blk_size (struct devfss_inode *pi)
{
        return crfss_blk_type_to_size[pi->i_blk_type];
}

/*
 * ROOT_INO: Start from DEVFS_SB_SIZE * 2
 */
static inline struct devfss_inode *crfss_get_basic_inode(struct super_block *sb,
        u64 inode_number)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);

        return (struct devfss_inode *)(sbi->virt_addr + DEVFS_SB_SIZE * 2 +
                         (inode_number - DEVFS_ROOT_INO) * DEVFS_INODE_SIZE);
}

/* If this is part of a read-modify-write of the inode metadata,
 * crfss_memunlock_inode() before calling! */
static inline struct devfss_inode *crfss_get_inode_by_ino(struct super_block *sb,
                                                  u64 ino)
{
        if (ino == 0 || ino >= DEVFS_NORMAL_INODE_START)
                return NULL;

        return crfss_get_basic_inode(sb, ino);
}

static inline struct devfss_inode *devfss_get_inode(struct super_block *sb,
        struct inode *inode)
{
        //struct crfss_inode *si = DEVFS_I(inode);
        //struct devfss_inode_info_header *sih = &si->header;
        //return (struct devfss_inode *)crfss_get_block(sb, si->pi_addr);
        return (struct devfss_inode *)NULL;
}


static inline unsigned long
crfss_get_blocknr(struct super_block *sb, u64 block, unsigned short btype)
{
        return block >> PAGE_SHIFT;
}


static inline unsigned long
crfss_get_numblocks(unsigned short btype)
{
        unsigned long num_blocks;

        if (btype == DEVFS_BLOCK_TYPE_4K) {
                num_blocks = 1;
        } else if (btype == DEVFS_BLOCK_TYPE_2M) {
                num_blocks = 512;
        } else {
                //btype == DEVFS_BLOCK_TYPE_1G
                num_blocks = 0x40000;
        }
        return num_blocks;
}



static inline unsigned long crfss_get_pfn(struct super_block *sb, u64 block)
{
	return (DEVFS_SB(sb)->phys_addr + block) >> PAGE_SHIFT;
}


/*static inline int nova_is_mounting(struct super_block *sb)
{
	struct crfss_sb_info *sbi = (struct crfss_sb_info *)sb->s_fs_info;
	return sbi->s_mount_opt & NOVA_MOUNT_MOUNTING;
}

static inline void check_eof_blocks(struct super_block *sb,
		struct devfss_inode *pi, loff_t size)
{
	if ((pi->i_flags & cpu_to_le32(NOVA_EOFBLOCKS_FL)) &&
		(size + sb->s_blocksize) > (le64_to_cpu(pi->i_blocks)
			<< sb->s_blocksize_bits))
		pi->i_flags &= cpu_to_le32(~NOVA_EOFBLOCKS_FL);
}*/

enum crfss_new_inode_type {
	TYPE_CREATE = 0,
	TYPE_MKNOD,
	TYPE_SYMLINK,
	TYPE_MKDIR
};


#if 0
static inline u64 next_log_page(struct super_block *sb, u64 curr_p)
{


	void *curr_addr = crfss_get_block(sb, curr_p);
	unsigned long page_tail = ((unsigned long)curr_addr & ~INVALID_MASK)
					+ LAST_ENTRY;
	return ((struct crfss_inode_page_tail *)page_tail)->next_page;
}
#endif


static inline void crfss_set_next_page_address(struct super_block *sb,
	struct crfss_inode_log_page *curr_page, u64 next_page, int fence)
{
	curr_page->page_tail.next_page = next_page;
	//nova_flush_buffer(&curr_page->page_tail,
        //	sizeof(struct crfss_inode_page_tail), 0);
	if (fence)
		DEVFS_PERSISTENT_BARRIER();
}

#define	CACHE_ALIGN(p)	((p) & ~(CACHELINE_SIZE - 1))

static inline bool is_last_entry(u64 curr_p, size_t size)
{
	unsigned int entry_end;

	entry_end = ENTRY_LOC(curr_p) + size;

	return entry_end > LAST_ENTRY;
}


#if 0
static inline bool goto_next_page(struct super_block *sb, u64 curr_p)
{
	void *addr;
	u8 type;

	/* Each kind of entry takes at least 32 bytes */
	if (ENTRY_LOC(curr_p) + 32 > LAST_ENTRY)
		return true;

	addr = crfss_get_block(sb, curr_p);
	type = crfss_get_entry_type(addr);
	if (type == NEXT_PAGE)
		return true;

	return false;
}
#endif

static inline int is_dir_init_entry(struct super_block *sb,
	struct crfss_dentry *entry)
{
	if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0)
		return 1;
	if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0)
		return 1;

	return 0;
}
#endif

/*devfs.c*/
#ifdef CRFS_MULTI_PROC
extern int g_crfss_scheduler_init[HOST_PROCESS_MAX];
#else
extern int g_crfss_scheduler_init;
#endif

int vfio_creatfile_cmd(unsigned long arg, int frm_kernel);
long vfio_submitio_cmd(unsigned long arg);
long vfio_crfss_io_read (struct crfss_fstruct *rd, 
                        nvme_cmdrw_t *cmdrw, u8 append);
int vfio_crfss_io_write (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
int vfio_crfss_io_append (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
int vfio_crfss_io_fsync (struct crfss_fstruct *rd);
int vfio_crfss_io_unlink (struct crfss_fstruct *rd);
int vfio_crfss_io_close (struct crfss_fstruct *rd);
int vfio_crfss_io_rename (struct crfss_fstruct *rd);
nvme_cmdrw_t *rd_dequeue(struct crfss_fstruct *rd, int sz);
nvme_cmdrw_t *rd_queue_readtail(struct crfss_fstruct *rd, int sz);
int rd_enqueue(struct crfss_fstruct *rd, int sz, nvme_cmdrw_t *cmd);
int crfss_free_file_queue(struct crfss_fstruct *rd); 
 
int vfio_close_cmd(unsigned long arg);
long crfss_direct_write(struct crfss_fstruct *rd, 
			nvme_cmdrw_t *cmdrw, u8 isappend);

int rd_write(struct crfss_fstruct *rd, void *buf,
                int sz, int fd, int append);

inline int vfio_process_read(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
inline int vfio_process_write(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
inline int vfio_process_append(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
inline int vfio_process_fsync(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw, 
			struct inode *inode);
inline int vfio_process_close(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);
inline int vfio_process_unlink(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw);

/*crfss_kernel.c*/
int crfss_rwtest(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw,
                size_t sz, int fd, int append);

int crfss_close(struct inode *inode, struct file *fp);



/*super.c*/
inline void crfss_set_default_opts(struct crfss_sb_info *sbi);

struct devfss_inode *devfss_init(struct super_block *sb,
                                      unsigned long size);

inline void crfss_free_inode_node(struct super_block *sb,
        struct crfss_range_node *node);
inline void crfss_free_blocknode(struct super_block *sb,
        struct crfss_range_node *node);
inline void crfss_free_range_node(struct crfss_range_node *node);
inline struct crfss_range_node 
	*crfss_alloc_blocknode(struct super_block *sb);
inline struct crfss_range_node 
	*crfss_alloc_inode_node(struct super_block *sb);

/* inode.c*/
int init_crfss_inodecache(void);
void destroy_crfss_inodecache(void);
void crfss_destroy_inode(struct inode *inode);
void crfss_evict_inode(struct inode *inode);

unsigned int crfss_str_hash_linux(const char *str, unsigned int length);
unsigned int crfss_str_hash(const char *s, unsigned int len);
struct crfss_inotree *crfss_inode_list_create(void);
int insert_inode_rbtree(struct rb_root *root, struct crfss_inode *ei);
void del_inode_rbtree(struct crfss_inotree *inotree, struct crfss_inode *ei);
struct crfss_fstruct *fd_to_queuebuf(int fd);
struct file *crfss_create_file(char *filename, int flags, umode_t mode, int *fd);
int crfss_write_begin(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata);
int crfss_write_end(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned copied,
                        struct page *page, void *fsdata);
ssize_t crfss_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                unsigned long nr_segs, loff_t pos);

ssize_t crfss_kernel_write(struct file *file, const char *buf,
                size_t count, loff_t *pos, u64 nvaddr);

ssize_t crfss_read(struct file *file, char __user *buf,
        size_t count, loff_t *pos);

long crfss_ioctl(void *iommu_data, unsigned int cmd, 
	unsigned long arg);

int do_crfss_open(struct inode *inode, struct file *file);
long do_crfss_unlink(const char __user *pathname);

int crfss_init_inode_inuse_list(struct super_block *sb);
u64 devfss_new_crfss_inode(struct super_block *sb, u64 *pi_addr);
int crfss_init_inode_table(struct super_block *sb);
unsigned int release_cache_pages(struct crfss_inode *ei, struct inode *inode);

u64 crfss_get_append_head(struct super_block *sb, struct devfss_inode *pi,
        struct crfss_inode *sih, u64 tail, size_t size, int *extended);

int crfss_allocate_inode_log_pages(struct super_block *sb,
        struct devfss_inode *pi, unsigned long num_pages,
        u64 *new_block);

struct inode *inode_offld_to_host(struct super_block *sb,
                      struct inode *inode, struct dentry *dentry);

struct inode *inode_ld_from_host(struct super_block *sb,
                      struct inode *inode);

ssize_t crfss_sync_read(struct file *filp, char __user *buf, 
		size_t len, loff_t *ppos);

struct inode *crfss_alloc_inode(struct super_block *sb);

/*crfss_super.c*/
phys_addr_t get_phys_addr(void **data);
int crfss_parse_options2(char *options, struct crfss_sb_info *sbi,
                               bool remount);


/*crfss_dir.c */
int crfss_append_dir_init_entries(struct super_block *sb,
        struct devfss_inode *pi, u64 self_ino, u64 parent_ino);

int crfss_add_dentry(struct dentry *dentry, u64 ino, int inc_link,
        u64 tail, u64 *new_tail);

int crfss_remove_dentry(struct dentry *dentry, int dec_link, u64 tail,
        u64 *new_tail);


/* balloc.c */
int crfss_alloc_block_free_lists(struct super_block *sb);
void crfss_delete_free_lists(struct super_block *sb);
inline struct crfss_range_node *crfss_alloc_blocknode(struct super_block *sb);
inline struct crfss_range_node *crfss_alloc_inode_node(struct super_block *sb);
inline void crfss_free_range_node(struct crfss_range_node *node);
inline void crfss_free_blocknode(struct super_block *sb,
        struct crfss_range_node *bnode);
inline void crfss_free_inode_node(struct super_block *sb,
        struct crfss_range_node *bnode);
extern int crfss_init_blockmap(struct super_block *sb, int recovery);

inline int crfss_insert_blocktree(struct crfss_sb_info *sbi,
        struct rb_root *tree, struct crfss_range_node *new_node);

inline int crfss_insert_inodetree(struct crfss_sb_info *sbi,
        struct crfss_range_node *new_node, int cpu);

inline struct crfss_range_node 
	*crfss_alloc_inode_node(struct super_block *sb);

inline int crfss_new_log_blocks(struct super_block *sb, struct devfss_inode *pi,
        unsigned long *blocknr, unsigned int num, int zero);

inline int crfss_new_log_blocks(struct super_block *sb, struct devfss_inode *pi,
        unsigned long *blocknr, unsigned int num, int zero);

inline int crfss_new_data_blocks(struct super_block *sb, struct devfss_inode *pi,
        unsigned long *blocknr, unsigned int num, unsigned long start_blk,
        int zero, int cow);

inline int crfss_search_inodetree(struct crfss_sb_info *sbi,
        unsigned long ino, struct crfss_range_node **ret_node);

int crfss_free_log_blocks(struct super_block *sb, struct devfss_inode *pi,
        unsigned long blocknr, int num);

int crfss_free_blocks(struct super_block *sb, unsigned long blocknr,
        int num, unsigned short btype, int log_page);


/*crfss_pmfs_balloc.c*/
int __init init_blocknode_cache(void);
struct pmfs_blocknode *pmfs_alloc_blocknode(struct super_block *sb);
void pmfs_init_blockmap(struct super_block *sb, unsigned long init_used_size);
void pmfs_free_block(struct super_block *sb, unsigned long blocknr,
                      unsigned short btype);
int pmfs_new_block(struct super_block *sb, unsigned long *blocknr,
        unsigned short btype, int zero);



/*crfss_mm.c*/
struct page *crfss_alloc_page(gfp_t gfp, int order, int node);
void crfss_free_pages(struct page *page);
void *crfss_ioremap(struct super_block *sb, phys_addr_t phys_addr,
                        ssize_t size, const char* cachetype);


/*DevFS file, link, directory deletion methods*/
int crfss_rmdir(struct inode *dir, struct dentry *dentry);
int crfss_unlink(struct inode *dir, struct dentry *dentry);
int crfss_symlink(struct inode * dir, struct dentry *dentry, 
        const char * symname);
void crfss_destroy_inode(struct inode *inode);


int crfss_mkdir(struct inode * dir, struct dentry * dentry,        
        umode_t mode);
int crfss_mknod(struct inode *dir, struct dentry *dentry,
        umode_t mode, dev_t dev);
int crfss_permission(struct inode *inode, int mask);
int crfss_create(struct inode *dir, struct dentry *dentry, 
        umode_t mode, bool excl);
int crfss_inode_update(struct inode *,  loff_t );
int crfss_readpage(struct file *file, struct page *page);

//Bitmap related functions
struct inode* 
crfss_new_inode(struct super_block *sb, struct inode *inode, 
			umode_t mode, int *error);
int crfss_set_sb_bit(struct super_block *sb);
int crfss_free_inode(struct inode * inode);







/* Journaling methods */
int init_journal( struct inode *inode);
int free_journal( struct inode *inode);
int create_journal(struct crfss_inode *ei);
crfss_journal_t *crfss_set_journal(struct crfss_inode *ei);
void crfss_free_journal(struct crfss_inode *ei);
int crfss_recover_undo_journal(struct crfss_inode *ei);


static inline void crfss_memunlock_range(struct crfss_inode *ei, void *p,
                                         unsigned long len)
{
#if defined(AFTER_DEADLINE)
        if (pmfs_is_protected(sb))
                __pmfs_memunlock_range(p, len, 0);
        else if (pmfs_is_protected_old(sb))
                __pmfs_memunlock_range(p, len, 1);
#endif
}

static inline void crfss_memlock_range(struct crfss_inode *ei, void *p,
                                       unsigned long len)
{
#if defined(AFTER_DEADLINE)
        if (pmfs_is_protected(sb))
                __pmfs_memlock_range(p, len, 0);
        else if (pmfs_is_protected_old(sb))
                __pmfs_memlock_range(p, len, 1);
#endif
}




unsigned long
crfss_ondemand_readahead(struct address_space *mapping,
                   struct file_ra_state *ra, struct file *filp,
                   bool hit_readahead_marker, pgoff_t offset,
                   unsigned long req_size, struct crfss_inode *ei);

struct page *crfss_find_get_page(struct address_space *mapping, 
	pgoff_t offset, struct crfss_inode *ei);

int crfss_set_node_page_dirty(struct page *page);

void
crfss_page_cache_async_readahead(struct address_space *mapping,
                           struct file_ra_state *ra, struct file *filp,
                           struct page *page, pgoff_t offset,
                           unsigned long req_size);

struct page *crfss_get_cache_page(struct address_space *mapping,
        pgoff_t index, unsigned flags, struct crfss_inode *ei, 
	struct inode *inode);

int init_crfss_transaction_cache(struct crfss_inode *ei);
int destroy_crfss_transaction_cache(struct crfss_inode *ei);

inline crfss_transaction_t *alloc_transaction
                                (struct crfss_inode *ei);
inline void free_transaction(struct crfss_inode *ei,
                        crfss_transaction_t *trans);

uint64_t crfss_get_journal_base(struct crfss_inode *ei);
int crfss_add_logentry(struct crfss_inode *ei,
        crfss_transaction_t *trans, void *addr, uint16_t size, u8 type);
crfss_transaction_t *crfss_new_transaction(struct crfss_inode *ei,
        int max_log_entries);
crfss_transaction_t *crfss_new_ino_trans (struct crfss_inode *ei);
int crfss_commit_transaction(struct crfss_inode *ei,
	crfss_transaction_t *trans);
crfss_transaction_t *crfss_new_ino_trans_log(struct crfss_inode *ei, 
	struct inode *inode);
int crfss_recover_journal(struct crfss_inode *ei);


/*Security*/
int crfss_set_cred(struct crfss_fstruct *fs);
const struct cred *crfss_get_cred(struct crfss_fstruct *fs);
int crfss_check_fs_cred(struct crfss_fstruct *fs);
void crfss_init_cred_table(void);
int crfss_add_cred_table(u8 *cred_id);
int crfss_del_cred_table(u8 *cred_id);
int crfss_check_cred_table(u8 *cred_id);

/*Kernel-level DevFS testing*/
int crfss_rwtest(struct crfss_fstruct *rd, 
	nvme_cmdrw_t *cmdrw, size_t sz, int fd, int append);

/* crfss_slab.c */
#if defined(_DEVFS_SLAB_ALLOC)
/*Initialize DevFS cache object*/
struct slab_chain *crfss_slab_init(struct crfss_sb_info *sbi, ssize_t size);
void slab_init(struct slab_chain *, ssize_t);
void slab_free(struct slab_chain *, const void *);
void slab_traverse(const struct slab_chain *, void (*)(const void *));
void slab_destroy(const struct slab_chain *);
void *crfss_slab_alloc(struct crfss_sb_info *sbi, struct slab_chain *sch,
                void **start_addr, unsigned long *off);
struct slab_chain *crfss_slab_free(struct crfss_sb_info *sbi, struct slab_chain *s);
#endif

/* crfss_scalability */
void crfss_scalability_flush_buffer(struct crfss_fstruct *rd);
int crfss_scalability_open_setup(struct crfss_inode *ei, struct crfss_fstruct *rd);
void crfss_scalability_close_setup(struct crfss_inode *ei, struct crfss_fstruct *rd);

int crfss_read_submission_tree_search (struct inode *inode, nvme_cmdrw_t *cmdrw,
							struct crfss_fstruct **target_rd);
int crfss_write_submission_tree_search (struct inode *inode, nvme_cmdrw_t *cmdrw,
							struct crfss_fstruct **target_rd);
int crfss_write_submission_tree_insert (struct inode *inode, nvme_cmdrw_t *cmdrw,
							struct crfss_fstruct *rd);
int crfss_write_submission_tree_delete (struct inode *inode, nvme_cmdrw_t *cmdrw);

/* crfss_scheduler */
#ifdef CRFS_MULTI_PROC
extern struct dev_thread_struct crfss_device_thread[HOST_PROCESS_MAX][DEVICE_THREAD_MAX];
extern int crfss_device_thread_nr[HOST_PROCESS_MAX];
#else
extern struct dev_thread_struct crfss_device_thread[DEVICE_THREAD_MAX];
extern int crfss_device_thread_nr;
#endif
//extern int crfss_scheduler_policy; 
extern struct list_head *g_rd_list;
extern spinlock_t g_rd_list_lock;
extern struct mutex g_mutex;
extern struct crfss_fstruct control_rd;

int crfss_scheduler_init(int nr, int policy);
void crfss_scheduler_exit(void);
int crfss_io_scheduler(void *data);
int crfss_scheduler_add_list(struct crfss_fstruct *rd);
int crfss_scheduler_del_list(struct crfss_fstruct *rd);

/* crfss_stat */
void crfss_stat_fp_queue_init(void);
void crfss_stat_fp_queue_access(void);
void crfss_stat_fp_queue_hit(void);
void crfss_stat_fp_queue_conflict(void);
void crfss_stat_fp_queue_count(void);
void crfss_stat_write_fin(void);
int crfss_stat_get_write_fin(void);

/* crfss_req_tree */

/* mutex lock wrapper */
static inline void crfss_mutex_init(struct mutex *lock) {
#ifndef _DEVFS_GLOBAL_LOCK
	mutex_init(lock);
#endif
}

static inline void crfss_mutex_lock(struct mutex *lock) {
#ifndef _DEVFS_GLOBAL_LOCK
	mutex_lock(lock);
#endif
}

static inline void crfss_mutex_unlock(struct mutex *lock) {
#ifndef _DEVFS_GLOBAL_LOCK
	mutex_unlock(lock);
#endif
}

static inline void global_mutex_init(struct mutex *lock) {
#ifdef _DEVFS_GLOBAL_LOCK
	mutex_init(lock);
#endif
}

static inline void global_mutex_lock(struct mutex *lock) {
#ifdef _DEVFS_GLOBAL_LOCK
	mutex_lock(lock);
#endif
}

static inline void global_mutex_unlock(struct mutex *lock) {
#ifdef _DEVFS_GLOBAL_LOCK
	mutex_unlock(lock);
#endif
}

static void* get_data_buffer(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	void *ret = NULL;
	ret = kmalloc(cmdrw->nlb, GFP_KERNEL);
	cmdrw->kalloc = 1;

	return ret; 
}

static void put_data_buffer(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	if (cmdrw->kalloc == 1) {
		kfree((void*)cmdrw->blk_addr);
		return;
	}
}

#endif


