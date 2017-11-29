#include "kutil.hh"
#include "d_desc.hh"

class JsonGhmRootDescHandler: public BaseJsonHandler<JsonGhmRootDescHandler> {
    enum class State {
        Default,

        DescArray,
        DescObject,

        DescGhmRoot,
        DescGhmRootDesc,
        DescDa,
        DescDaDesc,
        DescGa,
        DescGaDesc
    };

    State state = State::Default;

    GhmRootDesc desc = {};

public:
    HeapArray<GhmRootDesc> *out_catalog;
    Allocator *out_alloc;

    JsonGhmRootDescHandler(HeapArray<GhmRootDesc> *out_catalog = nullptr,
                           Allocator *out_alloc = nullptr)
        : out_catalog(out_catalog), out_alloc(out_alloc) {}

    bool StartArray()
    {
        if (state != State::Default) {
            LogError("Unexpected array");
            return false;
        }

        state = State::DescArray;
        return true;
    }
    bool EndArray(rapidjson::SizeType)
    {
        if (state != State::DescArray) {
            LogError("Unexpected end of array");
            return false;
        }

        state = State::Default;
        return true;
    }

    bool StartObject()
    {
        if (state != State::DescArray) {
            LogError("Unexpected object");
            return false;
        }

        state = State::DescObject;
        return true;
    }
    bool EndObject(rapidjson::SizeType)
    {
        if (state != State::DescObject) {
            LogError("Unexpected end of object");
            return false;
        }

        out_catalog->Append(desc);
        desc = {};

        state = State::DescArray;
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

        if (state != State::DescObject) {
            LogError("Unexpected key token '%1'", key);
            return false;
        }

        HANDLE_KEY("root", State::DescGhmRoot);
        HANDLE_KEY("root_desc", State::DescGhmRootDesc);
        HANDLE_KEY("da", State::DescDa);
        HANDLE_KEY("da_desc", State::DescDaDesc);
        HANDLE_KEY("ga", State::DescGa);
        HANDLE_KEY("ga_desc", State::DescGaDesc);

        LogError("Unknown authorization attribute '%1'", key);
        return false;

#undef HANDLE_KEY
    }

    bool String(const char *str, rapidjson::SizeType len, bool)
    {
        DebugAssert(out_alloc);

        switch (state) {
            case State::DescGhmRoot: {
                desc.ghm_root = GhmRootCode::FromString(str);
            } break;
            case State::DescGhmRootDesc:
                { desc.ghm_root_desc = DuplicateString(out_alloc, str).ptr; } break;
            case State::DescDa: {
                if (len == SIZE(desc.da) - 1) {
                    strcpy(desc.da, str);
                } else {
                    LogError("Malformed DA code (must be %1 characters)", SIZE(desc.da) - 1);
                }
            } break;
            case State::DescDaDesc: { desc.da_desc = DuplicateString(out_alloc, str).ptr; } break;
            case State::DescGa: {
                if (len == SIZE(desc.ga) - 1) {
                    strcpy(desc.ga, str);
                } else {
                    LogError("Malformed GA code (must be %1 characters)", SIZE(desc.ga) - 1);
                }
            } break;
            case State::DescGaDesc: { desc.ga_desc = DuplicateString(out_alloc, str).ptr; } break;

            default: {
                LogError("Unexpected string value '%1'", str);
                return false;
            } break;
        }

        state = State::DescObject;
        return true;
    }
};

bool LoadGhmRootCatalog(const char *filename, Allocator *str_alloc,
                        HeapArray<GhmRootDesc> *out_catalog, HashSet<GhmRootCode, GhmRootDesc> *out_map)
{
    DEFER_NC(out_guard, len = out_catalog->len) { out_catalog->RemoveFrom(len); };

    {
        StreamReader st(filename);
        if (st.error)
            return false;

        JsonGhmRootDescHandler json_handler(out_catalog, str_alloc);
        if (!ParseJsonFile(st, &json_handler))
            return false;
    }

    if (out_map) {
        for (const GhmRootDesc &desc: *out_catalog) {
            out_map->Append(desc);
        }
    }

    out_guard.disable();
    return true;
}
