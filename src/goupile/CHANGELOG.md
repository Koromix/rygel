# Goupile changelog

## Goupile 3.12 (beta)

*Currently in beta*

**Identifiers:**

- Support custom thread HID identifier in-lieu of sequence numbers, in addition to the custom per-page summary values.
- Recompute form data when saving with valid sequence number and counter values. This can be used to compute custom identifiers that use server-side computer values (such as the thread sequence number, a custom counter or random identifier).

**Data:**

- Implement draft status for each saved record page:

  * Draft status is automatically added during an autosave.
  * Draft status is automatically added to records created or modified in conception mode.
  * No sequence number or counter is used by these records.
  * A "Confirm" button allows removing the draft status, notably for records created in conception mode.

- Add support for non-blocking errors. Use this feature to add the error tag to records without blocking the user from saving data and without the need to annotate the variable.

- Improve autosave behavior:

  * The interface and displayed statuses do not change during an autosave.
  * Delayed errors are not triggered by autosave.
  * The draft status is assigned to the page, then removed on explicit save.

- Adjust behavior of variable status tags:

  * The error tag remains active even if a status is set.
  * Avoid showing both Error and Incomplete variable statuses when a variable is missing (redundant).

**Monitoring:**

- Adjust data monitoring table:

  * Add pagination.
  * Group columns and stats in two levels for complex multi-phase projects.
  * Show full creation time in monitoring table.

- Improve data monitoring filter controls:

  * Add a search box to filter records by their sequence number, HID or summary values.
  * Improve order and coherency of record status filters with variable statuses.
  * Fix various quirks of the status filter buttons.

- Show per-entry status tags (colored dots) in data monitoring table.

- Use natural sort for thread sequence/HID values instead of plain alphabetical order.

- Fix HID filter failing when integer and string values are mixed in legacy instances.

- Fix small UI problems in legacy instances (missing icon, unstyled menu titles).

**Exports:**

- Use single dialog to create and download exports.

- The *BulkExport* permission can now be used without the *BulkDownload* permission to create an export and download it immediately. This combination allows users to generate new exports but not to download existing (previous) exports.

- Add support for XLSX export templates, which can be used to integrate export data inside pe-exisiting XLSX file, in dedicated sheets.

- Improve dialog for API/export keys. A new code is generated only when the action is explicitly triggered, and a Copy button has been aded.

**Data entry:**

- Improve situational awareness of active thread:

  * Show sequence/HID value in main menu, and alongside actions (on top on big screens, bottom left on small screens).
  * Move "New" record button to data panel instead of putting in inside top menu.
  * Differentiate icon used for new records and draft records without sequence/HID value.

- Improve UI for simple surveys and remote users:

  * Keep action buttons at the bottom for remote surveys, instead of showing the classic *"Save"* button on the right on big screens.
  * Indicate that the record has been saved correctly, by changing the button *"Save"* label to *"Saved"*.
  * Do not mention annotation system in error message when no variable can be annotated.
  * Drop '#' prefix from section titles.

- Reduce content in main menu on small screens to make it usable for more projects without hacks.

- Instantly show delayed errors when loading existing record entry.

- Fix empty items sometimes showing up in table of sections.

- Fix visual bugs in bottom task bar (weird separating line, text and color contrast bugs).

**Admin:**

- Rename instance key and name settings to name and title.

- Add GUI domain settings for password complexity.

- Remove unused user permissions, such as *BuildBatch*. Once the feature works, it will be linked to *DataAudit* instead.

- Fix non-toggle-able admin panels on small screens.

- Prevent splitting legacy projects, because it does not work correctly and we want to kill legacy support eventually.

**Project scripts:**

- Support automatic page locking, triggered on save or after a configurable delay. Only users with *DataAudit* permission can unlock records.

- Replace `claim: false` page option with `forget: true` for clarity. The old option remains supported for now.

- Improve page `sequence` option:

  * Support dynamic page `sequence` option computed by a function, analogous to the `enabled` option.
  * Support use of custom sequence arrays.
  * Return to the status page must now be declared explicitly.

- Pages without data storage (no *store* defined) now work better. Widgets can be added, but there is no default action, and no warning is shown when the page is closed even if it contains data.

- Drop unused Goupile shortcut system (`app.shortcut()`).

- Fix file restore from history using bundled code instead of the original user script.

> [!NOTE]
> Custom `<style>` tags now fail by default because of the Content-Security-Policy. You need to enable the “Allow CSS style elements” project option in the admin panel to fix this.

**Form scripts:**

- Improve support for nested data objects:

  * Support path-like (arrays) data key specifiers
  * Fix broken `form.pushPath()` system for nested data objects

- Fix a bunch of quirks:

  * Fix default widget value not resetting when the default value goes back to null.
  * Fix default actions (such as Save) inheriting leftover disabled option after end of script.
  * Fix retroactive effect of `form.pushOptions` with `form.sameLine`.

- Run page code inside main code path instead of render function. This makes for cleaner code, and errors finally show up when the overview panel is not visible.

- Fix file restore from history using bundled code instead of the original user script.

- Fix various widget highlighting bugs in conception mode.

> [!NOTE]
> Custom `<style>` tags now fail by default because of the Content-Security-Policy. You need to enable the “Allow CSS style elements” project option in the admin panel to fix this.

**Security:**

As part of the NLnet grant, a security audit was performed by [Radically Open Security](https://www.radicallyopensecurity.com/). The security flaws have largely been fixed, and most changes have been implemented, in Goupile 3.11 and in this release.

The report is available here: https://goupile.org/static/nlnet/ros_goupile_2025.pdf

- Increase minimal password length to 10 characters instead of 8.

- Fix security flaw that could allow users without *DataRead* permission to lock records for which they have no claim.

- Fix inability to fetch some pages in split projects even if users are allowed to access suprojects. This regression was introduced in Goupile 3.11.

- Force password change for root/admin users after login if their password is too simple compared to the required complexity.

- Define `Content-Security-Policy` to prevent script and stylesheet injection. However, inline style attributes (*style-src-attr*) remain allowed.

- Set `X-Content-Type-Options` header to "nosniff" to prevent MIME confusion attacks.

- Replace the use of custom headers with `Sec-Fetch-Site` and `Origin` (fallback) to prevent CSRF attacks.

- Prevent injection of control characters from user-controlled strings to the log file.

**Miscellaneous bugs:**

- Whitelist several rarely-used syscalls in Linux seccomp sandbox that could cause a crash in rare occasions:

  * Whitelisted (*restart_syscalls*, *gettimeofday*).
  * Relax *ioctl/tty* seccomp filter to allow more commands.
 
- Fix excessive network isolation in landlock sandboxes that prevented SMTP from working at all on some kernels.

- Fix duplication of instances in split projects after some admin actions.

- Fix PWA caching problems caused by duplicate asset URLs and big assets (such as `esbuild.wasm`, which is only needed for conception) that caused the service worker to throw an exception.

- Send JSON responses with fast Zstd encoding if the browsers support it.

- Fix HTTP listener giving up when too many clients tried to connect and caused exhaustion of file descriptors.

- Fix possible thread sequence number runaway regression that could happen after editing existing records.

- Use POST request for TOTP secret generation.

**Translations:**

- Translate default Goupile project pages.

- Translate binary widget answers, which were still in french enven in english projects.

- Translate Goupile documentation into english: [https://goupile.org/en](https://goupile.org/en).

- Always use *"Close"* button in dialogs instead of *"Cancel"*.

- Various translation fixes and changes.

**Distribution:**

- Provide source tarballs in addition to the existing Debian and RPM, packages and Docker images.

- Clean up default config file used by `goupile init`.

- Add *About* dialog with basic debug info: user agent string, Goupile version, recent errors.

## Goupile 3.11

> [!IMPORTANT]
> The **deployment and initial configuration** of Goupile have changed with this version.
>
> Some settings are not carried over automatically and must be configured graphically after the update.
> After updating, you **must open the Goupile admin panel** to complete the configuration!

### Goupile 3.11.1

*Released on 2025-10-09*

- Avoid page refresh when saving as guest
- Follow sequence navigation when saving as guest
- Fix title style in left menu of legacy instances

### Goupile 3.11

*Released on 2025-10-06*

**Major bug fixes:**

- *[CRITICAL]* Fix JSON parsing crash (buffer overflow) with long number literals
- Protect mail/message code and API from dangerous values
- Protect SMS/message API from weird phone numbers
- Block access to files unless guest or offline mode is enabled
- Fix crashes caused by missing syscalls in sandbox seccomp list
- Fix non-root admin users being able to see list of all projects

**Initial setup and configuration:**

- Provide Docker development (*dev* tag) and release (*latest* tag) images
- Replace `goupile init` system with graphical setup
- Require root user to update public archive key for all domains
- Replace low-level INI domain settings with graphical setup and configuration
- Drop various obscure and/or unused low-level INI settings
- Replace low-level INI SMTP settings with graphical settings
- Add and default to Linux Landlock for filesystem sandboxing
- Rewrite domain and instance initialization code

**New or improved form/page features:**

- Add support for storing files and blobs
- Add proper widget for user-uploaded files
- Add basic user-controlled signup system
- Add public variable system for DELPHI-like studies
- Default to non-secret random counters
- Add stable `cache` object for use by form scripts
- Add per-page autosave support for modern projects
- Add proper support for single-thread token sessions
- Support calling `form.restart()` multiple times
- Improve vertical alignment of side-by-side widgets with empty labels
- Fix `querySelector()` errors caused by some section titles in conception mode

**Data changes and fixes:**

- Preserve and restore LocalDate/LocalTime objects
- Reset sequence number when all records are deleted
- Reduce unwanted occurence of non-consecutive sequence numbers
- Remove implicit draft tag for non-locked records
- Rewrite data annotation format and code
- Serve bundled main and page scripts to end users
- Fix blocking form conception bug in Safari
- Improve coherence between code and widget highlighting

**New export system features:**

- Keep list of past data exports
- Split export permissions in two (*ExportCreate* and *ExportDownload*)
- Support multiple exports modes (all records, new records, changed recorss)
- Add automatic/scheduled data exports

**Multi-language support:**

- Translate Goupile front-end in english and french
- Translate Goupile back-end in english and french
- Add per-instance (or per project) language setting

**UI changes and improvements:**

- Increase size/contrast of radio and checkbox widgets
- Improve navigation between data/form panels in conception mode
- Fix various menu layout issues on small screens
- Fix cases of changing panels when refreshing page
- Properly gray out disabled widgets
- Streamline layout of root "/" Goupile page and mention myself ;)

**Other changes:**

- Stream JSON writes instead of buffering them
- Fix various instance schema migration errors
- Enforce `synchronous = FULL` mode on SQLite databases
- Switch to c-ares (instead of libc/libresolv) for all domain resolutions
- Clean up init of external libraries (curl, libsodium, SQLite)
- Use our own random buffer filling routine in most places
- Support binding HTTP server to specific IP
- Improve HTTP server performance
- Remove unused Vosklet code in goupile

## Goupile 3.10

### Goupile 3.10.2

*Released on 2025-04-05*

- Reduce length of demo project names

### Goupile 3.10.1

*Released on 2025-04-04*

- Increase demo pruning delay to 7 days
- Fix early timeout when creating big archives from admin panel

### Goupile 3.10

*Released on 2025-04-03*

- Add `meta.count()` for custom sequential counters
- Add `meta.random()` for low-level for randomization
- Restore empty record summaries (regression introduced in Goupile 3.9)
- Improve some database schema migration steps
- Fix empty space in main toolbar
- Drop per-store sequence numbers (unused)
- Drop custom signup system (unused)

## Goupile 3.9

### Goupile 3.9.3

*Released on 2025-03-30*

- Add several syscalls to seccomp sandbox for ARM64 compatibility
- Fix startup crash in ARM64 Debian package
- Close demo warning after small delay

### Goupile 3.9.2

*Released on 2025-03-28*

- Keep active record after guest save
- Fix mismatch code/preview widget highlighting
- Fix internal server error related to record claims
- Improve demo mode:
  * Support reuse if random project URL is saved
  * Use JS to create demo instance to avoid project creation caused by HTTP probing
  * Clean up demo instances after two days
- Clean up authentifcation code
- Fix broken Windows build
- Reduce input auto-zoom issues on iPhone

### Goupile 3.9.1

*Released on 2025-02-21*

- Fix sequence value in exports

### Goupile 3.9

*Released on 2025-02-21*

- Fix incoherent completion status with disabled pages
- Implement proper per-thread sequential IDs
- Always show Continue button on sequenced pages
- Add app-global annotation setting
- Fix various variable tag inconsistencies
- Keep legacy record sequence values and HID
- Use simpler version numbers

## Goupile 3.8

### Goupile 3.8.3

*Released on 2025-02-17*

- Fix "page is undefined" error when navigating some pages
- Remove column masking heuristic from legacy exporter
- Include multi-valued null columns in legacy exports
- Keep ULID suffix in virtual export tables
- Fix migration FKEY errors with split projects and deleted records
- Fix SQL error when restoring archive
- Add root endpoints to exit goupile process
- Send internal server error details to root users

### Goupile 3.8.2

*Released on 2024-12-29*

- Fix possible infinite loop after failed HTTP request
- Hide forget action when autosave is in use
- Refuse compressed HTTP request bodies

### Goupile 3.8.1

*Released on 2024-12-20*

- Skip confusing Save button when Next is available
- Fix concurrency issue between autosave and confirm in legacy instances
- Replace code icon with design icon in legacy instances
- Update browser version requirements

### Goupile 3.8.0

*Released on 2024-12-20*

- Drop *New* user permission in favor of page `claim` option
- Improve support for dynamic `enabled` page option
- Add support for optional confirmation when saving page (option `confirm`)
- Always show status suffix in left menu
- Use different icons for conception menu and code panel
- Fix error when emptying user email or phone
- Fix stuck loading when record does not exist
- Avoid erroneous "confirm changes" dialog after code change
- Drop ill-designed export filter and dialog options
- Reduce unexpected errors caused by duplicate file changes
- Avoid noisy "single SQLite value" error when saving legacy records

## Goupile 3.7

### Goupile 3.7.7

*Released on 2024-11-23*

- Fix unsorted instances leading to duplicates and deadlocks (regression introduced in Goupile 3.7.3)

### Goupile 3.7.6

*Released on 2024-11-16*

- Enable development mode by default for demo instances
- Switch to form editor tab until it is explicitly changed
- Fix possible crash on mode change

### Goupile 3.7.5

*Released on 2024-11-12*

- Fix recurent crash in demo mode

### Goupile 3.7.4

*Released on 2024-11-12*

- Fix premature HTTP timeout when exporting data from legacy instance

### Goupile 3.7.3

*Released on 2024-11-12*

- Fix deadlocks after password change and TOTP activation
- Fix badly ordered sub-projects in admin panel
- Fix errors with empty email and phone user attributes

### Goupile 3.7.2

*Released on 2024-11-12*

- Restore missing Administration link in profile menu
- Switch to proper export buttons in legacy instances

### Goupile 3.7.1

*Released on 2024-11-07*

- Adjust default password constraints:
  * Moderate for normal users and project administrators
  * Hard for root users

### Goupile 3.7.0

*Released on 2024-10-31*

- Change `thread` object in scripts: `thread['form']` becomes `thread.entries['form']`
- Support custom category-level scripts
- Support custom content in form level elements
- Support custom form tile icons
- Increase height of top and bottom menu bar
- Center mobile action buttons when relevant
- Show home icon before main menu item
- Make menu categories clickables
- Switch to spaced buttons for enumerations
- Scroll to top on page change
- Simplify automatic sequencing of forms
- Support setting custom classes for most widgets
- Fix empty space at the bottom of forms on mobiles
- Improve styling and text of error boxes
- Fix broken "changed" attribute for widgets
- Fix broken `form.time()` widget in legacy Goupile
- Fix error tags showing up for delayed errors
- Reduce available annotation statuses in locked mode
- Target ES 2022 for `top-level await` support
- Introduce basic mail signup system

## Goupile 3.6

### Goupile 3.6.4

*Released on 2024-09-05*

- Fix default panels when building and opening URLs
- Fix infinite loop when assembling export structure
- Fix FOREIGN KEY bug when saving data in multi-center projects
- Fix incomplete app homepage option
- Fix disabled "Publish" button after adding file
- Switch to new HTTP server code in goupile
- Use mimalloc on Linux and BSD systems

### Goupile 3.6.3

*Released on 2024-07-26*

- Fix a possible crash when sending mails
- Expose hash and CRC32 functions to scripts
- Expose more utility functions to scripts

### Goupile 3.6.2

*Released on 2024-07-11*

- Fix navigation bugs in goupile (including main "Add" button not working)

### Goupile 3.6.1

*Released on 2024-07-10*

- Expose thread object in user scripts to access sibling entries
- Fix mandatory markers showing up on calc widgets

### Goupile 3.6.0

*Released on 2024-06-28*

- Add basic entry summary to replace old HID system
- Show list of entries when normal users connect
- Fix broken export endpoint for legacy instances
- Fix various offline cache and profile sync bugs
- Fix undefined username in initial password change dialog
- Fix broken automatic relogin after expiration
- Fix database migration error from some Goupile versions
- Fix some parsing errors in save endpoints
- Fix infinite spinner for late startup errors
- Clean up lock related code in the client

## Goupile 3.5

### Goupile 3.5.3

*Released on 2024-06-20*

- Fix compatiblity with legacy server-side locks
- Fix various client-side locking bugs
- Allow scripts to use lit `until` directive

### Goupile 3.5.2

*Released on 2024-06-20*

- Fix unstyled widget labels in legacy instances
- Fix esbuild.wasm fetch for non-Brotli content encodings
- Use identity when Content-Encoding is missing or empty
- Build Debian packages for ARM64

### Goupile 3.5.1

*Released on 2024-06-18*

- Add pushPath/popPath system to replace "path" widget option
- Use legacy form code in legacy instances
- Drop forced low-complexity password change for now
- Fix failure to enable develop mode in multi-center projects
- Expose PeriodPicker widget to legacy instances
- Clean up leftover debug statements

### Goupile 3.5.0

*Released on 2024-06-17*

- Support running legacy (v2) Goupile projects
- Bundle page scripts with esbuild to support import and provide better syntax error reports
- Fix non-unique HTML ids in generated forms
- Fix various offline bugs

## Goupile 3.4

### Goupile 3.4.0

*Released on 2024-03-22*

- Simplify goupile top menu
- Change automatic form page with separate levels and tiles
- Improve goupile task menu on small screens
- Refuse excessive password lengths
- Fix export key error when guest sessions are enabled
- Automatically create session when guest saves data
- Improve page redirection in sequence mode

## Goupile 3.3

### Goupile 3.3.6

*Released on 2024-02-08*

- Improve data migration from goupile2

### Goupile 3.3.5

*Released on 2024-01-26*

- Reduce permissions for demo users
- Assign export keys to master instance

### Goupile 3.3.4

*Released on 2024-01-24*

- Add option to disable SQLite snapshots
- Disable SQLite snapshots in demo domains

### Goupile 3.3.3

*Released on 2024-01-22*

- Warn user about demo mode
- Fix HTTP property being set from Defaults section

### Goupile 3.3.2

*Released on 2024-01-22*

- Add demo mode to Goupile
- Show username on password change dialog

### Goupile 3.3.1

*Released on 2024-01-22*

- Add hover effect to tiles
- Disable Publish button if main script does not work

### Goupile 3.3.0

*Released on 2024-01-20*

- Show completion tiles for groups of forms
- Show first-level columns and stats in data table
- Show basic record status in menus
- Support basic form sequence configuration
- Reintroduce support for page filename option
- Keep explicitly defined single-child menu relations
- Fix missing menu items in some cases
- Fix panel blink when clicking on current page menu item
- Drop broken offline login code for now
- Fix crash when requesting '/manifest.json'
- Improve main.js fallback code when guest sessions are enabled

## Goupile 3.2

### Goupile 3.2.3

*Released on 2024-01-17*

- Fix UI deadlock in develop mode

### Goupile 3.2.2

*Released on 2024-01-16*

- Fix support for repeated sections and notes
- Fix undefined variable error while exporting
- Fix minor deadlock issues

### Goupile 3.2.1

*Released on 2024-01-16*

- Fix missing code for record deletion
- Improve selection of visible data columns
- Handle modification time on the server
- Build with JavaScriptCore

### Goupile 3.2.0

*Released on 2024-01-14*

- Add configurable password complexity checks to goupile
- Fix generic 'NetworkError is not defined' error
- Fix error when creating multi-centric projects
- Fix possible NULL dereference in goupile login code
- Allow reset of email and phone in goupile admin panel

# Roadmap

## Goupile 3.13

- Improve export API to provide computed set of columns directly.
- Improve export of nested objects and arrays.
- Support bulk recompute API and functionality.
- Improve and document import API.
- Export/import project structure in ZIP file.

## Goupile 3.14

- Add page and permission groups to send subsets of thread metadata and data to users. This allow, for example, the implementation of a contact module.
- Support SSO authentication with OIDC.
- Support SCIM user provisioning.

## Goupile 3.15

- Support error codes for data-query-like system.
- Provide clear tablr of blocking and non-blocking errors.
- Improve data audit tools: merge records, split records.

## Goupile 3.16

- Rework offline support with new UI and new code.
- Restrict offline use to new data entry, drop mirror system entirely.

## Goupile 3.17

- Implement proper merging of data when changed by multiple users.
- Support collaborative edition of form scripts.
- Use WebSockets for realtime update of data and scripts, and see who is doing what.

## Goupile 4 (2027)

- Drop support for legacy (v2) instances.
