// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "mco_authorization.hh"
#include "mco_tables.hh"

Span<const mco_Authorization> mco_AuthorizationSet::FindUnit(drd_UnitCode unit) const
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

const mco_Authorization *mco_AuthorizationSet::FindUnit(drd_UnitCode unit, Date date) const
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

int8_t mco_AuthorizationSet::GetAuthorizationType(drd_UnitCode unit, Date date) const
{
    if (unit.number >= 10000) {
        return (int8_t)(unit.number % 100);
    } else if (unit.number) {
        const mco_Authorization *auth = FindUnit(unit, date);
        if (UNLIKELY(!auth))
            return 0;
        return auth->type;
    } else {
        return 0;
    }
}

bool mco_AuthorizationSet::TestAuthorization(drd_UnitCode unit, Date date, int8_t auth_type) const
{
    if (GetAuthorizationType(unit, date) == auth_type)
        return true;
    for (const mco_Authorization &auth: facility_authorizations) {
        if (auth.type == auth_type && date >= auth.dates[0] && date < auth.dates[1])
            return true;
    }

    return false;
}

bool mco_AuthorizationSetBuilder::LoadFicum(StreamReader &st)
{
    static const Date default_end_date = mco_ConvertDate1980(UINT16_MAX);

    Size authorizations_len = set.authorizations.len;
    DEFER_NC(out_guard, facility_authorizations_len = set.facility_authorizations.len) {
        set.authorizations.RemoveFrom(authorizations_len);
        set.facility_authorizations.RemoveFrom(facility_authorizations_len);
    };

    LineReader reader(&st);
    reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        Span<const char> line;
        while (reader.Next(&line)) {
            if (line.len < 28) {
                LogError("Truncated FICUM line");
                valid = false;
                continue;
            }

            mco_Authorization auth = {};
            HeapArray<mco_Authorization> *authorizations;

            if (line.Take(0, 3) == "$$$") {
                auth.unit.number = INT16_MAX;
                authorizations = &set.facility_authorizations;
            } else {
                auth.unit = drd_UnitCode::FromString(line.Take(0, 4));
                valid &= auth.unit.IsValid();
                authorizations = &set.authorizations;
            }
            valid &= ParseDec(line.Take(13, 3), &auth.type,
                              DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::End);
            ParseDec(line.Take(16, 2), &auth.dates[0].st.day,
                    DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            ParseDec(line.Take(18, 2), &auth.dates[0].st.month,
                     DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            ParseDec(line.Take(20, 4), &auth.dates[0].st.year,
                     DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::Log);
            auth.dates[1] = default_end_date;

            if (!auth.unit.number || !auth.dates[0].IsValid()) {
                LogError("Invalid authorization attributes");
                valid = false;
            }

            authorizations->Append(auth);
        }
    }
    if (reader.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

bool mco_AuthorizationSetBuilder::LoadIni(StreamReader &st)
{
    Size authorizations_len = set.authorizations.len;
    DEFER_NC(out_guard, facility_authorizations_len = set.facility_authorizations.len) {
        set.authorizations.RemoveFrom(authorizations_len);
        set.facility_authorizations.RemoveFrom(facility_authorizations_len);
    };

    IniParser ini(&st);

    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            mco_Authorization auth = {};

            HeapArray<mco_Authorization> *authorizations;
            if (prop.section == "Facility") {
                auth.unit.number = INT16_MAX;
                authorizations = &set.facility_authorizations;
            } else {
                auth.unit = drd_UnitCode::FromString(prop.section);
                valid &= auth.unit.IsValid();
                authorizations = &set.authorizations;
            }

            do {
                if (prop.key == "Authorization") {
                    valid &= ParseDec(prop.value, &auth.type, DEFAULT_PARSE_FLAGS & ~(int)ParseFlag::End);
                } else if (prop.key == "Date") {
                    auth.dates[0] = Date::FromString(prop.value);
                    valid &= !!auth.dates[0].value;
                } else if (prop.key == "End") {
                    auth.dates[1] = Date::FromString(prop.value);
                    valid &= !!auth.dates[1].value;
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!auth.unit.number || !auth.dates[0].value) {
                LogError("Missing authorization attributes");
                valid = false;
            }
            if (!auth.dates[1].value) {
                static const Date default_end_date = mco_ConvertDate1980(UINT16_MAX);
                auth.dates[1] = default_end_date;
            }

            authorizations->Append(auth);
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

bool mco_AuthorizationSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (mco_AuthorizationSetBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &mco_AuthorizationSetBuilder::LoadIni;
        } else if (extension == ".txt" || extension == ".ficum") {
            load_func = &mco_AuthorizationSetBuilder::LoadFicum;
        } else {
            LogError("Cannot load authorizations from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (st.error) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
    }

    return success;
}

void mco_AuthorizationSetBuilder::Finish(mco_AuthorizationSet *out_set)
{
    std::sort(set.authorizations.begin(), set.authorizations.end(),
              [](const mco_Authorization &auth1, const mco_Authorization &auth2) {
        return MultiCmp(auth1.unit.number - auth2.unit.number,
                        auth1.dates[0] - auth2.dates[0]) < 0;
    });

    // Fix end dates and map
    for (Size i = 0; i < set.authorizations.len; i++) {
        const mco_Authorization &auth = set.authorizations[i];

        if (i && set.authorizations[i - 1].unit == auth.unit) {
            set.authorizations[i - 1].dates[1] = auth.dates[0];
        }
        set.authorizations_map.Append(&auth);
    }

    SwapMemory(out_set, &set, SIZE(set));
}

bool mco_LoadAuthorizationSet(const char *profile_directory,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set)
{
    static const char *const default_names[] = {
        "mco_authorizations.ini",
        "mco_authorizations.txt"
    };

    BlockAllocator temp_alloc;

    const char *filename = nullptr;
    {
        if (authorization_filename) {
            filename = authorization_filename;
        } else {
            for (const char *default_name: default_names) {
                const char *test_filename = Fmt(&temp_alloc, "%1%/%2",
                                                profile_directory, default_name).ptr;
                if (TestFile(test_filename, FileType::File)) {
                    filename = test_filename;
                    break;
                }
            }
        }
    }

    if (filename && filename[0]) {
        mco_AuthorizationSetBuilder authorization_set_builder;
        if (!authorization_set_builder.LoadFiles(filename))
            return false;
        authorization_set_builder.Finish(out_set);
    } else {
        LogError("No authorization file specified or found");
    }

    return true;
}
