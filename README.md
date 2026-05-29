# FOS (Fayed Operating System) — OS'25 Project

A semester-long, multi-module operating system project built on top of the FOS kernel. The project is organized into prerequisite group modules and individual modules, each targeting a core OS subsystem.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Project Architecture](#project-architecture)
- [Group Modules (Part II)](#group-modules-part-ii)
  - [Dynamic Allocator](#dynamic-allocator)
  - [Kernel Heap](#kernel-heap)
  - [Fault Handler I — Placement](#fault-handler-i--placement)
- [Individual Modules (Part III)](#individual-modules-part-iii)
  - [Module 1 & 6 — Fault Handler II (Replacement)](#module-1--6--fault-handler-ii-replacement)
  - [Module 2 — User Heap](#module-2--user-heap)
  - [Module 3 — Shared Memory](#module-3--shared-memory)
  - [Module 4 — CPU Scheduling](#module-4--cpu-scheduling)
  - [Module 5 — Kernel Protection](#module-5--kernel-protection)
- [My Contribution](#my-contribution)
- [Key Files Reference](#key-files-reference)
- [Build & Run](#build--run)

---

## Project Overview

FOS is a teaching operating system that exposes students to the full stack of OS internals. The project is built incrementally: a shared group foundation (Dynamic Allocator, Kernel Heap, Fault Handler I) is implemented first, and then each team member takes ownership of one or more individual modules.

---

## Project Architecture

```
[Group] Prerequisite Modules
  └── Dynamic Allocator
  └── Kernel Heap
  └── Fault Handler I (Placement)
          │
          ▼
[Individual] M#1 / M#6 — Fault Handler II (Replacement: CLK / LRU / Mod-CLK)
[Individual] M#2        — User Heap
[Individual] M#3        — Shared Memory
[Individual] M#4        — CPU Scheduling (Priority Round-Robin)
[Individual] M#5        — Kernel Protection (SleepLocks & Semaphores)
          │
          ▼
[Group] Integration & Overall Testing / Bonuses
```

---

## Group Modules (Part II)

### Dynamic Allocator

**Goal:** Dynamically allocate and free small-size memory blocks in a fast and memory-efficient manner, usable by both the OS kernel and user programs.

**Core Idea:**
- Divide each memory page into equal-sized power-of-two blocks (minimum 8 bytes).
- Maintain a doubly-linked free-block list per block size (`freeBlockLists`).
- Track page metadata (block size, free-block count) in `pageBlockInfoArr`.
- Maintain a `freePagesList` for unallocated pages.

**Allocation Strategy (4 cases):**
1. A free block of the requested size exists → allocate directly.
2. No free block, but a free page exists → split the page into blocks, then allocate.
3. No free page → allocate from the next larger size list.
4. Nothing available → `panic()` (or block the process if Bonus #1 is implemented).

**Free Strategy:**
- Return the block to its size list and increment the free-block count.
- If the entire page becomes free, remove all its blocks from `freeBlockLists`, return the frame to the page allocator, and add the page to `freePagesList`.

**Functions implemented:**

| # | Function | File |
|---|----------|------|
| 1 | `initialize_dynamic_allocator(uint32 daStart, uint32 daEnd)` | `lib/dynamic_allocator.c` |
| 2 | `get_block_size(void *va)` | `lib/dynamic_allocator.c` |
| 3 | `alloc_block(uint32 required_size)` | `lib/dynamic_allocator.c` |
| 4 | `free_block(void* va)` | `lib/dynamic_allocator.c` |
| BONUS | Block on no free block instead of panic | `lib/dynamic_allocator.c` |

**Complexity:**
- Allocate: O(1) best case, O(C) bounded (C = max blocks/page).
- Free: O(1) average, O(K) worst case (K = free blocks in corresponding list).

---

### Kernel Heap

**Goal:** Allow the kernel to dynamically allocate and free data of varying sizes at runtime, beyond the 256 MB one-to-one mapped RAM limit.

**Two allocator regions inside the kernel heap:**

| Region | Range | Strategy |
|--------|-------|----------|
| Block Allocator | `[KERNEL_HEAP_START, BLK_ALLOC_LIMIT)` | Uses Dynamic Allocator for sizes ≤ `DYN_ALLOC_MAX_BLOCK_SIZE` |
| Page Allocator | `[BLK_ALLOC_LIMIT + PAGE_SIZE, KERNEL_HEAP_MAX)` | CUSTOM FIT for sizes > `DYN_ALLOC_MAX_BLOCK_SIZE`, page-boundary granularity |

**CUSTOM FIT Strategy (Page Allocator):**
1. Search for an exact fit.
2. If not found, search for the worst fit up to the current break.
3. If not found, extend the break.
4. If the heap is exhausted, return `NULL`.

**Functions implemented:**

| # | Function | File |
|---|----------|------|
| 5 | `kmalloc(unsigned int size)` | `kern/mem/kheap.c` |
| 6 | `kfree(void* virtual_address)` | `kern/mem/kheap.c` |
| 7 | `kheap_physical_address(unsigned int va)` | `kern/mem/kheap.c` |
| 8 | `kheap_virtual_address(unsigned int pa)` | `kern/mem/kheap.c` |
| BONUS 1 | `krealloc(void *va, uint32 new_size)` | `kern/mem/kheap.c` |
| BONUS 2 | Fast Page Allocator (efficient data structures) | `kern/mem/kheap.c` |

> **Note:** Any shared kernel data must be protected with spinlocks.

---

### Fault Handler I — Placement

**Goal:** Handle page faults by loading the faulted page from disk (page file) into RAM.

**Functions implemented:**

| # | Function | File |
|---|----------|------|
| 9 | `create_user_kern_stack(uint32* ptr_user_page_directory)` | `kern/mem/kheap.c` |
| 10 | `fault_handler(struct Trapframe *tf)` — invalid pointer check | `kern/trap/fault_handler.c` |
| 11 | `page_fault_handler(...)` — Placement (Scenario 1) | `kern/trap/fault_handler.c` |

---

## Individual Modules (Part III)

### Module 1 & 6 — Fault Handler II (Replacement)

**Goal:** When the Working Set is full, choose a victim page to evict and bring in the faulted page. Supports three replacement algorithms:

- **CLOCK:** Uses a "used bit"; scans the WS circularly and replaces the first page with `used = 0`.
- **LRU (Aging):** Maintains a 32-bit aging counter per page; updated at every clock interrupt by shifting right and adding the used bit to the MSB. Evicts the page with the lowest counter.
- **Modified CLOCK:** Two-pass scan using `(used, modified)` bits; prefers evicting `(0,0)` pages to minimize disk writes.
- **OPTIMAL (Benchmark):** Simulates the process to build a reference stream, then traces it offline to compute the minimum possible fault count.

**Switch commands:**
```
FOS> optimal
FOS> clock
FOS> lru
FOS> modclock
```

---

### Module 2 — User Heap

**Goal:** Allow user processes to dynamically allocate and free memory via `malloc()` / `free()`.

Mirrors the Kernel Heap design but operates in user space. Pages are only brought into RAM on access (via the Fault Handler), not at allocation time.

**Functions implemented:**

| # | Function | Side |
|---|----------|------|
| 1 | `malloc(unsigned int size)` — CUSTOM FIT | User (`lib/uheap.c`) |
| 2 | `free(void* virtual_address)` | User (`lib/uheap.c`) |
| 3 | `allocate_user_mem(struct Env*, uint32 va, uint32 size)` | Kernel (`kern/mem/chunk_operations.c`) |
| 4 | `free_user_mem(struct Env*, uint32 va, uint32 size)` | Kernel (`kern/mem/chunk_operations.c`) |

---

### Module 3 — Shared Memory

> **This is one of my two individual contributions. See [My Contribution](#my-contribution) for full details.**

---

### Module 4 — CPU Scheduling

**Goal:** Priority-based Round Robin scheduling with anti-starvation promotion.

- Multiple ready queues, one per priority level.
- Preemptive on clock tick.
- If a process exceeds the starvation threshold (in ticks), its priority is promoted.

**Functions implemented:**

| # | Function | File |
|---|----------|------|
| 1 | `env_set_priority(int envID, int priority)` + system call | `kern/cpu/sched_helpers.c` |
| 2 | `sched_init_PRIRR(uint8 numOfPriorities, uint8 quantum, uint32 starvThresh)` | `kern/cpu/sched.c` |
| 3 | `fos_scheduler_PRIRR()` | `kern/cpu/sched.c` |
| 4 | `clock_interrupt_handler()` | `kern/cpu/sched.c` |

---

### Module 5 — Kernel Protection

**Goal:** Implement SleepLocks and Semaphores for kernel-level mutual exclusion and synchronization.

**SleepLocks** — eliminates busy-waiting by blocking the process on a channel until the lock is released.

**Kernel Semaphores** — general-purpose counting semaphore with FIFO unblocking.

**Functions implemented:**

| # | Function | File |
|---|----------|------|
| 1 | `sleep(struct Channel *chan, struct spinlock* lk)` | `kern/conc/channel.c` |
| 2 | `wakeup_one(struct Channel *chan)` | `kern/conc/channel.c` |
| 3 | `wakeup_all(struct Channel *chan)` | `kern/conc/channel.c` |
| 4 | `acquire_sleeplock(struct sleeplock *lk)` | `kern/conc/sleeplock.c` |
| 5 | `release_sleeplock(struct sleeplock *lk)` | `kern/conc/sleeplock.c` |
| 6 | `wait_ksemaphore(struct ksemaphore sem)` | `kern/conc/ksemaphore.c` |
| 7 | `signal_ksemaphore(struct ksemaphore sem)` | `kern/conc/ksemaphore.c` |

---

## My Contribution

I was individually responsible for **two modules**: the **Dynamic Allocator** (group prerequisite) and **Shared Memory** (Individual Module #3). These two modules are tightly coupled — the Dynamic Allocator underpins all kernel and user memory management, and Shared Memory builds on top of the user heap's page allocator.

---

### 1. Dynamic Allocator

**Files:** `lib/dynamic_allocator.c`, `inc/dynamic_allocator.h`

The Dynamic Allocator provides fast, memory-efficient allocation of small blocks for use throughout the entire OS. It is the backbone of both the Kernel Heap (block allocator region) and the User Heap (block allocator region).

#### Design

The allocator uses a **slab-inspired, power-of-two free-list** design:

- **`pageBlockInfoArr`** — an array tracking each page's block size and free-block count.
- **`freeBlockLists`** — an array of doubly-linked lists, one per block size (8B, 16B, 32B, …, 2KB).
- **`freePagesList`** — a doubly-linked list of completely free pages, ready to be divided into any block size.

The minimum block size is 8 bytes, because each free block must store two pointers (`prev`, `next`) for its list linkage. Every allocation is rounded up to the nearest power of two to ensure natural alignment and simple list indexing.

#### `initialize_dynamic_allocator`

Sets up the DA limits (`daStart`, `daEnd`), zeroes out `pageBlockInfoArr`, and initializes `freePagesList` and all entries of `freeBlockLists` as empty lists. This function is called once during kernel boot via `kheap_init()`.

#### `get_block_size`

Looks up the virtual address in `pageBlockInfoArr` and returns the block size of the page that contains `va`. This is an O(1) operation used during `free_block` to determine which free list to return the block to.

#### `alloc_block`

1. Round `required_size` up to the nearest power of two.
2. **Case 1 — Free block exists:** Pop the head of `freeBlockLists[size]`, update `pageBlockInfoArr`, return the block's address.
3. **Case 2 — Free page exists:** Dequeue a page from `freePagesList`, split it into blocks of the target size, push all but one onto `freeBlockLists[size]`, update `pageBlockInfoArr`, and return the first block.
4. **Case 3 — No free page, escalate:** Try the next larger size list upward.
5. **Case 4 — Nothing available:** Call `panic()` (or, with Bonus #1, block the calling process on a wait channel until `free_block` wakes it up).

Special case: `required_size == 0` returns `NULL` immediately.

#### `free_block`

1. Determine the block's size via `get_block_size(va)`.
2. Push the block back onto the front of `freeBlockLists[size]`.
3. Increment `pageBlockInfoArr[page].numFreeBlocks`.
4. **If the entire page is now free:**
   - Remove all of that page's blocks from `freeBlockLists[size]` (linear scan, O(K)).
   - Call `return_page(pageVA)` to unmap the frame and return it to the kernel page allocator.
   - Push the page onto `freePagesList`.
   - If Bonus #1 is active, wake up any processes blocked waiting for a free block.

#### Bonus #1 — Block Instead of Panic

When `alloc_block` reaches Case 4 and no memory is available, instead of calling `panic()`, the calling process is put to sleep on a dedicated wait channel. When `free_block` returns a page to `freePagesList`, it wakes up all waiting processes, which then retry allocation. This allows graceful handling of memory pressure in a multi-process environment.

---

### 2. Shared Memory

**Files:** `kern/mem/shared_memory_manager.c`, `kern/mem/shared_memory_manager.h`, `lib/uheap.c` (user-side)

Shared Memory allows two or more user processes to map the **same physical frames** into their respective virtual address spaces, enabling zero-copy data sharing.

#### Design

Each shared object is described by a `struct Share`:

```c
struct Share {
    int32   ID;               // VA of object with MSB masked (always positive)
    char    name[64];         // Human-readable name
    int32   ownerID;          // Env ID of the creating process
    int     size;             // Total size in bytes
    uint32  references;       // Number of processes currently mapping this object
    uint8   isWritable;       // 0 = read-only, 1 = writable
    struct FrameInfo** framesStorage; // Array of physical frame pointers
    LIST_ENTRY(Share) prev_next_info;
};
```

All live shared objects are tracked in a global `AllShares.shares_list`, protected by `AllShares.shareslock` (a spinlock) to prevent race conditions in multi-process scenarios.

The physical frames of a shared object are allocated **eagerly at creation time** (unlike user heap pages which are lazy). This guarantees that every process that `sget`s the object can immediately map the same frames without triggering page faults for allocation.

#### `alloc_share` — Allocate a Share Descriptor

Allocates and initializes a new `struct Share` using `kmalloc`. Sets `references = 1`, computes the `ID` as the VA of the object with its MSB masked out (ensuring a positive identifier), and allocates `framesStorage` as a zero-initialized array of `(size / PAGE_SIZE)` frame pointers. Returns `NULL` and frees any partial allocations on failure.

#### `smalloc` — User-Side Creation (lib/uheap.c)

Called by the owning process to create a named shared object:

1. Apply the **CUSTOM FIT** strategy on the user heap's page allocator to find a suitable virtual address range aligned to 4 KB boundaries (same algorithm as `malloc` for large sizes).
2. If no fit is found, return `NULL`.
3. Invoke `sys_create_shared_object(ownerID, name, size, isWritable, va)` to cross into kernel mode.
4. Return the virtual address on success, `NULL` on failure.

#### `create_shared_object` — Kernel-Side Creation (shared_memory_manager.c)

Runs in kernel mode on behalf of `smalloc`:

1. Call `alloc_share` to allocate and initialize the `struct Share`.
2. Insert the object into `AllShares.shares_list` (under `shareslock`).
3. Allocate all required physical frames one page at a time and map them into the owner's address space at `virtual_address` with writable permissions.
4. Store each allocated `struct FrameInfo*` into `framesStorage` so they can be shared with other processes later.
5. Return `E_SHARED_MEM_EXISTS` if an object with the same owner and name already exists, `E_NO_SHARE` on allocation failure, or the object's `ID` on success.

#### `sget` — User-Side Access (lib/uheap.c)

Called by a non-owning process to gain access to an existing shared object:

1. Call `sys_size_of_shared_object(ownerEnvID, name)` to look up the object's size. Return `NULL` if not found.
2. Apply **CUSTOM FIT** on the caller's page allocator to find a virtual address range for the shared object.
3. Call `sys_get_shared_object(ownerID, name, va)` to cross into kernel mode.
4. Return the virtual address on success, `NULL` on failure.

#### `get_shared_object` — Kernel-Side Sharing (shared_memory_manager.c)

Runs in kernel mode on behalf of `sget`:

1. Search `AllShares.shares_list` via `find_share(ownerID, name)`.
2. Return `E_SHARED_MEM_NOT_EXISTS` if the object is not found.
3. Iterate over `framesStorage` and map each physical frame into the requesting process's address space starting at `virtual_address`, using `isWritable` to set read-only or writable permissions.
4. Increment `references` on the shared object.
5. Return the object's `ID`.

#### Memory Layout Example

```
Process 1 (owner)                     Process 2 (accessor)
PAGE ALLOCATOR                         PAGE ALLOCATOR
┌─────────────────┐                   ┌─────────────────┐
│  ptr_x (6 KB)   │──────────────────▶│  x2 (6 KB)      │
│  [F#30, F#100]  │   same frames     │  [F#30, F#100]  │
├─────────────────┤                   ├─────────────────┤
│  ptr_y (2 KB)   │──────────────────▶│  y2 (2 KB)      │
│  [F#99]         │   same frames     │  [F#99]         │
└─────────────────┘                   └─────────────────┘
         │                                     │
         └──────────── RAM ───────────────────┘
              F#30, F#99, F#100  (shared frames)
```

#### Concurrency & Safety

- All accesses to `AllShares.shares_list` are wrapped with `acquire/release` on `AllShares.shareslock`.
- The `references` counter is incremented atomically within the lock to prevent double-free or use-after-free of physical frames.
- `framesStorage` is fully populated before the object is inserted into the global list, so no reader can observe a partially-initialized object.

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `inc/dynamic_allocator.h` | DA constants, structs, and declarations |
| `lib/dynamic_allocator.c` | Dynamic Allocator implementation |
| `kern/mem/kheap.h` | Kernel Heap declarations |
| `kern/mem/kheap.c` | Kernel Heap implementation (kmalloc, kfree, …) |
| `kern/mem/shared_memory_manager.h` | Shared Memory structs and declarations |
| `kern/mem/shared_memory_manager.c` | Shared Memory kernel-side implementation |
| `lib/uheap.c` | User Heap + Shared Memory user-side (smalloc, sget, malloc, free) |
| `kern/mem/chunk_operations.c` | allocate_user_mem / free_user_mem |
| `kern/trap/fault_handler.c` | Page fault handler (placement & replacement) |
| `kern/cpu/sched.c` | Priority RR scheduler & clock handler |
| `kern/conc/channel.c` | Sleep/wakeup channels |
| `kern/conc/sleeplock.c` | SleepLock implementation |
| `kern/conc/ksemaphore.c` | Kernel semaphore implementation |

---

## Build & Run

```bash
# Build the kernel
make

# Run in QEMU
make qemu

# FOS prompt commands
FOS> run   <prog_name> <page_WS_size> [<priority>]
FOS> load  <prog_name> <page_WS_size> [<priority>]
FOS> setPri <envID> <priority>
FOS> setStarvThr <threshold>
FOS> runall
FOS> printall
FOS> sched?

# Switch page replacement algorithm
FOS> clock
FOS> optimal
FOS> lru
FOS> modclock

# Switch scheduler
FOS> schedRR <quantum>
FOS> schedPRIRR <#priorities> <quantum> <starvThresh>

# Enable Kernel Heap (required before building)
# In inc/memlayout.h, set USE_KHEAP to 1
```

---

> **Developed as part of the OS'25 course project.**
