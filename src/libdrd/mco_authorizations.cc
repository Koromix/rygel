// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_authorizations.hh"
#include "mco_tables.hh"

bool mco_LoadAuthorizationFile(const char *filename, mco_AuthorizationSet *out_set)
{
    Size authorizations_len = out_set->authorizations.len;
    DEFER_NC(out_guard, facility_authorizations_len = out_set->facility_authorizations.len) {
        out_set->authorizations.RemoveFrom(authorizations_len);
        out_set->facility_authorizations.RemoveFrom(facility_authorizations_len);
    };

    // Parse
    {
        StreamReader st(filename);
        if (st.error)
            return false;

        IniParser ini(&st);
        ini.reader.PushLogHandler();
        DEFER { PopLogHandler(); };

        IniProperty prop;
        bool valid = true;
        mco_Authorization *auth = nullptr;
        while (ini.Next(&prop)) {
            if (prop.flags & (int)IniProperty::Flag::NewSection) {
                if (prop.section == "Facility") {
                    auth = out_set->facility_authorizations.AppendDefault();
                    *auth = {};
                    auth->unit.number = INT16_MAX;
                } else {
                    auth = out_set->authorizations.AppendDefault();
                    *auth = {};
                    // FIXME: Use UnitCode::FromString() instead
                    valid &= ParseDec(prop.section, &auth->unit.number);
                    if (auth->unit.number > 9999) {
                        LogError("Invalid Unit number %1", auth->unit.number);
                        valid = false;
                    }
                }
            }

            if (prop.key == "Authorization") {
                valid &= ParseDec(prop.value, &auth->type, DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::End);
            } else if (prop.key == "Date") {
                static const Date default_end_date = mco_ConvertDate1980(UINT16_MAX);

                auth->dates[0] = Date::FromString(prop.value);
                auth->dates[1] = default_end_date;
                valid &= !!auth->dates[0].value;
            } else {
                LogError("Unknown attribute '%1'", prop.key);
                valid = false;
            }
        }
        if (ini.error || !valid)
            return false;

        // FIXME: Avoid incomplete authorizations (date errors can even crash)
    }

    Span<mco_Authorization> authorizations =
        out_set->authorizations.Take(authorizations_len, out_set->authorizations.len - authorizations_len);

    std::sort(authorizations.begin(), authorizations.end(),
              [](const mco_Authorization &auth1, const mco_Authorization &auth2) {
        return MultiCmp(auth1.unit.number - auth2.unit.number,
                        auth1.dates[0] - auth2.dates[0]) < 0;
    });

    // Fix end dates and map
    for (Size i = 0; i < out_set->authorizations.len; i++) {
        const mco_Authorization &auth = authorizations[i];

        if (i && authorizations[i - 1].unit == auth.unit) {
            authorizations[i - 1].dates[1] = auth.dates[0];
        }
        out_set->authorizations_map.Append(&auth);
    }

    out_guard.disable();
    return true;
}

Span<const mco_Authorization> mco_AuthorizationSet::FindUnit(UnitCode unit) const
{
    Span<const mco_Authorization> auths;
    auths.ptr = authorizations_map.FindValue(unit, nullptr);
    if (!auths.ptr)
        return {};

    {
        const mco_Authorization *end_auth = auths.ptr + 1;
        while (end_auth < authorizations.end() &&
               end_auth->unit == unit) {
            end_auth++;
        }
        auths.len = end_auth - auths.ptr;
    }

    return auths;
}

const mco_Authorization *mco_AuthorizationSet::FindUnit(UnitCode unit, Date date) const
{
    const mco_Authorization *auth = authorizations_map.FindValue(unit, nullptr);
    if (!auth)
        return nullptr;

    do {
        if (date >= auth->dates[0] && date < auth->dates[1])
            return auth;
    } while (++auth < authorizations.end() && auth->unit == unit);

    return nullptr;
}

int8_t mco_AuthorizationSet::GetAuthorizationType(UnitCode unit, Date date) const
{
    if (unit.number >= 10000) {
        return (int8_t)(unit.number % 100);
    } else if (unit.number) {
        const mco_Authorization *auth = FindUnit(unit, date);
        if (UNLIKELY(!auth)) {
            LogDebug("Unit %1 is missing from authorization set", unit);
            return 0;
        }
        return auth->type;
    } else {
        return 0;
    }
}

bool mco_AuthorizationSet::TestAuthorization(UnitCode unit, Date date, int8_t auth_type) const
{
    if (GetAuthorizationType(unit, date) == auth_type)
        return true;
    for (const mco_Authorization &auth: facility_authorizations) {
        if (auth.type == auth_type && date >= auth.dates[0] && date < auth.dates[1])
            return true;
    }

    return false;
}
