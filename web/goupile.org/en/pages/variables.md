# Available objects

Goupile provides several objects that can be used in scripts. Some have already been described in this documentation:

- The `app` object is used to define the [project structure](app) in the project script, or to access it from within page scripts.
- The `form` object is used to add [widgets](widgets) in page scripts.
- The `meta` object provides access to functions related to [identifiers](identifiers), summaries, and randomization.

Other objects include:

- The `thread` object gives access to the record status and [data from other pages](#data-from-other-pages).
- The `profile` object contains information about the [logged-in user's profile](#user-profile) and permissions.

# Data from other pages

## Usage contexts

The `thread` object can be used in page scripts to retrieve values from other pages. For example, you can retrieve a participant's age entered on the inclusion page and use it to adjust displayed questions on another page.

It is not available when the project script runs. However, the `thread` object is available when evaluating dynamic page properties—such as `enabled`, used to define [conditional pages](app#conditional-pages)).

## Properties of thread

The `thread` object contains the following properties:

```js
{
    tid: "01K8RCXZ4BPS272HHHWFM2T0S8", // Random TID identifier for each record
    saved: false,                      // True if at least one page is saved (and persisted in the database)
    locked: false,                     // True if the record is locked (automatically or by a user with audit rights)

    entries: {},                       // Contains entries for different stores/pages (see below)
    data: {},                          // Contains data for different stores/pages (see below)

    counters: {}                       // Contains values of visible (non-secret) counters
}
```

### thread.entries

> [!NOTE]
> Form pages created via `app.form()` each use a dedicated *store*, typically named after the page itself. 
>
> For simplicity, this documentation assumes each page stores its data in a store with the same name.

Use `thread.entries.name` to access different *stores* (i.e., pages in most Goupile use cases). For example, data and metadata for a page named "inclusion" are available under `thread.entries.inclusion`.

Each entry has the following structure:

```js
{
    summary: null,        // Summary value of the page
    ctime: 1761750347739, // Creation timestamp (Unix, ms)
    mtime: 1761750347739, // Modification timestamp (Unix, ms)
    data: {},             // Access to page data
    anchor: null,         // Null for unsaved entries; numeric otherwise
    store: "name",        // Store name, usually same as page key
    tags: [],             // List of tags aggregated from variable tags
    siblings: [],         // List of sibling entries in 1-to-N relations
}
```

To access variables from another page, use the `data` property of the relevant entry. For example, you can retrieve the age entered on the inclusion page with `thread.entries.inclusion.data.age`.

> [!IMPORTANT]
> The entry for a page is `null` if the page has not been saved yet. Be careful when trying to access a page that may not exist yet.
>
> Use optional chaining operators to avoid errors:, like so: `thread.entries.inclusion?.data?.age`. This ensures that if the page doesn’t exist, the result is `null` instead of an error.

### thread.data

Compared to `thread.entries`, the `thread.data` property simplifies access to data from other pages:

- Variables from each page are directly accessible.
- Unsaved pages are represented by empty objects rather than `null`.

For example, both lines below access the same value:

```js
let age1 = thread.entries.inclusion?.data?.age
let age2 = thread.data.inclusion.age

console.log(age1 == age2) // true
```

### thread.counters

Goupile offers [custom counters](identifiers#custom-counters) that can be visible (public) or secret.

The values of visible counters are available via `thread.counters`. FFor example, you can access the value of a counter named "group" with `thread.counters.group`.

# User profile

## Usage contexts

The `profile` object can be used in both the project and page scripts. It contains information about the user: their ID, username, and the user permissions for the current project and center.

Among other things, you can use this object to:

- Add a page visible only to users with audit rights (`data_audit: true`).
- Pre-fill a form field with the username.

## Available properties

The `profile` object has the following structure:

```js
{
    userid: 42,        // Internal user ID
    username: "niels", // Username
    develop: false,    // True in conception mode, false otherwise
    online: true,      // True if online, false when running offline
    permissions: {},   // Each key is a permission (e.g., data_read) with a true/false value
    root: false        // True if the user is a root (super-admin) user
}
```
