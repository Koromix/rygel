// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "d_stays.hh"

#pragma pack(push, 1)
struct PackHeader {
    char signature[13];
    int8_t version;
    int8_t native_size;
    int8_t endianness;

    int64_t stays_len;
    int64_t diagnoses_len;
    int64_t procedures_len;
};
#pragma pack(pop)
#define PACK_VERSION 4
#define PACK_SIGNATURE "DRD_STAY_PAK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));
StaticAssert(SIZE(Stay) == 104);
StaticAssert(SIZE(DiagnosisCode) == 8);
StaticAssert(SIZE(ProcedureRealisation) == 16);

bool StaySet::SavePack(StreamWriter &st) const
{
    PackHeader bh = {};

    strcpy(bh.signature, PACK_SIGNATURE);
    bh.version = PACK_VERSION;
    bh.native_size = (uint8_t)SIZE(Size);
    bh.endianness = (int8_t)ARCH_ENDIANNESS;
    bh.stays_len = stays.len;
    bh.diagnoses_len = store.diagnoses.len;
    bh.procedures_len = store.procedures.len;

    st.Write(&bh, SIZE(bh));
#ifdef ARCH_64
    st.Write(stays.ptr, stays.len * SIZE(*stays.ptr));
#else
    for (const Stay &stay: stays) {
        Stay stay2;
        memcpy(&stay2, &stay, SIZE(stay));

        union {
            uint8_t raw[32];
            struct {
                int64_t _pad1;
                int64_t diagnoses_len;
                int64_t _pad2;
                int64_t procedures_len;
            } st;
        } u;
        u.st.diagnoses_len = (int64_t)stay.diagnoses.len;
        u.st.procedures_len = (int64_t)stay.procedures.len;
        memcpy(&stay2.diagnoses, u.raw, 32);

        st.Write(&stay2, SIZE(stay2));
    }
#endif
    for (const Stay &stay: stays) {
        st.Write(stay.diagnoses.ptr, stay.diagnoses.len * SIZE(*stay.diagnoses.ptr));
    }
    for (const Stay &stay: stays) {
        st.Write(stay.procedures.ptr, stay.procedures.len * SIZE(*stay.procedures.ptr));
    }
    if (!st.Close())
        return false;

    return true;
}

bool StaySet::SavePack(const char *filename) const
{
    LocalArray<char, 16> extension;
    CompressionType compression_type;

    extension.len = GetPathExtension(filename, extension.data, &compression_type);

    if (!TestStr(extension, ".dspak")) {
        LogError("Unknown packing extension '%1', prefer '.dspak'", extension);
    }

    StreamWriter st(filename, compression_type);
    return SavePack(st);
}

bool StaySetBuilder::LoadPack(StreamReader &st, HashTable<int32_t, StayTest> *out_tests)
{
    const Size start_stays_len = set.stays.len;
    const Size start_diagnoses_len = set.store.diagnoses.len;
    const Size start_procedures_len = set.store.procedures.len;
    DEFER_N(set_guard) {
        set.stays.RemoveFrom(start_stays_len);
        set.store.diagnoses.RemoveFrom(start_diagnoses_len);
        set.store.procedures.RemoveFrom(start_procedures_len);
    };

    if (out_tests) {
        LogError("Testing is not supported by .dspak files");
    }

    PackHeader bh;
    if (st.Read(SIZE(bh), &bh) != SIZE(bh))
        goto corrupt_error;

    if (strncmp(bh.signature, PACK_SIGNATURE, SIZE(bh.signature)) != 0) {
        LogError("File '%1' does not have dspak signature", st.filename);
        return false;
    }
    if (bh.version != PACK_VERSION) {
        LogError("Cannot load '%1' (dspak version %2), expected version %3", st.filename,
                 bh.version, PACK_VERSION);
        return false;
    }
    if (bh.endianness != (int8_t)ARCH_ENDIANNESS) {
        LogError("File '%1' is not compatible with this platform (endianness issue)",
                 st.filename);
        return false;
    }
    if (bh.stays_len < 0 || bh.diagnoses_len < 0 || bh.procedures_len < 0)
        goto corrupt_error;

    if (bh.stays_len > (LEN_MAX - start_stays_len)
            || bh.diagnoses_len > (LEN_MAX - start_diagnoses_len)
            || bh.procedures_len > (LEN_MAX - start_procedures_len)) {
        LogError("Too much data to load in '%1'", st.filename);
        return false;
    }

    set.stays.Grow((Size)bh.stays_len);
    if (st.Read(SIZE(*set.stays.ptr) * (Size)bh.stays_len,
                set.stays.end()) != SIZE(*set.stays.ptr) * (Size)bh.stays_len)
        goto corrupt_error;
    set.stays.len += (Size)bh.stays_len;

    set.store.diagnoses.Grow((Size)bh.diagnoses_len);
    if (st.Read(SIZE(*set.store.diagnoses.ptr) * (Size)bh.diagnoses_len,
                set.store.diagnoses.end()) != SIZE(*set.store.diagnoses.ptr) * (Size)bh.diagnoses_len)
        goto corrupt_error;
    set.store.procedures.Grow((Size)bh.procedures_len);
    if (st.Read(SIZE(*set.store.procedures.ptr) * (Size)bh.procedures_len,
                set.store.procedures.end()) != SIZE(*set.store.procedures.ptr) * (Size)bh.procedures_len)
        goto corrupt_error;

    {
        Size store_diagnoses_len = set.store.diagnoses.len;
        Size store_procedures_len = set.store.procedures.len;

        for (Size i = set.stays.len - (Size)bh.stays_len; i < set.stays.len; i++) {
            Stay *stay = &set.stays[i];

#ifndef ARCH_64
            union {
                uint8_t raw[32];
                struct {
                    int64_t _pad1;
                    int64_t diagnoses_len;
                    int64_t _pad2;
                    int64_t procedures_len;
                } st;
            } u;
            memcpy(u.raw, &stay->diagnoses, 32);
            stay->diagnoses.len = (Size)u.st.diagnoses_len;
            stay->procedures.len = (Size)u.st.procedures_len;
#endif

            if (stay->diagnoses.len) {
                if (UNLIKELY(stay->diagnoses.len < 0))
                    goto corrupt_error;
                stay->diagnoses.ptr = (DiagnosisCode *)store_diagnoses_len;
                store_diagnoses_len += stay->diagnoses.len;
                if (UNLIKELY(store_diagnoses_len <= 0 ||
                             store_diagnoses_len > start_diagnoses_len + bh.diagnoses_len))
                    goto corrupt_error;
            }
            if (stay->procedures.len) {
                if (UNLIKELY(stay->procedures.len < 0))
                    goto corrupt_error;
                stay->procedures.ptr = (ProcedureRealisation *)store_procedures_len;
                store_procedures_len += stay->procedures.len;
                if (UNLIKELY(store_procedures_len <= 0 ||
                             store_procedures_len > start_procedures_len + bh.procedures_len))
                    goto corrupt_error;
            }
        }

        set.store.diagnoses.len = store_diagnoses_len;
        set.store.procedures.len = store_procedures_len;
    }


    // We assume stays are already sorted in pak files

    set_guard.disable();
    return true;

corrupt_error:
    LogError("Stay pack file '%1' appears to be corrupt or truncated", st.filename);
    return false;
}

bool StaySetBuilder::LoadFiles(Span<const char *const> filenames,
                               HashTable<int32_t, StayTest> *out_tests)
{
    for (const char *filename: filenames) {
        LocalArray<char, 16> extension;
        CompressionType compression_type;
        extension.len = GetPathExtension(filename, extension.data, &compression_type);

        bool (StaySetBuilder::*load_func)(StreamReader &st,
                                          HashTable<int32_t, StayTest> *out_tests);
        if (TestStr(extension, ".dspak")) {
            load_func = &StaySetBuilder::LoadPack;
        } else if (TestStr(extension, ".grp")) {
            load_func = &StaySetBuilder::LoadGrp;
        } else if (TestStr(extension, ".rss")) {
            load_func = &StaySetBuilder::LoadRss;
        } else if (TestStr(extension, ".rsa")) {
            load_func = &StaySetBuilder::LoadRsa;
        } else if (TestStr(extension, ".dsjson")) {
            load_func = &StaySetBuilder::LoadJson;
        } else {
            LogError("Cannot load stays from file '%1' with unknown extension '%2'",
                     filename, extension);
            return false;
        }

        StreamReader st(filename, compression_type);
        if (st.error)
            return false;
        if (!(this->*load_func)(st, out_tests))
            return false;
    }

    return true;
}

bool StaySetBuilder::Finish(StaySet *out_set)
{
    for (Stay &stay: set.stays) {
#define FIX_SPAN(SpanName) \
            stay.SpanName.ptr = set.store.SpanName.ptr + (Size)stay.SpanName.ptr

        FIX_SPAN(diagnoses);
        FIX_SPAN(procedures);

#undef FIX_SPAN
    }

    SwapMemory(out_set, &set, SIZE(set));
    return true;
}
