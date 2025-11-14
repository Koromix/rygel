// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "mco.hh"
#include "config.hh"
#include "structure.hh"
#include "thop.hh"
#include "user.hh"

namespace K {

mco_TableSet mco_table_set;
McoCacheSet mco_cache_set;

mco_AuthorizationSet mco_authorization_set;
mco_StaySet mco_stay_set;
LocalDate mco_stay_set_dates[2];

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

    // Do it in parallel for faster startup
    {
        Async async;

        // Content must not move (pointers)!
        mco_cache_set.constraints_set.Reserve(mco_table_set.indexes.len);

        HashTable<mco_GhmCode, mco_GhmConstraint> *constraints = nullptr;
        for (const mco_TableIndex &index: mco_table_set.indexes) {
            if (index.valid) {
                // Extend or remove this check when constraints go beyond the tree info (diagnoses, etc.)
                if (index.changed_tables & MaskEnum(mco_TableType::GhmDecisionTree) || !constraints) {
                    constraints = mco_cache_set.constraints_set.AppendDefault();
                    async.Run([=]() { return mco_ComputeGhmConstraints(index, constraints); });
                }
            } else {
                constraints = nullptr;
            }

            mco_cache_set.index_to_constraints.Set(&index, constraints);
        }

        if (!async.Sync())
            return true;
    }

    LogInfo("Build readable MCO trees");

    for (const mco_TableIndex &index: mco_table_set.indexes) {
        auto bucket = mco_cache_set.readable_nodes.SetDefault(&index);
        HeapArray<mco_ReadableGhmNode> *readable_nodes = &bucket->value;

        if (!mco_BuildReadableGhmTree(index.ghm_nodes, &mco_cache_set.str_alloc, readable_nodes))
            return false;
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
            EnumResult ret = EnumerateDirectory(dir, nullptr, 1024,
                                                [&](const char *basename, FileType file_type) {
                const char *filename = Fmt(&temp_alloc, "%1%/%2", dir, basename).ptr;

                CompressionType compression_type;
                Span<const char> ext = GetPathExtension(basename, &compression_type);

                if (file_type == FileType::Link) {
                    FileInfo file_info;
                    if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
                        return true;
                    file_type = file_info.type;
                }

                if (file_type == FileType::File &&
                        (ext == ".grp" || ext == ".rss" || ext == ".dmpak" || ext == ".txt")) {
                    filenames.Append(filename);
                }

                return true;
            });

            bool success = (ret == EnumResult::Success || ret == EnumResult::PartialEnum);
            return success;
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
                known_units.Set(ent.unit);
            }
        }

        bool valid = true;
        for (const mco_Stay &stay: mco_stay_set.stays) {
            if (stay.unit.number && !known_units.Find(stay.unit)) {
                LogError("Structure set is missing unit %1", stay.unit);
                known_units.Set(stay.unit);

                valid = false;
            }
        }
        if (!valid && !GetDebugFlag("SKIP_UNKNOWN_UNITS"))
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
            LocalDate exit_date = group[group.len - 1].exit.date;

            if (exit_date.IsValid()) [[likely]] {
                mco_stay_set_dates[0] = exit_date;
                break;
            }
        }
        for (Size i = groups.len - 1; i >= 0; i--) {
            const Span<const mco_Stay> &group = groups[i];
            LocalDate exit_date = group[group.len - 1].exit.date;

            if (exit_date.IsValid()) [[likely]] {
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
        mco_results_to_mono.TrySet(&result, &mco_mono_results[j]);

        i++;
        j += result.stays.len;
    }
    mco_results_to_mono.TrySet(mco_results.end(), mco_mono_results.end());

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

        mco_results_by_ghm_root.Set(ghm_root, ptrs);
    }

    return true;
}

static Span<const mco_Result> GetResultsRange(LocalDate min_date, LocalDate max_date)
{
    const mco_Result *start =
        std::lower_bound(mco_results.begin(), mco_results.end(), min_date,
                         [](const mco_Result &result, LocalDate date) {
        return result.stays[result.stays.len - 1].exit.date < date;
    });
    const mco_Result *end =
        std::upper_bound(start, (const mco_Result *)mco_results.end(), max_date - 1,
                         [](LocalDate date, const mco_Result &result) {
        return date < result.stays[result.stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

static Span<const mco_Result *> GetIndexRange(Span<const mco_Result *> index,
                                              LocalDate min_date, LocalDate max_date)
{
    const mco_Result **start =
        std::lower_bound(index.begin(), index.end(), min_date,
                         [](const mco_Result *result, LocalDate date) {
        return result->stays[result->stays.len - 1].exit.date < date;
    });
    const mco_Result **end =
        std::upper_bound(start, index.end(), max_date - 1,
                         [](LocalDate date, const mco_Result *result) {
        return date < result->stays[result->stays.len - 1].exit.date;
    });

    return MakeSpan(start, end);
}

bool McoResultProvider::Run(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    K_ASSERT(min_date.IsValid() && max_date.IsValid());

    if (filter) {
        return RunFilter(func);
    } else if (ghm_root.IsValid()) {
        return RunIndex(func);
    } else {
        return RunDirect(func);
    }
}

bool McoResultProvider::RunFilter(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    K_ASSERT(filter);

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
                        MemMove(&mono_results_buf[k], &mono_results_buf[m],
                                    result.stays.len * K_SIZE(mco_Result));

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

bool McoResultProvider::RunIndex(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    K_ASSERT(ghm_root.IsValid());

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

bool McoResultProvider::RunDirect(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func)
{
    K_ASSERT(!ghm_root.IsValid());

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

}
