// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libdrd/libdrd.hh"

void mco_DumpGhmDecisionTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                             Size node_idx = 0, int depth = 0);
void mco_DumpDiagnosisTable(Span<const mco_DiagnosisInfo> diagnoses,
                            Span<const mco_ExclusionInfo> exclusions = {});
void mco_DumpProcedureTable(Span<const mco_ProcedureInfo> procedures);
void mco_DumpGhmRootTable(Span<const mco_GhmRootInfo> ghm_roots);
void mco_DumpSeverityTable(Span<const mco_ValueRangeCell<2>> cells);

void mco_DumpGhsAccessTable(Span<const mco_GhsAccessInfo> ghs);
void mco_DumpGhsPriceTable(Span<const mco_GhsPriceInfo> ghs_prices);
void mco_DumpAuthorizationTable(Span<const mco_AuthorizationInfo> authorizations);
void mco_DumpSupplementPairTable(Span<const mco_SrcPair> pairs);

void mco_DumpTableSetHeaders(const mco_TableSet &table_set);
void mco_DumpTableSetContent(const mco_TableSet &table_set);

void mco_DumpGhmRootCatalog(Span<const mco_GhmRootDesc> ghm_roots);
void mco_DumpCatalogSet(const mco_CatalogSet &catalog_set);
