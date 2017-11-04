/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "../moya/libmoya.hh"

void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes,
                         Size node_idx = 0, int depth = 0);
void DumpDiagnosisTable(ArrayRef<const DiagnosisInfo> diagnoses,
                        ArrayRef<const ExclusionInfo> exclusions = {});
void DumpProcedureTable(ArrayRef<const ProcedureInfo> procedures);
void DumpGhmRootTable(ArrayRef<const GhmRootInfo> ghm_roots);
void DumpSeverityTable(ArrayRef<const ValueRangeCell<2>> cells);

void DumpGhsTable(ArrayRef<const GhsInfo> ghs);
void DumpAuthorizationTable(ArrayRef<const AuthorizationInfo> authorizations);
void DumpSupplementPairTable(ArrayRef<const SrcPair> pairs);

void DumpTableSet(const TableSet &table_set, bool detail = true);

void DumpGhsPricings(ArrayRef<const GhsPricing> ghs_pricings);

void DumpPricingSet(const PricingSet &pricing_set);
