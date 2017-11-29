#include "kutil.hh"
#include "d_authorizations.hh"
#include "d_tables.hh"

class JsonAuthorizationHandler: public BaseJsonHandler<JsonAuthorizationHandler> {
    enum class State {
        Default,

        AuthArray,
        AuthObject,
        AuthAuthorization,
        AuthBeginDate,
        AuthEndDate,
        AuthUnit
    };

    State state = State::Default;

    Authorization auth = {};

public:
    HeapArray<Authorization> *out_authorizations;

    JsonAuthorizationHandler(HeapArray<Authorization> *out_authorizations = nullptr)
        : out_authorizations(out_authorizations) {}

    bool StartArray()
    {
        if (state != State::Default) {
            LogError("Unexpected array");
            return false;
        }

        state = State::AuthArray;
        return true;
    }
    bool EndArray(rapidjson::SizeType)
    {
        if (state != State::AuthArray) {
            LogError("Unexpected end of array");
            return false;
        }

        state = State::Default;
        return true;
    }

    bool StartObject()
    {
        if (state != State::AuthArray) {
            LogError("Unexpected object");
            return false;
        }

        state = State::AuthObject;
        return true;
    }
    bool EndObject(rapidjson::SizeType)
    {
        if (state != State::AuthObject) {
            LogError("Unexpected end of object");
            return false;
        }

        if (!auth.dates[1].value) {
            static const Date default_end_date = ConvertDate1980(UINT16_MAX);
            auth.dates[1] = default_end_date;
        }

        out_authorizations->Append(auth);
        auth = {};

        state = State::AuthArray;
        return true;
    }

    bool Key(const char *key, rapidjson::SizeType, bool) {
#define HANDLE_KEY(Key, State) \
            do { \
                if (TestStr(key, (Key))) { \
                    state = State; \
                    return true; \
                } \
             } while (false)

        if (state != State::AuthObject) {
            LogError("Unexpected key token '%1'", key);
            return false;
        }

        HANDLE_KEY("authorization", State::AuthAuthorization);
        HANDLE_KEY("begin_date", State::AuthBeginDate);
        HANDLE_KEY("end_date", State::AuthEndDate);
        HANDLE_KEY("unit", State::AuthUnit);

        LogError("Unknown authorization attribute '%1'", key);
        return false;

#undef HANDLE_KEY
    }

    bool Int(int i)
    {
        switch (state) {
            case State::AuthAuthorization: {
                if (i >= 0 && i < 100) {
                    auth.type = (int8_t)i;
                } else {
                    LogError("Invalid authorization type %1", i);
                }
            } break;
            case State::AuthUnit: {
                if (i >= 0 && i < 10000) {
                    auth.unit.number = (int16_t)i;
                } else {
                    LogError("Invalid unit code %1", i);
                }
            } break;

            default: {
                LogError("Unexpected integer value %1", i);
                return false;
            } break;
        }

        state = State::AuthObject;
        return true;
    }
    bool String(const char *str, rapidjson::SizeType, bool)
    {
        switch (state) {
            case State::AuthAuthorization: {
                int8_t type;
                if (sscanf(str, "%" SCNd8, &type) == 1 && type >= 0 && type < 100) {
                    auth.type = type;
                } else {
                    LogError("Invalid authorization type '%1'", str);
                }
            } break;
            case State::AuthBeginDate: { SetDate(&auth.dates[0], str); } break;
            case State::AuthEndDate: { SetDate(&auth.dates[1], str); } break;
            case State::AuthUnit: {
                int16_t unit;
                if (TestStr(str, "facility")) {
                    auth.unit.number = INT16_MAX;
                } else if (sscanf(str, "%" SCNd16, &unit) == 1 && unit >= 0 && unit < 10000) {
                    auth.unit.number = unit;
                } else {
                    LogError("Invalid unit code '%1'", str);
                }
            } break;

            default: {
                LogError("Unexpected string value '%1'", str);
                return false;
            } break;
        }

        state = State::AuthObject;
        return true;
    }
};

bool LoadAuthorizationFile(const char *filename, AuthorizationSet *out_set)
{
    DEFER_NC(out_guard, len = out_set->authorizations.len)
        { out_set->authorizations.RemoveFrom(len); };

    {
        StreamReader st(filename);
        if (st.error)
            return false;

        JsonAuthorizationHandler json_handler(&out_set->authorizations);
        if (!ParseJsonFile(st, &json_handler))
            return false;
    }

    for (const Authorization &auth: out_set->authorizations) {
        out_set->authorizations_map.Append(&auth);
    }

    out_guard.disable();
    return true;
}

Span<const Authorization> AuthorizationSet::FindUnit(UnitCode unit) const
{
    Span<const Authorization> auths;
    auths.ptr = authorizations_map.FindValue(unit, nullptr);
    if (!auths.ptr)
        return {};

    {
        const Authorization *end_auth = auths.ptr + 1;
        while (end_auth < authorizations.end() &&
               end_auth->unit == unit) {
            end_auth++;
        }
        auths.len = end_auth - auths.ptr;
    }

    return auths;
}

const Authorization *AuthorizationSet::FindUnit(UnitCode unit, Date date) const
{
    const Authorization *auth = authorizations_map.FindValue(unit, nullptr);
    if (!auth)
        return nullptr;

    do {
        if (date >= auth->dates[0] && date < auth->dates[1])
            return auth;
    } while (++auth < authorizations.end() && auth->unit == unit);

    return nullptr;
}
