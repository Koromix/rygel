# Create snapshots

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

Each snapshot has a unique hash (which is actually a BLAKE3 hash in hexadecimal form), which is generated automatically when the snapshot is created.

By default, snapshots are named after the normalized path you are backing up. You can use `-n <NAME>` to use a custom name. You must give an explicit name if you specify multiple paths.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord save <PATH>
rekkord save -n <NAME> <PATH1> <PATH2> ...
```

The command will give you the snapshot hash once it finishes. You can retrieve the hash later with [rekkord list](#list-snapshots).

# List snapshots

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord snapshots
```

The output looks something like this:

```text
# <hash>                                                           <name>                   <date>

DA24E2C01C2AF6ACADF94FED087FD2695DF1E5352FA5474E091DABE38A104641   webserver1               [2023-11-26 06:09:57 +0100]
96389F2763173C9575C85A9D8972FE8DC06FC220BA7A05A673D7C19E520C22EB   webserver1               [2023-11-27 06:05:15 +0100]
221FA094C07FAABA1A6B8710EED613F441410C95855D69653A2BDCBB590C8E9F   webserver1               [2023-11-28 06:18:15 +0100]
```

Use `--format JSON` or `--format XML` to get this list in a JSON or XML format.

# Explore snapshot

You can list the directories and files in a snapshot with the `rekkord list` command. You can either use the unique [snapshot hash](#list-snapshots), or provide a snapshot name, in which case rekkord will use the most recent snapshot that matches it.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord list <HASH or NAME>
```

The output looks something like this:

```text
# <type> <name>                                                   <mode> <date>                      <size>

[S] vpn/opt                                                              [2023-12-15 06:18:10 +0100]
  [D] opt/                                                        (0755) [2023-10-05 17:03:38 +0200]
```

You can recursively list the content with `rekkord list <hash> --recurse` flag:

```text
# <type> <name>                                                   <mode> <date>                      <size>

[S] vpn/opt                                                              [2023-12-15 06:18:10 +0100]
  [D] opt/                                                        (0755) [2023-10-05 17:03:38 +0200]
    [D] certificates/                                             (0750) [2023-12-13 09:43:10 +0100]
      [F] cloud.intra.example.com.key                             (0640) [2023-12-13 09:43:10 +0100] 241 B
      [F] forum.intra.example.com.crt                             (0640) [2023-12-13 09:43:10 +0100] 5.269 kB
    [D] agnos/                                                    (0755) [2023-10-02 12:38:29 +0200]
      [D] certificates/                                           (0750) [2023-10-02 12:38:50 +0200]
        [F] cloud.intra.example.com.key                           (0640) [2023-11-20 23:26:06 +0100] 241 B
        [F] forum.intra.example.com.crt                           (0640) [2023-11-20 23:26:07 +0100] 5.269 kB
      [F] agnos                                                   (0755) [2023-09-21 16:53:54 +0200] 11.24 MB
      [F] agnos.toml                                              (0644) [2023-10-02 12:38:28 +0200] 1.893 kB
      [D] accounts/                                               (0750) [2023-09-21 16:53:55 +0200]
        [F] main.key                                              (0644) [2023-09-21 16:53:56 +0200] 3.243 kB
    [D] rekkord/                                                   (0755) [2023-12-13 15:38:23 +0100]
      [F] ENV                                                     (0600) [2023-10-05 17:03:45 +0200] 186 B
      [F] rekkord.sh                                               (0755) [2023-12-13 15:37:58 +0100] 102 B
      [F] rekkord.deb                                              (0644) [2023-12-11 20:45:18 +0100] 5.348 MB
    [D] wireguard/                                                (0755) [2023-09-21 14:28:46 +0200]
      [D] safe/                                                   (0755) [2023-09-21 15:59:39 +0200]
        [F] docker-compose.yml                                    (0644) [2023-09-21 14:28:47 +0200] 408 B
        [F] ENV                                                   (0600) [2023-09-21 15:59:39 +0200] 209 B
        [D] etc/                                                  (0700) [2023-09-21 14:29:06 +0200]
          [F] wg0.json                                            (0640) [2023-12-10 06:00:19 +0100] 7.969 kB
          [F] wg0.conf                                            (0600) [2023-12-10 06:00:19 +0100] 4.025 kB
    [D] rclone/                                                   (0755) [2023-10-05 15:32:04 +0200]
      [F] rclone.cfg                                              (0644) [2023-10-05 15:32:04 +0200] 3.705 kB
      [F] rclone.deb                                              (0644) [2023-09-21 14:26:52 +0200] 18.55 MB
    [D] coredns/                                                  (0755) [2023-12-13 09:42:56 +0100]
      [F] coredns.tar.gz                                          (0644) [2023-09-21 16:50:53 +0200] 15.79 MB
      [F] coredns.conf                                            (0644) [2023-09-21 16:50:56 +0200] 132 B
      [F] domains.zone                                            (0644) [2023-12-13 09:42:55 +0100] 505 B
      [F] coredns                                                 (0755) [2023-02-06 19:27:11 +0100] 53.41 MB
    [D] nginx/                                                    (0755) [2023-12-11 20:44:06 +0100]
      [D] ssl/                                                    (0755) [2023-12-11 20:43:55 +0100]
        [D] webroot/                                              (0755) [2023-09-21 14:28:34 +0200]
          [D] vpn.example.com/                                    (0755) [2023-11-20 19:10:04 +0100]
        [F] dhparam.pem                                           (0644) [2023-09-21 14:28:21 +0200] 424 B
        [D] letsencrypt/                                          (0755) [2023-12-11 20:44:22 +0100]
          [L] vpn.example.com.crt                                        [2023-12-11 20:44:22 +0100]
          [L] 57.128.60.175.key                                          [2023-12-11 20:44:07 +0100]
          [L] vpn.example.com.key                                        [2023-12-11 20:44:19 +0100]
          [L] 57.128.60.175.crt                                          [2023-12-11 20:44:07 +0100]
        [D] internal/                                             (0755) [2023-12-13 09:43:27 +0100]
          [F] ca.crt                                              (0644) [2023-12-13 09:43:24 +0100] 1.883 kB
          [F] vpn.crt                                             (0644) [2023-12-13 09:43:26 +0100] 1.891 kB
          [F] vpn.key                                             (0644) [2023-12-13 09:43:25 +0100] 1.704 kB
      [F] nginx.conf                                              (0644) [2023-12-11 20:44:06 +0100] 7.086 kB
```

Use `--format JSON` or `--format XML` to get the file tree in a JSON or XML format.

# Restore snapshot

Use the `rekkord restore` command to restore the files from a snapshot onto the local filesystem. You can either use the unique [object hash](#list-snapshots), or provide a snapshot name, in which case rekkord will use the most recent snapshot that matches it.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord restore <HASH or NAME> -O <PATH>
```

# Mount snapshot

You can also use `rekkord mount <hash> <mountpoint>` to mount a snapshot or a directory as a read-only filesystem. You can either use the unique [object hash](#list-snapshots), or provide a snapshot name, in which case rekkord will use the most recent snapshot that matches it.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord mount <HASH or NAME> <MOUNTPOINT>
```

This mode has several limitations for now:

- Only Linux is supported for now, though support for FreeBSD, OpenBSD and Windows will soon come.
- Performance (for listing directories and reading files) is slower than with the dedicated commands.

But the goal is to reach performance similar to other commands once Rekkord 1.0 is released.
