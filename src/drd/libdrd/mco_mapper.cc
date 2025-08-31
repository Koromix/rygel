// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "mco_classifier.hh"
#include "mco_mapper.hh"

namespace K {

struct MapperContext {
    const mco_TableIndex *index;

    HashMap<Size, uint64_t> warn_cmd28_jumps_cache;
};

static bool MergeConstraint(const mco_TableIndex &index,
                            const mco_GhmCode ghm, mco_GhmConstraint constraint,
                            HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
#define MERGE_CONSTRAINT(ModeChar, DurationMask, RaacMask) \
        do { \
            mco_GhmConstraint new_constraint = constraint; \
            new_constraint.ghm.parts.mode = (char)(ModeChar); \
            new_constraint.durations &= (DurationMask); \
            new_constraint.raac_durations = constraint.durations & (RaacMask); \
            if (new_constraint.durations) { \
                bool inserted; \
                mco_GhmConstraint *ptr = out_constraints->TrySet(new_constraint, &inserted); \
                if (!inserted) { \
                    ptr->cmds |= new_constraint.cmds; \
                    ptr->durations |= new_constraint.durations; \
                    ptr->raac_durations |= new_constraint.raac_durations; \
                    ptr->warnings &= new_constraint.warnings; \
                } \
            } \
        } while (false)

    constraint.ghm = ghm;

    const mco_GhmRootInfo *ghm_root_info = index.FindGhmRoot(ghm.Root());
    if (!ghm_root_info) {
        LogError("Unknown GHM root '%1'", ghm.Root());
        return false;
    }

    if (ghm_root_info->allow_ambulatory) {
        MERGE_CONSTRAINT('J', 0x1, 0);
        // Update base mask so that following GHM can't overlap with this one
        constraint.durations &= ~(uint32_t)0x1;
    }
    if (ghm_root_info->short_duration_threshold) {
        uint32_t short_mask = (uint32_t)(1 << ghm_root_info->short_duration_threshold) - 1;
        MERGE_CONSTRAINT('T', short_mask, 0);
        constraint.durations &= ~short_mask;
    }

    if (ghm.parts.mode != 'J' && ghm.parts.mode != 'T') {
        if (!ghm.parts.mode) {
            for (int severity = 0; severity < 4; severity++) {
                uint32_t mode_mask = (uint32_t)(1 << mco_GetMinimalDurationForSeverity(severity)) - 1;

                if (ghm_root_info->allow_raac) {
                    MERGE_CONSTRAINT('1' + severity, UINT32_MAX, mode_mask);
                } else {
                    MERGE_CONSTRAINT('1' + severity, ~mode_mask, 0);
                }
            }
        } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
            int severity = ghm.parts.mode - 'A';
            uint32_t mode_mask = (uint32_t)(1 << mco_GetMinimalDurationForSeverity(severity)) - 1;

            if (ghm_root_info->allow_raac) {
                MERGE_CONSTRAINT(ghm.parts.mode, UINT32_MAX, mode_mask);
            } else {
                MERGE_CONSTRAINT(ghm.parts.mode, ~mode_mask, 0);
            }
        } else {
            MERGE_CONSTRAINT(ghm.parts.mode, UINT32_MAX, 0);
        }
    }

#undef MERGE_CONSTRAINT

    return true;
}

static bool RecurseGhmTree(MapperContext &ctx, Size depth, Size node_idx,
                           mco_GhmConstraint constraint,
                           HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
    // This limit is arbitrary, quick tests show depth maxing at less than 100 so we
    // should be alright. If this becomes a problem, I'll rewrite this function to
    // avoid recursion.
    K_ASSERT(depth < 4096);

#define RUN_TREE_SUB(ChildIdx, ChangeCode) \
        do { \
            mco_GhmConstraint constraint_copy = constraint; \
            constraint_copy.ChangeCode; \
            success &= RecurseGhmTree(ctx, depth + 1, ghm_node.u.test.children_idx + (ChildIdx), \
                                      constraint_copy, out_constraints); \
        } while (false)

    K_ASSERT(node_idx < ctx.index->ghm_nodes.len);
    const mco_GhmDecisionNode &ghm_node = ctx.index->ghm_nodes[node_idx];

    bool success = true;

    switch (ghm_node.function) {
        case 0:
        case 1: {
            if (ghm_node.u.test.params[0] == 0) {
                for (Size i = 0; i < ghm_node.u.test.children_count; i++) {
                    uint32_t cmd_mask = 1u << i;
                    RUN_TREE_SUB(i, cmds &= cmd_mask);
                }

                return true;
            } else if (ghm_node.u.test.params[0] == 1) {
                uint64_t warn_cmd28_jumps;
                {
                    bool inserted;
                    uint64_t *ptr = ctx.warn_cmd28_jumps_cache.TrySet(node_idx, 0, &inserted);
                    if (inserted) {
                        warn_cmd28_jumps = UINT64_MAX;
                        K_ASSERT(ghm_node.u.test.children_count <= 64);
                        for (const mco_DiagnosisInfo &diag_info: ctx.index->diagnoses) {
                            if (constraint.cmds & (1u << diag_info.cmd) &&
                                    !(diag_info.raw[8] & 0x2)) {
                                warn_cmd28_jumps &= ~(1ull << diag_info.raw[1]);
                            }
                        }

                        *ptr = warn_cmd28_jumps;
                    } else {
                        warn_cmd28_jumps = *ptr;
                    }
                }

                for (Size i = 0; i < ghm_node.u.test.children_count; i++) {
                    uint32_t warning_mask = 0;
                    if (warn_cmd28_jumps & (1ull << i)) {
                        warning_mask |= (int)mco_GhmConstraint::Warning::PreferCmd28;
                    }
                    RUN_TREE_SUB(i, warnings |= warning_mask);
                }

                return true;
            }
        } break;

        case 12: {
            success &= MergeConstraint(*ctx.index, ghm_node.u.ghm.ghm, constraint, out_constraints);
            return success;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            if (param >= 31) {
                LogError("Incomplete GHM constraint due to duration >= 31 nights");
                success = false;
                break;
            }

            uint32_t test_mask = ((uint32_t)1 << param) - 1;
            RUN_TREE_SUB(0, durations &= ~test_mask);
            RUN_TREE_SUB(1, durations &= test_mask);

            return success;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            if (param >= 31) {
                LogError("Incomplete GHM constraint due to duration >= 31 nights");
                success = false;
                break;
            }

            uint32_t test_mask = (uint32_t)1 << param;
            RUN_TREE_SUB(0, durations &= ~test_mask);
            RUN_TREE_SUB(1, durations &= test_mask);

            return success;
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            if (param != 0) {
                LogError("Incomplete GHM constraint due to session count != 0");
                success = false;
                break;
            }

            RUN_TREE_SUB(0, durations &= 0x1);
            RUN_TREE_SUB(1, durations &= UINT32_MAX);

            return success;
        } break;
    }

    // Default case, for most functions and in case of error
    for (Size i = 0; i < ghm_node.u.test.children_count; i++) {
        success &= RecurseGhmTree(ctx, depth + 1, ghm_node.u.test.children_idx + i,
                                  constraint, out_constraints);
    }

#undef RUN_TREE_SUB

    return success;
}

bool mco_ComputeGhmConstraints(const mco_TableIndex &index,
                               HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
    K_ASSERT(!out_constraints->count);

    MapperContext ctx;
    ctx.index = &index;

    mco_GhmConstraint null_constraint = {};
    null_constraint.cmds = UINT32_MAX;
    null_constraint.durations = UINT32_MAX;

    return RecurseGhmTree(ctx, 0, 0, null_constraint, out_constraints);
}

}
