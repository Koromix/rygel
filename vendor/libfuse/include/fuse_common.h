/*  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

/** @file */

#include <stdbool.h>
#if !defined(FUSE_H_) && !defined(FUSE_LOWLEVEL_H_)
#error "Never include <fuse_common.h> directly; use <fuse.h> or <fuse_lowlevel.h> instead."
#endif

#ifndef FUSE_COMMON_H_
#define FUSE_COMMON_H_

#ifdef HAVE_LIBFUSE_PRIVATE_CONFIG_H
#include "fuse_config.h"
#endif

#include "libfuse_config.h"

#include "fuse_opt.h"
#include "fuse_log.h"
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>

#define FUSE_MAKE_VERSION(maj, min)  ((maj) * 100 + (min))
#define FUSE_VERSION FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#ifdef HAVE_STATIC_ASSERT
#define fuse_static_assert(condition, message) static_assert(condition, message)
#else
#define fuse_static_assert(condition, message)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Information about an open file.
 *
 * File Handles are created by the open, opendir, and create methods and closed
 * by the release and releasedir methods.  Multiple file handles may be
 * concurrently open for the same file.  Generally, a client will create one
 * file handle per file descriptor, though in some cases multiple file
 * descriptors can share a single file handle.
 *
 * Note: This data structure is ABI sensitive, new parameters have to be
 *       added within padding/padding2 bits and always below existing
 *       parameters.
 */
struct fuse_file_info {
	/** Open flags.	 Available in open(), release() and create() */
	int32_t flags;

	/** In case of a write operation indicates if this was caused
	    by a delayed write from the page cache. If so, then the
	    context's pid, uid, and gid fields will not be valid, and
	    the *fh* value may not match the *fh* value that would
	    have been sent with the corresponding individual write
	    requests if write caching had been disabled. */
	uint32_t writepage : 1;

	/** Can be filled in by open/create, to use direct I/O on this file. */
	uint32_t direct_io : 1;

	/** Can be filled in by open and opendir. It signals the kernel that any
	    currently cached data (ie., data that the filesystem provided the
	    last time the file/directory was open) need not be invalidated when
	    the file/directory is closed. */
	uint32_t keep_cache : 1;

	/** Indicates a flush operation.  Set in flush operation, also
	    maybe set in highlevel lock operation and lowlevel release
	    operation. */
	uint32_t flush : 1;

	/** Can be filled in by open, to indicate that the file is not
	    seekable. */
	uint32_t nonseekable : 1;

	/* Indicates that flock locks for this file should be
	   released.  If set, lock_owner shall contain a valid value.
	   May only be set in ->release(). */
	uint32_t flock_release : 1;

	/** Can be filled in by opendir. It signals the kernel to
	    enable caching of entries returned by readdir().  Has no
	    effect when set in other contexts (in particular it does
	    nothing when set by open()). */
	uint32_t cache_readdir : 1;

	/** Can be filled in by open, to indicate that flush is not needed
	    on close. */
	uint32_t noflush : 1;

	/** Can be filled by open/create, to allow parallel direct writes on this
	    file */
	uint32_t parallel_direct_writes : 1;

	/** Padding.  Reserved for future use*/
	uint32_t padding : 23;
	uint32_t padding2 : 32;
	uint32_t padding3 : 32;

	/** File handle id.  May be filled in by filesystem in create,
	 * open, and opendir().  Available in most other file operations on the
	 * same file handle. */
	uint64_t fh;

	/** Lock owner id.  Available in locking operations and flush */
	uint64_t lock_owner;

	/** Requested poll events.  Available in ->poll.  Only set on kernels
	    which support it.  If unsupported, this field is set to zero. */
	uint32_t poll_events;

	/** Passthrough backing file id.  May be filled in by filesystem in
	 * create and open.  It is used to create a passthrough connection
	 * between FUSE file and backing file. */
	int32_t backing_id;

	/** struct fuse_file_info api and abi flags  */
	uint64_t compat_flags;

	uint64_t reserved[2];
};
fuse_static_assert(sizeof(struct fuse_file_info) == 64,
		   "fuse_file_info size mismatch");

/**
 * Configuration parameters passed to fuse_session_loop_mt() and
 * fuse_loop_mt().
 * For FUSE API versions less than 312, use a public struct
 * fuse_loop_config in applications and struct fuse_loop_config_v1
 * is used in library (i.e., libfuse.so). These two structs are binary
 * compatible in earlier API versions and can be linked.
 * Deprecated and replaced by a newer private struct in FUSE API
 * version 312 (FUSE_MAKE_VERSION(3, 12)).
 */
#if FUSE_USE_VERSION < FUSE_MAKE_VERSION(3, 12)
struct fuse_loop_config_v1; /* forward declaration */
struct fuse_loop_config {
#else
struct fuse_loop_config_v1 {
#endif
	/**
	 * whether to use separate device fds for each thread
	 * (may increase performance)
	 */
	int clone_fd;

	/**
	 * The maximum number of available worker threads before they
	 * start to get deleted when they become idle. If not
	 * specified, the default is 10.
	 *
	 * Adjusting this has performance implications; a very small number
	 * of threads in the pool will cause a lot of thread creation and
	 * deletion overhead and performance may suffer. When set to 0, a new
	 * thread will be created to service every operation.
	 */
	unsigned int max_idle_threads;
};


/**************************************************************************
 * Capability bits for 'fuse_conn_info.capable' and 'fuse_conn_info.want' *
 **************************************************************************/

/**
 * Indicates that the filesystem supports asynchronous read requests.
 *
 * If this capability is not requested/available, the kernel will
 * ensure that there is at most one pending read request per
 * file-handle at any time, and will attempt to order read requests by
 * increasing offset.
 *
 * This feature is enabled by default when supported by the kernel.
 */
#define FUSE_CAP_ASYNC_READ (1 << 0)

/**
 * Indicates that the filesystem supports "remote" locking.
 *
 * This feature is enabled by default when supported by the kernel,
 * and if getlk() and setlk() handlers are implemented.
 */
#define FUSE_CAP_POSIX_LOCKS (1 << 1)

/**
 * Indicates that the filesystem supports the O_TRUNC open flag.  If
 * disabled, and an application specifies O_TRUNC, fuse first calls
 * truncate() and then open() with O_TRUNC filtered out.
 *
 * This feature is enabled by default when supported by the kernel.
 */
#define FUSE_CAP_ATOMIC_O_TRUNC (1 << 3)

/**
 * Indicates that the filesystem supports lookups of "." and "..".
 *
 * When this flag is set, the filesystem must be prepared to receive requests
 * for invalid inodes (i.e., for which a FORGET request was received or
 * which have been used in a previous instance of the filesystem daemon) and
 * must not reuse node-ids (even when setting generation numbers).
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_EXPORT_SUPPORT (1 << 4)

/**
 * Indicates that the kernel should not apply the umask to the
 * file mode on create operations.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_DONT_MASK (1 << 6)

/**
 * Indicates that libfuse should try to use splice() when writing to
 * the fuse device. This may improve performance.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_SPLICE_WRITE (1 << 7)

/**
 * Indicates that libfuse should try to move pages instead of copying when
 * writing to / reading from the fuse device. This may improve performance.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_SPLICE_MOVE (1 << 8)

/**
 * Indicates that libfuse should try to use splice() when reading from
 * the fuse device. This may improve performance.
 *
 * This feature is enabled by default when supported by the kernel and
 * if the filesystem implements a write_buf() handler.
 */
#define FUSE_CAP_SPLICE_READ (1 << 9)

/**
 * If set, the calls to flock(2) will be emulated using POSIX locks and must
 * then be handled by the filesystem's setlock() handler.
 *
 * If not set, flock(2) calls will be handled by the FUSE kernel module
 * internally (so any access that does not go through the kernel cannot be taken
 * into account).
 *
 * This feature is enabled by default when supported by the kernel and
 * if the filesystem implements a flock() handler.
 */
#define FUSE_CAP_FLOCK_LOCKS (1 << 10)

/**
 * Indicates that the filesystem supports ioctl's on directories.
 *
 * This feature is enabled by default when supported by the kernel.
 */
#define FUSE_CAP_IOCTL_DIR (1 << 11)

/**
 * Traditionally, while a file is open the FUSE kernel module only
 * asks the filesystem for an update of the file's attributes when a
 * client attempts to read beyond EOF. This is unsuitable for
 * e.g. network filesystems, where the file contents may change
 * without the kernel knowing about it.
 *
 * If this flag is set, FUSE will check the validity of the attributes
 * on every read. If the attributes are no longer valid (i.e., if the
 * *attr_timeout* passed to fuse_reply_attr() or set in `struct
 * fuse_entry_param` has passed), it will first issue a `getattr`
 * request. If the new mtime differs from the previous value, any
 * cached file *contents* will be invalidated as well.
 *
 * This flag should always be set when available. If all file changes
 * go through the kernel, *attr_timeout* should be set to a very large
 * number to avoid unnecessary getattr() calls.
 *
 * This feature is enabled by default when supported by the kernel.
 */
#define FUSE_CAP_AUTO_INVAL_DATA (1 << 12)

/**
 * Indicates that the filesystem supports readdirplus.
 *
 * This feature is enabled by default when supported by the kernel and if the
 * filesystem implements a readdirplus() handler.
 */
#define FUSE_CAP_READDIRPLUS (1 << 13)

/**
 * Indicates that the filesystem supports adaptive readdirplus.
 *
 * If FUSE_CAP_READDIRPLUS is not set, this flag has no effect.
 *
 * If FUSE_CAP_READDIRPLUS is set and this flag is not set, the kernel
 * will always issue readdirplus() requests to retrieve directory
 * contents.
 *
 * If FUSE_CAP_READDIRPLUS is set and this flag is set, the kernel
 * will issue both readdir() and readdirplus() requests, depending on
 * how much information is expected to be required.
 *
 * As of Linux 4.20, the algorithm is as follows: when userspace
 * starts to read directory entries, issue a READDIRPLUS request to
 * the filesystem. If any entry attributes have been looked up by the
 * time userspace requests the next batch of entries continue with
 * READDIRPLUS, otherwise switch to plain READDIR.  This will reasult
 * in eg plain "ls" triggering READDIRPLUS first then READDIR after
 * that because it doesn't do lookups.  "ls -l" should result in all
 * READDIRPLUS, except if dentries are already cached.
 *
 * This feature is enabled by default when supported by the kernel and
 * if the filesystem implements both a readdirplus() and a readdir()
 * handler.
 */
#define FUSE_CAP_READDIRPLUS_AUTO (1 << 14)

/**
 * Indicates that the filesystem supports asynchronous direct I/O submission.
 *
 * If this capability is not requested/available, the kernel will ensure that
 * there is at most one pending read and one pending write request per direct
 * I/O file-handle at any time.
 *
 * This feature is enabled by default when supported by the kernel.
 */
#define FUSE_CAP_ASYNC_DIO (1 << 15)

/**
 * Indicates that writeback caching should be enabled. This means that
 * individual write request may be buffered and merged in the kernel
 * before they are send to the filesystem.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_WRITEBACK_CACHE (1 << 16)

/**
 * Indicates support for zero-message opens. If this flag is set in
 * the `capable` field of the `fuse_conn_info` structure, then the
 * filesystem may return `ENOSYS` from the open() handler to indicate
 * success. Further attempts to open files will be handled in the
 * kernel. (If this flag is not set, returning ENOSYS will be treated
 * as an error and signaled to the caller).
 *
 * Setting this flag in the `want` field enables this behavior automatically
 * within libfuse for low level API users. If non-low level users wish to have
 * this behavior you must return `ENOSYS` from the open() handler on supporting
 * kernels.
 */
#define FUSE_CAP_NO_OPEN_SUPPORT (1 << 17)

/**
 * Indicates support for parallel directory operations. If this flag
 * is unset, the FUSE kernel module will ensure that lookup() and
 * readdir() requests are never issued concurrently for the same
 * directory.
 */
#define FUSE_CAP_PARALLEL_DIROPS (1 << 18)

/**
 * Indicates support for POSIX ACLs.
 *
 * If this feature is enabled, the kernel will cache and have
 * responsibility for enforcing ACLs. ACL will be stored as xattrs and
 * passed to userspace, which is responsible for updating the ACLs in
 * the filesystem, keeping the file mode in sync with the ACL, and
 * ensuring inheritance of default ACLs when new filesystem nodes are
 * created. Note that this requires that the file system is able to
 * parse and interpret the xattr representation of ACLs.
 *
 * Enabling this feature implicitly turns on the
 * ``default_permissions`` mount option (even if it was not passed to
 * mount(2)).
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_POSIX_ACL (1 << 19)

/**
 * Indicates that the filesystem is responsible for unsetting
 * setuid and setgid bits when a file is written, truncated, or
 * its owner is changed.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_HANDLE_KILLPRIV (1 << 20)

/**
 * Indicates that the filesystem is responsible for unsetting
 * setuid and setgid bit and additionally cap (stored as xattr) when a
 * file is written, truncated, or its owner is changed.
 * Upon write/truncate suid/sgid is only killed if caller
 * does not have CAP_FSETID. Additionally upon
 * write/truncate sgid is killed only if file has group
 * execute permission. (Same as Linux VFS behavior).
 * KILLPRIV_V2 requires handling of
 *   - FUSE_OPEN_KILL_SUIDGID (set in struct fuse_create_in::open_flags)
 *   - FATTR_KILL_SUIDGID (set in struct fuse_setattr_in::valid)
 *   - FUSE_WRITE_KILL_SUIDGID (set in struct fuse_write_in::write_flags)
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_HANDLE_KILLPRIV_V2 (1 << 21)

/**
 * Indicates that the kernel supports caching symlinks in its page cache.
 *
 * When this feature is enabled, symlink targets are saved in the page cache.
 * You can invalidate a cached link by calling:
 * `fuse_lowlevel_notify_inval_inode(se, ino, 0, 0);`
 *
 * This feature is disabled by default.
 * If the kernel supports it (>= 4.20), you can enable this feature by
 * setting this flag in the `want` field of the `fuse_conn_info` structure.
 */
#define FUSE_CAP_CACHE_SYMLINKS (1 << 23)

/**
 * Indicates support for zero-message opendirs. If this flag is set in
 * the `capable` field of the `fuse_conn_info` structure, then the filesystem
 * may return `ENOSYS` from the opendir() handler to indicate success. Further
 * opendir and releasedir messages will be handled in the kernel. (If this
 * flag is not set, returning ENOSYS will be treated as an error and signalled
 * to the caller.)
 *
 * Setting this flag in the `want` field enables this behavior automatically
 * within libfuse for low level API users.  If non-low level users with to have
 * this behavior you must return `ENOSYS` from the opendir() handler on
 * supporting kernels.
 */
#define FUSE_CAP_NO_OPENDIR_SUPPORT (1 << 24)

/**
 * Indicates support for invalidating cached pages only on explicit request.
 *
 * If this flag is set in the `capable` field of the `fuse_conn_info` structure,
 * then the FUSE kernel module supports invalidating cached pages only on
 * explicit request by the filesystem through fuse_lowlevel_notify_inval_inode()
 * or fuse_invalidate_path().
 *
 * By setting this flag in the `want` field of the `fuse_conn_info` structure,
 * the filesystem is responsible for invalidating cached pages through explicit
 * requests to the kernel.
 *
 * Note that setting this flag does not prevent the cached pages from being
 * flushed by OS itself and/or through user actions.
 *
 * Note that if both FUSE_CAP_EXPLICIT_INVAL_DATA and FUSE_CAP_AUTO_INVAL_DATA
 * are set in the `capable` field of the `fuse_conn_info` structure then
 * FUSE_CAP_AUTO_INVAL_DATA takes precedence.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_EXPLICIT_INVAL_DATA (1 << 25)

/**
 * Indicates support that dentries can be expired.
 *
 * Expiring dentries, instead of invalidating them, makes a difference for
 * overmounted dentries, where plain invalidation would detach all submounts
 * before dropping the dentry from the cache. If only expiry is set on the
 * dentry, then any overmounts are left alone and until ->d_revalidate()
 * is called.
 *
 * Note: ->d_revalidate() is not called for the case of following a submount,
 * so invalidation will only be triggered for the non-overmounted case.
 * The dentry could also be mounted in a different mount instance, in which case
 * any submounts will still be detached.
 */
#define FUSE_CAP_EXPIRE_ONLY (1 << 26)

/**
 * Indicates that an extended 'struct fuse_setxattr' is used by the kernel
 * side - extra_flags are passed, which are used (as of now by acl) processing.
 * For example FUSE_SETXATTR_ACL_KILL_SGID might be set.
 */
#define FUSE_CAP_SETXATTR_EXT (1 << 27)

/**
 * Files opened with FUSE_DIRECT_IO do not support MAP_SHARED mmap. This restriction
 * is relaxed through FUSE_CAP_DIRECT_IO_RELAX (kernel flag: FUSE_DIRECT_IO_RELAX).
 * MAP_SHARED is disabled by default for FUSE_DIRECT_IO, as this flag can be used to
 * ensure coherency between mount points (or network clients) and with kernel page
 * cache as enforced by mmap that cannot be guaranteed anymore.
 */
#define FUSE_CAP_DIRECT_IO_ALLOW_MMAP (1 << 28)

/**
 * Indicates support for passthrough mode access for read/write operations.
 *
 * If this flag is set in the `capable` field of the `fuse_conn_info`
 * structure, then the FUSE kernel module supports redirecting read/write
 * operations to the backing file instead of letting them to be handled
 * by the FUSE daemon.
 *
 * This feature is disabled by default.
 */
#define FUSE_CAP_PASSTHROUGH (1 << 29)

/**
 * Indicates that the file system cannot handle NFS export
 *
 * If this flag is set NFS export and name_to_handle_at
 * is not going to work at all and will fail with EOPNOTSUPP.
 */
#define FUSE_CAP_NO_EXPORT_SUPPORT (1 << 30)

/**
 * Ioctl flags
 *
 * FUSE_IOCTL_COMPAT: 32bit compat ioctl on 64bit machine
 * FUSE_IOCTL_UNRESTRICTED: not restricted to well-formed ioctls, retry allowed
 * FUSE_IOCTL_RETRY: retry with new iovecs
 * FUSE_IOCTL_DIR: is a directory
 *
 * FUSE_IOCTL_MAX_IOV: maximum of in_iovecs + out_iovecs
 */
#define FUSE_IOCTL_COMPAT	(1 << 0)
#define FUSE_IOCTL_UNRESTRICTED	(1 << 1)
#define FUSE_IOCTL_RETRY	(1 << 2)
#define FUSE_IOCTL_DIR		(1 << 4)

#define FUSE_IOCTL_MAX_IOV	256

/**
 * Connection information, passed to the ->init() method
 *
 * Some of the elements are read-write, these can be changed to
 * indicate the value requested by the filesystem.  The requested
 * value must usually be smaller than the indicated value.
 *
 * Note: The `capable` and `want` fields are limited to 32 bits for
 * ABI compatibility. For full 64-bit capability support, use the
 * `capable_ext` and `want_ext` fields instead.
 */
struct fuse_conn_info {
	/**
	 * Major version of the protocol (read-only)
	 */
	uint32_t proto_major;

	/**
	 * Minor version of the protocol (read-only)
	 */
	uint32_t proto_minor;

	/**
	 * Maximum size of the write buffer
	 */
	uint32_t max_write;

	/**
	 * Maximum size of read requests. A value of zero indicates no
	 * limit. However, even if the filesystem does not specify a
	 * limit, the maximum size of read requests will still be
	 * limited by the kernel.
	 *
	 * NOTE: For the time being, the maximum size of read requests
	 * must be set both here *and* passed to fuse_session_new()
	 * using the ``-o max_read=<n>`` mount option. At some point
	 * in the future, specifying the mount option will no longer
	 * be necessary.
	 */
	uint32_t max_read;

	/**
	 * Maximum readahead
	 */
	uint32_t max_readahead;

	/**
	 * Capability flags that the kernel supports (read-only)
	 *
	 * Deprecated left over for ABI compatibility, use capable_ext
	 */
	uint32_t capable;

	/**
	 * Capability flags that the filesystem wants to enable.
	 *
	 * libfuse attempts to initialize this field with
	 * reasonable default values before calling the init() handler.
	 *
	 * Deprecated left over for ABI compatibility.
	 * Use want_ext with the helper functions
	 * fuse_set_feature_flag() / fuse_unset_feature_flag()
	 */
	uint32_t want;

	/**
	 * Maximum number of pending "background" requests. A
	 * background request is any type of request for which the
	 * total number is not limited by other means. As of kernel
	 * 4.8, only two types of requests fall into this category:
	 *
	 *   1. Read-ahead requests
	 *   2. Asynchronous direct I/O requests
	 *
	 * Read-ahead requests are generated (if max_readahead is
	 * non-zero) by the kernel to preemptively fill its caches
	 * when it anticipates that userspace will soon read more
	 * data.
	 *
	 * Asynchronous direct I/O requests are generated if
	 * FUSE_CAP_ASYNC_DIO is enabled and userspace submits a large
	 * direct I/O request. In this case the kernel will internally
	 * split it up into multiple smaller requests and submit them
	 * to the filesystem concurrently.
	 *
	 * Note that the following requests are *not* background
	 * requests: writeback requests (limited by the kernel's
	 * flusher algorithm), regular (i.e., synchronous and
	 * buffered) userspace read/write requests (limited to one per
	 * thread), asynchronous read requests (Linux's io_submit(2)
	 * call actually blocks, so these are also limited to one per
	 * thread).
	 */
	uint32_t max_background;

	/**
	 * Kernel congestion threshold parameter. If the number of pending
	 * background requests exceeds this number, the FUSE kernel module will
	 * mark the filesystem as "congested". This instructs the kernel to
	 * expect that queued requests will take some time to complete, and to
	 * adjust its algorithms accordingly (e.g. by putting a waiting thread
	 * to sleep instead of using a busy-loop).
	 */
	uint32_t congestion_threshold;

	/**
	 * When FUSE_CAP_WRITEBACK_CACHE is enabled, the kernel is responsible
	 * for updating mtime and ctime when write requests are received. The
	 * updated values are passed to the filesystem with setattr() requests.
	 * However, if the filesystem does not support the full resolution of
	 * the kernel timestamps (nanoseconds), the mtime and ctime values used
	 * by kernel and filesystem will differ (and result in an apparent
	 * change of times after a cache flush).
	 *
	 * To prevent this problem, this variable can be used to inform the
	 * kernel about the timestamp granularity supported by the file-system.
	 * The value should be power of 10.  The default is 1, i.e. full
	 * nano-second resolution. Filesystems supporting only second resolution
	 * should set this to 1000000000.
	 */
	uint32_t time_gran;

	/**
	 * When FUSE_CAP_PASSTHROUGH is enabled, this is the maximum allowed
	 * stacking depth of the backing files.  In current kernel, the maximum
	 * allowed stack depth if FILESYSTEM_MAX_STACK_DEPTH (2), which includes
	 * the FUSE passthrough layer, so the maximum stacking depth for backing
	 * files is 1.
	 *
	 * The default is FUSE_BACKING_STACKED_UNDER (0), meaning that the
	 * backing files cannot be on a stacked filesystem, but another stacked
	 * filesystem can be stacked over this FUSE passthrough filesystem.
	 *
	 * Set this to FUSE_BACKING_STACKED_OVER (1) if backing files may be on
	 * a stacked filesystem, such as overlayfs or another FUSE passthrough.
	 * In this configuration, another stacked filesystem cannot be stacked
	 * over this FUSE passthrough filesystem.
	 */
#define FUSE_BACKING_STACKED_UNDER	(0)
#define FUSE_BACKING_STACKED_OVER	(1)
	uint32_t max_backing_stack_depth;

	/**
	 * Disable FUSE_INTERRUPT requests.
	 *
	 * Enable `no_interrupt` option to:
	 * 1) Avoid unnecessary locking operations and list operations,
	 * 2) Return ENOSYS for the reply of FUSE_INTERRUPT request to
	 * inform the kernel not to send the FUSE_INTERRUPT request.
	 */
	uint32_t no_interrupt : 1;

	/* reserved bits for future use */
	uint32_t padding : 31;

	/**
	 * Extended capability flags that the kernel supports (read-only)
	 * This field provides full 64-bit capability support.
	 */
	uint64_t capable_ext;

	/**
	 * Extended capability flags that the filesystem wants to enable.
	 * This field provides full 64-bit capability support.
	 *
	 * Don't set this field directly, but use the helper functions
	 * fuse_set_feature_flag() / fuse_unset_feature_flag()
	 *
	 */
	uint64_t want_ext;

	/**
	 * For future use.
	 */
	uint32_t reserved[16];
};
fuse_static_assert(sizeof(struct fuse_conn_info) == 128,
		   "Size of struct fuse_conn_info must be 128 bytes");

struct fuse_session;
struct fuse_pollhandle;
struct fuse_conn_info_opts;

/**
 * This function parses several command-line options that can be used
 * to override elements of struct fuse_conn_info. The pointer returned
 * by this function should be passed to the
 * fuse_apply_conn_info_opts() method by the file system's init()
 * handler.
 *
 * Before using this function, think twice if you really want these
 * parameters to be adjustable from the command line. In most cases,
 * they should be determined by the file system internally.
 *
 * The following options are recognized:
 *
 *   -o max_write=N         sets conn->max_write
 *   -o max_readahead=N     sets conn->max_readahead
 *   -o max_background=N    sets conn->max_background
 *   -o congestion_threshold=N  sets conn->congestion_threshold
 *   -o async_read          sets FUSE_CAP_ASYNC_READ in conn->want
 *   -o sync_read           unsets FUSE_CAP_ASYNC_READ in conn->want
 *   -o atomic_o_trunc      sets FUSE_CAP_ATOMIC_O_TRUNC in conn->want
 *   -o no_remote_lock      Equivalent to -o no_remote_flock,no_remote_posix_lock
 *   -o no_remote_flock     Unsets FUSE_CAP_FLOCK_LOCKS in conn->want
 *   -o no_remote_posix_lock  Unsets FUSE_CAP_POSIX_LOCKS in conn->want
 *   -o [no_]splice_write     (un-)sets FUSE_CAP_SPLICE_WRITE in conn->want
 *   -o [no_]splice_move      (un-)sets FUSE_CAP_SPLICE_MOVE in conn->want
 *   -o [no_]splice_read      (un-)sets FUSE_CAP_SPLICE_READ in conn->want
 *   -o [no_]auto_inval_data  (un-)sets FUSE_CAP_AUTO_INVAL_DATA in conn->want
 *   -o readdirplus=no        unsets FUSE_CAP_READDIRPLUS in conn->want
 *   -o readdirplus=yes       sets FUSE_CAP_READDIRPLUS and unsets
 *                            FUSE_CAP_READDIRPLUS_AUTO in conn->want
 *   -o readdirplus=auto      sets FUSE_CAP_READDIRPLUS and
 *                            FUSE_CAP_READDIRPLUS_AUTO in conn->want
 *   -o [no_]async_dio        (un-)sets FUSE_CAP_ASYNC_DIO in conn->want
 *   -o [no_]writeback_cache  (un-)sets FUSE_CAP_WRITEBACK_CACHE in conn->want
 *   -o time_gran=N           sets conn->time_gran
 *
 * Known options will be removed from *args*, unknown options will be
 * passed through unchanged.
 *
 * @param args argument vector (input+output)
 * @return parsed options
 **/
struct fuse_conn_info_opts* fuse_parse_conn_info_opts(struct fuse_args *args);

/**
 * This function applies the (parsed) parameters in *opts* to the
 * *conn* pointer. It may modify the following fields: wants,
 * max_write, max_readahead, congestion_threshold, max_background,
 * time_gran. A field is only set (or unset) if the corresponding
 * option has been explicitly set.
 */
void fuse_apply_conn_info_opts(struct fuse_conn_info_opts *opts,
			  struct fuse_conn_info *conn);

/**
 * Go into the background
 *
 * @param foreground if true, stay in the foreground
 * @return 0 on success, -1 on failure
 */
int fuse_daemonize(int foreground);

/**
 * Get the version of the library
 *
 * @return the version
 */
int fuse_version(void);

/**
 * Get the full package version string of the library
 *
 * @return the package version
 */
const char *fuse_pkgversion(void);

/**
 * Destroy poll handle
 *
 * @param ph the poll handle
 */
void fuse_pollhandle_destroy(struct fuse_pollhandle *ph);

/* ----------------------------------------------------------- *
 * Data buffer						       *
 * ----------------------------------------------------------- */

/**
 * Buffer flags
 */
enum fuse_buf_flags {
	/**
	 * Buffer contains a file descriptor
	 *
	 * If this flag is set, the .fd field is valid, otherwise the
	 * .mem fields is valid.
	 */
	FUSE_BUF_IS_FD		= (1 << 1),

	/**
	 * Seek on the file descriptor
	 *
	 * If this flag is set then the .pos field is valid and is
	 * used to seek to the given offset before performing
	 * operation on file descriptor.
	 */
	FUSE_BUF_FD_SEEK	= (1 << 2),

	/**
	 * Retry operation on file descriptor
	 *
	 * If this flag is set then retry operation on file descriptor
	 * until .size bytes have been copied or an error or EOF is
	 * detected.
	 */
	FUSE_BUF_FD_RETRY	= (1 << 3)
};

/**
 * Buffer copy flags
 */
enum fuse_buf_copy_flags {
	/**
	 * Don't use splice(2)
	 *
	 * Always fall back to using read and write instead of
	 * splice(2) to copy data from one file descriptor to another.
	 *
	 * If this flag is not set, then only fall back if splice is
	 * unavailable.
	 */
	FUSE_BUF_NO_SPLICE	= (1 << 1),

	/**
	 * Force splice
	 *
	 * Always use splice(2) to copy data from one file descriptor
	 * to another.  If splice is not available, return -EINVAL.
	 */
	FUSE_BUF_FORCE_SPLICE	= (1 << 2),

	/**
	 * Try to move data with splice.
	 *
	 * If splice is used, try to move pages from the source to the
	 * destination instead of copying.  See documentation of
	 * SPLICE_F_MOVE in splice(2) man page.
	 */
	FUSE_BUF_SPLICE_MOVE	= (1 << 3),

	/**
	 * Don't block on the pipe when copying data with splice
	 *
	 * Makes the operations on the pipe non-blocking (if the pipe
	 * is full or empty).  See SPLICE_F_NONBLOCK in the splice(2)
	 * man page.
	 */
	FUSE_BUF_SPLICE_NONBLOCK= (1 << 4)
};

/**
 * Single data buffer
 *
 * Generic data buffer for I/O, extended attributes, etc...  Data may
 * be supplied as a memory pointer or as a file descriptor
 */
struct fuse_buf {
	/**
	 * Size of data in bytes
	 */
	size_t size;

	/**
	 * Buffer flags
	 */
	enum fuse_buf_flags flags;

	/**
	 * Memory pointer
	 *
	 * Used unless FUSE_BUF_IS_FD flag is set.
	 */
	void *mem;

	/**
	 * File descriptor
	 *
	 * Used if FUSE_BUF_IS_FD flag is set.
	 */
	int fd;

	/**
	 * File position
	 *
	 * Used if FUSE_BUF_FD_SEEK flag is set.
	 */
	off_t pos;

	/**
	 * Size of memory pointer
	 *
	 * Used only if mem was internally allocated.
	 * Not used if mem was user-provided.
	 */
	size_t mem_size;
};

/**
 * Data buffer vector
 *
 * An array of data buffers, each containing a memory pointer or a
 * file descriptor.
 *
 * Allocate dynamically to add more than one buffer.
 */
struct fuse_bufvec {
	/**
	 * Number of buffers in the array
	 */
	size_t count;

	/**
	 * Index of current buffer within the array
	 */
	size_t idx;

	/**
	 * Current offset within the current buffer
	 */
	size_t off;

	/**
	 * Array of buffers
	 */
	struct fuse_buf buf[1];
};

/**
 * libfuse version a file system was compiled with. Should be filled in from
 * defines in 'libfuse_config.h'
 */
struct libfuse_version
{
	uint32_t major;
	uint32_t minor;
	uint32_t hotfix;
	uint32_t padding;
};

/* Initialize bufvec with a single buffer of given size */
#define FUSE_BUFVEC_INIT(size__)				\
	((struct fuse_bufvec) {					\
		/* .count= */ 1,				\
		/* .idx =  */ 0,				\
		/* .off =  */ 0,				\
		/* .buf =  */ { /* [0] = */ {			\
			/* .size =  */ (size__),		\
			/* .flags = */ (enum fuse_buf_flags) 0,	\
			/* .mem =   */ NULL,			\
			/* .fd =    */ -1,			\
			/* .pos =   */ 0,			\
			/* .mem_size = */ 0,                    \
		} }						\
	} )

/**
 * Get total size of data in a fuse buffer vector
 *
 * @param bufv buffer vector
 * @return size of data
 */
size_t fuse_buf_size(const struct fuse_bufvec *bufv);

/**
 * Copy data from one buffer vector to another
 *
 * @param dst destination buffer vector
 * @param src source buffer vector
 * @param flags flags controlling the copy
 * @return actual number of bytes copied or -errno on error
 */
ssize_t fuse_buf_copy(struct fuse_bufvec *dst, struct fuse_bufvec *src,
		      enum fuse_buf_copy_flags flags);

/* ----------------------------------------------------------- *
 * Signal handling					       *
 * ----------------------------------------------------------- */

/**
 * Exit session on HUP, TERM and INT signals and ignore PIPE signal
 *
 * Stores session in a global variable.	 May only be called once per
 * process until fuse_remove_signal_handlers() is called.
 *
 * Once either of the POSIX signals arrives, the signal handler calls
 * fuse_session_exit().
 *
 * @param se the session to exit
 * @return 0 on success, -1 on failure
 *
 * See also:
 * fuse_remove_signal_handlers()
 */
int fuse_set_signal_handlers(struct fuse_session *se);

/**
 * Print a stack backtrace diagnostic on critical signals ()
 *
 * Stores session in a global variable.	 May only be called once per
 * process until fuse_remove_signal_handlers() is called.
 *
 * Once either of the POSIX signals arrives, the signal handler calls
 * fuse_session_exit().
 *
 * @param se the session to exit
 * @return 0 on success, -1 on failure
 *
 * See also:
 * fuse_remove_signal_handlers()
 */
int fuse_set_fail_signal_handlers(struct fuse_session *se);

/**
 * Restore default signal handlers
 *
 * Resets global session.  After this fuse_set_signal_handlers() may
 * be called again.
 *
 * @param se the same session as given in fuse_set_signal_handlers()
 *
 * See also:
 * fuse_set_signal_handlers()
 */
void fuse_remove_signal_handlers(struct fuse_session *se);

/**
 * Config operations.
 * These APIs are introduced in version 312 (FUSE_MAKE_VERSION(3, 12)).
 * Using them in earlier versions will result in errors.
 */
#if FUSE_USE_VERSION >= FUSE_MAKE_VERSION(3, 12)
/**
 * Create and set default config for fuse_session_loop_mt and fuse_loop_mt.
 *
 * @return anonymous config struct
 */
struct fuse_loop_config *fuse_loop_cfg_create(void);

/**
 * Free the config data structure
 */
void fuse_loop_cfg_destroy(struct fuse_loop_config *config);

/**
 * fuse_loop_config setter to set the number of max idle threads.
 */
void fuse_loop_cfg_set_idle_threads(struct fuse_loop_config *config,
				    unsigned int value);

/**
 * fuse_loop_config setter to set the number of max threads.
 */
void fuse_loop_cfg_set_max_threads(struct fuse_loop_config *config,
				   unsigned int value);

/**
 * fuse_loop_config setter to enable the clone_fd feature
 */
void fuse_loop_cfg_set_clone_fd(struct fuse_loop_config *config,
				unsigned int value);

/**
 * Convert old config to more recernt fuse_loop_config2
 *
 * @param config current config2 type
 * @param v1_conf older config1 type (below FUSE API 312)
 */
void fuse_loop_cfg_convert(struct fuse_loop_config *config,
			   struct fuse_loop_config_v1 *v1_conf);
#endif

/**
 * Set a feature flag in the want_ext field of fuse_conn_info.
 *
 * @param conn connection information
 * @param flag feature flag to be set
 * @return true if the flag was set, false if the flag is not supported
 */
bool fuse_set_feature_flag(struct fuse_conn_info *conn, uint64_t flag);

/**
 * Unset a feature flag in the want_ext field of fuse_conn_info.
 *
 * @param conn connection information
 * @param flag feature flag to be unset
 */
void fuse_unset_feature_flag(struct fuse_conn_info *conn, uint64_t flag);

/**
 * Get the value of a feature flag in the want_ext field of fuse_conn_info.
 *
 * @param conn connection information
 * @param flag feature flag to be checked
 * @return true if the flag is set, false otherwise
 */
bool fuse_get_feature_flag(struct fuse_conn_info *conn, uint64_t flag);

/*
 * DO NOT USE: Not part of public API, for internal test use only.
 * The function signature or any use of it is not guaranteeed to
 * remain stable. And neither are results of what this function does.
 */
int fuse_convert_to_conn_want_ext(struct fuse_conn_info *conn);



/* ----------------------------------------------------------- *
 * Compatibility stuff					       *
 * ----------------------------------------------------------- */

#if !defined(FUSE_USE_VERSION) || FUSE_USE_VERSION < 30
#  error only API version 30 or greater is supported
#endif

#ifdef __cplusplus
}
#endif


/*
 * This interface uses 64 bit off_t.
 *
 * On 32bit systems please add -D_FILE_OFFSET_BITS=64 to your compile flags!
 */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(off_t) == 8, "fuse: off_t must be 64bit");
#else
struct _fuse_off_t_must_be_64bit_dummy_struct \
	{ unsigned _fuse_off_t_must_be_64bit:((sizeof(off_t) == 8) ? 1 : -1); };
#endif

#endif /* FUSE_COMMON_H_ */
