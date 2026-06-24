# Overview

Rekkord supports hooks, which are commands that are executed before and after some actions.

To use hooks, you must set a directory for each type of hook in the configuration file. When Rekkord runs the corresponding command (such as `rekkord save` for save hooks), it will run the hooks it finds in alphabetical order.

For each command there are two hook phases:

- The `pre` phase, these hooks run before the command takes place. If a `pre` phase hook fails to run or returns a non-zero exit code, the action will not run and Rekkord exit with a non-zero exit code.
- The `post` phase, these hooks run after the command has finished, successfully or not. If a `post` hook fails, Rekkord will exit with a non-zero exit code.

> [!NOTE]
> All hook scripts run during the corresponding phase, even if one fails. Rekkord executes them all in order, before aborting if any of them has failed.

Post hooks can use the *SUCCESS* environment variable to query whether the command has succeeded or failed. The value "1" will be one in case of success, or "0" if an error has happened. If one hook command fails, the value of *SUCCESS* is set to 0 in subsequent hooks.

# Save hooks

## Pre-save

These hooks run before a snapshot gets created by `rekkord save`.

Set the directory where pre-save hook commands exist with `PreSaveDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PreSaveDirectory = /opt/rekkord/hooks/presave
```

As stated before, if any hook fails, the save will not happen and Rekkord exits with an error code.

> [!TIP]
> Use pre-save hooks to dump databases, for example with mysqldump (MySQL or MariaDB), pg_dump (PostgreSQL) or make proper sqlite3 backups with `sqlite3 src.db '.backup copy.db'`.
>
> Do not compress these dumps (with gzip, for example) because it will reduce deduplication, and Rekkord uses its own compression. However, if you still want to compress your dump files, use `gzip --rsyncable` (or something equivalent).

## Post-save

These hooks run after a snapshot been created by `rekkord save`, successfully or not.

Set the directory where post-save hook commands exist with `PostSaveDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PostSaveDirectory = /opt/rekkord/hooks/postsave
```

> [!TIP]
> Use the *SUCCESS* environment variable to react to success or failure.

As stated before, if any hook fails, Rekkord will exit with an error code.

# Scan hooks

*New in Rekkord 0.104*

## Pre-scan

These hooks run before `rekkord scan` runs.

Set the directory where pre-scan hook commands exist with `PreScanDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PreScanDirectory = /opt/rekkord/hooks/prescan
```

As stated before, if any hook fails, the scan will not happen and Rekkord exits with an error code.

## Post-scan

These hooks run after a scan.

Set the directory where post-scan hook commands exist with `PostScanDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PostScanDirectory = /opt/rekkord/hooks/postscan
```

> [!TIP]
> Use the *SUCCESS* environment variable to react to success or failure.

As stated before, if any hook fails, Rekkord will exit with an error code.
