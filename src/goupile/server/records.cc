// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "src/core/libwrap/json.hh"
#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace RG {

void HandleRecordGet(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!stamp) {
        LogError("User is not allowed to load data");
        io->AttachError(403);
        return;
    }

    const char *tid = request.GetQueryValue("tid");
    if (!tid) {
        LogError("Missing 'tid' parameter");
        io->AttachError(422);
        return;
    }

    sq_Statement stmt;
    {
        LocalArray<char, 1024> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT e.eid, e.ctime, e.mtime, e.store, e.sequence, e.data
                                               FROM rec_threads t
                                               INNER JOIN rec_entries e ON (e.tid = t.tid)
                                               WHERE t.tid = ?1 AND t.deleted = 0)").len;
        if (!stamp->HasPermission(UserPermission::DataLoad)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM ins_claims WHERE userid = ?2)").len;
        }

        if (!instance->db->Prepare(sql.data, &stmt))
            return;
        sqlite3_bind_text(stmt, 1, tid, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, -session->userid);
    }

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Thread '%1' does not exist", tid);
            io->AttachError(404);
        }
        return;
    }

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    do {
        const char *store = (const char *)sqlite3_column_text(stmt, 3);

        json.Key(store); json.StartObject();
            json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 0));
            json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 1));
            json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 2));
            json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 4));
            json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 5));
        json.EndObject();
    } while (stmt.Step());
    if (!stmt.IsValid())
        return;
    json.EndObject();

    json.Finish();
}

void HandleRecordSave(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

void HandleRecordExport(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

}
