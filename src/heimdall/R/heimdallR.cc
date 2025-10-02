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

#include "src/core/base/base.hh"
#include "src/core/wrap/Rcc.hh"
#include "src/core/sqlite/sqlite.hh"
#include "../server/database.hh"

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif
#include <thread>

namespace K {

struct InstanceData {
    sq_Database db;
};

static inline SEXP GetInstanceTag()
{
    static SEXP tag = Rf_install("hmR_InstanceData");
    return tag;
}

// Give read/write permissions to owner and group.
// This makes it easier to share project databases between the user running R and the web server.
// SQLite copies the main file mode to the WAL and other files.
static void AdjustMode(const char *filename)
{
#if !defined(_WIN32)
    struct stat sb;
    if (stat(filename, &sb) < 0)
        return;

    mode_t mode = sb.st_mode | 0660;
    chmod(filename, mode);
#else
    (void)filename;
#endif
}

RcppExport SEXP hmR_Open(SEXP filename_xp, SEXP create_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    Rcpp::String filename(filename_xp);
    bool create = Rcpp::as<bool>(create_xp);

    InstanceData *inst = new InstanceData;
    K_DEFER_N(inst_guard) { delete inst; };

    int flags = SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);

    AdjustMode(filename.get_cstring());
    if (!inst->db.Open(filename.get_cstring(), flags))
        rcc_StopWithLastError();
    AdjustMode(filename.get_cstring());

    if (!inst->db.SetWAL(true))
        rcc_StopWithLastError();
    if (!MigrateDatabase(&inst->db))
        rcc_StopWithLastError();

    SEXP inst_xp = R_MakeExternalPtr(inst, GetInstanceTag(), R_NilValue);
    R_RegisterCFinalizerEx(inst_xp, [](SEXP inst_xp) {
        InstanceData *inst = (InstanceData *)R_ExternalPtrAddr(inst_xp);
        delete inst;
    }, TRUE);
    inst_guard.Disable();

    return inst_xp;

    END_RCPP
}

RcppExport SEXP hmR_Close(SEXP inst_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());

    inst->db.Close();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_Reset(SEXP inst_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());

    bool success = inst->db.Transaction([&]() {
        if (!inst->db.Run("DELETE FROM entities"))
            return false;
        if (!inst->db.Run("DELETE FROM views"))
            return false;
        if (!inst->db.Run("DELETE FROM domains"))
            return false;

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

static int64_t FindOrCreateEntity(sq_Database *db, const char *name)
{
    sq_Statement stmt;
    if (!db->Prepare(R"(INSERT INTO entities (name)
                        VALUES (?1)
                        ON CONFLICT DO UPDATE SET name = excluded.name
                        RETURNING entity)", &stmt, name))
        return false;

    if (!stmt.Step()) {
        K_ASSERT(!stmt.IsValid());
        return -1;
    }

    return sqlite3_column_int64(stmt, 0);
}

static int64_t FindConcept(sq_Database *db, const char *domain, const char *name)
{
    sq_Statement stmt;
    if (!db->Prepare(R"(SELECT c.concept
                        FROM domains d
                        INNER JOIN concepts c ON (c.domain = d.domain)
                        WHERE d.name = ?1 AND c.name = ?2)",
                     &stmt, domain, name))
        return -1;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown concept '%1' in domain '%2'", name, domain);
        }
        return -1;
    }

    return sqlite3_column_int64(stmt, 0);
}

RcppExport SEXP hmR_SetDomain(SEXP inst_xp, SEXP name_xp, SEXP concepts_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    Rcpp::String name(name_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector name;
        Rcpp::CharacterVector description;
        Rcpp::CharacterVector path;
    } concepts;

    concepts.df = Rcpp::DataFrame(concepts_xp);
    concepts.len = concepts.df.nrow();
    concepts.name = concepts.df["name"];
    if (concepts.df.containsElementNamed("description")) {
        concepts.description = concepts.df["description"];
    }
    if (concepts.df.containsElementNamed("path")) {
        concepts.path = concepts.df["path"];
    }

    uint8_t changeset[32];
    FillRandomSafe(changeset);

    bool success = inst->db.Transaction([&]() {
        int64_t domain;
        {
            sq_Statement stmt;
            if (!inst->db.Prepare(R"(INSERT INTO domains (name)
                                     VALUES (?1)
                                     ON CONFLICT DO UPDATE SET name = excluded.name
                                     RETURNING domain)", &stmt, (const char *)name.get_cstring()))
                return false;
            if (!stmt.GetSingleValue(&domain))
                return false;
        }

        int64_t view = -1;
        if (concepts.path.length()) {
            sq_Statement stmt;
            if (!inst->db.Prepare(R"(INSERT INTO views (name)
                                     VALUES (?1)
                                     ON CONFLICT DO UPDATE SET name = excluded.name
                                     RETURNING view)", &stmt, (const char *)name.get_cstring()))
                return false;
            if (!stmt.GetSingleValue(&view))
                return false;

            if (!inst->db.Run("DELETE FROM items WHERE view = ?1", view))
                return false;
        }

        for (Size i = 0; i < concepts.len; i++) {
            const char *name = (const char *)concepts.name[i];
            const char *description = concepts.description.length() ? (const char *)concepts.description[i] : "";

            int64_t cpt;
            {
                sq_Statement stmt;
                if (!inst->db.Prepare(R"(INSERT INTO concepts (domain, name, description, changeset)
                                         VALUES (?1, ?2, ?3, ?4)
                                         ON CONFLICT DO UPDATE SET description = excluded.description,
                                                                   changeset = excluded.changeset
                                         RETURNING concept)",
                                      &stmt, domain, name, description, sq_Binding(changeset)))
                    return false;
                if (!stmt.GetSingleValue(&cpt))
                    return false;
            }

            if (view >= 0) {
                const char *path = (const char *)concepts.path[i];

                if (path[0] != '/') {
                    LogError("Path '%1' does not start with '/'", path);
                    return false;
                }

                if (!inst->db.Run("INSERT INTO items (view, path, concept) VALUES (?1, ?2, ?3)", view, path, cpt))
                    return false;
            }
        }

        if (!inst->db.Run("DELETE FROM concepts WHERE domain = ?1 AND changeset IS NOT ?2",
                          domain, sq_Binding(changeset)))
            return false;

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_SetView(SEXP inst_xp, SEXP name_xp, SEXP items_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    Rcpp::String name(name_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector path;
        Rcpp::CharacterVector domain;
        Rcpp::CharacterVector name;
    } items;

    items.df = Rcpp::DataFrame(items_xp);
    items.len = items.df.nrow();
    items.path = items.df["path"];
    items.domain = items.df["domain"];
    items.name = items.df["concept"];

    bool success = inst->db.Transaction([&]() {
        int64_t view;
        {
            sq_Statement stmt;
            if (!inst->db.Prepare(R"(INSERT INTO views (name)
                                     VALUES (?1)
                                     ON CONFLICT DO UPDATE SET name = excluded.name
                                     RETURNING view)", &stmt, (const char *)name.get_cstring()))
                return false;
            if (!stmt.GetSingleValue(&view))
                return false;

            if (!inst->db.Run("DELETE FROM items WHERE view = ?1", view))
                return false;
        }

        for (Size i = 0; i < items.len; i++) {
            const char *path = (const char *)items.path[i];
            const char *domain = (const char *)items.domain[i];
            const char *name = (const char *)items.name[i];

            if (path[0] != '/') {
                LogError("Path '%1' does not start with '/'", path);
                return false;
            }

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0)
                return false;

            if (!inst->db.Run("INSERT INTO items (view, path, concept) VALUES (?1, ?2, ?3)", view, path, cpt))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_AddEvents(SEXP inst_xp, SEXP events_xp, SEXP reset_xp, SEXP strict_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    bool reset = Rcpp::as<bool>(reset_xp);
    bool strict = Rcpp::as<bool>(strict_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector entity;
        Rcpp::CharacterVector domain;
        Rcpp::CharacterVector name;
        Rcpp::NumericVector time;
        Rcpp::LogicalVector warning;
    } events;

    events.df = Rcpp::DataFrame(events_xp);
    events.len = events.df.nrow();
    events.entity = events.df["entity"];
    events.domain = events.df["domain"];
    events.name = events.df["concept"];
    events.time = events.df["time"];
    if (events.df.containsElementNamed("warning")) {
        events.warning = events.df["warning"];
    }

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < events.len; i++) {
            const char *target = (const char *)events.entity[i];
            const char *domain = (const char *)events.domain[i];
            const char *name = (const char *)events.name[i];
            int64_t time = events.time[i];
            bool warning = events.warning.length() ? events.warning[i] : false;

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0) {
                if (strict)
                    return false;
                continue;
            }

            if (reset) {
                bool inserted;
                set.TrySet(entity, &inserted);

                if (inserted && !inst->db.Run("DELETE FROM events WHERE entity = ?1", entity))
                    return false;
            }

            if (!inst->db.Run(R"(INSERT INTO events (entity, concept, timestamp, warning)
                                 VALUES (?1, ?2, ?3, ?4))",
                              entity, cpt, time, 0 + warning))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_AddPeriods(SEXP inst_xp, SEXP periods_xp, SEXP reset_xp, SEXP strict_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    bool reset = Rcpp::as<bool>(reset_xp);
    bool strict = Rcpp::as<bool>(strict_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector entity;
        Rcpp::CharacterVector domain;
        Rcpp::CharacterVector name;
        Rcpp::NumericVector time;
        Rcpp::NumericVector duration;
        Rcpp::CharacterVector color;
    } periods;

    periods.df = Rcpp::DataFrame(periods_xp);
    periods.len = periods.df.nrow();
    periods.entity = periods.df["entity"];
    periods.domain = periods.df["domain"];
    periods.name = periods.df["concept"];
    periods.time = periods.df["time"];
    periods.duration = periods.df["duration"];
    if (periods.df.containsElementNamed("color")) {
        periods.color = periods.df["color"];
    }

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < periods.len; i++) {
            const char *target = (const char *)periods.entity[i];
            const char *domain = (const char *)periods.domain[i];
            const char *name = (const char *)periods.name[i];
            int64_t time = periods.time[i];
            int64_t duration = periods.duration[i];
            const char *color = nullptr;

            if (periods.color.length()) {
                const char *str = (const char *)periods.color[i];
                color = (str != CHAR(NA_STRING)) ? str : nullptr;
            }

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0) {
                if (strict)
                    return false;
                continue;
            }

            if (reset) {
                bool inserted;
                set.TrySet(entity, &inserted);

                if (inserted && !inst->db.Run("DELETE FROM periods WHERE entity = ?1", entity))
                    return false;
            }

            if (!inst->db.Run(R"(INSERT INTO periods (entity, concept, timestamp, duration, color)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              entity, cpt, time, duration, color))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_AddValues(SEXP inst_xp, SEXP values_xp, SEXP reset_xp, SEXP strict_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    bool reset = Rcpp::as<bool>(reset_xp);
    bool strict = Rcpp::as<bool>(strict_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector entity;
        Rcpp::CharacterVector domain;
        Rcpp::CharacterVector name;
        Rcpp::NumericVector time;
        Rcpp::NumericVector value;
        Rcpp::LogicalVector warning;
    } values;

    values.df = Rcpp::DataFrame(values_xp);
    values.len = values.df.nrow();
    values.entity = values.df["entity"];
    values.domain = values.df["domain"];
    values.name = values.df["concept"];
    values.time = values.df["time"];
    values.value = values.df["value"];
    if (values.df.containsElementNamed("warning")) {
        values.warning = values.df["warning"];
    }

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < values.len; i++) {
            const char *target = (const char *)values.entity[i];
            const char *domain = (const char *)values.domain[i];
            const char *name = (const char *)values.name[i];
            int64_t time = values.time[i];
            double value = values.value[i];
            bool warning = values.warning.length() ? values.warning[i] : false;

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0) {
                if (strict)
                    return false;
                continue;
            }

            if (reset) {
                bool inserted;
                set.TrySet(entity, &inserted);

                if (inserted && !inst->db.Run("DELETE FROM measures WHERE entity = ?1", entity))
                    return false;
            }

            if (!inst->db.Run(R"(INSERT INTO measures (entity, concept, timestamp, value, warning)
                                 VALUES (?1, ?2, ?3, ?4, ?5))",
                              entity, cpt, time, value, 0 + warning))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_DeleteDomains(SEXP inst_xp, SEXP names_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    Rcpp::CharacterVector names(names_xp);

    bool success = inst->db.Transaction([&]() {
        for (const char *name: names) {
            if (!inst->db.Run("DELETE FROM domains WHERE name = ?1", name))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_DeleteViews(SEXP inst_xp, SEXP names_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    Rcpp::CharacterVector names(names_xp);

    bool success = inst->db.Transaction([&]() {
        for (const char *name: names) {
            if (!inst->db.Run("DELETE FROM views WHERE name = ?1", name))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_DeleteEntities(SEXP inst_xp, SEXP names_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    Rcpp::CharacterVector names(names_xp);

    bool success = inst->db.Transaction([&]() {
        for (const char *name: names) {
            if (!inst->db.Run("DELETE FROM entities WHERE name = ?1", name))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_ExportMarks(SEXP inst_xp, SEXP first_xp, SEXP limit_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp, GetInstanceTag());
    int first = !Rf_isNull(first_xp) ? Rcpp::as<int>(first_xp) : 0;
    int limit = Rcpp::as<int>(limit_xp);

    int64_t nrow;
    {
        sq_Statement stmt;
        if (!inst->db.Prepare(R"(SELECT COUNT(*) FROM marks
                                 WHERE entity IS NOT NULL AND mark >= IFNULL(?1, 0))", &stmt, first))
            rcc_StopWithLastError();
        if (!stmt.GetSingleValue(&nrow))
            rcc_StopWithLastError();
    }

    nrow = std::min(nrow, (int64_t)limit);

    rcc_DataFrameBuilder df_builder(nrow);
    rcc_Vector<int> id = df_builder.Add<int>("id");
    rcc_Vector<const char *> name = df_builder.Add<const char *>("name");
    rcc_Vector<int> timestamp = df_builder.Add<int>("timestamp");
    rcc_Vector<bool> status = df_builder.Add<bool>("status");
    rcc_Vector<const char *> comment = df_builder.Add<const char *>("comment");

    Size count = 0;
    {
        sq_Statement stmt;
        if (!inst->db.Prepare(R"(SELECT mark, name, timestamp, status, comment
                                 FROM marks
                                 WHERE entity IS NOT NULL AND mark >= IFNULL(?1, 0))", &stmt, first))
            rcc_StopWithLastError();

        while (stmt.Step() && count < nrow) {
            id[count] = sqlite3_column_int(stmt, 0);
            name.Set(count, (const char *)sqlite3_column_text(stmt, 1));
            timestamp[count] = (int)(sqlite3_column_int64(stmt, 2) / 1000);
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                status.Set(count, sqlite3_column_int(stmt, 3));
            } else {
                status.SetNA(count);
            }
            comment.Set(count, sqlite3_column_bytes(stmt, 4) ? (const char *)sqlite3_column_text(stmt, 4) : nullptr);

            count++;
        }
        if (!stmt.IsValid())
            rcc_StopWithLastError();
    }

    // Set time class on timestamp
    {
        rcc_AutoSexp cls = Rf_allocVector(STRSXP, 2);
        SET_STRING_ELT(cls, 0, Rf_mkChar("POSIXct"));
        SET_STRING_ELT(cls, 1, Rf_mkChar("POSIXt"));
        Rf_setAttrib(timestamp, R_ClassSymbol, cls);
    }

    return df_builder.Build(count);

    END_RCPP
}

}

RcppExport void R_init_heimdallR(DllInfo *dll) {
    static const R_CallMethodDef call_entries[] = {
        { "hmR_Open", (DL_FUNC)&K::hmR_Open, 2 },
        { "hmR_Close", (DL_FUNC)&K::hmR_Close, 1 },
        { "hmR_Reset", (DL_FUNC)&K::hmR_Reset, 1 },
        { "hmR_SetDomain", (DL_FUNC)&K::hmR_SetDomain, 3 },
        { "hmR_SetView", (DL_FUNC)&K::hmR_SetView, 3 },
        { "hmR_AddEvents", (DL_FUNC)&K::hmR_AddEvents, 4 },
        { "hmR_AddPeriods", (DL_FUNC)&K::hmR_AddPeriods, 4 },
        { "hmR_AddValues", (DL_FUNC)&K::hmR_AddValues, 4 },
        { "hmR_DeleteDomains", (DL_FUNC)&K::hmR_DeleteDomains, 2 },
        { "hmR_DeleteViews", (DL_FUNC)&K::hmR_DeleteViews, 2 },
        { "hmR_DeleteEntities", (DL_FUNC)&K::hmR_DeleteEntities, 2 },
        { "hmR_ExportMarks", (DL_FUNC)&K::hmR_ExportMarks, 3 },
        {}
    };

    R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);

    K::rcc_RedirectLog();
}
