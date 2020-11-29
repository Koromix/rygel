// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"

namespace RG {

void HandleListInstances(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list instances");
        io->AttachError(403);
        return;
    }

    std::shared_lock<std::shared_mutex> lock(goupile_domain.instances_mutex);

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    for (InstanceGuard *guard: goupile_domain.instances) {
        json.String(guard->instance.key);
    }
    json.EndArray();

    json.Finish(io);
}

void HandleListUsers(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list users");
        io->AttachError(403);
        return;
    }

    sq_Statement stmt;
    if (!goupile_domain.db.Prepare(R"(SELECT u.rowid, u.username, u.admin, p.instance, p.permissions FROM dom_users u
                                      LEFT JOIN dom_permissions p ON (p.username = u.username)
                                      ORDER BY u.rowid, p.instance;)", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    if (stmt.Next()) {
        do {
            int64_t rowid = sqlite3_column_int64(stmt, 0);

            json.StartObject();

            json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 1));
            json.Key("admin"); json.Bool(sqlite3_column_int(stmt, 2));
            json.Key("instances"); json.StartArray();
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                do {
                    uint32_t permissions = (uint32_t)sqlite3_column_int64(stmt, 4);

                    json.StartObject();
                    json.Key("instance"); json.String((const char *)sqlite3_column_text(stmt, 3));
                    json.Key("permissions"); json.StartObject();
                    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                        char js_name[64];
                        ConvertToJsonName(UserPermissionNames[i], js_name);

                        json.Key(js_name); json.Bool(permissions & (1 << i));
                    }
                    json.EndObject();

                    json.EndObject();
                } while (stmt.Next() && sqlite3_column_int64(stmt, 0) == rowid);
            } else {
                stmt.Next();
            }
            json.EndArray();

            json.EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

}
