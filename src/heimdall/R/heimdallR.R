# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

#' Open database file for use in subsequent heimdall R functions
#'
#' @details
#' This function creates the file unless create = FALSE is used.
#'
#' Create it the heimdall server directory to make the project available in the web app.
#' When installed by the debian or RPM package, the directory is `/var/lib/heimdall/projects`.
#' Use the *.db* extension or the server will not see the file!
#'
#' The server probably runs with another UNIX user (such as "heimdall"). To reduce UNIX permission
#' issues, the project directory uses the g+s permission flag and the R function sets the file
#' project mode as 0664. This permission extends to other SQLite files such as the WAL and SHM files.
#'
#' In practice, this simply means that any user in the *heimdall* group (or whatever group
#' */var/lib/heimdall/projects* belongs to) will be able to read and write project databases in
#' this directory.
#'
#' @param filename Database filename, use '.db' extension to be visible by the server.
#' @param create Create the database if it does not exist.
#' @return An instance object that can be used with other heimdall functions.
#'
#' @md
hm_open <- function(filename, create = TRUE) {
    .Call(`hmR_Open`, filename, create)
}

#' Close database file
#'
#' @details
#' Once closed, an instance object must not be used with other Heimdall functions.
#' Trying to do so will trigger an error.
#'
#' @param inst Instance object, see [hm_open()].
#' @param create Create the database if it does not exist.
#' @return An instance object that can be used with other heimdall functions.
#'
#' @md
hm_close <- function(inst) {
    .Call(`hmR_Close`, inst)
}

#' Clear all data inside database
#'
#' @details
#' This functions clear domains, views and entity elements (events, periods, values)
#' from the project database.
#'
#' Entity marks added by Heimdall users are kept in a detached state in the database
#' by default, unless `marks = TRUE` is used.
#'
#' @param inst Instance object, see [hm_open()].
#' @param marks Delete entity marks too.
#'
#' @md
hm_reset <- function(inst, marks = FALSE) {
    .Call(`hmR_Reset`, inst, marks)
}

#' Create or replace domain
#'
#' @details
#' A domain is a set of related concepts, tipically in a related domain (such as medical laboratory values),
#' to which each entity element (event, period or value) is related.
#'
#' As an example, medical laboratory values can be linked to a LOINC domain, with one concept per LOINC
#' code name. Same thing for the ATC terminology and pharmaceutical prescriptions, and more.
#'
#' To create or update a domain and the related concepts, specifiy the *name* and a *data.frame of concepts*
#' with the following columns:
#'
#' - **name**: Concept code/key, to which entity elements will be linked
#' - **description**: Optional concept description (visible in view editor)
#' - **path**: Optional path if you want to create a default view for this domain
#'
#' If a domain with the same name already exists, it will be replaced.
#'
#' If the *path* column is specified, a default view with the same name as this domain will be created.
#' See [hm_set_view()] for more information.
#'
#' @param inst Instance object, see [hm_open()].
#' @param name Name of the domain.
#' @param concepts Data.frame of concepts, columns: *name*, *description*, *path*.
#'
#' @examples
#' \donttest{
#' library(tibble)
#'
#' inst <- hm_open('test.db')
#'
#' hm_set_domain(inst, 'LOINC', tribble(
#'     ~name, ~description,
#'    '2340-8', 'Glucose (automated strip)',
#'    '16931-8', 'Hematocrit',
#'    #...
#' ))
#' }
#' @md
hm_set_domain <- function(inst, name, concepts) {
    .Call(`hmR_SetDomain`, inst, name, concepts)
}

#' Create or replace view
#'
#' @details
#' A view defines the levels of the tree in which entity elements are arranged and
#' displayed to the user.
#'
#' To create a view, specify the *name* and a *data.frame of items* with the following columns:
#'
#' - **path**: path (starting with '/') of the item, parent nodes will be created
#' - **domain**: name of the domain (see [hm_set_domain()]) in which the concept can be found
#' - **concept**: concept in the domain
#'
#' If a view with the same name already exists, it will be replaced.
#'
#' Paths work like UNIX file paths, the '/' split character defines parent nodes and leaves. Just like
#' with filenames, each level cannot use the '/' character.
#'
#' For example, a path like `/Biology/Chemistry/Glucose` will be displayed like this:
#'
#' ```
#' .
#' └── Biology
#'     └── Chemistry
#'         └── Glucose
#' ```
#'
#' To put multiple concepts in the same leave (full path), simply repeat the path for each pair
#' of domain/concept.
#'
#' @param inst Instance object, see [hm_open()].
#' @param name Name of the view
#' @param concepts Data.frame of items, columns: *path*, *domain*, *concept*.
#'
#' @examples
#' \donttest{
#' library(tibble)
#'
#' inst <- hm_open('test.db')
#'
#' # View to study link between testosterone and glucose level + hematocrit
#' # Always beware of confounding factors!
#'
#' hm_set_view(inst, 'Testosterone effects', tribble(
#'     ~path, ~domain, ~concept,
#'     '/Drugs/Testosterone', 'ATC', 'G03BA03',
#'     '/Biology/Glucose', 'LOINC', '2340-8',
#'     '/Biology/Hematocrit', 'LOINC', '16932-8'
#' ))
#' }
#' @md
hm_set_view <- function(inst, name, items) {
    .Call(`hmR_SetView`, inst, name, items)
}

#' Add events to entities
#'
#' @details
#' In Heimdall, each entity contains three types of elements: events, periods, and values.
#'
#' Events are the simplest type of element. Each event has a timestamp, a domain/concept pair, and
#' an optional alert flag.
#'
#' Graphically, they show up as small triangles, blue or orange depending on the alert flag.
#'
#' To add events to entities, use a data.frame with the following columns:
#'
#' - **entity**: entity name (it will be created if it does not exist yet)
#' - **domain**: name of the domain (see [hm_set_domain()]) in which the concept can be found
#' - **concept**: concept in the domain
#' - **time**: numeric timestamp, or POSIXct values
#' - **alert** (optional): logical vector which can be set to TRUE to show tje event with an alert (or warning)
#'
#' If an entity with the specified name does not exist, it is created. In fact, the only way to create
#' an entity in Heimdall is to add elements to a non-existent entity, which gets created.
#'
#' When an entity name is encountered for the **first time during the call** to hm_add_events(), all existing
#' events for this entity are deleted unless `reset = FALSE` is used (defaults to TRUE). This is done this way
#' to avoid repeated events when running the same script multiple times.
#'
#' If a concept does not exist in the specified domain, a warning will be issued and the event will be ignored.
#' Use `strict = TRUE` to throw an error and revert changes instead.
#'
#' @md
hm_add_events <- function(inst, events, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddEvents`, inst, events, reset, strict)
}

#' Add periods to entities
#'
#' @details
#' In Heimdall, each entity contains three types of elements: events, periods, and values.
#'
#' Unlike events, which happen at a single point in time, periods extend for a specified duration.
#' You cannot set an alert on a period (unlike events and values), but you can use a custom color which will
#' show up in the web app.
#'
#' To add periods to entities, use a data.frame with the following columns:
#'
#' - **entity**: entity name (it will be created if it does not exist yet)
#' - **domain**: name of the domain (see [hm_set_domain()]) in which the concept can be found
#' - **concept**: concept in the domain
#' - **time**: numeric timestamp, or POSIXct values
#' - **duration**: numeric duration
#' - **color** (optional): CSS color to use, named color or hex value, see [MDN documentation](https://developer.mozilla.org/en-US/docs/Web/CSS/color_value))
#'
#' If an entity with the specified name does not exist, it is created. In fact, the only way to create
#' an entity in Heimdall is to add elements to a non-existent entity, which gets created.
#'
#' When an entity name is encountered for the **first time during the call** to hm_add_periods(), all existing
#' periods for this entity are deleted unless `reset = FALSE` is used (defaults to TRUE). This is done this way
#' to avoid repeated periods when running the same script multiple times.
#'
#' If a concept does not exist in the specified domain, a warning will be issued and the period will be ignored.
#' Use `strict = TRUE` to throw an error and revert changes instead.
#'
#' @md
hm_add_periods <- function(inst, periods, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddPeriods`, inst, periods, reset, strict)
}

#' Add values (or measures) to entities
#'
#' @details
#' In Heimdall, each entity contains three types of elements: events, periods, and values.
#'
#' Values happen a specified time, and have a specific value. They can be used for measures, such as laboratory values.
#'
#' To add values to entities, use a data.frame with the following columns:
#'
#' - **entity**: entity name (it will be created if it does not exist yet)
#' - **domain**: name of the domain (see [hm_set_domain()]) in which the concept can be found
#' - **concept**: concept in the domain
#' - **time**: numeric timestamp, or POSIXct values
#' - **value**: actual value, it can be anything (obviously but the unit should match whatever is relevant for the concept used)
#' - **alert** (optional): logical vector which can be set to TRUE to show the event with an alert (or warning)
#'
#' If an entity with the specified name does not exist, it is created. In fact, the only way to create
#' an entity in Heimdall is to add elements to a non-existent entity, which gets created.
#'
#' When an entity name is encountered for the **first time during the call** to hm_add_values(), all existing
#' values for this entity are deleted unless `reset = FALSE` is used (defaults to TRUE). This is done this way
#' to avoid repeated values when running the same script multiple times.
#'
#' If a concept does not exist in the specified domain, a warning will be issued and the value will be ignored.
#' Use `strict = TRUE` to throw an error and revert changes instead.
#'
#' @md
hm_add_values <- function(inst, values, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddValues`, inst, values, reset, strict)
}

#' Delete specified domains from project database
#'
#' @details
#' This functions deletes the domains with the specified names. You can delete one or more
#' domains at the same time.
#'
#' The deletion is transactional; if a domain cannot be deleted, the function fails and all
#' domains are left intact.
#'
#' @param inst Instance object, see [hm_open()].
#' @param names Character vector with one or more domain names to delete.
#'
#' @md
hm_delete_domains <- function(inst, names) {
    .Call(`hmR_DeleteDomains`, inst, names)
}

#' Delete specified views from project database
#'
#' @details
#' This functions deletes the views with the specified names. You can delete one or more
#' views at the same time.
#'
#' The deletion is transactional; if a view cannot be deleted, the function fails and all
#' views are left intact.
#'
#' @param inst Instance object, see [hm_open()].
#' @param names Character vector with one or more view names to delete.
#'
#' @md
hm_delete_views <- function(inst, names) {
    .Call(`hmR_DeleteViews`, inst, names)
}

#' Delete specified entity elements (events, periods, values) from project database
#'
#' @details
#' This functions deletes all events, periods and values related to the specified entities.
#' You can delete one or more entities at the same time.
#'
#' The deletion is transactional; if an entity cannot be deleted, the function fails and all
#' entities are left intact.
#'
#' @param inst Instance object, see [hm_open()].
#' @param names Character vector with one or more entity names to delete.
#'
#' @md
hm_delete_entities <- function(inst, names) {
    .Call(`hmR_DeleteEntities`, inst, names)
}

#' Export entity marks
#'
#' @details
#' Heimdall users can mark each entity with a status (positive, negative or undecided) and a
#' text comment.
#'
#' Use this function to export a data.frame with the active entity marks, with the following columns:
#'
#' - **id**: Mark ID (used for pagination as explained below)
#' - **name**: Name of entity
#' - **timestamp**: Date of last modification
#' - **status**: `TRUE` for positive marks, `FALSE` for negative marks, `NA` if undecided
#' - **comment**: Optional text comment (`NA` is empty)
#'
#' This function only exports a limited number of marks, 10000 by default. To export
#' more marks, set the *first* argument to the ID of the last mark + 1, as shown in the
#' example below. 
#'
#' @param inst Instance object, see [hm_open()].
#' @param first ID of the first mark to export, or NULL to export from start.
#' @param limit Maximum number of exported marks.
#' @return A data.frame() with exported entity marks.
#'
#' @examples
#' \donttest{
#' library(heimdallR)
#'
#' inst <- hm_open('test.db')
#'
#' # Get first 10000 marks
#' marks <- hm_export_marks(inst)
#'
#' # Paginate until marks is complete
#' page <- marks
#' while (nrow(page) > 0) {
#'     page <- hm_export_marks(inst, first = tail(page$id, 1) + 1)
#'     marks <- rbind(marks, page)
#' }
#'
#' print(marks)
#' }
#' @md
hm_export_marks <- function(inst, first = NULL, limit = 10000) {
    .Call(`hmR_ExportMarks`, inst, first, limit)
}
