# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

hm_init <- function() {
    .Call(`heimdallR_Init`)
}

hm_add_events <- function(inst, source, values, keys) {
    .Call(`heimdallR_AddEvents`, inst, source, values, keys)
}

hm_add_measures <- function(inst, source, values, keys) {
    .Call(`heimdallR_AddMeasures`, inst, source, values, keys)
}

hm_add_periods <- function(inst, source, values, keys) {
    .Call(`heimdallR_AddPeriods`, inst, source, values, keys)
}

hm_set_concepts <- function(inst, name, concepts) {
    .Call(`heimdallR_SetConcepts`, inst, name, concepts)
}

hm_run <- function(inst) {
    .Call(`heimdallR_Run`, inst)
}

hm_run_sync <- function(inst) {
    .Call(`heimdallR_RunSync`, inst)
}

hm_stop <- function(inst) {
    .Call(`heimdallR_Stop`, inst)
}
