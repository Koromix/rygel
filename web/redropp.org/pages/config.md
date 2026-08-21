# Main configuration

## Mandatory settings

The following settings are mandatory, and msut be set before Redropp can work:

```ini
[General]
# Your instance title
Title =
# Public-facing URL
URL =

[S3]
# Endpoint and bucket, example: https://sos-de-fra-1.exo.io/bucket
Location =
# AWS/S3 access key ID
AccessKeyID = 
# AWS/S3 secret access key
SecretKey =

[SMTP]
# SMTP information
# Read the CURL documentation for supported URLs: https://everything.curl.dev/usingcurl/smtp.html
URL =
Username =
Password =
From =
```

## Drop settings

Redropp provides several settings to adjust the storage allowed for each registered user. These settings are grouped in the `[Drop]` section:

```ini
# [Drop]
# Change maximum per-user storage
Quota = 1G
# Change maximum expiration delay of drops
MaxDuration = 90d
# Allow drops without expiration delay (infinite duration)
AllowInfinite = No
```

You can create the default configuration file with the following command:

```sh
redropp init > redropp.ini
```

# Authentication methods

Only registered users can use Redropp to send files, but the download URL is public and can be shared with anyone.

By default, Redropp uses internal accounts, and anyone with a valid email can create an account. However, you can:

- Disable registration of new users
- Configure SSO providers (OIDC only)
- Disable internal authentication

## Internal users

Redropp supports its own authentication method, with internal user accounts.

Internal accounts require a valid email address, which serves as the login identifier. By default, new users can create an account, and after validating their email, they can share files.

You can however disable the registration of new users, or disable internal authentication altogether, with the  `[Authentication]` section of the config file:

```ini
[Authentication]
# Set to No to disable internal user accounts (login, registration, password loss recovery, etc.)
AllowInternal = Yes

# Set to No to disable registration of new users, including SSO accounts that did not previously
# connect to Redropp.
AllowRegister = Yes
```

## SSO providers

Redropp only supports OpenID Connect (OIDC). You can configure one or more providers with multiple `[SSO]` sections in the configuration file.

This requires a provider that provides a discovery URL, which usually lives at `/.well-known/openid-configuration`.

```ini
[SSO]
# URL to the SSO provider, to which Redropp will append /.well-known/openid-configuration in order to fetch
# the OIDC configuration. You can instead specify the full discovery URL with the DiscoveryURL= setting.
URL = https://example.com

# OIDC client ID and secrets required by the SSO provider.
ClientID = 
ClientSecret = 

# Sets the JWT algorithm to use with this provider. Support values: RS256 (default), PS256, HS256.
# JwtAlgorithm = RS256

# Set to Yes to transparently link a new SSO account to an existing account (if any), if the mail
# addresses match.
# By default, this is disabled, but it is still possible to link to an exisiting account, but it
# requires that the user validates this by mail.
# AutoLink = No

# [SSO]
# Repeat SSO section and settings to add multiple providers
```
> [!NOTE]
> Configuring SSO requires the use of a configuration file. It is not possible to configure SSO through environment variables.
