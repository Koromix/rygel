// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "common.hh"
#include "mco_table.hh"

namespace K {

struct mco_ReadableGhmNode {
    const char *type;
    const char *key;
    const char *header;
    const char *text;
    const char *reverse;

    uint8_t function;
    Size children_idx;
    Size children_count;
};

bool mco_BuildReadableGhmTree(Span<const mco_GhmDecisionNode> ghm_nodes, Allocator *str_alloc,
                              HeapArray<mco_ReadableGhmNode> *out_nodes);
void mco_DumpGhmDecisionTree(Span<const mco_ReadableGhmNode> readable_nodes, StreamWriter *out_st);
void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes, StreamWriter *out_st);

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

}
