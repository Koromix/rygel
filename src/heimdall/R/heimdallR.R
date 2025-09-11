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

hm_open <- function(filename) {
    .Call(`hmR_Open`, filename)
}

hm_close <- function(inst) {
    .Call(`hmR_Close`, inst)
}

hm_set_domain <- function(inst, name, concepts) {
    .Call(`hmR_SetDomain`, inst, name, concepts)
}

hm_set_view <- function(inst, name, items) {
    .Call(`hmR_SetView`, inst, name, items)
}

hm_add_events <- function(inst, events, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddEvents`, inst, events, reset, strict)
}

hm_add_periods <- function(inst, periods, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddPeriods`, inst, periods, reset, strict)
}

hm_add_values <- function(inst, values, reset = TRUE, strict = FALSE) {
    .Call(`hmR_AddValues`, inst, values, reset, strict)
}

hm_delete_domain <- function(inst, name) {
    .Call(`hmR_DeleteDomain`, inst, name)
}

hm_delete_view <- function(inst, name) {
    .Call(`hmR_DeleteView`, inst, name)
}

hm_delete_entities <- function(inst, names) {
    .Call(`hmR_DeleteEntities`, inst, names)
}
