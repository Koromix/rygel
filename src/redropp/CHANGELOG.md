# Version history

## Alpha versions

### Redropp 0.7.1

*Released on 2026-07-10*

- Fix non-working download button after first download
- Make sure download button stays disabled while it starts

### Redropp 0.7

*Released on 2026-07-10*

- Improve progress and error report for downloads and uploads
- Show expiration date on download page
- Show proper error for unknown or expired drop links
- Block recover button for incomplete uploads
- Minor style fixes

### Redropp 0.6

*Released on 2026-07-09*

- Fix broken quota usage display for everyone except first user
- Prevent tab close during upload and download
- Reduce jankiness of progress rate meter
- Fix ugly space above single SSO button
- Add setting to disable account registration
- Add setting to auto link new SSO identities to existing users
- Remove SSO/OIDC AuthorizationClaims setting

### Redropp 0.5

*Released on 2026-07-06*

- Add `redropp init` command
- Add front-end customization settings
- Switch to 4MB file fragments
- Use smooth speed and ETA estimation (especially for slow connections)
- Remove automatic SSO redirection if only one SSO provider exists
- Improve documentation of config file

### Redropp 0.4

*Released on 2026-07-04*

- Fix service worker download on recent Safari iOS versions
- Fix minor UI issues
