// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "d_stays.hh"

template <typename T>
static bool ParsePmsiInt(Span<const char> str, T *out_value)
{
    DebugAssert(str.len > 0);

    if (str[0] == ' ')
        return true;

    T value = 0;
    for (Size i = 0; i < str.len && str[i] != ' '; i++) {
        int digit = str[i] - '0';
        if (UNLIKELY((unsigned int)digit > 9))
            return false;
        T new_value = (T)((value * 10) + digit);
        if (UNLIKELY(new_value < value))
            return false;
        value = new_value;
    }

    *out_value = value;
    return true;
}

static bool ParsePmsiDate(Span<const char> str, Date *out_date)
{
    DebugAssert(str.len == 8);

    if (UNLIKELY(!IsAsciiDigit(str[0]) || !IsAsciiDigit(str[1]) || !IsAsciiDigit(str[2]) ||
                 !IsAsciiDigit(str[3]) || !IsAsciiDigit(str[4]) || !IsAsciiDigit(str[5]) ||
                 !IsAsciiDigit(str[6]) || !IsAsciiDigit(str[7])))
        return false;

    Date date;
    date.st.day = (int8_t)((str[0] - '0') * 10 + (str[1] - '0'));
    date.st.month = (int8_t)((str[2] - '0') * 10 + (str[3] - '0'));
    date.st.year = (int16_t)((str[4] - '0') * 1000 + (str[5] - '0') * 100 +
                             (str[6] - '0') * 10 + (str[7] - '0'));

    *out_date = date;
    return true;
}

bool StaySetBuilder::LoadRssOrGrp(StreamReader &st, bool grp,
                                  HashTable<int32_t, StayTest> *out_tests)
{
    Size stays_len = set.stays.len;
    DEFER_NC(set_guard, diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    Size errors = 0;
    {
        LineReader reader(&st);

        while (!reader.eof) {
            Span<const char> line = reader.GetLine();
            if (reader.error)
                return false;

            Size offset = grp ? 24 : 9;
            if (UNLIKELY(line.len < offset + 168)) {
                LogError("Truncated RUM line %1 in '%2'", reader.line_number, st.filename);
                errors++;
                continue;
            }

            Stay stay = {};
            int das_count = 0;
            int dad_count = 0;
            int procedures_count = 0;

            // Declaring (simple) lambdas inside loops does not seem to impact performance
            const auto ReadFragment = [&](Size len) {
                Span<const char> frag = line.Take(offset, len);
                offset += len;
                return frag;
            };
            const auto SetErrorFlag = [&stay](Stay::Error flag, bool error) {
                if (UNLIKELY(error)) {
                    stay.error_mask |= (uint32_t)flag;
                } else {
                    stay.error_mask &= ~(uint32_t)flag;
                }
            };

            int16_t version = 0;
            ParsePmsiInt(ReadFragment(3), &version);
            if (UNLIKELY(version < 16 || version > 18)) {
                LogError("Unsupported RUM version %1 in '%2'", version, st.filename);
                errors++;
                continue;
            }

            ParsePmsiInt(ReadFragment(20), &stay.bill_id);
            ParsePmsiInt(ReadFragment(20), &stay.admin_id);
            offset += 10; // Skip RUM id
            SetErrorFlag(Stay::Error::MalformedBirthdate,
                         !ParsePmsiDate(ReadFragment(8), &stay.birthdate));
            SetErrorFlag(Stay::Error::MalformedSex,
                         !ParsePmsiInt(ReadFragment(1), &stay.sex));
            ParsePmsiInt(ReadFragment(4), &stay.unit.number);
            ParsePmsiInt(ReadFragment(2), &stay.bed_authorization);
            SetErrorFlag(Stay::Error::MalformedEntryDate,
                         !ParsePmsiDate(ReadFragment(8), &stay.entry.date));
            if (LIKELY(line[offset] != ' ')) {
                stay.entry.mode = line[offset];
            }
            offset += 1;
            if (line[offset] != ' ') {
                stay.entry.origin = line[offset];
            }
            offset += 1;
            SetErrorFlag(Stay::Error::MalformedExitDate,
                         !ParsePmsiDate(ReadFragment(8), &stay.exit.date));
            if (LIKELY(line[offset] != ' ')) {
                stay.exit.mode = line[offset];
            }
            offset += 1;
            if (line[offset] != ' ') {
                stay.exit.destination = line[offset];
            }
            offset += 1;
            offset += 5; // Skip postal code
            SetErrorFlag(Stay::Error::MalformedNewbornWeight,
                         !ParsePmsiInt(ReadFragment(4), &stay.newborn_weight));
            ParsePmsiInt(ReadFragment(2), &stay.gestational_age);
            ParsePmsiDate(ReadFragment(8), &stay.last_menstrual_period);
            SetErrorFlag(Stay::Error::MalformedSessionCount,
                         !ParsePmsiInt(ReadFragment(2), &stay.session_count));
            ParsePmsiInt(ReadFragment(2), &das_count);
            ParsePmsiInt(ReadFragment(2), &dad_count);
            ParsePmsiInt(ReadFragment(3), &procedures_count);
            if (LIKELY(line[offset] != ' ')) {
                stay.main_diagnosis =
                    DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
                if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                    stay.error_mask |= (int)Stay::Error::MalformedMainDiagnosis;
                }
            }
            offset += 8;
            if (line[offset] != ' ') {
                stay.linked_diagnosis =
                    DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
                if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                    stay.error_mask |= (int)Stay::Error::MalformedLinkedDiagnosis;
                }
            }
            offset += 8;
            ParsePmsiInt(ReadFragment(3), &stay.igs2);
            offset += 33; // Skip a bunch of fields

            if (UNLIKELY(line.len < offset + 8 * das_count + 8 * dad_count +
                                    (version >= 17 ? 29 : 26) * procedures_count)) {
                LogError("Truncated RUM line %1 in '%2'", reader.line_number, st.filename);
                errors++;
                continue;
            }

            stay.diagnoses.ptr = (DiagnosisCode *)set.store.diagnoses.len;
            if (LIKELY(stay.main_diagnosis.IsValid())) {
                set.store.diagnoses.Append(stay.main_diagnosis);
            }
            if (stay.linked_diagnosis.IsValid()) {
                set.store.diagnoses.Append(stay.linked_diagnosis);
            }
            for (int i = 0; i < das_count; i++) {
                DiagnosisCode diag =
                    DiagnosisCode::FromString(ReadFragment(8), (int)ParseFlag::End);
                if (LIKELY(diag.IsValid())) {
                    set.store.diagnoses.Append(diag);
                } else {
                    stay.error_mask |= (int)Stay::Error::MalformedAssociatedDiagnosis;
                }
            }
            stay.diagnoses.len = set.store.diagnoses.len - (Size)stay.diagnoses.ptr;
            offset += 8 * dad_count; // Skip documentary diagnoses

            stay.procedures.ptr = (ProcedureRealisation *)set.store.procedures.len;
            for (int i = 0; i < procedures_count; i++) {
                ProcedureRealisation proc = {};
                ParsePmsiDate(ReadFragment(8), &proc.date);
                proc.proc = ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
                if (version >= 17) {
                    offset += 3; // Skip CCAM extension
                }
                ParsePmsiInt(ReadFragment(1), &proc.phase);
                {
                    int activity = 0;
                    ParsePmsiInt(ReadFragment(1), &activity);
                    proc.activities = (uint8_t)(1 << activity);
                }
                offset += 7; // Skip extension, modifiers, etc.
                ParsePmsiInt(ReadFragment(2), &proc.count);
                set.store.procedures.Append(proc);
            }
            stay.procedures.len = set.store.procedures.len - (Size)stay.procedures.ptr;

            if (out_tests && grp) {
                StayTest test = {};

                bool valid = true;
                test.bill_id = stay.bill_id;
                test.ghm = GhmCode::FromString(line.Take(2, 6));
                valid &= test.ghm.IsValid();
                valid &= ParsePmsiInt(line.Take(12, 3), &test.error);

                if (valid) {
                    StayTest *it = out_tests->Append(test).first;
                    it->cluster_len++;
                } else {
                    StayTest *it = out_tests->Find(test.bill_id);
                    if (it) {
                        it->cluster_len++;
                    }
                }
            }

            set.stays.Append(stay);
        }
    }
    if (errors && set.stays.len == stays_len)
        return false;

    std::stable_sort(set.stays.begin() + stays_len, set.stays.end(),
                     [](const Stay &stay1, const Stay &stay2) {
        return MultiCmp(stay1.admin_id - stay2.admin_id,
                        stay1.bill_id - stay2.bill_id) < 0;
    });

    set_guard.disable();
    return true;
}

bool StaySetBuilder::LoadRsa(StreamReader &st, HashTable<int32_t, StayTest> *out_tests)
{
    LogError("RSA files are not supported yet");
    return false;
}
