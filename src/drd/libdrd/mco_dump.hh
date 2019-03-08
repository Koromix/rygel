// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "common.hh"
#include "mco_tables.hh"

void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                             Size node_idx, int depth, StreamWriter *out_st);
static inline void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                                           StreamWriter *out_st)
    { mco_DumpGhmDecisionTree(ghm_nodes, 0, 0, out_st); }
void mco_DumpDiagnosisTable(Span<const mco_DiagnosisInfo> diagnoses,
                            Span<const mco_ExclusionInfo> exclusions, StreamWriter *out_st);
void mco_DumpProcedureTable(Span<const mco_ProcedureInfo> procedures, StreamWriter *out_st);
void mco_DumpGhmRootTable(Span<const mco_GhmRootInfo> ghm_roots, StreamWriter *out_st);
void mco_DumpSeverityTable(Span<const mco_ValueRangeCell<2>> cells, StreamWriter *out_st);

void mco_DumpGhmToGhsTable(Span<const mco_GhmToGhsInfo> ghs, StreamWriter *out_st);
void mco_DumpGhsPriceTable(Span<const mco_GhsPriceInfo> ghs_prices, StreamWriter *out_st);
void mco_DumpAuthorizationTable(Span<const mco_AuthorizationInfo> authorizations,
                                StreamWriter *out_st);
void mco_DumpSupplementPairTable(Span<const mco_SrcPair> pairs, StreamWriter *out_st);

void mco_DumpTableSetHeaders(const mco_TableSet &table_set, StreamWriter *out_st);
void mco_DumpTableSetContent(const mco_TableSet &table_set, StreamWriter *out_st);
