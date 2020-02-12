/*
     This file is part of libmicrohttpd
     Copyright (C) 2007--2019 Daniel Pittman, Christian Grothoff and
     Karlson2k (Evgeny Grin)

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file memorypool.c
 * @brief memory pool
 * @author Christian Grothoff
 * @author Karlson2k (Evgeny Grin)
 */
#include "memorypool.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mhd_assert.h"
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef HAVE_SYSCONF
#include <unistd.h>
#if defined(_SC_PAGE_SIZE)
#define MHD_SC_PAGESIZE _SC_PAGE_SIZE
#elif defined(_SC_PAGESIZE)
#define MHD_SC_PAGESIZE _SC_PAGESIZE
#endif /* _SC_PAGESIZE */
#endif /* HAVE_SYSCONF */

/* define MAP_ANONYMOUS for Mac OS X */
#if defined(MAP_ANON) && ! defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(_WIN32)
#define MAP_FAILED NULL
#elif ! defined(MAP_FAILED)
#define MAP_FAILED ((void*) -1)
#endif

/**
 * Align to 2x word size (as GNU libc does).
 */
#define ALIGN_SIZE (2 * sizeof(void*))

/**
 * Round up 'n' to a multiple of ALIGN_SIZE.
 */
#define ROUND_TO_ALIGN(n) (((n) + (ALIGN_SIZE - 1)) \
                           / (ALIGN_SIZE) *(ALIGN_SIZE))

#if defined(PAGE_SIZE)
#define MHD_DEF_PAGE_SIZE_ PAGE_SIZE
#elif defined(PAGESIZE)
#define MHD_DEF_PAGE_SIZE_ PAGE_SIZE
#else  /* ! PAGESIZE */
#define MHD_DEF_PAGE_SIZE_ (4096)
#endif /* ! PAGESIZE */

/**
 * Size of memory page
 */
static size_t MHD_sys_page_size_ = MHD_DEF_PAGE_SIZE_; /* Default fallback value */

/**
 * Initialise values for memory pools
 */
void
MHD_init_mem_pools_ (void)
{
#ifdef MHD_SC_PAGESIZE
  long result;
  result = sysconf (MHD_SC_PAGESIZE);
  if (-1 != result)
    MHD_sys_page_size_ = (size_t) result;
  else
    MHD_sys_page_size_ = MHD_DEF_PAGE_SIZE_;
#elif defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo (&si);
  MHD_sys_page_size_ = (size_t) si.dwPageSize;
#else
  MHD_sys_page_size_ = MHD_DEF_PAGE_SIZE_;
#endif /* _WIN32 */
}


/**
 * Handle for a memory pool.  Pools are not reentrant and must not be
 * used by multiple threads.
 */
struct MemoryPool
{

  /**
   * Pointer to the pool's memory
   */
  uint8_t *memory;

  /**
   * Size of the pool.
   */
  size_t size;

  /**
   * Offset of the first unallocated byte.
   */
  size_t pos;

  /**
   * Offset of the byte after the last unallocated byte.
   */
  size_t end;

  /**
   * 'false' if pool was malloc'ed, 'true' if mmapped (VirtualAlloc'ed for W32).
   */
  bool is_mmap;
};


/**
 * Create a memory pool.
 *
 * @param max maximum size of the pool
 * @return NULL on error
 */
struct MemoryPool *
MHD_pool_create (size_t max)
{
  struct MemoryPool *pool;
  size_t alloc_size;

  pool = malloc (sizeof (struct MemoryPool));
  if (NULL == pool)
    return NULL;
#if defined(MAP_ANONYMOUS) || defined(_WIN32)
  if ( (max <= 32 * 1024) ||
       (max < MHD_sys_page_size_ * 4 / 3) )
  {
    pool->memory = MAP_FAILED;
  }
  else
  {
    /* Round up allocation to page granularity. */
    alloc_size = max + MHD_sys_page_size_ - 1;
    alloc_size -= alloc_size % MHD_sys_page_size_;
#if defined(MAP_ANONYMOUS) && ! defined(_WIN32)
    pool->memory = mmap (NULL,
                         alloc_size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);
#elif defined(_WIN32)
    pool->memory = VirtualAlloc (NULL,
                                 alloc_size,
                                 MEM_COMMIT | MEM_RESERVE,
                                 PAGE_READWRITE);
#endif /* _WIN32 */
  }
#else  /* ! _WIN32 && ! MAP_ANONYMOUS */
  pool->memory = MAP_FAILED;
#endif /* ! _WIN32 && ! MAP_ANONYMOUS */
  if (MAP_FAILED == pool->memory)
  {
    alloc_size = ROUND_TO_ALIGN (max);
    pool->memory = malloc (alloc_size);
    if (NULL == pool->memory)
    {
      free (pool);
      return NULL;
    }
    pool->is_mmap = false;
  }
#if defined(MAP_ANONYMOUS) || defined(_WIN32)
  else
  {
    pool->is_mmap = true;
  }
#endif /* _WIN32 || MAP_ANONYMOUS */
  pool->pos = 0;
  pool->end = alloc_size;
  pool->size = alloc_size;
  return pool;
}


/**
 * Destroy a memory pool.
 *
 * @param pool memory pool to destroy
 */
void
MHD_pool_destroy (struct MemoryPool *pool)
{
  if (NULL == pool)
    return;

  mhd_assert (pool->end >= pool->pos);
  mhd_assert (pool->size >= pool->end - pool->pos);
  if (! pool->is_mmap)
    free (pool->memory);
  else
#if defined(MAP_ANONYMOUS) && ! defined(_WIN32)
    munmap (pool->memory,
            pool->size);
#elif defined(_WIN32)
    VirtualFree (pool->memory,
                 0,
                 MEM_RELEASE);
#else
    abort ();
#endif
  free (pool);
}


/**
 * Check how much memory is left in the @a pool
 *
 * @param pool pool to check
 * @return number of bytes still available in @a pool
 */
size_t
MHD_pool_get_free (struct MemoryPool *pool)
{
  mhd_assert (pool->end >= pool->pos);
  mhd_assert (pool->size >= pool->end - pool->pos);
  return (pool->end - pool->pos);
}


/**
 * Allocate size bytes from the pool.
 *
 * @param pool memory pool to use for the operation
 * @param size number of bytes to allocate
 * @param from_end allocate from end of pool (set to 'true');
 *        use this for small, persistent allocations that
 *        will never be reallocated
 * @return NULL if the pool cannot support size more
 *         bytes
 */
void *
MHD_pool_allocate (struct MemoryPool *pool,
                   size_t size,
                   bool from_end)
{
  void *ret;
  size_t asize;

  mhd_assert (pool->end >= pool->pos);
  mhd_assert (pool->size >= pool->end - pool->pos);
  asize = ROUND_TO_ALIGN (size);
  if ( (0 == asize) && (0 != size) )
    return NULL; /* size too close to SIZE_MAX */
  if ( (pool->pos + asize > pool->end) ||
       (pool->pos + asize < pool->pos))
    return NULL;
  if (from_end)
  {
    ret = &pool->memory[pool->end - asize];
    pool->end -= asize;
  }
  else
  {
    ret = &pool->memory[pool->pos];
    pool->pos += asize;
  }
  return ret;
}


/**
 * Reallocate a block of memory obtained from the pool.
 * This is particularly efficient when growing or
 * shrinking the block that was last (re)allocated.
 * If the given block is not the most recently
 * (re)allocated block, the memory of the previous
 * allocation may be leaked until the pool is
 * destroyed or reset.
 *
 * @param pool memory pool to use for the operation
 * @param old the existing block
 * @param old_size the size of the existing block
 * @param new_size the new size of the block
 * @return new address of the block, or
 *         NULL if the pool cannot support @a new_size
 *         bytes (old continues to be valid for @a old_size)
 */
void *
MHD_pool_reallocate (struct MemoryPool *pool,
                     void *old,
                     size_t old_size,
                     size_t new_size)
{
  size_t asize;
  uint8_t *new_blc;

  mhd_assert (pool->end >= pool->pos);
  mhd_assert (pool->size >= pool->end - pool->pos);
  mhd_assert (old != NULL || old_size == 0);
  mhd_assert (old == NULL || pool->memory <= (uint8_t*) old);
  mhd_assert (old == NULL || pool->memory + pool->size >= (uint8_t*) old
              + old_size);
  /* Blocks "from the end" must not be reallocated */
  mhd_assert (old == NULL || pool->memory + pool->pos > (uint8_t*) old);

  if (0 != old_size)
  {   /* Need to save some data */
    const size_t old_offset = (uint8_t*) old - pool->memory;
    const bool shrinking = (old_size > new_size);
    /* Try resizing in-place */
    if (shrinking)
    {     /* Shrinking in-place, zero-out freed part */
      memset ((uint8_t*) old + new_size, 0, old_size - new_size);
    }
    if (pool->pos == ROUND_TO_ALIGN (old_offset + old_size))
    {     /* "old" block is the last allocated block */
      const size_t new_apos = ROUND_TO_ALIGN (old_offset + new_size);
      if (! shrinking)
      {       /* Grow in-place, check for enough space. */
        if ( (new_apos > pool->end) ||
             (new_apos < pool->pos) )       /* Value wrap */
          return NULL;       /* No space */
      }
      /* Resized in-place */
      pool->pos = new_apos;
      return old;
    }
    if (shrinking)
      return old;   /* Resized in-place, freed part remains allocated */
  }
  /* Need to allocate new block */
  asize = ROUND_TO_ALIGN (new_size);
  if ( ( (0 == asize) &&
         (0 != new_size) ) || /* Value wrap, too large new_size. */
       (asize > pool->end - pool->pos) ) /* Not enough space */
    return NULL;

  new_blc = pool->memory + pool->pos;
  pool->pos += asize;

  if (0 != old_size)
  {
    /* Move data to new block, old block remains allocated */
    memcpy (new_blc, old, old_size);
    /* Zero-out old block */
    memset (old, 0, old_size);
  }
  return new_blc;
}


/**
 * Clear all entries from the memory pool except
 * for @a keep of the given @a size. The pointer
 * returned should be a buffer of @a new_size where
 * the first @a copy_bytes are from @a keep.
 *
 * @param pool memory pool to use for the operation
 * @param keep pointer to the entry to keep (maybe NULL)
 * @param copy_bytes how many bytes need to be kept at this address
 * @param new_size how many bytes should the allocation we return have?
 *                 (should be larger or equal to @a copy_bytes)
 * @return addr new address of @a keep (if it had to change)
 */
void *
MHD_pool_reset (struct MemoryPool *pool,
                void *keep,
                size_t copy_bytes,
                size_t new_size)
{
  mhd_assert (pool->end >= pool->pos);
  mhd_assert (pool->size >= pool->end - pool->pos);
  mhd_assert (copy_bytes < new_size);
  mhd_assert (keep != NULL || copy_bytes == 0);
  mhd_assert (keep == NULL || pool->memory <= (uint8_t*) keep);
  mhd_assert (keep == NULL || pool->memory + pool->size >= (uint8_t*) keep
              + copy_bytes);
  if ( (NULL != keep) &&
       (keep != pool->memory) )
  {
    if (0 != copy_bytes)
      memmove (pool->memory,
               keep,
               copy_bytes);
  }
  /* technically not needed, but safer to zero out */
  if (pool->size > copy_bytes)
  {
    size_t to_zero;   /** Size of area to zero-out */

    to_zero = pool->size - copy_bytes;
#ifdef _WIN32
    if (pool->is_mmap)
    {
      size_t to_recommit;     /** Size of decommitted and re-committed area. */
      uint8_t *recommit_addr;
      /* Round down to page size */
      to_recommit = to_zero - to_zero % MHD_sys_page_size_;
      recommit_addr = pool->memory + pool->size - to_recommit;

      /* De-committing and re-committing again clear memory and make
       * pages free / available for other needs until accessed. */
      if (VirtualFree (recommit_addr,
                       to_recommit,
                       MEM_DECOMMIT))
      {
        to_zero -= to_recommit;

        if (recommit_addr != VirtualAlloc (recommit_addr,
                                           to_recommit,
                                           MEM_COMMIT,
                                           PAGE_READWRITE))
          abort ();      /* Serious error, must never happen */
      }
    }
#endif /* _WIN32 */
    memset (&pool->memory[copy_bytes],
            0,
            to_zero);
  }
  pool->pos = ROUND_TO_ALIGN (new_size);
  pool->end = pool->size;
  return pool->memory;
}


/* end of memorypool.c */
