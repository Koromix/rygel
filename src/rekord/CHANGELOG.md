# Changelog

## Rekord 0.10

- Replace object ID mentions with object hash
- Generate or prompt for passwords after config check
- Add `rekord change_id` command
- Fix broken highlight in help texts

## Rekord 0.9.1

- Fix possible use-after-free while snapshotting

## Rekord 0.9

- Add user management commands
- Support `<username>@<host>:[path]` syntax for SSH URLs

## Rekord 0.8

- Export master key after repository initialization
- Change `rekord export_key` to export in base64 format
- Change `rekord init` to use -R for repository URL (like other commands)

## Rekord 0.7

- Fix init of local and S3 rekord repositories
- Adjust names of config keys and environment variables
- Support s3:// scheme for S3 repository URLs
- Minor CLI output changes

## Rekord 0.6

- Fix excessive memory usage when storing big files in parallel

## Rekord 0.5

- Fix unbounded task queue for large put and get tasks
- Change plain output format for `rekord list`
- Minor other output changes

## Rekord 0.4

- Add JSON and XML list output formats
- Add `rekord tree` command to list files

## Rekord 0.3

- Improve rekord performance on low-core machines
- Improve error when rekord cannot reserve space for file

## Rekord 0.2

- Automatically create Linux cache directory if needed
- Show base64 SHA256 fingerprints for unknown SSH servers
- Support config-based SSH known fingerprint
- Restore files with temporary name until complete
- Use "default" repository username if not set

## Rekord 0.1

- First proper release
