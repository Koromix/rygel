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

#include <thread>

namespace K {

struct InstanceData {
    sq_Database db;
};

RcppExport SEXP hmR_Open(SEXP filename_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    Rcpp::String filename(filename_xp);

    InstanceData *inst = new InstanceData;
    K_DEFER_N(inst_guard) { delete inst; };

    if (!inst->db.Open(filename.get_cstring(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        rcc_StopWithLastError();
    if (!MigrateDatabase(&inst->db))
        rcc_StopWithLastError();

    SEXP inst_xp = R_MakeExternalPtr(inst, R_NilValue, R_NilValue);
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

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);

    inst->db.Close();

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

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    Rcpp::String name(name_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector name;
        Rcpp::CharacterVector path;
    } concepts;

    concepts.df = Rcpp::DataFrame(concepts_xp);
    concepts.len = concepts.df.nrow();
    concepts.name = concepts.df["name"];
    concepts.path = concepts.df["path"];

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

        for (Size i = 0; i < concepts.len; i++) {
            const char *name = (const char *)concepts.name[i];
            const char *path = (const char *)concepts.path[i];

            if (path[0] != '/') {
                LogError("Path '%1' does not start with '/'", path);
                return false;
            }

            int64_t cpt;
            {
                sq_Statement stmt;
                if (!inst->db.Prepare(R"(INSERT INTO concepts (domain, name, changeset)
                                         VALUES (?1, ?2, ?3)
                                         ON CONFLICT DO UPDATE SET name = excluded.name,
                                                                   changeset = excluded.changeset
                                         RETURNING concept)",
                                      &stmt, domain, name, sq_Binding(changeset)))
                    return false;
                if (!stmt.GetSingleValue(&cpt))
                    return false;
            }

            if (!inst->db.Run("INSERT INTO items (view, path, concept) VALUES (?1, ?2, ?3)", view, path, cpt))
                return false;
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

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
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

RcppExport SEXP hmR_AddEvents(SEXP inst_xp, SEXP events_xp, SEXP reset_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    bool reset = Rcpp::as<bool>(reset_xp);

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
    events.warning = events.df["warning"];

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < events.len; i++) {
            const char *target = (const char *)events.entity[i];
            const char *domain = (const char *)events.domain[i];
            const char *name = (const char *)events.name[i];
            int64_t time = events.time[i];
            bool warning = events.warning[i];

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0)
                return false;

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

RcppExport SEXP hmR_AddPeriods(SEXP inst_xp, SEXP periods_xp, SEXP reset_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    bool reset = Rcpp::as<bool>(reset_xp);

    struct {
        Rcpp::DataFrame df;
        Size len;
        Rcpp::CharacterVector entity;
        Rcpp::CharacterVector domain;
        Rcpp::CharacterVector name;
        Rcpp::NumericVector time;
        Rcpp::NumericVector duration;
    } periods;

    periods.df = Rcpp::DataFrame(periods_xp);
    periods.len = periods.df.nrow();
    periods.entity = periods.df["entity"];
    periods.domain = periods.df["domain"];
    periods.name = periods.df["concept"];
    periods.time = periods.df["time"];
    periods.duration = periods.df["duration"];

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < periods.len; i++) {
            const char *target = (const char *)periods.entity[i];
            const char *domain = (const char *)periods.domain[i];
            const char *name = (const char *)periods.name[i];
            int64_t time = periods.time[i];
            int64_t duration = periods.duration[i];

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0)
                return false;

            if (reset) {
                bool inserted;
                set.TrySet(entity, &inserted);

                if (inserted && !inst->db.Run("DELETE FROM periods WHERE entity = ?1", entity))
                    return false;
            }

            if (!inst->db.Run(R"(INSERT INTO periods (entity, concept, timestamp, duration)
                                 VALUES (?1, ?2, ?3, ?4))",
                              entity, cpt, time, duration))
                return false;
        }

        return true;
    });
    if (!success)
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_AddValues(SEXP inst_xp, SEXP values_xp, SEXP reset_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    bool reset = Rcpp::as<bool>(reset_xp);

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
    values.warning = values.df["warning"];

    bool success = inst->db.Transaction([&]() {
        HashSet<int64_t> set;

        for (Size i = 0; i < values.len; i++) {
            const char *target = (const char *)values.entity[i];
            const char *domain = (const char *)values.domain[i];
            const char *name = (const char *)values.name[i];
            int64_t time = values.time[i];
            double value = values.value[i];
            bool warning = values.warning[i];

            int64_t entity = FindOrCreateEntity(&inst->db, target);
            if (entity < 0)
                return false;

            int64_t cpt = FindConcept(&inst->db, domain, name);
            if (cpt < 0)
                return false;

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

RcppExport SEXP hmR_DeleteDomain(SEXP inst_xp, SEXP name_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    Rcpp::String name(name_xp);

    if (!inst->db.Run("DELETE FROM domains WHERE name = ?1", (const char *)name.get_cstring()))
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_DeleteView(SEXP inst_xp, SEXP name_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    Rcpp::String name(name_xp);

    if (!inst->db.Run("DELETE FROM views WHERE name = ?1", (const char *)name.get_cstring()))
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

RcppExport SEXP hmR_DeleteEntity(SEXP inst_xp, SEXP name_xp)
{
    BEGIN_RCPP
    K_DEFER { rcc_DumpWarnings(); };

    InstanceData *inst = (InstanceData *)rcc_GetPointerSafe(inst_xp);
    Rcpp::String name(name_xp);

    if (!inst->db.Run("DELETE FROM entities WHERE name = ?1", (const char *)name.get_cstring()))
        rcc_StopWithLastError();

    return R_NilValue;

    END_RCPP
}

}

RcppExport void R_init_heimdallR(DllInfo *dll) {
    static const R_CallMethodDef call_entries[] = {
        { "hmR_Open", (DL_FUNC)&K::hmR_Open, 1 },
        { "hmR_Close", (DL_FUNC)&K::hmR_Close, 1 },
        { "hmR_SetDomain", (DL_FUNC)&K::hmR_SetDomain, 3 },
        { "hmR_SetView", (DL_FUNC)&K::hmR_SetView, 3 },
        { "hmR_AddEvents", (DL_FUNC)&K::hmR_AddEvents, 3 },
        { "hmR_AddPeriods", (DL_FUNC)&K::hmR_AddPeriods, 3 },
        { "hmR_AddValues", (DL_FUNC)&K::hmR_AddValues, 3 },
        { "hmR_DeleteDomain", (DL_FUNC)&K::hmR_DeleteDomain, 1 },
        { "hmR_DeleteView", (DL_FUNC)&K::hmR_DeleteView, 1 },
        { "hmR_DeleteEntity", (DL_FUNC)&K::hmR_DeleteEntity, 1 },
        {}
    };

    R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);

    K::rcc_RedirectLog();
}
