# Changelog

## Alpha versions

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

### Rekkord 0.17

- Optimize tag storage and listing
- Isolate advanced rekkord commands
- Add `rekkord rebuild_cache` command

### Rekkord 0.16

- Add FUSE support with `rekkord mount`
- Rename put/get commands to save/restore
- Store number of subdirectories in directory entries (causes transient repository size expansion)
- Reduce unnecessary traffic while listing tree content
- Reduce binary size

### Rekkord 0.15

- Improve error message for uninitialized repository
- Better handle transient repository errors

### Rekkord 0.14

- Rename `rekord` project to `rekkord`

### Rekkord 0.13

- Rename `rekord log` command to `rekord snapshots`

### Rekkord 0.12

- Combine repository ID and url to secure cache ID

### Rekkord 0.11.1

- Use case-insensitive test for rekord CLI output format name
- Add Windows and macOS builds

### Rekkord 0.11

- Rename exploration commands
- Fix broken compilation on Windows

### Rekkord 0.10

- Replace object ID mentions with object hash
- Generate or prompt for passwords after config check
- Add `rekord change_id` command
- Fix broken highlight in help texts

### Rekkord 0.9.1

- Fix possible use-after-free while snapshotting

### Rekkord 0.9

- Add user management commands
- Support `<username>@<host>:[path]` syntax for SSH URLs

### Rekkord 0.8

- Export master key after repository initialization
- Change `rekord export_key` to export in base64 format
- Change `rekord init` to use -R for repository URL (like other commands)

### Rekkord 0.7

- Fix init of local and S3 rekord repositories
- Adjust names of config keys and environment variables
- Support s3:// scheme for S3 repository URLs
- Minor CLI output changes

### Rekkord 0.6

- Fix excessive memory usage when storing big files in parallel

### Rekkord 0.5

- Fix unbounded task queue for large put and get tasks
- Change plain output format for `rekord list`
- Minor other output changes

### Rekkord 0.4

- Add JSON and XML list output formats
- Add `rekord tree` command to list files

### Rekkord 0.3

- Improve rekord performance on low-core machines
- Improve error when rekord cannot reserve space for file

### Rekkord 0.2

- Automatically create Linux cache directory if needed
- Show base64 SHA256 fingerprints for unknown SSH servers
- Support config-based SSH known fingerprint
- Restore files with temporary name until complete
- Use "default" repository username if not set

### Rekkord 0.1

- First proper release
