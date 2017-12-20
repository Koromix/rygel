// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../common/kutil.hh"
#include "a_constraints.hh"
#include "a_classifier.hh"

static bool MergeConstraint(const TableIndex &index,
                            const GhmCode ghm, GhmConstraint constraint,
                            HashSet<GhmCode, GhmConstraint> *out_constraints)
{
    // TODO: Simplify with AppendOrGet() in HashSet
#define MERGE_CONSTRAINT(ModeChar, DurationMask) \
        do { \
            GhmConstraint new_constraint = constraint; \
            new_constraint.ghm.parts.mode = (char)(ModeChar); \
            new_constraint.duration_mask &= (DurationMask); \
            if (new_constraint.duration_mask) { \
                GhmConstraint *prev_constraint = out_constraints->Find(new_constraint.ghm); \
                if (prev_constraint) { \
                    prev_constraint->duration_mask |= new_constraint.duration_mask; \
                } else { \
                    out_constraints->Append(new_constraint); \
                } \
            } \
        } while (false)

    constraint.ghm = ghm;

    const GhmRootInfo *ghm_root_info = index.FindGhmRoot(ghm.Root());
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

    if (!ghm.parts.mode) {
        for (int severity = 0; severity < 4; severity++) {
            uint32_t mode_mask = ~((uint32_t)(1 << GetMinimalDurationForSeverity(severity)) - 1);
            MERGE_CONSTRAINT('1' + severity, mode_mask);
        }
    } else if (ghm.parts.mode >= 'A' && ghm.parts.mode < 'E') {
        int severity = ghm.parts.mode - 'A';
        uint32_t mode_mask = ~((uint32_t)(1 << GetMinimalDurationForSeverity(severity)) - 1);
        MERGE_CONSTRAINT('A' + severity, mode_mask);
    } else if (ghm.parts.mode != 'J' && ghm.parts.mode != 'T') {
        // FIXME: Ugly construct
        MERGE_CONSTRAINT(ghm.parts.mode, UINT32_MAX);
    }

#undef MERGE_CONSTRAINT

    return true;
}

// TODO: Convert to non-recursive code
static bool RecurseGhmTree(const TableIndex &index, Size depth, Size ghm_node_idx,
                           GhmConstraint constraint,
                           HashSet<GhmCode, GhmConstraint> *out_constraints)
{
    if (UNLIKELY(depth >= index.ghm_nodes.len)) {
        LogError("Empty GHM tree or infinite loop (%2)", index.ghm_nodes.len);
        return false;
    }

#define RUN_TREE_SUB(ChildIdx, ChangeCode) \
        do { \
            GhmConstraint constraint_copy = constraint; \
            constraint_copy.ChangeCode; \
            success &= RecurseGhmTree(index, depth + 1, ghm_node.u.test.children_idx + (ChildIdx), \
                                      constraint_copy, out_constraints); \
        } while (false)

    bool success = true;

    // FIXME: Check ghm_node_idx against CountOf()
    const GhmDecisionNode &ghm_node = index.ghm_nodes[ghm_node_idx];
    switch (ghm_node.type) {
        case GhmDecisionNode::Type::Test: {
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

        case GhmDecisionNode::Type::Ghm: {
            success &= MergeConstraint(index, ghm_node.u.ghm.ghm, constraint, out_constraints);
        } break;
    }

#undef RUN_TREE_SUB

    return success;
}

bool ComputeGhmConstraints(const TableIndex &index,
                           HashSet<GhmCode, GhmConstraint> *out_constraints)
{
    Assert(!out_constraints->count);

    GhmConstraint null_constraint = {};
    null_constraint.duration_mask = UINT32_MAX;

    return RecurseGhmTree(index, 0, 0, null_constraint, out_constraints);
}
