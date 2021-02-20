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

#pragma once

#include "../../core/libcc/libcc.hh"
#include "http.hh"

namespace RG {

class http_SessionManager {
    RG_DELETE_COPY(http_SessionManager)

    struct Session {
        char session_key[65];
        char session_rnd[33];
        char user_agent[134];

        int64_t login_time;
        int64_t register_time;
        int64_t lock_time;

        RetainPtr<RetainObject> udata;

        RG_HASHTABLE_HANDLER_T(Session, const char *, session_key);
    };

    const char *cookie_path = "/";

    std::shared_mutex mutex;
    BucketArray<Session> sessions;
    HashTable<const char *, Session *> sessions_map;

public:
    http_SessionManager() = default;

    void SetCookiePath(const char *new_path) { cookie_path = new_path; }

    template<typename T>
    void Open(const http_RequestInfo &request, http_IO *io, RetainPtr<T> udata)
    {
        RetainPtr<RetainObject> *udata2 = (RetainPtr<RetainObject> *)&udata;
        Open2(request, io, *udata2);
    }
    void Close(const http_RequestInfo &request, http_IO *io);

    template<typename T>
    RetainPtr<T> Find(const http_RequestInfo &request, http_IO *io)
    {
        RetainObject *udata = Find2(request, io);
        return RetainPtr<T>((T *)udata, false);
    }

private:
    void Open2(const http_RequestInfo &request, http_IO *io, RetainPtr<RetainObject> udata);
    Session *CreateSession(const http_RequestInfo &request, http_IO *io);

    RetainObject *Find2(const http_RequestInfo &request, http_IO *io);
    Session **FindSession(const http_RequestInfo &request, bool *out_mismatch, bool *out_locked);

    void DeleteSessionCookies(const http_RequestInfo &request, http_IO *io);

    void PruneStaleSessions();
};

}
