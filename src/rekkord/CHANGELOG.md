# Changelog

## Alpha versions

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

### Rekkord 0.80

*Released on 2025-07-28*

- Support non-root Rekkord S3 repositories
- Automatically detect S3 conditional write support
- Properly handle PutObject/GetObject request restarts
- Default to simple clear without list in `rekkord reset_cache`

### Rekkord 0.79

*Released on 2025-07-15*

- Fix `rekkord mount` I/O error when listing empty directories
- Add *--reuse_key* option to `rekkord init`
- Add *--force* option to `rekkord save`
- Increase S3 and SFTP concurrency
- Fix ignored unknown option when passed before command
- Fix ignored key path when specified before init
- Change prefix character to reverse sort in `rekkord snapshots`
- Cache computed/stored file size instead of stat info
- Reorganize some options in rekkord help texts

### Rekkord 0.77

*Released on 2025-07-07*

- Fix possible NULL dereference in `rekkord delete_user`
- Remove user migration command

### Rekkord 0.76

*Released on 2025-07-06*

- Remove default repository users created by `rekkord init`
- Directly set master key filename with `REKKORD_KEYFILE` environment variable
- Directly set master key filename with `Repository.KeyFile` INI setting
- Switch to per-user public signing key pairs
- Fix xattrs being restored on symlink target instead of link itself
- Use smaller virtual ACL xattr keys
- Protect memory containing key derived from user password
- Fix small stack overflow bug
- Remove `rekkord migrate_tags` command

> [!WARNING]
> This is a breaking change, and there no migration path is provided. A new repository must be used.

### Rekkord 0.74

*Released on 2025-06-26*

- Fix "Blob is not a Snapshot" error during repository check
- Avoid unreadable progress output when not run in VT-100 terminal

### Rekkord 0.73

*Released on 2025-06-23*

- Use new tamper-resistant snapshot tags
- Compute and record additional storage size in snapshots
- Improve accuracy of per-snapshot storage values
- Report snapshot-specific values in `rekkord list`
- Adjust log text from `rekkord init`

> [!WARNING]
> The snapshot tag format has changed, but you can use `rekkord migrate_tags` to migrate snapshot tags made with previous versions.

### Rekkord 0.72

*Released on 2025-06-21*

- Fix broken use of conditional S3 writes (regression)
- Improve S3 compatibility with some S3 providers
- Improve FUSE file read performance for random reads
- Fix OpenBSD build

### Rekkord 0.71

*Released on 2025-06-20*

- Extend object locks during repository checks
- Fix PutObjectRetention signing errors with some S3 providers
- Allow changing number of threads through config file
- Reorder some sections in main help text
- Fix possible use-after-free in S3 code
- Fix noisy error message when running `rekkord --help`
- Fix bogus assertion in cache code

### Rekkord 0.70

*Released on 2025-06-18*

- Try to load `rekkord.ini` from standard paths
- Drop confusing default user *write*
- Simplify manual S3 configuration
- Clean up S3 retry conditions

### Rekkord 0.69

*Released on 2025-06-16*

- Fix erroneous hash mismatch bug with legacy blob types
- Fix stale check mark and validity values

### Rekkord 0.68

*Released on 2025-06-16*

- Accept common options before command, such as `rekkord -R /tmp/repo init`
- Accept authentification with master key in all commands
- Separate common and specific options in command help texts
- Validate OID hash against unencrypted blob content
- Retry failed blob check without delay
- Rewrite cache code for performance and reliability
- Fix rare password generation bug in `rekkord init`
- Hide progress spam from `rekkord mount` output

### Rekkord 0.67

*Released on 2025-06-14*

- Support disabling conditional S3 writes
- Support *S3_DEFAULT_REGION* environment variable
- Rename S3 access key setting
- Simplify supported S3 URL schemes
- Fail early for NotImplemented S3 errors
- Fix non-working S3 LockMode setting in Rekkord
- Fix broken compilation on Windows
- Fix use-after-free in S3 region detection code
- Use *UNSIGNED-PAYLOAD* to increase S3 performance

### Rekkord 0.66

*Released on 2025-06-12*

- Improve automatic region detection (tested with Garage and R2)
- Fix S3 signing regression after region detection
- Fix possible off-by-one access in S3 config code

### Rekkord 0.65

*Released on 2025-06-12*

- Add `rekkord check` command to check snapshot validity
- Change `rekkord save` syntax to always take a snapshot channel
- Change `rekkord restore` syntax for consistency
- Change save and restore metadata CLI options
- Support default directory POSIX ACLs
- Use slightly more compact directory metadata
- Fix inaccurate number of entries in some snapshots
- Fix ignored S3 Region config file setting
- Fix broken FreeBSD build
- Fix various incompatibilites with Big Endian platforms (untested)
- Show progress bar during cache rebuild
- Improve S3 performance by reducing SigV4 signing overhead
- Improve SFTP repository initialization speed
- Refactor librekkord to separate disk, repository and tape layers

> [!NOTE]
> The new directory metadata format will cause transient repository size expansion.

### Rekkord 0.64

*Released on 2025-06-01*

- Commit local cache periodically to increase performance
- Fix error when using `rekkord save` on Windows
- Fix empty warning about snapshot entry rename
- Uppercase Windows drive letters in rekkord snapshots

### Rekkord 0.63

*Released on 2025-05-29*

- Fix stack overflow in S3 code
- Accept *S3_\** environment variables for S3 configuration
- List *OID* attributes as *Object*
- Add `rekkord check_blobs` to make sure all blobs are readable
- Reduce indentation width in XML and JSON outputs
- Remove `rekkord migrate_blobs` command

### Rekkord 0.62

*Released on 2025-05-28*

- Separate metadata and content blobs
- Add command to migrate blobs from old Rekkord versions
- Reuse HTTPS connections to improve S3 performance
- Increase default Rekkord compression level
- Skip fetch when restoring empty files
- Fix silent early restore exit when using *--dry_run*
- Fix reported hash when using `rekkord save --raw`
- Remove unused command from `rekkord --help all` text

> [!WARNING]
> This is a breaking change, use `rekkord migrate_blobs` to migrate older repositories.

### Rekkord 0.61

*Released on 2025-05-26*

- Avoid blob overwrites (with conditional S3 writes)
- Remove probabilistic blob cache rebuilds
- Adjust default Rekkord concurrency values
- Rename `rekkord rebuild_cache` command to `rekkord reset_cache`
- Change `rekkord --help full` to `rekkord --help all`
- Rename public librekkord repository API

### Rekkord 0.59

*Released on 2025-05-18*

- Store full storage size in each snapshot

### Rekkord 0.58

*Released on 2025-05-12*

- Fix wrong key used to sign user keysets

> [!WARNING]
> Existing users are not migrated and new users must be created with the master key.

### Rekkord 0.56

*Released on 2025-05-11*

- Create new default log user in `rekkord init`
- Fix inaccurate `rekkord restore` progress with small files
- Report number of restored entries
- Make *--dry_run* behave closer to normal restore (except it does not write to disk)
- Support SSH key setting in base64 format
- Accept SSH usernames with capitals
- Replace libacl with handmade code for Linux

### Rekkord 0.55

*Released on 2025-05-06*

- Store ctime and atime (if *--store_atime* is used) values
- Fix `rekkord list` error with special or failed files
- Properly handle symlink following in new xattr code
- Support xattrs (with *--store_xattrs*) on Linux and FreeBSD
- Add --channel option to `rekkord snapshots`
- Rename `rekkord restore` options for consistency
- Fix minor endianness issues

> [!NOTE]
> The new metadata (ctime and atime) will cause transient repository size expansion.

### Rekkord 0.54

*Released on 2025-05-02*

- Add `rekkord channels` to help notice failing snapshots
- Remove tag migration command

### Rekkord 0.53

*Released on 2025-05-01*

- Reduce delay before initial S3 connection error
- Improve support for S3 URLs with dotted bucket names
- Fix silent exit with missing S3 bucket (status 404)

### Rekkord 0.52

*Released on 2025-05-01*

- Fix broken big file deduplication caused by unstable splitter seed
- Fix default snapshot channel reported as null after save
- Adjust naming of tag files

### Rekkord 0.51

*Released on 2025-04-30*

- Replace snapshot names with channels
- Fix incoherent mode checks with WriteOnly role
- Rename *--sort* values in `rekkord snapshots`

### Rekkord 0.50

*Released on 2025-04-28*

- Differentiate disk access modes and user roles
- Support four user roles: *Admin, ReadWrite, WriteOnly and LogOnly*
- Sign tags to prevent tampering by LogOnly users
- Increase maximum snapshot name length (256 bytes)

> [!WARNING]
> Existing users are not migrated and new users must be created with the master key.
>
> The snapshot tag format has changed, but you can use `rekkord migrate_tags` to migrate snapshot tags made with previous versions.

### Rekkord 0.49

*Released on 2025-04-24*

- Use two 128-bit repository IDs: RID (repository ID) and CID (cache ID)
- Replace `rekkord change_id` command with `rekkord change_cid`
- Prevent stale leftover cache files after ID change

### Rekkord 0.48

*Released on 2025-04-24*

- Sign root config file to detect tampering

> [!WARNING]
> The cache ID file (rekkord) must be recreated manually.

### Rekkord 0.46

*Released on 2025-04-23*

- Sign user keyfiles to detect tampering
- Add option to check user signatures with `rekkord list_users`
- Fix exit code when `rekkord add_user -K` fails
- Protect keys in more places

> [!WARNING]
> Existing users are not migrated and new users must be created with the master key.

### Rekkord 0.45

*Released on 2025-04-22*

- Use OS-specific API to lock and conceal memory containing encryption keys
- Change format of user key files for simplicity
- Default to *write* user if not specified
- Remove *--depth* option from `rekkord list`

> [!WARNING]
> Existing users are not migrated and new users must be created with the master key.

### Rekkord 0.44

*Released on 2025-04-21*

- Auto-generate user passwords when prompt is left empty
- Fix ambiguous *--password* option in `rekkord add_user`

### Rekkord 0.43

*Released on 2025-04-21*

- Change users to be single-role and create two users by default (*full* and *write*)
- Rename repository environment variable from `REKKORD_URL` to `REKKORD_REPOSITORY`
- Rename user setting and environment variable
- Support -f as shortcut for `--force` in Rekkord user commands
- Try to prevent sensitive memory from ending up in core dumps

> [!WARNING]
> Existing users are not migrated and new users must be created with the master key.

### Rekkord 0.42

*Released on 2025-04-18*

- Fix error when trying to restore special files (such as sockets)
- Skip creation of root directory in `rekkord restore` when *--dry_run* is used

### Rekkord 0.41

*Released on 2025-04-17*

- Drop support for long snapshot names (reduce list performance)
- Explictly limit snapshot name length to 80 bytes
- Remove username suffix in default snapshot names
- Fix ignored value from *REKKORD_USERNAME* environment variable

### Rekkord 0.40

*Released on 2025-04-16*

- Derive encryption keys from the master key
- Use separate data key pair (for blobs) and tag/log key pair (for snapshot log)
- Derive the FastCDC chunker seed from the write key
- Derive the BLAKE3 hash salt from the write key
- Pad blobs with Padmé algorithm to reduce size leakage
- Always prompt user for master key export path
- Drop `rekkord export_key` command since the master key is not stored anymore
- Drop *--skip_key* option from `rekkord init` since export is not possible after that
- Sort `rekkord list_users` entries alphabetically

> [!WARNING]
> This is a breaking change, and there is no migration path. A new repository must be used.

### Rekkord 0.39

*Released on 2025-03-23*

- Fix excessive memory usage during `rekkord save`

### Rekkord 0.38

*Released on 2025-03-23*

- Use 256 subdirectories for blobs instead of 4096

> [!WARNING]
> This is a breaking change, and requires migrating older repositories manually!

### Rekkord 0.37

*Released on 2025-03-23*

- Add retry logic to handle transient SFTP errors in rekkord
- Add *--preserve_atime* option to `rekkord save`
- Fix wrong cache ID used by `rekkord rebuild_cache`
- Fix minor typos in help and log

### Rekkord 0.36

*Released on 2025-03-22*

- Fix stalling and deadlock when restoring many files in `rekkord restore`

### Rekkord 0.35

*Released on 2024-12-25*

- Fix error when listing empty directories and snapshots (regression introduced in Rekkord 0.30)

### Rekkord 0.34

*Released on 2024-12-09*

- Use more standard syntax in usage texts
- Use assembly implementations of blake3 and libsodium algorithms

### Rekkord 0.33

*Released on 2024-11-29*

- Show busy progress bar while running `rekkord save`

### Rekkord 0.32

*Released on 2024-11-28*

- Support sub-object listing and restoration in rekkord
- Drop *--flat* option from `rekkord restore` command
- Drop *--flat* option from `rekkord mount` command

### Rekkord 0.31

*Released on 2024-11-25*

- Fix use of unix time format for `rekkord list -f XML`
- Change style of CLI progress bars

### Rekkord 0.30

*Released on 2024-11-24*

- Adjust rekkord data format for better progress reports (causes transient repository size expansion)
- Report progress while running `rekkord restore`
- Report progress while running `rekkord list`
- Reduce number of threads for local rekkord repositories
- Default to 100 threads for S3 access in rekkord
- Fix detection of *sftp:* URLs in rekkord
- Fix stripped root '/' with short SSH urls
- Expose tag filename in `rekkord snapshots`
- Use ISO time strings for JSON and XML rekkord outputs
- Fix typos in help and error messages

### Rekkord 0.29

*Released on 2024-10-16*

- Revert "Always append username to snapshot names" change

### Rekkord 0.28

*Released on 2024-10-15*

- Add *--pattern* option to `rekkord snapshots`
- Clean up and rename `rekkord mount` fuse options

### Rekkord 0.27

*Released on 2024-10-08*

- Replace base64 master key display with file export
- Always append username to snapshot names

### Rekkord 0.26

*Released on 2024-10-02*

- Fix `rekkord save` crash when saving in raw mode
- Fix read-beyond-end of hash identifiers
- Append username to default snapshot names
- Fix possible NULL name when listing snapshots

### Rekkord 0.25

*Released on 2024-10-02*

- Support locating snapshots by name in main commands
- Show SSH error message if SFTP error is "Success"
- Fix broken multi-sort in `rekkord snapshots`

### Rekkord 0.24

*Released on 2024-10-01*

- Remove support for anonymous snapshots
- Add sort option to `rekkord snapshots`
- Improve exploration commands with colors
- Fix ugly double-slash in restore paths
- Use O_TMPFILE for `rekkord restore` on Linux
- Improve Windows compatibility
- Switch to GPL 3+ license

### Rekkord 0.23

*Released on 2024-09-03*

- Support *--verbose* and *--dry_run* flags in `rekkord restore`

### Rekkord 0.22

*Released on 2024-09-03*

- Add option to delete extra files and directories with `rekkord restore`
- Fix incorrect mtime when restoring files on POSIX systems

### Rekkord 0.21

*Released on 2024-07-28*

- Update to LZ4 1.10.0
- Use higher LZ4 compression level by default
- Add setting to change LZ4 compression level

### Rekkord 0.20

*Released on 2024-07-19*

- Fix error when using `rekkord build_cache`

### Rekkord 0.19

*Released on 2024-07-18*

- Reduce intempestive rekkord cache rebuilds
- Drop buggy error code check in SFTP code
- Always run cache rebuild inside transaction

### Rekkord 0.18

*Released on 2024-07-18*

- Rebuild local cache when ID changes
- Fix silent SSH overwrite fails in rekkord commands
- Normalize and check mount point ahead of time
- Fix crash after `rekkord mount` exit due to destructor ordering
- Use capitalized attribute names in XML outputs
- Adjust open mode required for advanced commands
- Protect rekkord secrets with mlock() and zero erase

### Rekkord 0.17

*Released on 2024-04-01*

- Optimize tag storage and listing
- Isolate advanced rekkord commands
- Add `rekkord rebuild_cache` command

### Rekkord 0.16

*Released on 2024-03-18*

- Add FUSE support with `rekkord mount`
- Rename put/get commands to save/restore
- Store number of subdirectories in directory entries (causes transient repository size expansion)
- Reduce unnecessary traffic while listing tree content
- Reduce binary size

### Rekkord 0.15

*Released on 2024-02-19*

- Improve error message for uninitialized repository
- Better handle transient repository errors

### Rekkord 0.14

*Released on 2024-02-12*

- Rename `rekord` project to `rekkord`

### Rekkord 0.13

*Released on 2024-01-21*

- Rename `rekord log` command to `rekord snapshots`

### Rekkord 0.12

*Released on 2024-01-18*

- Combine repository ID and url to secure cache ID

### Rekkord 0.11.1

*Released on 2024-01-11*

- Use case-insensitive test for rekord CLI output format name
- Add Windows and macOS builds

### Rekkord 0.11

*Released on 2024-01-05*

- Rename exploration commands
- Fix broken compilation on Windows

### Rekkord 0.10

*Released on 2023-12-22*

- Replace object ID mentions with object hash
- Generate or prompt for passwords after config check
- Add `rekord change_id` command
- Fix broken highlight in help texts

### Rekkord 0.9.1

*Released on 2023-12-22*

- Fix possible use-after-free while snapshotting

### Rekkord 0.9

*Released on 2023-12-22*

- Add user management commands
- Support `<username>@<host>:[path]` syntax for SSH URLs

### Rekkord 0.8

*Released on 2023-12-18*

- Export master key after repository initialization
- Change `rekord export_key` to export in base64 format
- Change `rekord init` to use -R for repository URL (like other commands)

### Rekkord 0.7

*Released on 2023-12-15*

- Fix init of local and S3 rekord repositories
- Adjust names of config keys and environment variables
- Support s3:// scheme for S3 repository URLs
- Minor CLI output changes

### Rekkord 0.6

*Released on 2023-12-13*

- Fix excessive memory usage when storing big files in parallel

### Rekkord 0.5

*Released on 2023-12-12*

- Fix unbounded task queue for large put and get tasks
- Change plain output format for `rekord list`
- Minor other output changes

### Rekkord 0.4

*Released on 2023-12-03*

- Add JSON and XML list output formats
- Add `rekord tree` command to list files

### Rekkord 0.3

*Released on 2023-11-29*

- Improve rekord performance on low-core machines
- Improve error when rekord cannot reserve space for file

### Rekkord 0.2

*Released on 2023-11-16*

- Automatically create Linux cache directory if needed
- Show base64 SHA256 fingerprints for unknown SSH servers
- Support config-based SSH known fingerprint
- Restore files with temporary name until complete
- Use "default" repository username if not set

### Rekkord 0.1

- First proper release
