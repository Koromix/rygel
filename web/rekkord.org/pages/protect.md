# Overview

Rekkord provides several ways to protect backups from leaks and tampering, especially when combined with an S3 backend:

- [Object versioning and object locks](#object-versioning-and-object-locks) (S3)
- [Fine-grained bucket policy/permissions](#fine-grained-s3-bucket-policy) (S3)
- [Write-only repository keys](#write-only-keyfiles) (using asymmetric encryption)

These features require additional configuration and are not enabled by default. Read the documentation below to enable them.

# Object versioning and object locks ^ S3 object locks

It is **strongly recommended** to enable object versioning and object locks for S3 repositories.

Rekkord does not use object locks by default, you need to configure them. To do so, set `RetainDuration = <duration>` inside the *Protection* section of the Rekkord config file, as shown below:

```ini
[Protection]
RetainDuration = 30d # 30 days

[S3]
LockMode = GOVERNANCE # Optional (defaults to GOVERNANCE if not set)
```

With this setting, new blobs will be retained automatically when `rekkord save` runs.

However, **existing object locks are not extended** when snapshots are created. You need to run periodic scans with `rekkord scan` from a dedicated machine. After the repository has been checked, this command will extend the locks of objects that are used by existing snapshots.

> [!NOTE]
> It is essential to run periodic scans to detect errors and corrupted blobs, even if you choose to not use object locks!
>
> Please consult the [relevant documentation](maintenance#periodic-scans) for more information.

# Fine-grained S3 bucket policy ^ S3 permissions

The repository format is specifically designed to support the creation of snapshots without ever reading existing blobs. You should use a restictive S3 policy, especially for S3 keys used to create snapshots (except for the repository configuration inside the `rekkord` root object).

### Required permissions

#### Create snapshots

When Rekkord opens a repository to create a snapshot, it reads some config information from the `rekkord` object at the root of the repository. After that, `rekkord save` never reads any other blob. All it needs to is create new S3 objects, for blobs and tags.

To create snapshots, Rekkord needs to access the following objects:

Object key   | Permission
------------ | -------------------------
`rekkord`    | *s3:GetObject* (read-only)
`cw`         | *s3:PutObject* (write-only)
`blobs/*`    | *s3:PutObject* (write-only)
`tags/*`     | *s3:PutObject* (write-only)

That's it! Please see below for an [example S3 policy](#example-policy) that provides the ability to create snapshots.

#### Read snapshots

To explore and restore snapshots, Rekkords needs to access the following objects:

Object key   | Permission
------------ | -------------------------
`rekkord`    | *s3:GetObject* (read-only)
`blobs/*`    | *s3:GetObject* (read-only)
`tags/*`     | *s3:ListBucket*

#### Perform scan and prune repositories

To run `rekkord scan`, Rekkords needs to access all blobs. Enable the following S3 permissions:

- *s3:GetObject*
- *s3:ListBucket*
- *s3:PutObject*
- *s3:PutObjectRetention*

> [!CAUTION]
> An attacker with this access can severly damage the repository. Run `rekkord scan` from a secure machine!

### Example policy

The following policy provides statements to create snapshots, to explore and restore snapshots, and to run periodic scans with `rekkord scan`. You will need to replaces the *Principal* values, and set the correct *BUCKET* names.

```json
{
  "Version": "2023-04-17",
  "Id": "Rekkord backups",
  "Statement": [
    {
      "Sid": "Open repository and read config",
      "Effect": "Allow",
      "Principal": "*",
      "Action": [
        "s3:GetBucketLocation",
        "s3:GetObject"
      ],
      "Resource": [
        "BUCKET",
        "BUCKET/rekkord"
      ]
    },

    {
      "Sid": "Create snapshots",
      "Effect": "Allow",
      "Principal": {
        // Whatever your S3 provider needs for backup S3 keys
      },
      "Action": "s3:PutObject",
      "Resource": [
        "BUCKET",
        "BUCKET/rekkord",
        "BUCKET/blobs/*",
        "BUCKET/tags/*",
        "BUCKET/cw"
      ]
    },

    {
      "Sid": "Read snapshots",
      "Effect": "Allow",
      "Principal": {
        // Whatever your S3 provider needs for the S3 key you want to use to list and restore snapshots
      },
      "Action": [
        "s3:ListBucket",
        "s3:GetObject"
      ],
      "Resource": [
        "BUCKET",
        "BUCKET/rekkord",
        "BUCKET/blobs/*"
      ]
    },

    {
        "Sid": "Scan and prune repository on secure dedicated machine",
        "Effect": "Allow",
        "Principal": {
          // Whatever your provider needs for the S3 key you will use for repository scans
        },
        "Action": [
          "s3:GetObject",
          "s3:ListBucket",
          "s3:PutObject",
          "s3:PutObjectRetention"
        ],
        "Resource": [
          "BUCKET",
          "BUCKET/*"
        ]
      }
  ]
}
```

> [!NOTE]
> If your S3 provider does not support conditional writes, you need to allow *s3:ListBucket* for the creation of snapshots. This makes the repository slightly more vulnerable to poisoning, especially without object versioning.

# Write-only keyfiles

Rekkord does not need to read existing objects to create new snapshots. Rekkord uses asymmetric encryption for blobs, the cryptographic key used to create them cannot be used to read them back.

It is thus possible to create write-only keyfiles and to use them to run `rekkord save`. Each machine can thus create snapshots (and even share a repository) without the ability to decrypt anything contained inside the repository.

> [!NOTE]
> This means that `rekkord save` cannot check that existing blobs are valid.
>
> You need to run [periodic scans](maintenance#periodic-scans) from a secure machine to detect errors and corrupted blobs!

You must use the master key to create write-only keyfiles. Use `rekkord derive` to create them, like this:

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini

rekkord derive -K master.key -t WriteOnly -O server1.key
rekkord derive -K master.key -t WriteOnly -O server2.key
rekkord derive -K master.key -t WriteOnly -O server3.key
rekkord derive -K master.key -t WriteOnly -O server4.key
```

Consult the documentation about [manual configuration](config#restricted-keyfiles) for more information about restricted keyfiles.
