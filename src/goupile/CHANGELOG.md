# Changelog

## Goupile 3.3.2

- Add demo mode to Goupile
- Show username on password change dialog

## Goupile 3.3.1

- Add hover effect to tiles
- Disable Publish button if main script does not work

## Goupile 3.3

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

## Goupile 3.2.3

- Fix UI deadlock in develop mode

## Goupile 3.2.2

- Fix support for repeated sections and notes
- Fix undefined variable error while exporting
- Fix minor deadlock issues

## Goupile 3.2.1

- Fix missing code for record deletion
- Improve selection of visible data columns
- Handle modification time on the server
- Build with JavaScriptCore

## Goupile 3.2

- Add configurable password complexity checks to goupile
- Fix generic 'NetworkError is not defined' error
- Fix error when creating multi-centric projects
- Fix possible NULL dereference in goupile login code
- Allow reset of email and phone in goupile admin panel

## Goupile 3.1.1

- Fix error when using UNIX socket for HTTP with `goupile --sandbox`
- Fix broken support for record claims
