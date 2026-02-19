// Copyright (c) 2015, The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#ifndef DEVMEM_ALLOC_H_
#define DEVMEM_ALLOC_H_

/*
GAP Benchmark Suite
File:   DevMemArena
Author: (research extension)

Bump allocator backed by a contiguous physical memory region exposed via
/dev/mem.  Enabled at compile time by defining DEVMEM_PHYS_ADDR and
DEVMEM_SIZE, e.g.:

    make DEVMEM_PHYS_ADDR=0xF00000000UL DEVMEM_SIZE=0x100000000UL

System requirements:
  - Boot with  mem=<total-X>  to reserve the physical range from the OS
  - Boot with  iomem=relaxed  (or CONFIG_STRICT_DEVMEM=n) to allow /dev/mem
    access to RAM pages
  - Run the binary as root (or with CAP_SYS_RAWIO)

All graph CSR arrays (out_neighbors_, out_index_, in_neighbors_, in_index_)
are allocated from this arena.  Temporary build structures (pvector edge
lists, degree arrays) remain on the normal heap and are freed before the
kernel runs.
*/

#ifdef DEVMEM_PHYS_ADDR

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>


class DevMemArena {
 public:
    /*
     * Open /dev/mem and mmap phys_addr..phys_addr+size into the process
     * address space.  Exits on any error - no silent fallback to heap.
     */
    static void init(uint64_t phys_addr, size_t size)
    {
        if (base_ != nullptr) {
            fprintf(stderr, "DevMemArena::init called twice\n");
            return;
        }
        int fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (fd < 0) {
            perror("DevMemArena: open /dev/mem");
            fprintf(stderr,
                "  Hint: run as root and boot with iomem=relaxed\n");
            exit(1);
        }
        void *mapped = mmap(nullptr, size,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd,
                            static_cast<off_t>(phys_addr));
        close(fd);
        if (mapped == MAP_FAILED) {
            perror("DevMemArena: mmap /dev/mem");
            fprintf(stderr,
                "  phys_addr=0x%" PRIx64 " size=%zu\n", phys_addr, size);
            exit(1);
        }
        base_   = static_cast<char *>(mapped);
        size_   = size;
        offset_ = 0;
        printf("DevMemArena: mapped phys 0x%" PRIx64
               " (%zu MiB) at virt %p\n",
               phys_addr, size >> 20, base_);
    }

    /*
     * Aligned bump allocation.  Aborts if the arena is exhausted.
     */
    template <typename T>
    static T *alloc(size_t n)
    {
        size_t align   = alignof(T);
        size_t cur     = (offset_ + align - 1) & ~(align - 1);
        size_t needed  = n * sizeof(T);
        if (cur + needed > size_) {
            fprintf(stderr,
                "DevMemArena: out of memory (need %zu, used %zu / %zu)\n",
                needed, cur, size_);
            exit(1);
        }
        offset_ = cur + needed;
        return reinterpret_cast<T *>(base_ + cur);
    }

    /*
     * Returns true if p points inside the arena.  Used by CSRGraph
     * ReleaseResources() to avoid calling delete[] on arena memory.
     */
    static bool is_arena_ptr(const void *p)
    {
        const char *cp = static_cast<const char *>(p);
        return (cp >= base_) && (cp < base_ + size_);
    }

    static void print_stats()
    {
        printf("DevMemArena: used %zu MiB / %zu MiB\n",
               offset_ >> 20, size_ >> 20);
    }

 private:
    static char   *base_;
    static size_t  size_;
    static size_t  offset_;
};

/* Static member definitions - header-only, included by a single TU per binary */
char   *DevMemArena::base_   = nullptr;
size_t  DevMemArena::size_   = 0;
size_t  DevMemArena::offset_ = 0;


/* Convenience macros used across builder.h, graph.h, reader.h */
#define GAPBS_ALLOC(T, n)  (DevMemArena::alloc<T>(n))
#define GAPBS_FREE(p)      /* no-op: bump allocator, freed at process exit */

#else  /* DEVMEM_PHYS_ADDR not defined - standard heap paths */

#define GAPBS_ALLOC(T, n)  (new T[n])
#define GAPBS_FREE(p)      (delete[] (p))

#endif  /* DEVMEM_PHYS_ADDR */

#endif  /* DEVMEM_ALLOC_H_ */
