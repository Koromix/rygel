# Repository backends

## S3 storage

To create a repository stored on an S3-compatible server, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = https://<endpoint>/<bucket>

[S3]
AccessKeyID = <AWS access key ID>
SecretKey = <AWS secret key>
```

To help you form a valid URL, here are a few examples for common S3 providers:

| Provider                       | URL                                                  | Conditional writes |
|--------------------------------|------------------------------------------------------|--------------------|
| Amazon S3 (virtual-host style) | `https://<bucket>.s3.<region>.amazonaws.com`         | Yes                |
| Amazon S3 (path style)         | `https://s3.<region>.amazonaws.com/<bucket>`         | Yes                |
| Backblaze B2                   | `https://s3.<region>.backblazeb2.com/<bucket>`       | No                 |
| Cloudflare R2                  | `https://<userid>.r2.cloudflarestorage.com/<bucket>` | Yes                |
| Exoscale Object Storage        | `https://sos-<region>.exo.io/<bucket>`               | Yes                |
| Scaleway Object Storage        | `https://<bucket>.s3.<region>.scw.cloud`             | Yes                |

> [!NOTE]
> Rekkord may issue ListObjects API calls to reduce blob overwrites on hosts without conditional write support, which may incur additional cost.
>
> It is recommended to use a provider **with support for conditional writes**!

You can omit the `SecretKey` value, in which case a prompt will ask you the access key.

Once this is done, use [rekkord init -C config.ini](#initialize-repository) to create the repository.

## SFTP server

To create a repository stored on an SFTP host, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = <user>@<host>:/path/to/repository

[SFTP]
Password = <SSH password>
KeyFile = <SSH keyfile>
```

Use either a password or a key file. If you omit both, a prompt will ask you for a password.

Just like with `ssh`, you will need to validate the host fingerprint the first time you connect to it. However, you can instead directly provide the correct host fingerprint as the `Fingerprint` to avoid this, as shown in the example below:

```ini
[Repository]
URL = foo@example.com:/backup

[SFTP]
KeyFile = /home/bar/.ssh/id_ed25519
Fingerprint = SHA256:Y9pmJfkaok8t0dFJrfi8/LLUNhOYwAZGHUNGsYAiJUM
```

Once this is done, use [rekkord -C config.ini init](#initialize-repository) to create the repository.

## Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekkord -C config.ini init](#initialize-repository) to create the repository.
