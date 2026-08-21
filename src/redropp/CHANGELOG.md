# Version history

## Alpha versions

### Redropp 0.9.5

*Released on 2026-08-21*

- Simplify `rekkord init` usage
- Use underscores and digits in drop codenames
- Use per-provider SSO auto-link setting
- Support directly setting OpenID discovery URL
- Add signature to ZIP data descriptors
- Encode precise file time in ZIP file (NTFS extra)
- Make it easier to navigate back to drop download
- Remove useless download/upload error prefix

### Redropp 0.9.4

*Released on 2026-08-06*

- Fix password ending up in local secret database
- Show drop password protection status
- Keep drop password dialog open until it works
- Try to prevent tab close if send form contains files
- Improve UI and accessibility on small screens

### Redropp 0.9.3

*Released on 2026-08-06*

- Generate codenames on server to reduce JS bundle size

### Redropp 0.9.2

*Released on 2026-08-05*

- Generate and use default codename for multi-file drops
- Fix error message when using wrong secret or password
- Fix reversed table header sort arrows
- Fix weird download stats when triggering successive downloads
- Adjust position and duration of Redropp log entries

### Redropp 0.9.1

*Released on 2026-08-05*

- Support file paste on drop creation page
- Show drop publication date and time
- Disable autocomplete for drop password inputs
- Small UI changes and fixes
- Reduce scrypt work factor even more

### Redropp 0.9.0

*Released on 2026-08-04*

**Highlights:**

- Refactor Redropp to support multiple files per drop
- Show active background tasks in the background right corner
- Store drop creation time in database

**Other changes:**

- Use relative strings for drop expiration time
- Ask for protected drop password in separate dialog
- Reduce scrypt work factor to avoid delay when download starts
- Block dangerous file names
- Add proper UTF-8 encoded name to Content-Disposition header
- Harmonize various button colors

**Bug fixes:**

- Fix broken favicon URL on some pages
- Fix never-ending small file downloads on Chrome
- Fix sandbox SIGKILL caused by getcpu syscall

### Redropp 0.8.2

*Released on 2026-07-27*

- Generate easy-to-save image for QR code (with app logo)
- Add links to toggle between drop share/download pages
- Fix error when downloading files whose names contain UTF-8 characters
- Fix small UI inconsistencies and missing labels

### Redropp 0.8.1

*Released on 2026-07-13*

- Adjust style of link and command widgets
- Adjust style of reactive Copy buttons
- Fix unbalanced margins around drop list on mobile screens

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
