// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/util.hh"
#include "mco_filter.hh"
#include "mco_pricing.hh"

#include "../../lib/wren/src/include/wren.hpp"

static const char *const InitCode = R"(
import "meta" for Meta

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

    foreign other_diagnoses
    foreign procedures
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

class MCO {
    foreign static stays
    foreign static result

    static filter(fn) { fn.call() }
    static build(exp) { Meta.compileExpression(exp) }
}
)";

// Variables exposed to Meta.compileExpression
static const char *VarCode = R"(
var stays = MCO.stays
var result = MCO.result
)";

template <typename T>
struct ListObject {
    WrenHandle *var;
    Span<WrenHandle *> vars;
    Span<const T> values;
    HeapArray<T> copies;
};

template <typename T>
struct ListProxy {
    ListObject<T> *list;
    Size idx;
};

struct ResultObject {
    WrenHandle *var;
    const mco_Result *result;
    mco_Pricing pricing;
};

class mco_WrenRunner {
    // FIXME: Make sure all deallocations are disabled
    BlockAllocator vm_alloc { Kibibytes(256) };

public:
    WrenVM *vm;

    WrenHandle *date_class;
    WrenHandle *stay_class;
    ListObject<mco_Stay> *stays_obj;
    ResultObject *result_obj;
    WrenHandle *mco_class;
    WrenHandle *mco_build;
    HeapArray<WrenHandle *> other_diagnoses_vars;
    HeapArray<WrenHandle *> procedures_vars;

    WrenHandle *expression_var = nullptr;
    WrenHandle *expression_call;

    bool Init(const char *filter, Size max_results);

    Size Process(Span<const mco_Result> results, const mco_Result mono_results[],
                 HeapArray<const mco_Result *> *out_results,
                 HeapArray<const mco_Result *> *out_mono_results,
                 mco_StaySet *out_stay_set);
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
            const mco_WrenRunner &runner = *(mco_WrenRunner *)wrenGetUserData(vm);

            if (UNLIKELY(!wrenForeignIsClass(vm, slot, runner.date_class))) {
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
            const Type &obj = *(const Type *)wrenGetSlotForeign(vm, 0); \
            wrenSetSlotDouble(vm, 0, (Value)); \
        }; \
    }
#define ELSE_IF_GET_STRING(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const Type &obj = *(const Type *)wrenGetSlotForeign(vm, 0); \
            wrenSetSlotString(vm, 0, (Value)); \
        }; \
    }
#define ELSE_IF_GET_DATE(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm); \
            const Type &obj = *(const Type *)wrenGetSlotForeign(vm, 0); \
             \
            wrenSetSlotHandle(vm, 0, runner.date_class); \
            Date *date = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(Date)); \
            *date = (Value); \
        }; \
    }
#define ELSE_IF_GET_MODE(Signature, Type, Value) \
    else if (TestStr(signature, (Signature))) { \
        return [](WrenVM *vm) { \
            const Type &obj = *(const Type *)wrenGetSlotForeign(vm, 0); \
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
        const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm);

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

                wrenSetSlotHandle(vm, 0, runner.date_class);
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
        const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm);

        Date date = *(Date *)wrenGetSlotForeign(vm, 0);
        if (UNLIKELY(!date.IsValid())) {
            TriggerError(vm, "Cannot compute on invalid date");
            return;
        }

        int16_t days = GetSlotIntegerSafe<int16_t>(vm, 1);

        wrenSetSlotHandle(vm, 0, runner.date_class);
        Date *ret = (Date *)wrenSetSlotNewForeign(vm, 0, 0, SIZE(date));
        *ret = date + days;
    })
    ELSE_IF_GET_NUM("year", Date, obj.st.year)
    ELSE_IF_GET_NUM("month", Date, obj.st.month)
    ELSE_IF_GET_NUM("day", Date, obj.st.day)
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
        const ListObject<char> &obj = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)obj.vars.len);
    })
    ELSE_IF_METHOD("[_]", [](WrenVM *vm) {
        const ListObject<char> &obj = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIntegerSafe<Size>(vm, 1);

        if (idx >= 0 && idx < obj.vars.len) {
            wrenSetSlotHandle(vm, 0, obj.vars[idx]);
        } else if (idx < 0 && idx >= -obj.vars.len) {
            wrenSetSlotHandle(vm, 0, obj.vars[obj.vars.len + idx]);
        } else {
            TriggerError(vm, "Index is out-of-bound");
        }
    })
    ELSE_IF_METHOD("iterate(_)", [](WrenVM *vm) {
        const ListObject<char> &obj = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);

        Size idx;
        switch (wrenGetSlotType(vm, 1)) {
            case WREN_TYPE_NULL: { idx = -1; } break;
            case WREN_TYPE_NUM: { idx = (Size)wrenGetSlotDouble(vm, 1); } break;

            default: {
                TriggerError(vm, "Iterator must be null or number");
                return;
            } break;
        }

        if (++idx < obj.vars.len) {
            wrenSetSlotDouble(vm, 0, (double)idx);
        } else {
            wrenSetSlotBool(vm, 0, false);
        }
    })
    ELSE_IF_METHOD("iteratorValue(_)", [](WrenVM *vm) {
        const ListObject<char> &obj = *(const ListObject<char> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIntegerSafe<Size>(vm, 1);

        if (LIKELY(idx >= 0 && idx < obj.vars.len)) {
            wrenSetSlotHandle(vm, 0, obj.vars[idx]);
        } else {
            TriggerError(vm, "Index is out-of-bound");
        }
    })

    return nullptr;
}

static inline mco_Stay *GetMutableStay(ListProxy<mco_Stay> *obj)
{
    ListObject<mco_Stay> *list = obj->list;

    if (!list->copies.len) {
        list->copies.Append(list->values);
        list->values = list->copies;
    }

    return &list->copies[obj->idx];
}

static WrenForeignMethodFn BindMcoStayMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_NUM("admin_id", ListProxy<mco_Stay>, obj.list->values[obj.idx].admin_id)
    ELSE_IF_GET_NUM("bill_id", ListProxy<mco_Stay>, obj.list->values[obj.idx].bill_id)
    ELSE_IF_GET_NUM("sex", ListProxy<mco_Stay>, obj.list->values[obj.idx].sex)
    ELSE_IF_METHOD("sex=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (list->values[obj->idx].sex != new_value) {
            GetMutableStay(obj)->sex = new_value;
        }
    })
    ELSE_IF_GET_DATE("birthdate", ListProxy<mco_Stay>, obj.list->values[obj.idx].birthdate)
    ELSE_IF_METHOD("birthdate=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[obj->idx].birthdate != new_date) {
            GetMutableStay(obj)->birthdate = new_date;
        }
    })
    ELSE_IF_GET_DATE("entry_date", ListProxy<mco_Stay>, obj.list->values[obj.idx].entry.date)
    ELSE_IF_METHOD("entry_date=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[obj->idx].entry.date != new_date) {
            GetMutableStay(obj)->entry.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("entry_mode", ListProxy<mco_Stay>, obj.list->values[obj.idx].entry.mode)
    ELSE_IF_METHOD("entry_mode=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[obj->idx].entry.mode != new_value) {
            GetMutableStay(obj)->entry.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("entry_origin", ListProxy<mco_Stay>, obj.list->values[obj.idx].entry.origin)
    ELSE_IF_METHOD("entry_origin=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[obj->idx].entry.origin != new_value) {
            GetMutableStay(obj)->entry.origin = new_value;
        }
    })
    ELSE_IF_GET_DATE("exit_date", ListProxy<mco_Stay>, obj.list->values[obj.idx].exit.date)
    ELSE_IF_METHOD("exit_date=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[obj->idx].exit.date != new_date) {
            GetMutableStay(obj)->exit.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("exit_mode", ListProxy<mco_Stay>, obj.list->values[obj.idx].exit.mode)
    ELSE_IF_METHOD("exit_mode=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[obj->idx].exit.mode != new_value) {
            GetMutableStay(obj)->exit.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("exit_destination", ListProxy<mco_Stay>, obj.list->values[obj.idx].exit.destination)
    ELSE_IF_METHOD("exit_destination=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        char new_value = GetSlotModeSafe(vm, 1);

        if (list->values[obj->idx].exit.destination != new_value) {
            GetMutableStay(obj)->exit.destination = new_value;
        }
    })
    ELSE_IF_GET_NUM("unit", ListProxy<mco_Stay>, obj.list->values[obj.idx].unit.number)
    ELSE_IF_METHOD("unit=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].unit.number != new_value) {
            GetMutableStay(obj)->unit = UnitCode(new_value);
        }
    })
    ELSE_IF_GET_NUM("bed_authorization", ListProxy<mco_Stay>, obj.list->values[obj.idx].bed_authorization)
    ELSE_IF_METHOD("bed_authorization=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (list->values[obj->idx].bed_authorization != new_value) {
            GetMutableStay(obj)->bed_authorization = new_value;
        }
    })
    ELSE_IF_GET_NUM("session_count", ListProxy<mco_Stay>, obj.list->values[obj.idx].session_count)
    ELSE_IF_METHOD("session_count=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].session_count != new_value) {
            GetMutableStay(obj)->session_count = new_value;
        }
    })
    ELSE_IF_GET_NUM("igs2", ListProxy<mco_Stay>, obj.list->values[obj.idx].igs2)
    ELSE_IF_METHOD("igs2=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].igs2 != new_value) {
            GetMutableStay(obj)->igs2 = new_value;
        }
    })
    ELSE_IF_GET_DATE("last_menstrual_period", ListProxy<mco_Stay>, obj.list->values[obj.idx].last_menstrual_period)
    ELSE_IF_METHOD("last_menstrual_period=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        Date new_date = GetSlotDateSafe(vm, 1);

        if (list->values[obj->idx].last_menstrual_period != new_date) {
            GetMutableStay(obj)->last_menstrual_period = new_date;
        }
    })
    ELSE_IF_GET_NUM("gestational_age", ListProxy<mco_Stay>, obj.list->values[obj.idx].gestational_age)
    ELSE_IF_METHOD("gestational_age=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].gestational_age != new_value) {
            GetMutableStay(obj)->gestational_age = new_value;
        }
    })
    ELSE_IF_GET_NUM("newborn_weight", ListProxy<mco_Stay>, obj.list->values[obj.idx].newborn_weight)
    ELSE_IF_METHOD("newborn_weight=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].newborn_weight != new_value) {
            GetMutableStay(obj)->newborn_weight = new_value;
        }
    })
    ELSE_IF_GET_NUM("dip_count", ListProxy<mco_Stay>, obj.list->values[obj.idx].dip_count)
    ELSE_IF_METHOD("dip_count=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (list->values[obj->idx].dip_count != new_value) {
            GetMutableStay(obj)->dip_count = new_value;
        }
    })
    ELSE_IF_GET_STRING("main_diagnosis", ListProxy<mco_Stay>, obj.list->values[obj.idx].main_diagnosis.str)
    ELSE_IF_METHOD("main_diagnosis=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        const char *new_value = GetSlotStringSafe(vm, 1);
        DiagnosisCode new_diag = DiagnosisCode::FromString(new_value, (int)ParseFlag::End);
        if (UNLIKELY(!new_diag.IsValid())) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (list->values[obj->idx].main_diagnosis != new_diag) {
            GetMutableStay(obj)->main_diagnosis = new_diag;
        }
    })
    ELSE_IF_GET_STRING("linked_diagnosis", ListProxy<mco_Stay>, obj.list->values[obj.idx].linked_diagnosis.str)
    ELSE_IF_METHOD("linked_diagnosis=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        const char *new_value = GetSlotStringSafe(vm, 1);
        DiagnosisCode new_diag = DiagnosisCode::FromString(new_value, (int)ParseFlag::End);
        if (UNLIKELY(!new_diag.IsValid())) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (list->values[obj->idx].linked_diagnosis != new_diag) {
            GetMutableStay(obj)->linked_diagnosis = new_diag;
        }
    })
    ELSE_IF_METHOD("confirmed", [](WrenVM *vm) {
        const ListProxy<mco_Stay> &obj = *(ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, !!(obj.list->values[obj.idx].flags & (int)mco_Stay::Flag::Confirmed));
    })
    ELSE_IF_METHOD("confirmed=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        bool new_value = GetSlotIntegerSafe<int>(vm, 1);
        uint32_t new_flags = ApplyMask(list->values[obj->idx].flags,
                                       (int)mco_Stay::Flag::Confirmed, new_value);

        if (new_flags != list->values[obj->idx].flags) {
            GetMutableStay(obj)->flags = new_flags;
        }
    })
    ELSE_IF_METHOD("ucd", [](WrenVM *vm) {
        const ListProxy<mco_Stay> &obj = *(ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, !!(obj.list->values[obj.idx].flags & (int)mco_Stay::Flag::Ucd));
    })
    ELSE_IF_METHOD("ucd=(_)", [](WrenVM *vm) {
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ListObject<mco_Stay> *list = obj->list;

        bool new_value = GetSlotIntegerSafe<int>(vm, 1);
        uint32_t new_flags = ApplyMask(list->values[obj->idx].flags,
                                       (int)mco_Stay::Flag::Ucd, new_value);

        if (new_flags != list->values[obj->idx].flags) {
            GetMutableStay(obj)->flags = new_flags;
        }
    })

    ELSE_IF_METHOD("other_diagnoses", [](WrenVM *vm) {
        mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        const mco_Stay &stay = obj->list->values[obj->idx];

        // TODO: Use ForeignList instead? (same for procedures)
        if (runner->other_diagnoses_vars[obj->idx]) {
            wrenSetSlotHandle(vm, 0, runner->other_diagnoses_vars[obj->idx]);
        } else {
            wrenEnsureSlots(vm, 2);
            wrenSetSlotNewList(vm, 0);
            for (DiagnosisCode diag: stay.other_diagnoses) {
                wrenSetSlotString(vm, 1, diag.str);
                wrenInsertInList(vm, 0, -1, 1);
            }

            runner->other_diagnoses_vars[obj->idx] = wrenGetSlotHandle(vm, 0);
        }
    })
    ELSE_IF_METHOD("procedures", [](WrenVM *vm) {
        mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);
        ListProxy<mco_Stay> *obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        const mco_Stay &stay = obj->list->values[obj->idx];

        if (runner->procedures_vars[obj->idx]) {
            wrenSetSlotHandle(vm, 0, runner->procedures_vars[obj->idx]);
        } else {
            wrenEnsureSlots(vm, 2);
            wrenSetSlotNewList(vm, 0);
            for (const mco_ProcedureRealisation &proc: stay.procedures) {
                wrenSetSlotString(vm, 1, proc.proc.str);
                wrenInsertInList(vm, 0, -1, 1);
            }

            runner->procedures_vars[obj->idx] = wrenGetSlotHandle(vm, 0);
        }
    })

    return nullptr;
}

static inline const mco_Pricing &GetResultPricing(ResultObject *obj)
{
    if (!obj->pricing.stays_count) {
        mco_Price(*obj->result, false, &obj->pricing);
    }

    return obj->pricing;
}

static WrenForeignMethodFn BindMcoResultMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_NUM("main_stay_idx", ResultObject, obj.result->main_stay_idx)
    ELSE_IF_GET_NUM("duration", ResultObject, obj.result->duration)
    ELSE_IF_GET_NUM("age", ResultObject, obj.result->age)
    ELSE_IF_METHOD("ghm", [](WrenVM *vm) {
        const ResultObject &obj = *(const ResultObject *)wrenGetSlotForeign(vm, 0);
        char buf[32];
        wrenSetSlotString(vm, 0, obj.result->ghm.ToString(buf).ptr);
    })
    ELSE_IF_GET_NUM("main_error", ResultObject, obj.result->main_error)
    ELSE_IF_GET_NUM("ghs", ResultObject, obj.result->ghs.number)
    ELSE_IF_GET_NUM("ghs_duration", ResultObject, obj.result->ghs_duration)

    ELSE_IF_METHOD("ghs_coefficient", [](WrenVM *vm) {
        ResultObject *obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, GetResultPricing(obj).ghs_coefficient);
    })
    ELSE_IF_METHOD("ghs_cents", [](WrenVM *vm) {
        ResultObject *obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)GetResultPricing(obj).ghs_cents);
    })
    ELSE_IF_METHOD("price_cents", [](WrenVM *vm) {
        ResultObject *obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)GetResultPricing(obj).price_cents);
    })
    ELSE_IF_METHOD("exb_exh", [](WrenVM *vm) {
        ResultObject *obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, GetResultPricing(obj).exb_exh);
    })
    ELSE_IF_METHOD("total_cents", [](WrenVM *vm) {
        ResultObject *obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)GetResultPricing(obj).total_cents);
    })

    return nullptr;
}

static WrenForeignMethodFn BindMcoMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_METHOD("result", [](WrenVM *vm) {
        const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm);
        wrenSetSlotHandle(vm, 0, runner.result_obj->var);
    })
    ELSE_IF_METHOD("stays", [](WrenVM *vm) {
        const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm);
        wrenSetSlotHandle(vm, 0, runner.stays_obj->var);
    })

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
    } else if (!is_static && TestStr(class_name, "McoStay")) {
        return BindMcoStayMethod(signature);
    } else if (!is_static && TestStr(class_name, "McoResult")) {
        return BindMcoResultMethod(signature);
    } else if (is_static && TestStr(class_name, "MCO")) {
        return BindMcoMethod(signature);
    } else {
        return nullptr;
    }
}

bool mco_WrenRunner::Init(const char *expression, Size max_results)
{
    vm_alloc.ReleaseAll();
    thread_alloc = &vm_alloc;

    // Init Wren VM
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

        // Limit execution time and space, and disable GC
        config.maxRunOps = 200000;
        config.maxHeapSize = Kibibytes(max_results) / 2;
        config.initialHeapSize = 0;

        // We don't need to free this because all allocations go through the
        // WrenMemory allocator above.
        vm = wrenNewVM(&config);
    }

    wrenSetUserData(vm, this);

    // Run init code
    DebugAssert(wrenInterpret(vm, "mco", InitCode) == WREN_RESULT_SUCCESS);
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, "mco", "Date", 0);
    date_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "McoStay", 0);
    stay_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "McoResult", 0);
    wrenSetSlotNewForeign(vm, 0, 0, SIZE(ResultObject));
    result_obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
    result_obj->var = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "ForeignList", 0);
    wrenSetSlotNewForeign(vm, 0, 0, SIZE(ListObject<char>));
    stays_obj = (ListObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
    stays_obj->var = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "MCO", 0);
    mco_class = wrenGetSlotHandle(vm, 0);
    mco_build = wrenMakeCallHandle(vm, "build(_)");
    expression_call = wrenMakeCallHandle(vm, "call()");
    DebugAssert(wrenInterpret(vm, "mco", VarCode) == WREN_RESULT_SUCCESS);

    // Compile expression
    wrenEnsureSlots(vm, 2);
    wrenSetSlotHandle(vm, 0, mco_class);
    wrenSetSlotString(vm, 1, expression);
    if (wrenCall(vm, mco_build) != WREN_RESULT_SUCCESS)
        return false;
    expression_var = wrenGetSlotHandle(vm, 0);

    return true;
}

Size mco_WrenRunner::Process(Span<const mco_Result> results, const mco_Result mono_results[],
                               HeapArray<const mco_Result *> *out_results,
                               HeapArray<const mco_Result *> *out_mono_results,
                               mco_StaySet *out_stay_set)
{
    thread_alloc = &vm_alloc;

    // Reuse for performance
    HeapArray<WrenHandle *> stay_vars;

    Size stays_count = 0;
    for (const mco_Result &result: results) {
        while (stay_vars.len < result.stays.len) {
            wrenEnsureSlots(vm, 1);
            wrenSetSlotHandle(vm, 0, stay_class);
            wrenSetSlotNewForeign(vm, 0, 0, SIZE(ListProxy<mco_Stay>));

            WrenHandle *stay_var = wrenGetSlotHandle(vm, 0);
            ListProxy<mco_Stay> *stay_obj = (ListProxy<mco_Stay> *)wrenGetSlotForeign(vm, 0);
            stay_obj->list = stays_obj;
            stay_obj->idx = stay_vars.len;

            stay_vars.Append(stay_var);
        }

        other_diagnoses_vars.RemoveFrom(0);
        other_diagnoses_vars.AppendDefault(result.stays.len);
        procedures_vars.RemoveFrom(0);
        procedures_vars.AppendDefault(result.stays.len);

        stays_obj->vars = stay_vars.Take(0, result.stays.len);
        stays_obj->values = result.stays;
        stays_obj->copies.RemoveFrom(0);
        result_obj->result = &result;
        result_obj->pricing = {};

        wrenEnsureSlots(vm, 1);
        wrenSetSlotHandle(vm, 0, expression_var);
        if (wrenCall(vm, expression_call) != WREN_RESULT_SUCCESS)
            return -1;

        if (wrenGetSlotType(vm, 0) != WREN_TYPE_BOOL || wrenGetSlotBool(vm, 0)) {
            if (stays_obj->copies.len) {
                if (UNLIKELY(!out_stay_set)) {
                    LogError("Cannot mutate stays");
                    return -1;
                }

                out_stay_set->stays.Append(stays_obj->copies);
            } else {
                out_results->Append(&result);
                if (out_mono_results) {
                    for (Size i = 0; i < result.stays.len; i++) {
                        const mco_Result &mono_result = mono_results[stays_count + i];
                        DebugAssert(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                        out_mono_results->Append(&mono_result);
                    }
                }
            }
        }

        stays_count += result.stays.len;
    }

    return stays_count;
}

mco_FilterRunner::~mco_FilterRunner()
{
    if (wren)
        delete wren;
}

bool mco_FilterRunner::Init(const char *filter)
{
    // Wren expressions must not start or end with newlines
    filter_buf.Clear();
    filter_buf.Append(TrimStr((Span<const char>)filter));
    filter_buf.Append(0);

    return ResetRunner();
}

// TODO: Parallelize filtering, see old mco_Filter() API
bool mco_FilterRunner::Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                         HeapArray<const mco_Result *> *out_results,
                         HeapArray<const mco_Result *> *out_mono_results,
                         mco_StaySet *out_stay_set)
{
    DEFER_NC(out_guard, results_len = out_results->len,
                        stays_len = out_stay_set ? out_stay_set->stays.len : 0) {
        out_results->RemoveFrom(results_len);
        if (out_stay_set) {
            out_stay_set->stays.RemoveFrom(stays_len);
        }
    };

    while (results.len) {
        if (!wren_count && !ResetRunner())
            return false;

        Size process_results = std::min(results.len, wren_count);
        Size process_stays = wren->Process(results.Take(0, process_results), mono_results.ptr,
                                           out_results, out_mono_results, out_stay_set);
        if (process_stays < 0)
            return false;

        results = results.Take(process_results, results.len - process_results);
        if (out_mono_results) {
            mono_results = mono_results.Take(process_stays, mono_results.len - process_stays);
        }

        wren_count -= process_results;
    }

    out_guard.disable();
    return true;
}

bool mco_FilterRunner::ResetRunner()
{
    if (wren)
        delete wren;
    wren = new mco_WrenRunner;
    wren_count = 16384;

    return wren->Init(filter_buf.ptr, wren_count);
}
