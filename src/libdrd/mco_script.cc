// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_script.hh"

#include "../../lib/wren/src/include/wren.hpp"

static const char *const InitCode = R"(
foreign class Date {
    construct new() {}
    foreign construct new(year, month, day)

    foreign year
    foreign month
    foreign day

    foreign toString
}

foreign class Stay {
    construct new() {}

    // Stay
    foreign admin_id
    foreign bill_id
    foreign sex
    foreign birthdate
    foreign entry_date
    foreign entry_mode
    foreign entry_origin
    foreign exit_date
    foreign exit_mode
    foreign exit_destination
    foreign unit
    foreign bed_authorization
    foreign session_count
    foreign igs2
    foreign last_menstrual_period
    foreign gestational_age
    foreign newborn_weight
    foreign dip_count
    foreign main_diagnosis
    foreign linked_diagnosis
    foreign confirmed

    // Result
    foreign main_stay
    foreign duration
    foreign ghm
    foreign main_error
    foreign ghs
    foreign ghs_duration

    // Pricing
    foreign ghs_coefficient
    foreign ghs_cents
    foreign price_cents
    foreign exb_exh
    foreign total_cents
}

var rss = Stay.new()
)";

struct FilterContext {
    WrenHandle *date_class;
};

struct StayObject {
    const mco_Stay *stay;
    const mco_Result *result;
    const mco_Pricing *pricing;

    mco_Stay stay_copy;
};

// FIXME: Check parameter and result validity
static void DateNew(WrenVM *vm)
{
    Date *date = (Date *)wrenGetSlotForeign(vm, 0);

    date->st.year = (int16_t)wrenGetSlotDouble(vm, 1);
    date->st.month = (int8_t)wrenGetSlotDouble(vm, 2);
    date->st.day = (int8_t)wrenGetSlotDouble(vm, 3);
}

static void DateToString(WrenVM *vm)
{
    Date *date = (Date *)wrenGetSlotForeign(vm, 0);

    char buf[64];
    Fmt(buf, "%1", *date);
    wrenSetSlotString(vm, 0, buf);
}

static WrenForeignClassMethods BindForeignClass(WrenVM *, const char *, const char *class_name)
{
    WrenForeignClassMethods methods = {};

    if (TestStr(class_name, "Date")) {
        methods.allocate = [](WrenVM *vm) {
            Date *date = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(Date));
            *date = {};
        };
    }

    if (TestStr(class_name, "Stay")) {
        methods.allocate = [](WrenVM *vm) {
            // FIXME: Put somethind in result and pricing or disable creation of Stay (somehow)
            StayObject *obj = (StayObject *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(StayObject));
            obj->stay_copy = {};
            obj->stay = &obj->stay_copy;
        };
    }

    return methods;
}

static WrenForeignMethodFn BindForeignMethod(WrenVM *, const char *,
                                             const char *class_name, bool is_static,
                                             const char *signature)
{
    if (is_static)
        return nullptr;

#define ELSE_IF_METHOD(Signature, Func) \
        else if (TestStr(signature, (Signature))) { \
            return (Func); \
        }
#define ELSE_IF_PROPERTY_NUM(Signature, Type, Member) \
        else if (TestStr(signature, (Signature))) { \
            return [](WrenVM *vm) { \
                const Type *obj = (Type *)wrenGetSlotForeign(vm, 0); \
                wrenSetSlotDouble(vm, 0, obj->Member); \
            }; \
        }
#define ELSE_IF_PROPERTY_STRING(Signature, Type, Member) \
        else if (TestStr(signature, (Signature))) { \
            return [](WrenVM *vm) { \
                const Type *obj = (Type *)wrenGetSlotForeign(vm, 0); \
                wrenSetSlotString(vm, 0, obj->Member); \
            }; \
        }
#define ELSE_IF_PROPERTY_DATE(Signature, Type, Member) \
        else if (TestStr(signature, (Signature))) { \
            return [](WrenVM *vm) { \
                FilterContext *ctx = (FilterContext *)wrenGetUserData(vm); \
                const Type *obj = (Type *)wrenGetSlotForeign(vm, 0); \
                wrenSetSlotHandle(vm, 0, ctx->date_class); \
                Date *date = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(Date)); \
                *date = obj->Member; \
            }; \
        }
#define ELSE_IF_PROPERTY_MODE(Signature, Type, Member) \
        else if (TestStr(signature, (Signature))) { \
            return [](WrenVM *vm) { \
                const Type *obj = (Type *)wrenGetSlotForeign(vm, 0); \
                if (obj->Member >= '0' && obj->Member <= '9') { \
                    wrenSetSlotDouble(vm, 0, obj->Member - '0'); \
                } else { \
                    char buf[2] = {obj->Member, 0}; \
                    wrenSetSlotString(vm, 0, buf); \
                } \
            }; \
        }

    if (TestStr(class_name, "Date")) {
        if (false) {}
        ELSE_IF_METHOD("init new(_,_,_)", DateNew)
        ELSE_IF_PROPERTY_NUM("year", Date, st.year)
        ELSE_IF_PROPERTY_NUM("month", Date, st.month)
        ELSE_IF_PROPERTY_NUM("day", Date, st.day)
        ELSE_IF_METHOD("toString", DateToString)
    }

    if (TestStr(class_name, "Stay")) {
        if (false) {}
        ELSE_IF_PROPERTY_NUM("admin_id", StayObject, stay->admin_id)
        ELSE_IF_PROPERTY_NUM("bill_id", StayObject, stay->bill_id)
        ELSE_IF_PROPERTY_NUM("sex", StayObject, stay->sex)
        ELSE_IF_PROPERTY_DATE("birthdate", StayObject, stay->birthdate)
        ELSE_IF_PROPERTY_DATE("entry_date", StayObject, stay->entry.date)
        ELSE_IF_PROPERTY_MODE("entry_mode", StayObject, stay->entry.mode)
        ELSE_IF_PROPERTY_MODE("entry_origin", StayObject, stay->entry.origin)
        ELSE_IF_PROPERTY_DATE("exit_date", StayObject, stay->exit.date)
        ELSE_IF_PROPERTY_MODE("exit_mode", StayObject, stay->exit.mode)
        ELSE_IF_PROPERTY_MODE("exit_destination", StayObject, stay->exit.destination)
        ELSE_IF_PROPERTY_NUM("unit", StayObject, stay->unit.number)
        ELSE_IF_PROPERTY_NUM("bed_authorization", StayObject, stay->bed_authorization)
        ELSE_IF_PROPERTY_NUM("session_count", StayObject, stay->session_count)
        ELSE_IF_PROPERTY_NUM("igs2", StayObject, stay->igs2)
        ELSE_IF_PROPERTY_DATE("last_menstrual_period", StayObject, stay->last_menstrual_period)
        ELSE_IF_PROPERTY_NUM("gestational_age", StayObject, stay->gestational_age)
        ELSE_IF_PROPERTY_NUM("newborn_weight", StayObject, stay->newborn_weight)
        ELSE_IF_PROPERTY_NUM("dip_count", StayObject, stay->dip_count)
        ELSE_IF_PROPERTY_STRING("main_diagnosis", StayObject, stay->main_diagnosis.str)
        ELSE_IF_PROPERTY_STRING("linked_diagnosis", StayObject, stay->linked_diagnosis.str)
        ELSE_IF_METHOD("confirmed", [](WrenVM *vm) {
            const StayObject *obj = (StayObject *)wrenGetSlotForeign(vm, 0);
            wrenSetSlotBool(vm, 0, obj->stay->flags & (int)mco_Stay::Flag::Confirmed);
        })

        ELSE_IF_PROPERTY_NUM("main_stay", StayObject, result->main_stay_idx)
        ELSE_IF_PROPERTY_NUM("duration", StayObject, result->duration)
        ELSE_IF_METHOD("ghm", [](WrenVM *vm) {
            const StayObject *obj = (StayObject *)wrenGetSlotForeign(vm, 0);
            char buf[32];
            wrenSetSlotString(vm, 0, obj->result->ghm.ToString(buf).ptr);
        })
        ELSE_IF_PROPERTY_NUM("main_error", StayObject, result->main_error)
        ELSE_IF_PROPERTY_NUM("ghs", StayObject, result->ghs.number)
        ELSE_IF_PROPERTY_NUM("ghs_duration", StayObject, result->ghs_duration)

        ELSE_IF_PROPERTY_NUM("ghs_coefficient", StayObject, pricing->ghs_coefficient)
        ELSE_IF_PROPERTY_NUM("ghs_cents", StayObject, pricing->ghs_cents)
        ELSE_IF_PROPERTY_NUM("price_cents", StayObject, pricing->price_cents)
        ELSE_IF_PROPERTY_NUM("exb_exh", StayObject, pricing->exb_exh)
        ELSE_IF_PROPERTY_NUM("total_cents", StayObject, pricing->total_cents)
    }

#undef ELSE_IF_PROPERTY_MODE
#undef ELSE_IF_PROPERTY_DATE
#undef ELSE_IF_PROPERTY_STRING
#undef ELSE_IF_PROPERTY_NUM
#undef ELSE_IF_METHOD

    return nullptr;
}

bool mco_RunScript(const char *script,
                   Span<const mco_Result> results, Span<const mco_Result> mono_results,
                   Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings)
{
    // Init Wren VM
    WrenVM* vm;
    {
        WrenConfiguration config;
        wrenInitConfiguration(&config);

        config.writeFn = [](WrenVM *, const char *text) {
            fputs(text, stdout);
        };
        config.errorFn = [](WrenVM *, WrenErrorType,
                            const char *module, int line, const char* message) {
            LogError("%1(%2): %3", module, line, message);
        };
        config.bindForeignClassFn = BindForeignClass;
        config.bindForeignMethodFn = BindForeignMethod;

        // We don't need to free this because all allocations go through the
        // WrenMemory allocator above.
        vm = wrenNewVM(&config);
    }
    DEFER { wrenFreeVM(vm); };

    if (wrenInterpret(vm, __FILE__, InitCode) != WREN_RESULT_SUCCESS)
        return false;
    if (wrenInterpret(vm, "script", script) != WREN_RESULT_SUCCESS)
        return false;

    WrenHandle *rss_var;
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, __FILE__, "rss", 0);
    rss_var = wrenGetSlotHandle(vm, 0);
    DEFER { wrenReleaseHandle(vm, rss_var); };

    WrenHandle *filter_class;
    WrenHandle *new_method;
    WrenHandle *rss_method;
    WrenHandle *finish_method;
    {
        wrenEnsureSlots(vm, 1);
        wrenGetVariable(vm, "script", "FilterMco", 0);
        filter_class = wrenGetSlotHandle(vm, 0);

        new_method = wrenMakeCallHandle(vm, "new()");
        rss_method = wrenMakeCallHandle(vm, "rss(_)");
        finish_method = wrenMakeCallHandle(vm, "finish()");
    }
    DEFER { wrenReleaseHandle(vm, filter_class); };
    DEFER { wrenReleaseHandle(vm, new_method); };
    DEFER { wrenReleaseHandle(vm, rss_method); };
    DEFER { wrenReleaseHandle(vm, finish_method); };

    WrenHandle *filter_obj;
    wrenEnsureSlots(vm, 1);
    wrenSetSlotHandle(vm, 0, filter_class);
    wrenCall(vm, new_method);
    filter_obj = wrenGetSlotHandle(vm, 0);
    DEFER { wrenReleaseHandle(vm, filter_obj); };

    FilterContext ctx;
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, __FILE__, "Date", 0);
    ctx.date_class = wrenGetSlotHandle(vm, 0);
    DEFER { wrenReleaseHandle(vm, ctx.date_class); };
    wrenSetUserData(vm, &ctx);

    for (Size i = 0; i < results.len; i++) {
        const mco_Result &result = results[i];
        const mco_Pricing &pricing = pricings[i];

        wrenEnsureSlots(vm, 2);
        wrenSetSlotHandle(vm, 0, filter_obj);
        wrenSetSlotHandle(vm, 1, rss_var);

        StayObject *obj = (StayObject *)wrenGetSlotForeign(vm, 1);
        obj->stay = &result.stays[0]; // FIXME: Completely wrong
        obj->result = &result;
        obj->pricing = &pricing;

        wrenCall(vm, rss_method);
    }

    wrenEnsureSlots(vm, 1);
    wrenSetSlotHandle(vm, 0, filter_obj);
    wrenCall(vm, finish_method);

    return true;
}
