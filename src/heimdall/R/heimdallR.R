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
