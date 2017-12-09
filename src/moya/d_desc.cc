#include "kutil.hh"
#include "d_desc.hh"

class JsonGhmRootDescHandler: public BaseJsonHandler<JsonGhmRootDescHandler> {
    enum class State {
        Default,
        DescArray,
        DescObject
    };

    State state = State::Default;

    GhmRootDesc desc = {};

public:
    HeapArray<GhmRootDesc> *out_catalog;
    Allocator *out_alloc;

    JsonGhmRootDescHandler(HeapArray<GhmRootDesc> *out_catalog = nullptr,
                           Allocator *out_alloc = nullptr)
        : out_catalog(out_catalog), out_alloc(out_alloc) {}

    bool Branch(JsonBranchType type, const char *)
    {
        switch (state) {
            case State::Default: {
                switch (type) {
                    case JsonBranchType::Array: { state = State::DescArray; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::DescArray: {
                switch (type) {
                    case JsonBranchType::Object: { state = State::DescObject; } break;
                    case JsonBranchType::EndArray: { state = State::Default; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::DescObject: {
                switch (type) {
                    case JsonBranchType::EndObject: {
                        out_catalog->Append(desc);
                        desc = {};

                        state = State::DescArray;
                    } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;
        }

        return true;
    }

    bool Value(const char *key, const JsonValue &value)
    {
        DebugAssert(out_alloc);

        if (state == State::DescObject) {
            if (TestStr(key, "root")) {
                if (value.type == JsonValue::Type::String) {
                    desc.ghm_root = GhmRootCode::FromString(value.u.str.ptr);
                } else {
                    UnexpectedType(value.type);
                }
            } else if (TestStr(key, "root_desc")) {
                SetString(value, out_alloc, &desc.ghm_root_desc);
            } else if (TestStr(key, "da")) {
                if (value.type == JsonValue::Type::String && value.u.str.len == SIZE(desc.da) - 1) {
                    strcpy(desc.da, value.u.str.ptr);
                } else {
                    LogError("Malformed DA code (must be %1 characters)", SIZE(desc.da) - 1);
                }
            } else if (TestStr(key, "da_desc")) {
                SetString(value, out_alloc, &desc.da_desc);
            } else if (TestStr(key, "ga")) {
                if (value.type == JsonValue::Type::String && value.u.str.len == SIZE(desc.ga) - 1) {
                    strcpy(desc.ga, value.u.str.ptr);
                } else {
                    LogError("Malformed GA code (must be %1 characters)", SIZE(desc.ga) - 1);
                }
            } else if (TestStr(key, "ga_desc")) {
                SetString(value, out_alloc, &desc.ga_desc);
            } else {
                return UnknownAttribute(key);
            }
        } else {
            return UnexpectedValue();
        }

        state = State::DescObject;
        return true;
    }
};

bool LoadGhmRootCatalog(const char *filename, Allocator *str_alloc,
                        HeapArray<GhmRootDesc> *out_catalog,
                        HashSet<GhmRootCode, GhmRootDesc> *out_map)
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
