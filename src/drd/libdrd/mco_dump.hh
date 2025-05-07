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

#pragma once

#include "src/core/base/base.hh"
#include "common.hh"
#include "mco_table.hh"

namespace RG {

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
