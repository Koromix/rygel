// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_classifier.hh"
#include "mco_mapper.hh"

static bool MergeConstraint(const mco_TableIndex &index,
                            const mco_GhmCode ghm, mco_GhmConstraint constraint,
                            HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
#define MERGE_CONSTRAINT(ModeChar, DurationMask) \
        do { \
            mco_GhmConstraint new_constraint = constraint; \
            new_constraint.ghm.parts.mode = (char)(ModeChar); \
            new_constraint.duration_mask &= (DurationMask); \
            if (new_constraint.duration_mask) { \
                std::pair<mco_GhmConstraint *, bool> ret = out_constraints->Append(new_constraint); \
                if (!ret.second) { \
                    ret.first->duration_mask |= new_constraint.duration_mask; \
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
        MERGE_CONSTRAINT('J', 0x1);
        // Update base mask so that following GHM can't overlap with this one
        constraint.duration_mask &= ~(uint32_t)0x1;
    }
    if (ghm_root_info->short_duration_treshold) {
        uint32_t short_mask = (uint32_t)(1 << ghm_root_info->short_duration_treshold) - 1;
        MERGE_CONSTRAINT('T', short_mask);
        constraint.duration_mask &= ~short_mask;
    }

    if (ghm.parts.mode != 'J' && ghm.parts.mode != 'T') {
        if (!ghm.parts.mode) {
            for (int severity = 0; severity < 4; severity++) {
                uint32_t mode_mask = ~((uint32_t)(1 << mco_GetMinimalDurationForSeverity(severity)) - 1);
                MERGE_CONSTRAINT('1' + severity, mode_mask);
            }
        } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
            int severity = ghm.parts.mode - 'A';
            uint32_t mode_mask = ~((uint32_t)(1 << mco_GetMinimalDurationForSeverity(severity)) - 1);
            MERGE_CONSTRAINT('A' + severity, mode_mask);
        } else {
            MERGE_CONSTRAINT(ghm.parts.mode, UINT32_MAX);
        }
    }

#undef MERGE_CONSTRAINT

    return true;
}

static bool RecurseGhmTree(const mco_TableIndex &index, Size depth, Size ghm_node_idx,
                           mco_GhmConstraint constraint,
                           HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
    // This limit is arbitrary, quick tests show depth maxing at less than 100 so we
    // should be alright. If this becomes a problem, I'll rewrite this function to
    // avoid recursion.
    Assert(depth < 4096);

#define RUN_TREE_SUB(ChildIdx, ChangeCode) \
        do { \
            mco_GhmConstraint constraint_copy = constraint; \
            constraint_copy.ChangeCode; \
            success &= RecurseGhmTree(index, depth + 1, ghm_node.u.test.children_idx + (ChildIdx), \
                                      constraint_copy, out_constraints); \
        } while (false)

    Assert(ghm_node_idx < index.ghm_nodes.len);
    const mco_GhmDecisionNode &ghm_node = index.ghm_nodes[ghm_node_idx];

    bool success = true;
    switch (ghm_node.type) {
        case mco_GhmDecisionNode::Type::Test: {
            switch (ghm_node.u.test.function) {
                case 22: {
                    uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
                    if (param >= 31) {
                        LogError("Incomplete GHM constraint due to duration >= 31 nights");
                        success = false;
                        break;
                    }

                    uint32_t test_mask = ((uint32_t)1 << param) - 1;
                    RUN_TREE_SUB(0, duration_mask &= ~test_mask);
                    RUN_TREE_SUB(1, duration_mask &= test_mask);

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
                    RUN_TREE_SUB(0, duration_mask &= ~test_mask);
                    RUN_TREE_SUB(1, duration_mask &= test_mask);

                    return success;
                } break;

                case 30: {
                    uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
                    if (param != 0) {
                        LogError("Incomplete GHM constraint due to session count != 0");
                        success = false;
                        break;
                    }

                    RUN_TREE_SUB(0, duration_mask &= 0x1);
                    RUN_TREE_SUB(1, duration_mask &= UINT32_MAX);

                    return success;
                } break;
            }

            // Default case, for most functions and in case of error
            for (Size i = 0; i < ghm_node.u.test.children_count; i++) {
                success &= RecurseGhmTree(index, depth + 1, ghm_node.u.test.children_idx + i,
                                          constraint, out_constraints);
            }
        } break;

        case mco_GhmDecisionNode::Type::Ghm: {
            success &= MergeConstraint(index, ghm_node.u.ghm.ghm, constraint, out_constraints);
        } break;
    }

#undef RUN_TREE_SUB

    return success;
}

bool mco_ComputeGhmConstraints(const mco_TableIndex &index,
                               HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints)
{
    Assert(!out_constraints->count);

    mco_GhmConstraint null_constraint = {};
    null_constraint.duration_mask = UINT32_MAX;

    return RecurseGhmTree(index, 0, 0, null_constraint, out_constraints);
}
