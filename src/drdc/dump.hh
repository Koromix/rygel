// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libdrd/libdrd.hh"

void DumpGhmDecisionTree(Span<const GhmDecisionNode> ghm_nodes,
                         Size node_idx = 0, int depth = 0);
void DumpDiagnosisTable(Span<const DiagnosisInfo> diagnoses,
                        Span<const ExclusionInfo> exclusions = {});
void DumpProcedureTable(Span<const ProcedureInfo> procedures);
void DumpGhmRootTable(Span<const GhmRootInfo> ghm_roots);
void DumpSeverityTable(Span<const ValueRangeCell<2>> cells);

void DumpGhsAccessTable(Span<const GhsAccessInfo> ghs);
void DumpGhsPriceTable(Span<const GhsPriceInfo> ghs_prices);
void DumpAuthorizationTable(Span<const AuthorizationInfo> authorizations);
void DumpSupplementPairTable(Span<const SrcPair> pairs);

void DumpTableSetHeaders(const TableSet &table_set);
void DumpTableSetContent(const TableSet &table_set);

void DumpGhmRootCatalog(Span<const GhmRootDesc> ghm_roots);
void DumpCatalogSet(const CatalogSet &catalog_set);
