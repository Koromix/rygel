# Periodic scans

Rekkord deduplicates data by reusing existing blobs each time a new snapshot is created. It does not check the integrity oif these blobs. They could have been corrupted by bit-rot or a malveillant actor, or be outright missing.

> [!NOTE]
> This is by design!
>
> Rekkord uses an asymmetric encryption scheme which means the key used to create blobs cannot be used to read them back. In addition, Rekkord can create snapshots with write-only backends, for example when using a [PutObject-only S3 bucket policy](protect#fine-grained-s3-bucket-policy) for blobs and tags.

You must run periodic scans in order to:

- Detect invalid or corrupted blobs
- Extend S3 object locks (if you use them)

You must use the master key to run `rekkord scan`.

Run it every day, or every other day. It keeps track of which blobs have been checked recently, so it will not check everything every time you run it... if you always run it from the same machine, **which you should**.

> [!CAUTION]
> Because this requires the master key, you should run scans from a secure machine, that does nothing else.
>
> Ideally it should always run from the same machine, and this machine should be kept safe. You don't want to leak the master key!

You can run this command while other machines are doing backups.

# Prune old snapshots

TODO
