# Version history

## Alpha versions

### Redropp 0.8

*Released on 2026-07-12*

- Improve UI/UX on mobile screens:
  * Hide menu logo on small screens
  * Use responsive table for list of drops
  * Display form labels above widgets
  * Use correct keyboard type for mail and number inputs
- Format numbers with respect to user locale
- Use WASM (@awasm-noble) for faster cryptography

### Redropp 0.7.4

*Released on 2026-07-11*

- Fix missing "Recover link" buttons

### Redropp 0.7.3

*Released on 2026-07-11*

- Reduce progress/refresh spam and desync during download
- Translate more error strings

### Redropp 0.7.2

*Released on 2026-07-11*

- Use reactive buttons for clipboard copy actions
- Fix persistent browser tab lock after download failure
- Keep error state after download error
- Fix missing error notification when upload fails
- Hide recover link button for incomplete uploads
- Default to Paranoid build in source package
- Translate more strings and messages

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
