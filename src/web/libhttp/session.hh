// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "http.hh"

#include <shared_mutex>

namespace RG {

class http_SessionManager {
    RG_DELETE_COPY(http_SessionManager)

    struct Session {
        char session_key[129];
        char client_addr[RG_SIZE(http_RequestInfo::client_addr)];
        char user_agent[134];

        int64_t login_time;
        int64_t register_time;

        RetainPtr<RetainObject> udata;

        RG_HASHTABLE_HANDLER_T(Session, const char *, session_key);
    };

    std::shared_mutex mutex;
    HashTable<const char *, Session> sessions;

public:
    http_SessionManager() = default;

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
    Session *FindSession(const http_RequestInfo &request, bool *out_mismatch = nullptr);

    void PruneStaleSessions();
};

}
