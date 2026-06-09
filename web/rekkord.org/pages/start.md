# Create configuration

Rekkord provides a step-by-step interactive command that will help you configure a repository.

Run the following command to get started:

```sh
rekkord setup
```

This command only creates the configuration but it **does not initialize the repository**. Follow the next section to continue.

> [!NOTE]
> You can also [configure Rekkord manually](#manual-configuration) if you prefer.

# Initialize repository

Once you have configured your repository, through a configuration file or with environment variables, you can initialize it with the following command:

```sh
rekkord init
```

> [!NOTE]
> rekkord looks for `~/.config/rekkord/rekkord.ini` or `/etc/rekkord/rekkord.ini` by default.
>
> Set `REKKORD_CONFIG_FILE` to a different path if your config file lives somewhere else.
>
> ```sh
> export REKKORD_CONFIG_FILE=/path/to/config.ini
> rekkord init
> ```
>
> You can also use the `-C filename` option to use a custom config file path.
>
> ```sh
> rekkord -C /path/to/config.ini init
> ```

This command will initialize the repository with a random 256-bit master key.

After initialization, the **master key** is exported to a binary file, named `master.key` by default (in the current directory). You can change the export file with the `--key_file <file>` option.

> [!IMPORTANT]
> You should **store this master key in a secure place**. It cannot be recreated if you lose the file. If it leaks, everyone will be able to decrypt or edit your snapshots!

Rekkord uses **multiple encryption keys** which are derived from this master key:

- The *config key (ckey)* is paired with an *access key (akey)* to sign config files and user keyfiles
- The *data key (dkey)* is paired with a *write key (wkey)* for data encryption (snapshot information, directory metadata, file content)
- The *log key (lkey)* is paired with a *tag key (tkey)*, to manage snapshots and record snapshot information

For simple use cases, you can simply use the master key for everything. However, we recommend that you create [restricted keyfiles](#restricted-keys), for two reasons:

- Each restricted keyfile can have limited capabilities: *ReadWrite, WriteOnly or LogOnly*
- Snapshots are signed with each keyfile-specific signing key, this can be used to detect cross-server tampering during repository checks

# Next steps

Once your setup is ready, you can [create your first snapshot](snapshot#create-snapshots).

You might also want to [secure your installation](protect) to protect your repository from data leaks, tampering and deletion.
