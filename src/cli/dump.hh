#pragma once

#include "../core/kutil.hh"
#include "../core/tables.hh"

void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes);
void DumpDiagnosisTable(ArrayRef<const DiagnosisInfo> diagnoses,
                        ArrayRef<const ExclusionInfo> exclusions = {});
void DumpProcedureTable(ArrayRef<const ProcedureInfo> procedures);
void DumpGhmRootTable(ArrayRef<const GhmRootInfo> ghm_roots);
void DumpSeverityTable(ArrayRef<const ValueRangeCell<2>> cells);

void DumpGhsTable(ArrayRef<const GhsInfo> ghs);
void DumpAuthorizationTable(ArrayRef<const AuthorizationInfo> authorizations);
void DumpSupplementPairTable(ArrayRef<const SrcPair> pairs);

void DumpTableSet(const TableSet &table_set, bool detail = true);
