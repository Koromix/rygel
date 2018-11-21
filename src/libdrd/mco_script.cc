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

    foreign ==(value)
    foreign !=(value)
    foreign <(value)
    foreign <=(value)
    foreign >(value)
    foreign >=(value)

    foreign -(value)
    foreign +(value)

    foreign year
    foreign month
    foreign day

    foreign toString
}

foreign class ForeignList is Sequence {
    foreign count
    foreign [index]
    foreign iterate(it)
    foreign iteratorValue(it)
}

foreign class Filter {
    foreign name
    foreign alias
}

class MCO {
    foreign static register(name, fn)
    foreign static register(name, alias, fn)
}

foreign class McoStay {
    foreign admin_id
    foreign bill_id
    foreign sex
    foreign sex=(value)
    foreign birthdate
    foreign birthdate=(value)
    foreign entry_date
    foreign entry_date=(value)
    foreign entry_mode
    foreign entry_mode=(value)
    foreign entry_origin
    foreign entry_origin=(value)
    foreign exit_date
    foreign exit_date=(value)
    foreign exit_mode
    foreign exit_mode=(value)
    foreign exit_destination
    foreign exit_destination=(value)
    foreign unit
    foreign unit=(value)
    foreign bed_authorization
    foreign bed_authorization=(value)
    foreign session_count
    foreign session_count=(value)
    foreign igs2
    foreign igs2=(value)
    foreign last_menstrual_period
    foreign last_menstrual_period=(value)
    foreign gestational_age
    foreign gestational_age=(value)
    foreign newborn_weight
    foreign newborn_weight=(value)
    foreign dip_count
    foreign dip_count=(value)
    foreign main_diagnosis
    foreign main_diagnosis=(value)
    foreign linked_diagnosis
    foreign linked_diagnosis=(value)
    foreign confirmed
    foreign confirmed=(value)
    foreign ucd
    foreign ucd=(value)
}

foreign class McoResult {
    // mco_Result
    foreign main_stay_idx
    foreign duration
    foreign age
    foreign ghm
    foreign main_error
    foreign ghs
    foreign ghs_duration

    // mco_Pricing
    foreign ghs_coefficient
    foreign ghs_cents
    foreign price_cents
    foreign exb_exh
    foreign total_cents
}
)";

template <typename T>
struct ListObject {
    Span<WrenHandle *> vars;
    Span<const T> values;
    HeapArray<T> copies;
};

struct StayObject {
    ListObject<mco_Stay> *list;
    Size idx;
};

struct ResultObject {
    const mco_Result *result;
    const mco_Pricing *pricing;
};

struct ScriptFilter {
    const char *name;
    const char *alias;
    WrenHandle *object;
};

struct ScriptContext {
    WrenHandle *date_class;
    WrenHandle *filter_class;
    WrenHandle *filter_method;
    WrenHandle *stay_class;
    WrenHandle *stays_var;
    ListObject<mco_Stay> *stays_object;
    WrenHandle *result_var;
    ResultObject *result_object;

    HeapArray<ScriptFilter> filters;
};

static THREAD_LOCAL Allocator *thread_alloc;
static THREAD_LOCAL bool first_error;

template <typename... Args>
static void TriggerError(WrenVM *vm, Args... args)
{
    if (wrenWillAbort(vm))
        return;

    char buf[512];
    Fmt(buf, args...);

    wrenEnsureSlots(vm, 64);
    wrenSetSlotString(vm, 63, buf);
    wrenAbortFiber(vm, 63);
}

static inline bool GetSlotBoolSafe(WrenVM *vm, int slot)
{
    if (UNLIKELY(wrenGetSlotType(vm, slot) != WREN_TYPE_BOOL)) {
        TriggerError(vm, "Expected bool value");
        return false;
    }

    return wrenGetSlotBool(vm, slot);
}

template <typename T>
static inline T GetSlotIntegerSafe(WrenVM *vm, int slot)
{
    if (UNLIKELY(wrenGetSlotType(vm, slot) != WREN_TYPE_NUM)) {
        TriggerError(vm, "Expected numeric value");
        return 0;
    }

    double value = wrenGetSlotDouble(vm, slot);
    if (UNLIKELY(value < (double)std::numeric_limits<T>::min() ||
                 value > (double)std::numeric_limits<T>::max())) {
        TriggerError(vm, "Expected integer value between %1 and %2",
                     std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        return 0;
    }

    return (T)value;
}

static inline const char *GetSlotStringSafe(WrenVM *vm, int slot)
{
    if (UNLIKELY(wrenGetSlotType(vm, slot) != WREN_TYPE_STRING)) {
        TriggerError(vm, "Expected string value");
        return nullptr;
    }

    return wrenGetSlotString(vm, slot);
}

static inline Date GetSlotDateSafe(WrenVM *vm, int slot)
{
    switch (wrenGetSlotType(vm, slot)) {
        case WREN_TYPE_FOREIGN: {
            const ScriptContext &ctx = *(ScriptContext *)wrenGetUserData(vm);

            if (UNLIKELY(!wrenForeignIsClass(vm, slot, ctx.date_class))) {
                TriggerError(vm, "Expected Date or null");
                return {};
            }

            return *(Date *)wrenGetSlotForeign(vm, slot);
        } break;
        case WREN_TYPE_NULL: { return {}; } break;

        default: {
            TriggerError(vm, "Expected Date or null");
            return {};
        } break;
    }
}

static inline char GetSlotModeSafe(WrenVM *vm, int slot)
{
    switch (wrenGetSlotType(vm, slot)) {
        case WREN_TYPE_NUM: {
            double value = wrenGetSlotDouble(vm, slot);
            if (UNLIKELY(value < 0.0 || value >= 10.0)) {
                TriggerError(vm, "Mode must be between 0 and 9");
                return 0;
            }

            return '0' + (char)value;
        } break;
        case WREN_TYPE_STRING: {
            const char *value = wrenGetSlotString(vm, slot);
            if (UNLIKELY(!value[0] || value[1])) {
                TriggerError(vm, "Mode must be one character");
                return 0;
            }

            return value[0];
        } break;
        case WREN_TYPE_NULL: { return 0; } break;

        default: {
            TriggerError(vm, "Expected number or character");
            return 0;
        } break;
    }
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

    return methods;
}

#define ELSE_IF_METHOD(Signature, Func) \
    else if (TestStr(signature, (Signature))) { \
        return (Func); \
    }
#define ELSE_IF_GET_NUM(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const Type &object = *(const Type *)wrenGetSlotForeign(vm, 0); \
            wrenSetSlotDouble(vm, 0, (Value)); \
        }; \
    }
#define ELSE_IF_GET_STRING(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const Type &object = *(const Type *)wrenGetSlotForeign(vm, 0); \
            wrenSetSlotString(vm, 0, (Value)); \
        }; \
    }
#define ELSE_IF_GET_DATE(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            ScriptContext *ctx = (ScriptContext *)wrenGetUserData(vm); \
            const Type &object = *(const Type *)wrenGetSlotForeign(vm, 0); \
            wrenSetSlotHandle(vm, 0, ctx->date_class); \
             \
            Date *date = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(Date)); \
            *date = (Value); \
        }; \
    }
#define ELSE_IF_GET_MODE(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const Type &object = *(const Type *)wrenGetSlotForeign(vm, 0); \
            if ((Value) >= '0' && (Value) <= '9') { \
                wrenSetSlotDouble(vm, 0, (Value) - '0'); \
            } else { \
                char buf[2] = {(Value), 0}; \
                wrenSetSlotString(vm, 0, buf); \
            } \
        }; \
    }

static WrenForeignMethodFn BindDateMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_METHOD("init new(_,_,_)", [](WrenVM *vm) {
        Date *date = (Date *)wrenGetSlotForeign(vm, 0);

        date->st.year = GetSlotIntegerSafe<int16_t>(vm, 1);
        date->st.month = GetSlotIntegerSafe<int8_t>(vm, 2);
        date->st.day = GetSlotIntegerSafe<int8_t>(vm, 3);

        if (!date->IsValid()) {
            TriggerError(vm, "Date is not valid");
        }
    })
    ELSE_IF_METHOD("==(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 == date2);
    })
    ELSE_IF_METHOD("!=(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 != date2);
    })
    ELSE_IF_METHOD("<(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 < date2);
    })
    ELSE_IF_METHOD("<=(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 <= date2);
    })
    ELSE_IF_METHOD(">(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 > date2);
    })
    ELSE_IF_METHOD(">=(_)", [](WrenVM *vm) {
        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        Date date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 >= date2);
    })
    ELSE_IF_METHOD("-(_)", [](WrenVM *vm) {
        const ScriptContext &ctx = *(const ScriptContext *)wrenGetUserData(vm);

        Date date1 = *(Date *)wrenGetSlotForeign(vm, 0);
        if (UNLIKELY(!date1.IsValid())) {
            TriggerError(vm, "Cannot compute on invalid date");
            return;
        }

        switch (wrenGetSlotType(vm, 1)) {
            case WREN_TYPE_FOREIGN: {
                Date date2 = GetSlotDateSafe(vm, 1);
                if (UNLIKELY(!date2.IsValid())) {
                    TriggerError(vm, "Cannot compute days between invalid dates");
                    return;
                }

                wrenSetSlotDouble(vm, 0, (double)(date1 - date2));
            } break;
            case WREN_TYPE_NUM: {
                int16_t days = GetSlotIntegerSafe<int16_t>(vm, 1);

                wrenSetSlotHandle(vm, 0, ctx.date_class);
                Date *ret = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(date1));
                *ret = date1 - days;
            } break;

            default: {
                TriggerError(vm, "Operand must be Date or number");
                return;
            } break;
        }
    })
    ELSE_IF_METHOD("+(_)", [](WrenVM *vm) {
        const ScriptContext &ctx = *(const ScriptContext *)wrenGetUserData(vm);

        Date date = *(Date *)wrenGetSlotForeign(vm, 0);
        if (UNLIKELY(!date.IsValid())) {
            TriggerError(vm, "Cannot compute on invalid date");
            return;
        }

        int16_t days = GetSlotIntegerSafe<int16_t>(vm, 1);

        wrenSetSlotHandle(vm, 0, ctx.date_class);
        Date *ret = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(date));
        *ret = date + days;
    })
    ELSE_IF_GET_NUM("year", Date, object.st.year)
    ELSE_IF_GET_NUM("month", Date, object.st.month)
    ELSE_IF_GET_NUM("day", Date, object.st.day)
    ELSE_IF_METHOD("toString", [](WrenVM *vm) {
        Date date = *(Date *)wrenGetSlotForeign(vm, 0);

        char buf[64];
        Fmt(buf, "%1", date);
        wrenSetSlotString(vm, 0, buf);
    })

    return nullptr;
}

static WrenForeignMethodFn BindForeignListMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_METHOD("count", [](WrenVM *vm) {
        const ListObject<char> &object = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)object.vars.len);
    })
    ELSE_IF_METHOD("[_]", [](WrenVM *vm) {
        const ListObject<char> &object = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIntegerSafe<Size>(vm, 1);

        if (idx >= 0 && idx < object.vars.len) {
            wrenSetSlotHandle(vm, 0, object.vars[idx]);
        } else if (idx < 0 && idx >= -object.vars.len) {
            wrenSetSlotHandle(vm, 0, object.vars[object.vars.len + idx]);
        } else {
            TriggerError(vm, "Index is out-of-bound");
        }
    })
    ELSE_IF_METHOD("iterate(_)", [](WrenVM *vm) {
        const ListObject<char> &object = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);

        Size idx;
        switch (wrenGetSlotType(vm, 1)) {
            case WREN_TYPE_NULL: { idx = -1; } break;
            case WREN_TYPE_NUM: { idx = (Size)wrenGetSlotDouble(vm, 1); } break;

            default: {
                TriggerError(vm, "Iterator must be null or number");
                return;
            } break;
        }

        if (++idx < object.vars.len) {
            wrenSetSlotDouble(vm, 0, (double)idx);
        } else {
            wrenSetSlotBool(vm, 0, false);
        }
    })
    ELSE_IF_METHOD("iteratorValue(_)", [](WrenVM *vm) {
        const ListObject<char> &object = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIntegerSafe<Size>(vm, 1);

        if (LIKELY(idx >= 0 && idx < object.vars.len)) {
            wrenSetSlotHandle(vm, 0, object.vars[idx]);
        } else {
            TriggerError(vm, "Index is out-of-bound");
        }
    })

    return nullptr;
}

static WrenForeignMethodFn BindFilterMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_STRING("name", ScriptFilter *, object->name)
    ELSE_IF_GET_STRING("alias", ScriptFilter *, object->alias)

    return nullptr;
}

static void RegisterFilter(WrenVM *vm, int name_slot, int alias_slot, int fn_slot)
{
    ScriptContext *ctx = (ScriptContext *)wrenGetUserData(vm);

    ScriptFilter filter;
    filter.name = GetSlotStringSafe(vm, name_slot);
    filter.alias = GetSlotStringSafe(vm, alias_slot);
    if (wrenGetSlotType(vm, fn_slot) != WREN_TYPE_CLOSURE) {
        TriggerError(vm, "Expected function argument");
        return;
    }
    filter.object = wrenGetSlotHandle(vm, fn_slot);

    wrenSetSlotHandle(vm, 0, ctx->date_class);
    ScriptFilter **ptr = (ScriptFilter **)wrenSetSlotNewForeign(vm, 0, 0, SIZE(ScriptFilter *));
    *ptr = ctx->filters.Append(filter);
}

static WrenForeignMethodFn BindMcoMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_METHOD("register(_,_)", [](WrenVM *vm) { RegisterFilter(vm, 1, 1, 2); })
    ELSE_IF_METHOD("register(_,_,_)", [](WrenVM *vm) { RegisterFilter(vm, 1, 2, 3); })

    return nullptr;
}

static inline mco_Stay *GetMutableStay(StayObject *object)
{
    ListObject<mco_Stay> *list = object->list;

    if (!list->copies.len) {
        list->copies.Append(list->values);
        list->values = list->copies;
    }

    return &list->copies[object->idx];
}

static WrenForeignMethodFn BindMcoStayMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_NUM("admin_id", StayObject, object.list->values[object.idx].admin_id)
    ELSE_IF_GET_NUM("bill_id", StayObject, object.list->values[object.idx].bill_id)
    ELSE_IF_GET_NUM("sex", StayObject, object.list->values[object.idx].sex)
    ELSE_IF_METHOD("sex=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (list->values[object->idx].sex != new_value) {
            GetMutableStay(object)->sex = new_value;
        }
    })
    ELSE_IF_GET_DATE("birthdate", StayObject, object.list->values[object.idx].birthdate)
    ELSE_IF_METHOD("birthdate=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[object->idx].birthdate != new_date) {
            GetMutableStay(object)->birthdate = new_date;
        }
    })
    ELSE_IF_GET_DATE("entry_date", StayObject, object.list->values[object.idx].entry.date)
    ELSE_IF_METHOD("entry_date=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[object->idx].entry.date != new_date) {
            GetMutableStay(object)->entry.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("entry_mode", StayObject, object.list->values[object.idx].entry.mode)
    ELSE_IF_METHOD("entry_mode=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[object->idx].entry.mode != new_value) {
            GetMutableStay(object)->entry.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("entry_origin", StayObject, object.list->values[object.idx].entry.origin)
    ELSE_IF_METHOD("entry_origin=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[object->idx].entry.origin != new_value) {
            GetMutableStay(object)->entry.origin = new_value;
        }
    })
    ELSE_IF_GET_DATE("exit_date", StayObject, object.list->values[object.idx].exit.date)
    ELSE_IF_METHOD("exit_date=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[object->idx].exit.date != new_date) {
            GetMutableStay(object)->exit.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("exit_mode", StayObject, object.list->values[object.idx].exit.mode)
    ELSE_IF_METHOD("exit_mode=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[object->idx].exit.mode != new_value) {
            GetMutableStay(object)->exit.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("exit_destination", StayObject, object.list->values[object.idx].exit.destination)
    ELSE_IF_METHOD("exit_destination=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[object->idx].exit.destination != new_value) {
            GetMutableStay(object)->exit.destination = new_value;
        }
    })
    ELSE_IF_GET_NUM("unit", StayObject, object.list->values[object.idx].unit.number)
    ELSE_IF_METHOD("unit=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].unit.number != new_value) {
            GetMutableStay(object)->unit = UnitCode(new_value);
        }
    })
    ELSE_IF_GET_NUM("bed_authorization", StayObject, object.list->values[object.idx].bed_authorization)
    ELSE_IF_METHOD("bed_authorization=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (list->values[object->idx].bed_authorization != new_value) {
            GetMutableStay(object)->bed_authorization = new_value;
        }
    })
    ELSE_IF_GET_NUM("session_count", StayObject, object.list->values[object.idx].session_count)
    ELSE_IF_METHOD("session_count=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].session_count != new_value) {
            GetMutableStay(object)->session_count = new_value;
        }
    })
    ELSE_IF_GET_NUM("igs2", StayObject, object.list->values[object.idx].igs2)
    ELSE_IF_METHOD("igs2=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].igs2 != new_value) {
            GetMutableStay(object)->igs2 = new_value;
        }
    })
    ELSE_IF_GET_DATE("last_menstrual_period", StayObject, object.list->values[object.idx].last_menstrual_period)
    ELSE_IF_METHOD("last_menstrual_period=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[object->idx].last_menstrual_period != new_date) {
            GetMutableStay(object)->last_menstrual_period = new_date;
        }
    })
    ELSE_IF_GET_NUM("gestational_age", StayObject, object.list->values[object.idx].gestational_age)
    ELSE_IF_METHOD("gestational_age=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].gestational_age != new_value) {
            GetMutableStay(object)->gestational_age = new_value;
        }
    })
    ELSE_IF_GET_NUM("newborn_weight", StayObject, object.list->values[object.idx].newborn_weight)
    ELSE_IF_METHOD("newborn_weight=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].newborn_weight != new_value) {
            GetMutableStay(object)->newborn_weight = new_value;
        }
    })
    ELSE_IF_GET_NUM("dip_count", StayObject, object.list->values[object.idx].dip_count)
    ELSE_IF_METHOD("dip_count=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[object->idx].dip_count != new_value) {
            GetMutableStay(object)->dip_count = new_value;
        }
    })
    ELSE_IF_GET_STRING("main_diagnosis", StayObject, object.list->values[object.idx].main_diagnosis.str)
    ELSE_IF_METHOD("main_diagnosis=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        const char *new_value = GetSlotStringSafe(vm, 1);
        DiagnosisCode new_diag = DiagnosisCode::FromString(new_value, (int)ParseFlag::End);
        if (UNLIKELY(!new_diag.IsValid())) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (list->values[object->idx].main_diagnosis != new_diag) {
            GetMutableStay(object)->main_diagnosis = new_diag;
        }
    })
    ELSE_IF_GET_STRING("linked_diagnosis", StayObject, object.list->values[object.idx].linked_diagnosis.str)
    ELSE_IF_METHOD("linked_diagnosis=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        const char *new_value = GetSlotStringSafe(vm, 1);
        DiagnosisCode new_diag = DiagnosisCode::FromString(new_value, (int)ParseFlag::End);
        if (UNLIKELY(!new_diag.IsValid())) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (list->values[object->idx].linked_diagnosis != new_diag) {
            GetMutableStay(object)->linked_diagnosis = new_diag;
        }
    })
    ELSE_IF_METHOD("confirmed", [](WrenVM *vm) {
        const StayObject &object = *(StayObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotBool(vm, 0, object.list->values[object.idx].flags & (int)mco_Stay::Flag::Confirmed);
    })
    ELSE_IF_METHOD("confirmed=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        bool new_value = GetSlotBoolSafe(vm, 1);
        uint32_t new_flags = ApplyMask(list->values[object->idx].flags,
                                       (int)mco_Stay::Flag::Confirmed, new_value);

        if (new_flags != list->values[object->idx].flags) {
            GetMutableStay(object)->flags = new_flags;
        }
    })
    ELSE_IF_METHOD("ucd", [](WrenVM *vm) {
        const StayObject &object = *(StayObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotBool(vm, 0, object.list->values[object.idx].flags & (int)mco_Stay::Flag::Ucd);
    })
    ELSE_IF_METHOD("ucd=(_)", [](WrenVM *vm) {
        StayObject *object = (StayObject *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = object->list;

        bool new_value = GetSlotBoolSafe(vm, 1);
        uint32_t new_flags = ApplyMask(list->values[object->idx].flags,
                                       (int)mco_Stay::Flag::Ucd, new_value);

        if (new_flags != list->values[object->idx].flags) {
            GetMutableStay(object)->flags = new_flags;
        }
    })

    return nullptr;
}

static WrenForeignMethodFn BindMcoResultMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_NUM("main_stay_idx", ResultObject, object.result->main_stay_idx)
    ELSE_IF_GET_NUM("duration", ResultObject, object.result->duration)
    ELSE_IF_GET_NUM("age", ResultObject, object.result->age)
    ELSE_IF_METHOD("ghm", [](WrenVM *vm) {
        const ResultObject &object = *(const ResultObject *)wrenGetSlotForeign(vm, 0);
        char buf[32];
        wrenSetSlotString(vm, 0, object.result->ghm.ToString(buf).ptr);
    })
    ELSE_IF_GET_NUM("main_error", ResultObject, object.result->main_error)
    ELSE_IF_GET_NUM("ghs", ResultObject, object.result->ghs.number)
    ELSE_IF_GET_NUM("ghs_duration", ResultObject, object.result->ghs_duration)
    ELSE_IF_GET_NUM("ghs_coefficient", ResultObject, object.pricing->ghs_coefficient)
    ELSE_IF_GET_NUM("ghs_cents", ResultObject, object.pricing->ghs_cents)
    ELSE_IF_GET_NUM("price_cents", ResultObject, object.pricing->price_cents)
    ELSE_IF_GET_NUM("exb_exh", ResultObject, object.pricing->exb_exh)
    ELSE_IF_GET_NUM("total_cents", ResultObject, object.pricing->total_cents)

    return nullptr;
}

#undef ELSE_IF_GET_MODE
#undef ELSE_IF_GET_DATE
#undef ELSE_IF_GET_STRING
#undef ELSE_IF_GET_NUM
#undef ELSE_IF_METHOD

static WrenForeignMethodFn BindForeignMethod(WrenVM *, const char *,
                                             const char *class_name, bool is_static,
                                             const char *signature)
{
    if (!is_static && TestStr(class_name, "Date")) {
        return BindDateMethod(signature);
    } else if (!is_static && TestStr(class_name, "ForeignList")) {
        return BindForeignListMethod(signature);
    } else if (!is_static && TestStr(class_name, "Filter")) {
        return BindFilterMethod(signature);
    } else if (is_static && TestStr(class_name, "MCO")) {
        return BindMcoMethod(signature);
    } else if (!is_static && TestStr(class_name, "McoStay")) {
        return BindMcoStayMethod(signature);
    } else if (!is_static && TestStr(class_name, "McoResult")) {
        return BindMcoResultMethod(signature);
    } else {
        return nullptr;
    }
}

bool mco_RunScript(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                   const char *script, Span<mco_Result> results, Span<mco_Pricing> pricings,
                   mco_StaySet *out_stay_set)
{
    // FIXME: Make sure all deallocations are disabled
    BlockAllocator temp_alloc(Megabytes(1));
    thread_alloc = &temp_alloc;

    // Init Wren VM
    WrenVM* vm;
    {
        WrenConfiguration config;
        wrenInitConfiguration(&config);

        // Use fast bump allocator and avoid GC as much as possible for
        // maximum performance.
        config.reallocateFn = [](void *mem, size_t old_size, size_t new_size) {
            Assert(old_size <= LEN_MAX && new_size <= LEN_MAX);
            Allocator::Resize(thread_alloc, &mem, (Size)old_size, (Size)new_size);
            return mem;
        };

        config.writeFn = [](WrenVM *, const char *text) {
            fputs(text, stdout);
        };

        // Default issues stack-trace like errors, hack around it to show (when possible)
        // a single error message to the user.
        first_error = true;
        config.errorFn = [](WrenVM *, WrenErrorType, const char *, int, const char *msg) {
            if (first_error) {
                LogError("%1", msg);
                first_error = false;
            }
        };

        config.bindForeignClassFn = BindForeignClass;
        config.bindForeignMethodFn = BindForeignMethod;

        // Limit execution time and space, and (basically) disable GC
        config.maxRunOps = 20000;
        config.maxHeapSize = Mebibytes(8);
        config.initialHeapSize = 0;

        // We don't need to free this because all allocations go through the
        // WrenMemory allocator above.
        vm = wrenNewVM(&config);
    }

    ScriptContext ctx = {};
    wrenSetUserData(vm, &ctx);

    Assert(wrenInterpret(vm, "mco", InitCode) == WREN_RESULT_SUCCESS);

    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, "mco", "Date", 0);
    ctx.date_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "Filter", 0);
    ctx.filter_class = wrenGetSlotHandle(vm, 0);
    ctx.filter_method = wrenMakeCallHandle(vm, "call(_,_)");
    wrenGetVariable(vm, "mco", "McoStay", 0);
    ctx.stay_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "McoResult", 0);
    wrenSetSlotNewForeign(vm, 0, 0, SIZE(ResultObject));
    ctx.result_var = wrenGetSlotHandle(vm, 0);
    ctx.result_object = (ResultObject *)wrenGetSlotForeign(vm, 0);
    wrenGetVariable(vm, "mco", "ForeignList", 0);
    wrenSetSlotNewForeign(vm, 0, 0, SIZE(ListObject<char>));
    ctx.stays_var = wrenGetSlotHandle(vm, 0);
    ctx.stays_object = (ListObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);

    if (wrenInterpret(vm, "script", script) != WREN_RESULT_SUCCESS)
        return false;

    {
        // Reuse for performance
        HeapArray<WrenHandle *> stay_vars;

        for (Size i = 0; i < results.len; i++) {
            mco_Result &result = results[i];
            mco_Pricing &pricing = pricings[i];

            while (stay_vars.len < result.stays.len) {
                wrenEnsureSlots(vm, 1);
                wrenSetSlotHandle(vm, 0, ctx.stay_class);
                wrenSetSlotNewForeign(vm, 0, 0, SIZE(StayObject));

                WrenHandle *stay_var = wrenGetSlotHandle(vm, 0);
                StayObject *stay_object = (StayObject *)wrenGetSlotForeign(vm, 0);
                stay_object->list = ctx.stays_object;
                stay_object->idx = stay_vars.len;

                stay_vars.Append(stay_var);
            }

            ctx.stays_object->vars = stay_vars.Take(0, result.stays.len);
            ctx.stays_object->values = result.stays;
            ctx.stays_object->copies.RemoveFrom(0);
            ctx.result_object->result = &result;
            ctx.result_object->pricing = &pricing;

            for (const ScriptFilter &filter: ctx.filters) {
                wrenEnsureSlots(vm, 3);
                wrenSetSlotHandle(vm, 0, filter.object);
                wrenSetSlotHandle(vm, 1, ctx.stays_var);
                wrenSetSlotHandle(vm, 2, ctx.result_var);
                if (wrenCall(vm, ctx.filter_method) != WREN_RESULT_SUCCESS)
                    return false;
            }

            if (ctx.stays_object->copies.len) {
                Span<const mco_Stay> prev_stays = result.stays;

                mco_RunClassifier(table_set, authorization_set, ctx.stays_object->copies, 0,
                                  &result);
                pricing = {};
                mco_Price(result, false, &pricing);

                if (!out_stay_set) {
                    result.stays = prev_stays;
                    pricing.stays = prev_stays;
                }
            }
        }
    }

    return true;
}
