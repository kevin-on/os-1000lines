#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[], __kernel_base[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid,
                       long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

int getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2 /* Console Getchar */);
    return ret.error;
}

__attribute__((naked)) __attribute__((aligned(4))) void kernel_entry(void) {
    __asm__ __volatile__("csrrw sp, sscratch, sp\n"

                         "addi sp, sp, -4 * 31\n"
                         "sw ra,  4 * 0(sp)\n"
                         "sw gp,  4 * 1(sp)\n"
                         "sw tp,  4 * 2(sp)\n"
                         "sw t0,  4 * 3(sp)\n"
                         "sw t1,  4 * 4(sp)\n"
                         "sw t2,  4 * 5(sp)\n"
                         "sw t3,  4 * 6(sp)\n"
                         "sw t4,  4 * 7(sp)\n"
                         "sw t5,  4 * 8(sp)\n"
                         "sw t6,  4 * 9(sp)\n"
                         "sw a0,  4 * 10(sp)\n"
                         "sw a1,  4 * 11(sp)\n"
                         "sw a2,  4 * 12(sp)\n"
                         "sw a3,  4 * 13(sp)\n"
                         "sw a4,  4 * 14(sp)\n"
                         "sw a5,  4 * 15(sp)\n"
                         "sw a6,  4 * 16(sp)\n"
                         "sw a7,  4 * 17(sp)\n"
                         "sw s0,  4 * 18(sp)\n"
                         "sw s1,  4 * 19(sp)\n"
                         "sw s2,  4 * 20(sp)\n"
                         "sw s3,  4 * 21(sp)\n"
                         "sw s4,  4 * 22(sp)\n"
                         "sw s5,  4 * 23(sp)\n"
                         "sw s6,  4 * 24(sp)\n"
                         "sw s7,  4 * 25(sp)\n"
                         "sw s8,  4 * 26(sp)\n"
                         "sw s9,  4 * 27(sp)\n"
                         "sw s10, 4 * 28(sp)\n"
                         "sw s11, 4 * 29(sp)\n"

                         "csrr a0, sscratch\n"
                         "sw a0, 4 * 30(sp)\n"

                         "addi a0, sp, 4 * 31\n"
                         "csrw sscratch, a0\n"

                         "mv a0, sp\n"
                         "call handle_trap\n"

                         "lw ra,  4 * 0(sp)\n"
                         "lw gp,  4 * 1(sp)\n"
                         "lw tp,  4 * 2(sp)\n"
                         "lw t0,  4 * 3(sp)\n"
                         "lw t1,  4 * 4(sp)\n"
                         "lw t2,  4 * 5(sp)\n"
                         "lw t3,  4 * 6(sp)\n"
                         "lw t4,  4 * 7(sp)\n"
                         "lw t5,  4 * 8(sp)\n"
                         "lw t6,  4 * 9(sp)\n"
                         "lw a0,  4 * 10(sp)\n"
                         "lw a1,  4 * 11(sp)\n"
                         "lw a2,  4 * 12(sp)\n"
                         "lw a3,  4 * 13(sp)\n"
                         "lw a4,  4 * 14(sp)\n"
                         "lw a5,  4 * 15(sp)\n"
                         "lw a6,  4 * 16(sp)\n"
                         "lw a7,  4 * 17(sp)\n"
                         "lw s0,  4 * 18(sp)\n"
                         "lw s1,  4 * 19(sp)\n"
                         "lw s2,  4 * 20(sp)\n"
                         "lw s3,  4 * 21(sp)\n"
                         "lw s4,  4 * 22(sp)\n"
                         "lw s5,  4 * 23(sp)\n"
                         "lw s6,  4 * 24(sp)\n"
                         "lw s7,  4 * 25(sp)\n"
                         "lw s8,  4 * 26(sp)\n"
                         "lw s9,  4 * 27(sp)\n"
                         "lw s10, 4 * 28(sp)\n"
                         "lw s11, 4 * 29(sp)\n"
                         "lw sp,  4 * 30(sp)\n"

                         "sret\n");
}

/**
 * Allocate 'n' pages of physical memory.
 * TODO: Implement deallocation.
 */
paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr = (paddr_t)__free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t)__free_ram_end)
        PANIC("out of memory");

    memset((void *)paddr, 0, n * PAGE_SIZE);
    return paddr;
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the 1st level page table if it doesn't exist.
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n" // *prev_sp = sp;
        "lw sp, (a1)\n" // Switch stack pointer (sp) here

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n" // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n" // We've popped 13 4-byte registers from the stack
        "ret\n");
}

struct process procs[PROCS_MAX]; // All process control structures.

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__("csrw sepc, %[sepc]\n"
                         "csrw sstatus, %[sstatus]\n"
                         "sret\n"
                         :
                         : [sepc] "r"(USER_BASE), [sstatus] "r"(SSTATUS_SPIE | SSTATUS_SUM));
}

struct process *create_process(const void *image, size_t image_size) {
    // Find an unused process control structure.
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    uint32_t *sp = (uint32_t *)&proc->stack[sizeof(proc->stack)];
    *--sp = 0;                    // s11
    *--sp = 0;                    // s10
    *--sp = 0;                    // s9
    *--sp = 0;                    // s8
    *--sp = 0;                    // s7
    *--sp = 0;                    // s6
    *--sp = 0;                    // s5
    *--sp = 0;                    // s4
    *--sp = 0;                    // s3
    *--sp = 0;                    // s2
    *--sp = 0;                    // s1
    *--sp = 0;                    // s0
    *--sp = (uint32_t)user_entry; // ra

    // Initialize page table
    uint32_t *page_table = (uint32_t *)alloc_pages(1);

    // Map kernel pages
    for (paddr_t paddr = (paddr_t)__kernel_base; paddr < (paddr_t)__free_ram_end;
         paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // Map user pages
    for (uint32_t offset = 0; offset < image_size; offset += PAGE_SIZE) {
        paddr_t paddr = alloc_pages(1);

        size_t remaining_size = image_size - offset;
        size_t copy_size = remaining_size > PAGE_SIZE ? PAGE_SIZE : remaining_size;

        memcpy((void *)paddr, (void *)((uint32_t)image + offset), copy_size);
        map_page(page_table, USER_BASE + offset, paddr, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Map virtio-blk MMIO region
    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t)sp;
    proc->page_table = page_table;
    return proc;
}

struct process *current_proc;
struct process *idle_proc;

void yield(void) {
    // Search for a runnable process
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    // If current process is the only runnable process, return and continue processing
    if (next == current_proc)
        return;

    __asm__ __volatile__("sfence.vma\n"
                         "csrw satp, %[satp]\n"
                         "sfence.vma\n"
                         "csrw sscratch, %[sscratch]\n"
                         :
                         : [satp] "r"(SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE)),
                           [sscratch] "r"((uint32_t)&next->stack[sizeof(next->stack)]));

    // Switch context and update current process
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

void idle_entry(void) {
    for (;;) {
        yield();
        __asm__ __volatile__("wfi");
    }
}

/* Virtio */
uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *)(VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
uint64_t blk_capacity;

struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *)virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *)&vq->used.index;
    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 5. Notify the device about the queue size by writing the size to QueueNum.
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 6. Notify the device about the used alignment by writing its value in bytes to QueueAlign.
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    // 7. Write the physical number of the first page of the queue to the QueuePFN register.
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device id");

    // 1. Reset the device.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set the DRIVER status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set the FEATURES_OK status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    blk_request_vq = virtq_init(0);
    // 8. Set the DRIVER_OK status bit.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Get the disk capacity.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", (int)blk_capacity);

    // Allocate a region to store requests to the device.
    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *)blk_req_paddr;
}

// Notifies the device that there is a new request. `desc_index` is the index
// of the head descriptor of the new request.
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// Returns whether there are requests being processed by the device.
bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

// Reads/writes from/to virtio-blk device.
void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n", sector,
               blk_capacity / SECTOR_SIZE);
        return;
    }

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Construct the virtqueue descriptors (using 3 descriptors).
    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = offsetof(struct virtio_blk_req, data);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // Notify the device that there is a new request.
    virtq_kick(vq, 0);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n", sector, blk_req->status);
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

/**
 * File system
 */
struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void int2oct(int dec, char *oct, int len) {
    for (int i = len - 1; i >= 0; i--) {
        oct[i] = '0' + dec % 8;
        dec /= 8;
    }
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header *header = (struct tar_header *)&disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            PANIC("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        printf("file: %s, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *)&disk[off];
        memset(header, 0, sizeof(struct tar_header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        int2oct(file->size, header->size, sizeof(header->size));

        int checksum = 0;
        for (int i = 0; i < sizeof(struct tar_header); i++)
            checksum += disk[off + i];
        int2oct(checksum, header->checksum, sizeof(header->checksum));

        memcpy(header->data, file->data, file->size);

        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    printf("wrote %d bytes to disk\n", sizeof(disk));
}

struct file *fs_lookup(const char *path) {
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (strcmp(file->name, path) == 0)
            return file;
    }
    return NULL;
}

/**
 * Trap handler
 */
void handle_syscall(struct trap_frame *f) {
    uint32_t sysno = f->a3;
    uint32_t arg0 = f->a0;
    uint32_t arg1 = f->a1;
    uint32_t arg2 = f->a2;

    switch (sysno) {
    case SYS_PUTCHAR:
        putchar(arg0);
        break;
    case SYS_GETCHAR:
        while (1) {
            long ch = getchar();
            if (ch >= 0) {
                f->a0 = ch;
                break;
            }
            yield();
        }
        break;
    case SYS_EXIT:
        printf("process %d exited\n", current_proc->pid);
        current_proc->state = PROC_EXITED;
        // TODO: free process resources
        yield();
        PANIC("unreachable");
    case SYS_READFILE:
    case SYS_WRITEFILE: {
        const char *path = (const char *)arg0;
        char *buf = (char *)arg1;
        int size = arg2;
        struct file *file = fs_lookup(path);
        if (file == NULL) {
            printf("file not found: %s\n", path);
            f->a0 = -1;
            break;
        }

        if (sysno == SYS_READFILE) {
            memcpy(buf, file->data, file->size);
            f->a0 = file->size;
        } else {
            if (size > (int)sizeof(file->data)) {
                size = sizeof(file->data);
            }
            memcpy(file->data, buf, size);
            file->size = size;
            fs_flush();
            f->a0 = size;
        }

        break;
    }
    default:
        PANIC("unexpected syscall sysno=%x\n", sysno);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

/* Test functions */
void test_printf(void) {
    // Test putchar
    const char *s = "\n\nHello World!\n";
    for (int i = 0; s[i] != '\0'; i++) {
        putchar(s[i]);
    }

    // Test printf
    printf("\n\nHello %s\n", "World!");
    printf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);
}

void test_common_functions(void) {
    // Test strcpy
    char dst[10];
    strcpy(dst, "Hello");
    printf("strcpy(dst, \"Hello\") = %s\n", dst);

    // Test strcmp
    printf("strcmp(\"Hello\", \"World\") = %d\n", strcmp("Hello", "World"));
    printf("strcmp(\"Hello\", \"%s\") = %d\n", dst, strcmp("Hello", dst));

    // Test memset
    memset(dst, 'A', 5);
    printf("memset(dst, 'A', 10) = %s\n", dst);

    // Test memcpy
    memcpy(dst, "World", 5);
    printf("memcpy(dst, \"World\", 5) = %s\n", dst);
}

void test_exceptions(void) {
    // Test illegal instruction exception
    __asm__ __volatile__("unimp");
}

void test_alloc_pages(void) {
    paddr_t paddr0 = alloc_pages(2);
    paddr_t paddr1 = alloc_pages(1);
    printf("alloc_pages test: paddr0=%x\n", paddr0);
    printf("alloc_pages test: paddr1=%x\n", paddr1);
}

/* Test context switching */
void delay(void) {
    for (int i = 0; i < 30000000; i++)
        __asm__ __volatile__("nop"); // do nothing
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) {
        uint32_t current_sp;
        __asm__ __volatile__("mv %0, sp" : "=r"(current_sp));
        if (current_sp < (uint32_t)proc_a->stack ||
            current_sp > (uint32_t)proc_a->stack + sizeof(proc_a->stack)) {
            // Temporarily disable panic
            // PANIC("proc_a: current sp is not inside the process's stack");
        }

        putchar('A');
        delay();
        yield();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        uint32_t current_sp;
        __asm__ __volatile__("mv %0, sp" : "=r"(current_sp));
        if (current_sp < (uint32_t)proc_b->stack ||
            current_sp > (uint32_t)proc_b->stack + sizeof(proc_b->stack)) {
            PANIC("proc_b: current sp is not inside the process's stack");
        }

        putchar('B');
        delay();
        yield();
    }
}

void test_context_switching(void) {
    // This function is outdated as create_process is updated to take image and image size as
    // arguments proc_a = create_process((uint32_t) proc_a_entry); proc_b =
    // create_process((uint32_t) proc_b_entry); proc_a_entry(); yield(); PANIC("switched to idle
    // process");
}

void test_user_process(void) {
    create_process(_binary_shell_bin_start, (size_t)_binary_shell_bin_size);
    yield();
}

void test_disk_io(void) {
    char buf[SECTOR_SIZE];

    // Read logical sector 0
    read_write_disk(buf, 0, false);
    printf("Sector 0: %s\n", buf);

    // Write to logical sector 0
    strcpy(buf, "hello from kernel!!!\n");
    read_write_disk(buf, 0, true);
}

/**
 * Kernel main function.
 */
void kernel_main(void) {
    // Initialize BSS
    memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

    // Set the trap vector
    WRITE_CSR(stvec, (uint32_t)kernel_entry);

    // Initialize virtio-blk
    virtio_blk_init();

    // Initialize file system
    fs_init();

    // Initialize idle process
    idle_proc = create_process(idle_entry, 0);
    idle_proc->pid = 0;
    current_proc = idle_proc;

    /* Test functions */
    // test_printf();
    // test_common_functions();
    // test_exceptions();
    // test_alloc_pages();
    // test_context_switching();
    test_user_process();
    // test_disk_io();

    PANIC("kernel_main ended");
}

__attribute__((section(".text.boot"))) __attribute__((naked)) void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // Set the stack pointer
        "j kernel_main\n"       // Jump to the kernel main function
        :
        : [stack_top] "r"(__stack_top) // Pass the stack top address as %[stack_top]
    );
}
