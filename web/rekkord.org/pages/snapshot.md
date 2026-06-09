# Create snapshots

Each snapshot has a channel, which is a non-unique string that you choose when you call `rekkord save`. Please note that there is a *maximum snapshot channel length* (256 bytes).

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord save <channel> <path1> <path2> ...
```

The command will give you the object ID (OID) of the snapshot once it finishes. You can retrieve the OID later with [rekkord snapshots](#list-snapshots).

# Hooks

*New in Rekkord 0.97*

Rekkord supports hooks, which are commands that are executed before and after some actions.

To use hooks, you must set a directory for each type of hook in the configuration file. When Rekkord runs the corresponding command (such as `rekkord save` for save hooks), it will run the hooks it finds in alphabetical order.

For each command there are two hook phases:

- The `pre` phase, these hooks run before the command takes place. If a `pre` phase hook fails to run or returns a non-zero exit code, the action will not run and Rekkord exit with a non-zero exit code.
- The `post` phase, these hooks run after the command has succeeded. If a `post` hook fails, Rekkord will exit with a non-zero exit code.

> [!NOTE]
> All hook scripts run during the corresponding phase, even if one fails. Rekkord executes them all in order, before aborting if any of them has failed.

## Pre-save

These hooks run before a snapshot gets created by `rekkord save`.

Set the directory where pre-save hook commands exist with `PreSaveDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PreSaveDirectory = /opt/rekkord/hooks/presave
```

As stated before, if any command fails, the save will not happen and Rekkord exits with an error code.

## Post-save

These hooks run after a snapshot been created by `rekkord save`.

Set the directory where pre-save hook commands exist with `PreSaveDirectory` in the `Hooks` section of the config file, as shown in the example below:

```ini
# ...

[Hooks]
PostSaveDirectory = /opt/rekkord/hooks/postsave
```

As stated before, if any command fails, Rekkord will exit with an error code.
