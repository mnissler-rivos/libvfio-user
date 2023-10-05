/*
 * Copyright (c) 2019 Nutanix Inc. All rights reserved.
 *
 * Authors: Mike Cui <cui@nutanix.com>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Nutanix nor the names of its contributors may be
 *        used to endorse or promote products derived from this software without
 *        specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/param.h>

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>

#include "dma.h"
#include "private.h"

EXPORT size_t
dma_sg_size(void)
{
    return sizeof(dma_sg_t);
}

bool
dma_sg_is_mappable(const dma_controller_t *dma, const dma_sg_t *sg) {
    return sg->region[dma->regions].info.vaddr != NULL;
}

static inline ssize_t
fd_get_blocksize(int fd)
{
    struct stat st;

    if (fstat(fd, &st) != 0)
        return -1;

    return st.st_blksize;
}

/* Returns true if 2 fds refer to the same file.
   If any fd is invalid, return false. */
static inline bool
fds_are_same_file(int fd1, int fd2)
{
    struct stat st1, st2;

    if (fd1 == fd2) {
        return true;
    }

    return (fstat(fd1, &st1) == 0 && fstat(fd2, &st2) == 0 &&
            st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}

dma_controller_t *
dma_controller_create(vfu_ctx_t *vfu_ctx, size_t max_regions, size_t max_size)
{
    dma_controller_t *dma;

    dma = malloc(offsetof(dma_controller_t, regions) +
                 max_regions * sizeof(dma->regions[0]));

    if (dma == NULL) {
        return dma;
    }

    dma->vfu_ctx = vfu_ctx;
    dma->max_regions = (int)max_regions;
    dma->max_size = max_size;
    dma->nregions = 0;
    memset(dma->regions, 0, max_regions * sizeof(dma->regions[0]));
    dma->dirty_pgsize = 0;

    return dma;
}

void
MOCK_DEFINE(dma_controller_unmap_region)(dma_controller_t *dma,
                                         dma_memory_region_t *region)
{
    int err;

    assert(dma != NULL);
    assert(region != NULL);

    err = munmap(region->info.mapping.iov_base, region->info.mapping.iov_len);
    if (err != 0) {
        vfu_log(dma->vfu_ctx, LOG_DEBUG, "failed to unmap fd=%d "
                "mapping=[%p, %p): %m",
                region->fd, region->info.mapping.iov_base,
                iov_end(&region->info.mapping));
    }

    assert(region->fd != -1);

    close_safely(&region->fd);
}

static void
array_remove(void *array, size_t elem_size, size_t index, int *nr_elemsp)
{
    void *dest;
    void *src;
    size_t nr;

    assert((size_t)*nr_elemsp > index);

    nr = *nr_elemsp - (index + 1);
    dest = (char *)array + (index * elem_size);
    src = (char *)array + ((index + 1) * elem_size);

    memmove(dest, src, nr * elem_size);

    (*nr_elemsp)--;
}

/* FIXME not thread safe */
int
MOCK_DEFINE(dma_controller_remove_region)(
    dma_controller_t *dma, vfu_dma_addr_t dma_addr, uint32_t pasid, size_t size,
    vfu_dma_unregister_cb_t *dma_unregister, void *data)
{
    int idx;
    dma_memory_region_t *region;

    assert(dma != NULL);

    for (idx = 0; idx < dma->nregions; idx++) {
        region = &dma->regions[idx];
        if (region->info.iova.iov_base != dma_addr ||
            region->info.iova.iov_len != size ||
            region->info.pasid != pasid) {
            continue;
        }

        if (dma_unregister != NULL) {
            dma->vfu_ctx->in_cb = CB_DMA_UNREGISTER;
            dma_unregister(data, &region->info);
            dma->vfu_ctx->in_cb = CB_NONE;
        }

        if (region->info.vaddr != NULL) {
            dma_controller_unmap_region(dma, region);
        } else {
            assert(region->fd == -1);
        }

        array_remove(&dma->regions, sizeof (*region), idx, &dma->nregions);
        return 0;
    }
    return ERROR_INT(ENOENT);
}

void
dma_controller_remove_all_regions(dma_controller_t *dma,
                                  vfu_dma_unregister_cb_t *dma_unregister,
                                  void *data)
{
    int i;

    assert(dma != NULL);

    for (i = 0; i < dma->nregions; i++) {
        dma_memory_region_t *region = &dma->regions[i];

        vfu_log(dma->vfu_ctx, LOG_DEBUG, "removing DMA region "
                "iova=[%p, %p) vaddr=%p mapping=[%p, %p)",
                region->info.iova.iov_base, iov_end(&region->info.iova),
                region->info.vaddr,
                region->info.mapping.iov_base, iov_end(&region->info.mapping));

        if (dma_unregister != NULL) {
            dma->vfu_ctx->in_cb = CB_DMA_UNREGISTER;
            dma_unregister(data, &region->info);
            dma->vfu_ctx->in_cb = CB_NONE;
        }

        if (region->info.vaddr != NULL) {
            dma_controller_unmap_region(dma, region);
        } else {
            assert(region->fd == -1);
        }
    }

    memset(dma->regions, 0, dma->max_regions * sizeof(dma->regions[0]));
    dma->nregions = 0;
}

void
dma_controller_destroy(dma_controller_t *dma)
{
    assert(dma->nregions == 0);
    free(dma);
}

static int
dma_map_region(dma_controller_t *dma, dma_memory_region_t *region)
{
    void *mmap_base;
    size_t mmap_len;
    off_t offset;

    offset = ROUND_DOWN(region->offset, region->info.page_size);
    mmap_len = ROUND_UP(region->info.iova.iov_len, region->info.page_size);

    mmap_base = mmap(NULL, mmap_len, region->info.prot, MAP_SHARED,
                     region->fd, offset);

    if (mmap_base == MAP_FAILED) {
        return -1;
    }

    // Do not dump.
    madvise(mmap_base, mmap_len, MADV_DONTDUMP);

    region->info.mapping.iov_base = mmap_base;
    region->info.mapping.iov_len = mmap_len;
    region->info.vaddr = mmap_base + (region->offset - offset);

    vfu_log(dma->vfu_ctx, LOG_DEBUG, "mapped DMA region iova=[%p, %p) "
            "vaddr=%p page_size=%zx mapping=[%p, %p)",
            region->info.iova.iov_base, iov_end(&region->info.iova),
            region->info.vaddr, region->info.page_size,
            region->info.mapping.iov_base, iov_end(&region->info.mapping));


    return 0;
}

static int
dirty_page_logging_start_on_region(dma_memory_region_t *region, size_t pgsize)
{
    assert(region->fd != -1);

    ssize_t size = get_bitmap_size(region->info.iova.iov_len, pgsize);
    if (size < 0) {
        return size;
    }

    region->dirty_bitmap = calloc(size, 1);
    if (region->dirty_bitmap == NULL) {
        return ERROR_INT(errno);
    }
    return 0;
}

int
MOCK_DEFINE(dma_controller_add_region)(dma_controller_t *dma,
                                       vfu_dma_addr_t dma_addr, uint32_t pasid,
                                       uint64_t size, int fd, off_t offset,
                                       uint32_t prot)
{
    dma_memory_region_t *region;
    int page_size = 0;
    char rstr[1024];
    int idx;

    assert(dma != NULL);

    snprintf(rstr, sizeof(rstr), "[%p, %p) fd=%d offset=%#llx prot=%#x",
             dma_addr, dma_addr + size, fd, (ull_t)offset, prot);

    // Upstream has a size limit check here. With address translation, that
    // check does not make a lot of sense, since there is no such thing as a
    // RAM region, and it is generally neither suitable nor practical to
    // propagate *all* IOMMU mappings. Thus, the host would configure a region
    // for whatever portion of the address space is used to allocate I/O
    // virtual addresses - which is the entire address space in the limit.
    //
    // Note that things are different with ATS (and possibly PRI) enabled: In
    // that case it is OK to start with no DMA regions at all, and request
    // mappings on demand via vfu_page_request.

    for (idx = 0; idx < dma->nregions; idx++) {
        region = &dma->regions[idx];

        if (region->info.pasid != pasid) {
            continue;
        }

        /* First check if this is the same exact region. */
        if (region->info.iova.iov_base == dma_addr &&
            region->info.iova.iov_len == size) {
            if (offset != region->offset) {
                vfu_log(dma->vfu_ctx, LOG_ERR, "bad offset for new DMA region "
                        "%s; existing=%#llx", rstr,
                        (ull_t)region->offset);
                return ERROR_INT(EINVAL);
            }
            if (!fds_are_same_file(region->fd, fd)) {
                /*
                 * Printing the file descriptors here doesn't really make
                 * sense as they can be different but actually pointing to
                 * the same file, however in the majority of cases we'll be
                 * using a single fd.
                 */
                vfu_log(dma->vfu_ctx, LOG_ERR, "bad fd for new DMA region %s; "
                        "existing=%d", rstr, region->fd);
                return ERROR_INT(EINVAL);
            }
            /* Allow protection changes. */
            region->info.prot = prot;
            return idx;
        }

        /* Check for overlap, i.e. start of one region is within another. */
        if ((dma_addr >= region->info.iova.iov_base &&
             dma_addr < iov_end(&region->info.iova)) ||
            (region->info.iova.iov_base >= dma_addr &&
             region->info.iova.iov_base < dma_addr + size)) {
            vfu_log(dma->vfu_ctx, LOG_INFO, "new DMA region %s overlaps with "
                    "DMA region [%p, %p)", rstr, region->info.iova.iov_base,
                    iov_end(&region->info.iova));
            return ERROR_INT(EINVAL);
        }
    }

    if (dma->nregions == dma->max_regions) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "hit max regions %d", dma->max_regions);
        return ERROR_INT(EINVAL);
    }

    idx = dma->nregions;
    region = &dma->regions[idx];

    if (fd != -1) {
        page_size = fd_get_blocksize(fd);
        if (page_size < 0) {
            vfu_log(dma->vfu_ctx, LOG_ERR, "bad page size %d", page_size);
            return ERROR_INT(EINVAL);
        }
    }
    page_size = MAX(page_size, getpagesize());

    memset(region, 0, sizeof (*region));

    region->info.iova.iov_base = (void *)dma_addr;
    region->info.iova.iov_len = size;
    region->info.pasid = pasid;
    region->info.page_size = page_size;
    region->info.prot = prot;
    region->offset = offset;
    region->fd = fd;

    if (fd != -1) {
        int ret;

        /*
         * TODO introduce a function that tells whether dirty page logging is
         * enabled
         */
        if (dma->dirty_pgsize != 0) {
            if (dirty_page_logging_start_on_region(region, dma->dirty_pgsize) < 0) {
                /*
                 * TODO We don't necessarily have to fail, we can continue
                 * and fail the get dirty page bitmap request later.
                 */
                return -1;
            }
        }

        ret = dma_map_region(dma, region);

        if (ret != 0) {
            ret = errno;
            vfu_log(dma->vfu_ctx, LOG_ERR,
                   "failed to memory map DMA region %s: %m", rstr);

            close_safely(&region->fd);
            free(region->dirty_bitmap);
            return ERROR_INT(ret);
        }
    }

    dma->nregions++;
    return idx;
}

int
_dma_addr_sg_split(const dma_controller_t *dma, vfu_dma_addr_t dma_addr,
                   uint32_t pasid, uint64_t len, dma_sg_t *sg, int max_nr_sgs,
                   int prot)
{
    int idx;
    int cnt = 0, ret;
    bool found = true;          // Whether the current region is found.

    while (found && len > 0) {
        found = false;
        for (idx = 0; idx < dma->nregions; idx++) {
            const dma_memory_region_t *const region = &dma->regions[idx];
            vfu_dma_addr_t region_start = region->info.iova.iov_base;
            vfu_dma_addr_t region_end = iov_end(&region->info.iova);

            if (region->info.pasid != pasid) {
                continue;
            }

            while (dma_addr >= region_start && dma_addr < region_end) {
                size_t region_len = MIN((uint64_t)(region_end - dma_addr), len);

                if (cnt < max_nr_sgs) {
                    ret = dma_init_sg(dma, &sg[cnt], dma_addr, pasid,
                                      region_len, prot, idx);
                    if (ret < 0) {
                        return ret;
                    }
                }

                cnt++;

                // dma_addr found, may need to start from the top for the
                // next dma_addr.
                found = true;
                dma_addr += region_len;
                len -= region_len;

                if (len == 0) {
                    goto out;
                }
            }
        }
    }

out:
    if (!found) {
        // There is still a region which was not found.
        assert(len > 0);
        return ERROR_INT(ENOENT);
    } else if (cnt > max_nr_sgs) {
        cnt = -cnt - 1;
    }
    errno = 0;
    return cnt;
}

int
dma_controller_dirty_page_logging_start(dma_controller_t *dma, size_t pgsize)
{
    size_t i;

    assert(dma != NULL);

    if (pgsize == 0) {
        return ERROR_INT(EINVAL);
    }

    if (dma->dirty_pgsize > 0) {
        if (dma->dirty_pgsize != pgsize) {
            return ERROR_INT(EINVAL);
        }
        return 0;
    }

    for (i = 0; i < (size_t)dma->nregions; i++) {
        dma_memory_region_t *region = &dma->regions[i];

        if (region->fd == -1) {
            continue;
        }

        if (dirty_page_logging_start_on_region(region, pgsize) < 0) {
            int _errno = errno;
            size_t j;

            for (j = 0; j < i; j++) {
                region = &dma->regions[j];
                free(region->dirty_bitmap);
                region->dirty_bitmap = NULL;
            }
            return ERROR_INT(_errno);
        }
    }
    dma->dirty_pgsize = pgsize;

    vfu_log(dma->vfu_ctx, LOG_DEBUG, "dirty pages: started logging");

    return 0;
}

void
dma_controller_dirty_page_logging_stop(dma_controller_t *dma)
{
    int i;

    assert(dma != NULL);

    if (dma->dirty_pgsize == 0) {
        return;
    }

    for (i = 0; i < dma->nregions; i++) {
        free(dma->regions[i].dirty_bitmap);
        dma->regions[i].dirty_bitmap = NULL;
    }
    dma->dirty_pgsize = 0;

    vfu_log(dma->vfu_ctx, LOG_DEBUG, "dirty pages: stopped logging");
}


#ifdef DEBUG
static void
log_dirty_bitmap(vfu_ctx_t *vfu_ctx, dma_memory_region_t *region,
                 char *bitmap, size_t size, size_t pgsize)
{
    size_t i;
    size_t count;
    for (i = 0, count = 0; i < size; i++) {
        count += __builtin_popcount((uint8_t)bitmap[i]);
    }
    vfu_log(vfu_ctx, LOG_DEBUG,
            "dirty pages: get [%p, %p), %zu dirty pages of size %zu",
            region->info.iova.iov_base, iov_end(&region->info.iova),
            count, pgsize);
}
#endif

static void
dirty_page_exchange(uint8_t *outp, uint8_t *bitmap)
{
    /*
     * If no bits are dirty, avoid the atomic exchange. This is obviously
     * racy, but it's OK: if we miss a dirty bit being set, we'll catch it
     * the next time around.
     *
     * Otherwise, atomically exchange the dirty bits with zero: as we use
     * atomic or in _dma_mark_dirty(), this cannot lose set bits - we might
     * miss a bit being set after, but again, we'll catch that next time
     * around.
     */
    if (*bitmap == 0) {
        *outp = 0;
    } else {
        uint8_t zero = 0;
        __atomic_exchange(bitmap, &zero, outp, __ATOMIC_SEQ_CST);
    }
}

static void
dirty_page_get_same_pgsize(dma_memory_region_t *region, char *bitmap,
                           size_t bitmap_size)
{
    for (size_t i = 0; i < bitmap_size; i++) {
        dirty_page_exchange((uint8_t *)&bitmap[i], &region->dirty_bitmap[i]);
    }
}

static void
dirty_page_get_extend(dma_memory_region_t *region, char *bitmap,
                      size_t server_bitmap_size, size_t server_pgsize,
                      size_t client_bitmap_size, size_t client_pgsize)
{
    /*
     * The index of the bit in the client bitmap that we are currently
     * considering. By keeping track of this separately to the for loop, we
     * allow for one server bit to be repeated for multiple client bytes.
     */
    uint8_t client_bit_idx = 0;
    size_t server_byte_idx;
    int server_bit_idx;
    size_t factor = server_pgsize / client_pgsize;

    /*
     * Iterate through the bytes of the server bitmap.
     */
    for (server_byte_idx = 0; server_byte_idx < server_bitmap_size;
         server_byte_idx++) {

        if (client_bit_idx / CHAR_BIT >= client_bitmap_size) {
            break;
        }

        uint8_t out = 0;

        dirty_page_exchange(&out, &region->dirty_bitmap[server_byte_idx]);

        /*
         * Iterate through the bits of the server byte, repeating bits to reach
         * the desired page size.
         */
        for (server_bit_idx = 0; server_bit_idx < CHAR_BIT; server_bit_idx++) {
            uint8_t server_bit = (out >> server_bit_idx) & 1;

            /*
             * Repeat `factor` times the bit at index `j` of `out`.
             *
             * OR the same bit from the server bitmap (`server_bit`) with
             * `factor` bits in the client bitmap, from `client_bit_idx` to
             * `end_client_bit_idx`.
             */
            for (size_t end_client_bit_idx = client_bit_idx + factor;
                 client_bit_idx < end_client_bit_idx;
                 client_bit_idx++) {

                bitmap[client_bit_idx / CHAR_BIT] |=
                    server_bit << (client_bit_idx % CHAR_BIT);
            }
        }
    }
}

static void
dirty_page_get_combine(dma_memory_region_t *region, char *bitmap,
                       size_t server_bitmap_size, size_t server_pgsize,
                       size_t client_bitmap_size, size_t client_pgsize)
{
    /*
     * The index of the bit in the client bitmap that we are currently
     * considering. By keeping track of this separately to the for loop, we
     * allow multiple bytes' worth of server bits to be OR'd together to
     * calculate one client bit.
     */
    uint8_t client_bit_idx = 0;
    size_t server_byte_idx;
    int server_bit_idx;
    size_t factor = client_pgsize / server_pgsize;

    /*
     * Iterate through the bytes of the server bitmap.
     */
    for (server_byte_idx = 0; server_byte_idx < server_bitmap_size;
         server_byte_idx++) {

        if (client_bit_idx / CHAR_BIT >= client_bitmap_size) {
            break;
        }
            
        uint8_t out = 0;

        dirty_page_exchange(&out, &region->dirty_bitmap[server_byte_idx]);

        /*
         * Iterate through the bits of the server byte, combining bits to reach
         * the desired page size.
         */
        for (server_bit_idx = 0; server_bit_idx < CHAR_BIT; server_bit_idx++) {
            uint8_t server_bit = (out >> server_bit_idx) & 1;

            /*
             * OR `factor` bits of the server bitmap with the same bit at
             * index `client_bit_idx` in the client bitmap.
             */
            bitmap[client_bit_idx / CHAR_BIT] |=
                server_bit << (client_bit_idx % CHAR_BIT);

            /*
             * Only move onto the next bit in the client bitmap once we've
             * OR'd `factor` bits.
             */
            if (((server_byte_idx * CHAR_BIT) + server_bit_idx) % factor
                    == factor - 1) {
                client_bit_idx++;
                
                if (client_bit_idx / CHAR_BIT >= client_bitmap_size) {
                    return;
                }
            }
        }
    }
}

int
dma_controller_dirty_page_get(dma_controller_t *dma, vfu_dma_addr_t addr,
                              uint64_t len, size_t client_pgsize, size_t size,
                              char *bitmap)
{
    dma_memory_region_t *region;
    ssize_t server_bitmap_size;
    ssize_t client_bitmap_size;
    dma_sg_t sg;
    int ret;

    assert(dma != NULL);
    assert(bitmap != NULL);

    /*
     * FIXME for now we support IOVAs that match exactly the DMA region. This
     * is purely for simplifying the implementation. We MUST allow arbitrary
     * IOVAs.
     */
    ret = dma_addr_to_sgl(dma, addr, VFIO_USER_PASID_INVALID, len, &sg, 1,
                          PROT_NONE);
    if (unlikely(ret != 1)) {
        vfu_log(dma->vfu_ctx, LOG_DEBUG, "failed to translate %#llx-%#llx: %m",
                (unsigned long long)(uintptr_t)addr,
		(unsigned long long)(uintptr_t)addr + len - 1);
        return ret;
    }

    if (unlikely(sg.dma_addr != addr || sg.length != len)) {
        return ERROR_INT(ENOTSUP);
    }

    /*
     * If dirty page logging is not enabled, the requested page size is zero,
     * or the requested page size is not a power of two, return an error.
     */
    if (dma->dirty_pgsize == 0) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "dirty page logging not enabled");
        return ERROR_INT(EINVAL);
    }
    if (client_pgsize == 0 || (client_pgsize & (client_pgsize - 1)) != 0) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "bad client page size %zu",
                client_pgsize);
        return ERROR_INT(EINVAL);
    }

    server_bitmap_size = get_bitmap_size(len, dma->dirty_pgsize);
    if (server_bitmap_size < 0) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "failed to get server bitmap size");
        return server_bitmap_size;
    }

    client_bitmap_size = get_bitmap_size(len, client_pgsize);
    if (client_bitmap_size < 0) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "bad client page size %zu",
                client_pgsize);
        return client_bitmap_size;
    }

    /*
     * They must be equal because this is how much data the client expects to
     * receive.
     */
    if (size != (size_t)client_bitmap_size) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "bad client bitmap size %zu != %zu",
                size, client_bitmap_size);
        return ERROR_INT(EINVAL);
    }

    region = &dma->regions[sg.region];

    if (region->fd == -1) {
        vfu_log(dma->vfu_ctx, LOG_ERR, "region %d is not mapped", sg.region);
        return ERROR_INT(EINVAL);
    }

    if (client_pgsize == dma->dirty_pgsize) {
        dirty_page_get_same_pgsize(region, bitmap, client_bitmap_size);
    } else if (client_pgsize < dma->dirty_pgsize) {
        /*
         * If the requested page size is less than that used for logging by
         * the server, the bitmap will need to be extended, repeating bits.
         */
        dirty_page_get_extend(region, bitmap, server_bitmap_size,
                              dma->dirty_pgsize, client_bitmap_size,
                              client_pgsize);
    } else {
        /*
         * If the requested page size is larger than that used for logging by
         * the server, the bitmap will need to combine bits with OR, losing
         * accuracy.
         */
        dirty_page_get_combine(region, bitmap, server_bitmap_size,
                               dma->dirty_pgsize, client_bitmap_size,
                               client_pgsize);
    }

#ifdef DEBUG
    log_dirty_bitmap(dma->vfu_ctx, region, bitmap, size, client_pgsize);
#endif

    return 0;
}

/* ex: set tabstop=4 shiftwidth=4 softtabstop=4 expandtab: */
