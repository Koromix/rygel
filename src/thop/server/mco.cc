// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "mco.hh"
#include "config.hh"
#include "structure.hh"
#include "thop.hh"
#include "user.hh"

namespace RG {

mco_TableSet mco_table_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> mco_constraints_set;
HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> mco_index_to_constraints;

mco_AuthorizationSet mco_authorization_set;
mco_StaySet mco_stay_set;
Date mco_stay_set_dates[2];

HeapArray<mco_Result> mco_results;
HeapArray<mco_Result> mco_mono_results;
HashMap<mco_GhmRootCode, Span<const mco_Result *>> mco_results_by_ghm_root;
HashMap<const void *, const mco_Result *> mco_results_to_mono;

static HeapArray<const mco_Result *> results_by_ghm_root_ptrs;

bool InitMcoTables(Span<const char *const> table_directories)
{
    LogInfo("Load MCO tables");

    if (!mco_LoadTableSet(table_directories, {}, &mco_table_set) || !mco_table_set.indexes.len)
        return false;

    LogInfo("Compute MCO constraints");

    {
        Async async;

        mco_constraints_set.Reserve(mco_table_set.indexes.len);
        for (Size i = 0; i < mco_table_set.indexes.len; i++) {
            if (mco_table_set.indexes[i].valid) {
                // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
                if (mco_table_set.indexes[i].changed_tables & MaskEnum(mco_TableType::GhmDecisionTree) ||
                        !mco_index_to_constraints[mco_index_to_constraints.len - 1]) {
                    HashTable<mco_GhmCode, mco_GhmConstraint> *constraints = mco_constraints_set.AppendDefault();
                    async.Run([=]() {
                        return mco_ComputeGhmConstraints(mco_table_set.indexes[i], constraints);
                    });
                }
                mco_index_to_constraints.Append(&mco_constraints_set[mco_constraints_set.len - 1]);
            } else {
                mco_index_to_constraints.Append(nullptr);
            }
        }

        if (!async.Sync())
            return true;
    }

    return true;
}

bool InitMcoProfile(const char *profile_directory, const char *authorization_filename)
{
    BlockAllocator temp_alloc;

    LogInfo("Load MCO profile");

    if (!mco_LoadAuthorizationSet(profile_directory, authorization_filename,
                                  &mco_authorization_set))
        return false;
    if (const char *filename = Fmt(&temp_alloc, "%1%/mco_structures.ini", profile_directory).ptr;
            !LoadStructureSet(filename, &thop_structure_set))
        return false;

    return true;
}

bool InitMcoStays(Span<const char *const> stay_directories, Span<const char *const> stay_filenames)
{
    BlockAllocator temp_alloc;

    LogInfo("Load MCO stays");

    // Aggregate stay files
    HeapArray<const char *> filenames;
    {
        const auto enumerate_directory_files = [&](const char *dir) {
            EnumStatus status = EnumerateDirectory(dir, nullptr, 1024,
                                                   [&](const char *filename, FileType file_type) {
                CompressionType compression_type;
                Span<const char> ext = GetPathExtension(filename, &compression_type);

                if (file_type == FileType::File &&
                        (ext == ".grp" || ext == ".rss" || ext == ".dmpak")) {
                    filenames.Append(Fmt(&temp_alloc, "%1%/%2", dir, filename).ptr);
                }

                return true;
            });

            return status != EnumStatus::Error;
        };

        bool success = true;
        for (const char *dir: stay_directories) {
            success &= enumerate_directory_files(dir);
        }
        filenames.Append(stay_filenames);
        if (!success)
            return false;
    }

    // Load stays
    mco_StaySetBuilder stay_set_builder;
    if (!stay_set_builder.LoadFiles(filenames))
        return false;
    if (!stay_set_builder.Finish(&mco_stay_set))
        return false;
    if (!mco_stay_set.stays.len) {
        LogError("Cannot continue without any loaded stay");
        return false;
    }

    LogInfo("Check and sort MCO stays");

    // Check units
    {
        HashSet<drd_UnitCode> known_units;
        for (const Structure &structure: thop_structure_set.structures) {
            for (const StructureEntity &ent: structure.entities) {
                known_units.Append(ent.unit);
            }
        }

        bool valid = true;
        for (const mco_Stay &stay: mco_stay_set.stays) {
            if (stay.unit.number && !known_units.Find(stay.unit)) {
                LogError("Structure set is missing unit %1", stay.unit);
                known_units.Append(stay.unit);

                valid = false;
            }
        }
        if (!valid && !GetDebugFlag("THOP_IGNORE_MISSING_UNITS"))
            return false;
    }

    // Sort by date
    {
        HeapArray<Span<const mco_Stay>> groups;
        {
            Span<const mco_Stay> remain = mco_stay_set.stays;
            while (remain.len) {
                Span<const mco_Stay> group = mco_Split(remain, 1, &remain);
                groups.Append(group);
            }
        }
        std::sort(groups.begin(), groups.end(),
                  [](const Span<const mco_Stay> &group1, const Span<const mco_Stay> &group2) {
            return group1[group1.len - 1].exit.date < group2[group2.len - 1].exit.date;
        });

        for (const Span<const mco_Stay> &group: groups) {
            Date exit_date = group[group.len - 1].exit.date;

            if (RG_LIKELY(exit_date.IsValid())) {
                mco_stay_set_dates[0] = exit_date;
                break;
            }
        }
        for (Size i = groups.len - 1; i >= 0; i--) {
            const Span<const mco_Stay> &group = groups[i];
            Date exit_date = group[group.len - 1].exit.date;

            if (RG_LIKELY(exit_date.IsValid())) {
                mco_stay_set_dates[1] = exit_date + 1;
                break;
            }
        }
        if (!mco_stay_set_dates[1].value) {
            LogError("Could not determine date range for stay set");
            return false;
        }

        HeapArray<mco_Stay> stays(mco_stay_set.stays.len);
        for (Span<const mco_Stay> group: groups) {
            stays.Append(group);
        }

        std::swap(stays, mco_stay_set.stays);
    }

    LogInfo("Classify MCO stays");

    // Classify
    mco_Classify(mco_table_set, mco_authorization_set, thop_config.sector, mco_stay_set.stays, 0,
                 &mco_results, &mco_mono_results);
    mco_results.Trim();
    mco_mono_results.Trim();

    LogInfo("Index MCO results");

    // Index results
    for (Size i = 0, j = 0; i < mco_results.len;) {
        const mco_Result &result = mco_results[i];

        results_by_ghm_root_ptrs.Append(&result);
        mco_results_to_mono.Append(&result, &mco_mono_results[j]);

        i++;
        j += result.stays.len;
    }
    mco_results_to_mono.Append(mco_results.end(), mco_mono_results.end());

    // Finalize index by GHM
    std::stable_sort(results_by_ghm_root_ptrs.begin(), results_by_ghm_root_ptrs.end(),
                     [](const mco_Result *result1, const mco_Result *result2) {
        return result1->ghm.Root() < result2->ghm.Root();
    });
    for (Size i = 0; i < results_by_ghm_root_ptrs.len;) {
        Span<const mco_Result *> ptrs = MakeSpan(&results_by_ghm_root_ptrs[i], 1);
        const mco_GhmRootCode ghm_root = ptrs[0]->ghm.Root();

        while (++i < results_by_ghm_root_ptrs.len &&
               results_by_ghm_root_ptrs[i]->ghm.Root() == ghm_root) {
            ptrs.len++;
        }

        mco_results_by_ghm_root.Append(ghm_root, ptrs);
    }

    return true;
}

static Span<const mco_Result> GetResultsRange(Date min_date, Date max_date)
{
    const mco_Result *start =
        std::lower_bound(mco_results.begin(), mco_results.end(), min_date,
                         [](const mco_Result &result, Date date) {
        return result.stays[result.stays.len - 1].exit.date < date;
    });
    const mco_Result *end =
        std::upper_bound(start, (const mco_Result *)mco_results.end(), max_date - 1,
                         [](Date date, const mco_Result &result) {
        return date < result.stays[result.stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

static Span<const mco_Result *> GetIndexRange(Span<const mco_Result *> index,
                                              Date min_date, Date max_date)
{
    const mco_Result **start =
        std::lower_bound(index.begin(), index.end(), min_date,
                         [](const mco_Result *result, Date date) {
        return result->stays[result->stays.len - 1].exit.date < date;
    });
    const mco_Result **end =
        std::upper_bound(start, index.end(), max_date - 1,
                         [](Date date, const mco_Result *result) {
        return date < result->stays[result->stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

bool McoResultProvider::Run(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    RG_ASSERT_DEBUG(min_date.IsValid() && max_date.IsValid());

    if (filter) {
        return RunFilter(func);
    } else if (ghm_root.IsValid()) {
        return RunIndex(func);
    } else {
        return RunDirect(func);
    }
}

bool McoResultProvider::RunFilter(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    RG_ASSERT_DEBUG(filter);

    const Size split_size = 8192;

    Span<const mco_Result> results = GetResultsRange(min_date, max_date);

    mco_FilterRunner filter_runner;
    if (!filter_runner.Init(filter))
        return false;

    // Reuse for performance
    HeapArray<const mco_Result *> index;
    HeapArray<const mco_Result *> mono_index;
    mco_StaySet changed_stay_set;
    HeapArray<mco_Result> results_buf;
    HeapArray<mco_Result> mono_results_buf;

    for (Size i = 0; i < results.len; i += split_size) {
        Size split_len = std::min(split_size, results.len - i);

        Span<const mco_Result> split_results = results.Take(i, split_len);
        Span<const mco_Result> split_mono_results =
            MakeSpan(mco_results_to_mono.FindValue(split_results.begin(), nullptr),
                     mco_results_to_mono.FindValue(split_results.end(), nullptr));

        // Run filter
        index.RemoveFrom(0);
        mono_index.RemoveFrom(0);
        changed_stay_set.stays.RemoveFrom(0);
        changed_stay_set.array_alloc.ReleaseAll();
        if (!filter_runner.Process(split_results, split_mono_results, &index, &mono_index,
                                   allow_mutation ? &changed_stay_set : nullptr))
            return false;

        // Gather filtered results
        results_buf.RemoveFrom(0);
        mono_results_buf.RemoveFrom(0);
        for (Size j = 0, k = 0; j < index.len;) {
            const mco_Result &result = *index[j];
            Span<const mco_Result> mono_results = MakeSpan(mono_index[k], result.stays.len);

            if (!ghm_root.IsValid() || result.ghm.Root() == ghm_root) {
                results_buf.Append(result);
                mono_results_buf.Append(mono_results);
            }

            j++;
            k += result.stays.len;
        }

        // Classify changed stays
        {
            Size j = results_buf.len, k = mono_results_buf.len;
            mco_Classify(mco_table_set, mco_authorization_set, thop_config.sector,
                         changed_stay_set.stays, 0, &results_buf, &mono_results_buf);

            if (ghm_root.IsValid()) {
                for (Size l = j, m = k; l < results_buf.len;) {
                    const mco_Result &result = results_buf[l];

                    if (result.ghm.Root() == ghm_root) {
                        results_buf[j] = result;
                        memmove(&mono_results_buf[k], &mono_results_buf[m],
                                result.stays.len * RG_SIZE(mco_Result));

                        j++;
                        k += result.stays.len;
                    }

                    l++;
                    m += result.stays.len;
                }

                results_buf.len = j;
                mono_results_buf.len = k;
            }
        }

        func(results_buf, mono_results_buf);
    }

    return true;
}

bool McoResultProvider::RunIndex(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    RG_ASSERT_DEBUG(ghm_root.IsValid());

    const Size split_size = 8192;

    Span<const mco_Result *> index = mco_results_by_ghm_root.FindValue(ghm_root, {});
    index = GetIndexRange(index, min_date, max_date);

    // Reuse for performance
    HeapArray<mco_Result> results_buf;
    HeapArray<mco_Result> mono_results_buf;

    for (Size i = 0; i < index.len; i += split_size) {
        Size split_len = std::min(split_size, index.len - i);

        results_buf.RemoveFrom(0);
        mono_results_buf.RemoveFrom(0);
        for (Size j = 0; j < split_len; j++) {
            const mco_Result &result = *index[i + j];
            const mco_Result *mono_result = mco_results_to_mono.FindValue(&result, nullptr);

            results_buf.Append(result);
            mono_results_buf.Append(MakeSpan(mono_result, result.stays.len));
        }

        func(results_buf, mono_results_buf);
    }

    return true;
}

bool McoResultProvider::RunDirect(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    RG_ASSERT_DEBUG(!ghm_root.IsValid());

    const Size split_size = 65536;

    Span<const mco_Result> results = GetResultsRange(min_date, max_date);

    for (Size i = 0; i < results.len; i += split_size) {
        Size split_len = std::min(split_size, results.len - i);
        Span<const mco_Result> split_results = results.Take(i, split_len);
        Span<const mco_Result> split_mono_results =
            MakeSpan(mco_results_to_mono.FindValue(split_results.begin(), nullptr),
                     mco_results_to_mono.FindValue(split_results.end(), nullptr));

        func(split_results, split_mono_results);
    }

    return true;
}

void ProduceMcoSettings(const http_RequestInfo &request, const User *user, http_IO *io)
{
    if (!user) {
        io->flags |= (int)http_IO::Flag::EnableCache;
    }

    http_JsonPageBuilder json(request.compression_type);
    char buf[32];

    json.StartObject();

    json.Key("indexes"); json.StartArray();
    for (const mco_TableIndex &index: mco_table_set.indexes) {
        if (!index.valid)
            continue;

        char buf[32];

        json.StartObject();
        json.Key("begin_date"); json.String(Fmt(buf, "%1", index.limit_dates[0]).ptr);
        json.Key("end_date"); json.String(Fmt(buf, "%1", index.limit_dates[1]).ptr);
        if (index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic)) {
            json.Key("changed_tables"); json.Bool(true);
        }
        if (index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic)) {
            json.Key("changed_prices"); json.Bool(true);
        }
        json.EndObject();
    }
    json.EndArray();

    if (user) {
        json.Key("start_date"); json.String(Fmt(buf, "%1", mco_stay_set_dates[0]).ptr);
        json.Key("end_date"); json.String(Fmt(buf, "%1", mco_stay_set_dates[1]).ptr);

        // Algorithms
        {
            const OptionDesc &default_desc = mco_DispenseModeOptions[(int)thop_config.mco_dispense_mode];

            json.Key("algorithms"); json.StartArray();
            for (Size i = 0; i < RG_LEN(mco_DispenseModeOptions); i++) {
                if (user->CheckMcoDispenseMode((mco_DispenseMode)i)) {
                    const OptionDesc &desc = mco_DispenseModeOptions[i];

                    json.StartObject();
                    json.Key("name"); json.String(desc.name);
                    json.Key("help"); json.String(desc.help);
                    json.EndObject();
                }
            }
            json.EndArray();

            json.Key("default_algorithm"); json.String(default_desc.name);
        }

        json.Key("permissions"); json.StartArray();
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            if (user->permissions & (1 << i)) {
                json.String(UserPermissionNames[i]);
            }
        }
        json.EndArray();

        json.Key("structures"); json.StartArray();
        for (const Structure &structure: thop_structure_set.structures) {
            json.StartObject();
            json.Key("name"); json.String(structure.name);
            json.Key("entities"); json.StartArray();
            for (const StructureEntity &ent: structure.entities) {
                if (user->mco_allowed_units.Find(ent.unit)) {
                    json.StartObject();
                    json.Key("unit"); json.Int(ent.unit.number);
                    json.Key("path"); json.String(ent.path);
                    json.EndObject();
                }
            }
            json.EndArray();
            json.EndObject();
        }
        json.EndArray();
    }

    json.EndObject();

    json.Finish(io);
}

}
