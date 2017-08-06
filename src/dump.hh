#pragma once

#include "kutil.hh"
#include "data_fg.hh"

void DumpGhmDecisionTree(ArrayRef<const GhmDecisionNode> ghm_nodes);
void DumpDiagnosisTable(ArrayRef<const DiagnosisInfo> diagnoses,
                        ArrayRef<const ExclusionInfo> exclusions = {});
void DumpProcedureTable(ArrayRef<const ProcedureInfo> procedures);
void DumpGhmRootTable(ArrayRef<const GhmRootInfo> ghm_roots);
void DumpSeverityTable(ArrayRef<const ValueRangeCell<2>> cells);

void DumpGhsDecisionTree(ArrayRef<const GhsDecisionNode> ghs_nodes);
void DumpAuthorizationTable(ArrayRef<const AuthorizationInfo> authorizations);
void DumpSupplementPairTable(ArrayRef<const DiagnosisProcedurePair> pairs);

void DumpClassifierSet(const ClassifierSet &set, bool detail = true);
