// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "mco_filter.hh"
#include "mco_pricing.hh"

#include "vendor/wren/src/include/wren.hpp"

namespace K {

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

foreign class StayArray is Sequence {
    foreign [index]
    foreign iteratorValue(it)
    foreign iterate(it)
    foreign count
}

foreign class DiagnosisArray is Sequence {
    foreign add(str)
    foreign remove(str)

    foreign [index]
    foreign iteratorValue(it)
    foreign iterate(it)
    foreign count
}

foreign class ProcedureArray is Sequence {
    foreign add(str, date, phase, activities, extensions)
    foreign remove(str)

    foreign [index]
    foreign iteratorValue(it)
    foreign iterate(it)
    foreign count
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
    foreign raac
    foreign raac=(value)

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
struct ProxyArray {
    WrenHandle *var;
    Span<const T> values;

    // XXX: Move out of here, it is used only for the stays array
    HeapArray<WrenHandle *> vars;
    HeapArray<T> copies;
};

template <typename T>
struct ProxyArrayObject {
    ProxyArray<T> *array;
    Size idx;
};

struct ResultObject {
    WrenHandle *var;
    const mco_Result *result;
    mco_Pricing pricing;
};

class mco_WrenRunner {
    K_DELETE_COPY(mco_WrenRunner)

    BlockAllocator vm_alloc { Kibibytes(256) };
    bool first_error;

public:
    WrenVM *vm;

    WrenHandle *date_class;
    WrenHandle *stay_class;
    WrenHandle *diagnosis_array_class;
    WrenHandle *procedure_array_class;
    ProxyArray<mco_Stay> *stays_arr;
    ResultObject *result_obj;
    WrenHandle *mco_class;
    WrenHandle *mco_build;

    // We don't bother shrinking those
    HeapArray<ProxyArray<drd_DiagnosisCode> *> other_diagnosis_arrays;
    HeapArray<ProxyArray<mco_ProcedureRealisation> *> procedure_arrays;

    WrenHandle *expression_var = nullptr;
    WrenHandle *expression_call;

    mco_WrenRunner() = default;

    bool Init(const char *filter, Size max_results);

    Size Process(Span<const mco_Result> results, const mco_Result mono_results[],
                 HeapArray<const mco_Result *> *out_results,
                 HeapArray<const mco_Result *> *out_mono_results,
                 mco_StaySet *out_stay_set);

private:
    void InitProxyArrays(Size count);
};

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
    if (wrenGetSlotType(vm, slot) != WREN_TYPE_NUM) [[unlikely]] {
        TriggerError(vm, "Expected numeric value");
        return 0;
    }

    double value = wrenGetSlotDouble(vm, slot);
    if (value < (double)std::numeric_limits<T>::min() ||
            value > (double)std::numeric_limits<T>::max()) [[unlikely]] {
        TriggerError(vm, "Expected integer value between %1 and %2",
                     std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        return 0;
    }

    return (T)value;
}

static inline const char *GetSlotStringSafe(WrenVM *vm, int slot)
{
    if (wrenGetSlotType(vm, slot) != WREN_TYPE_STRING) [[unlikely]] {
        TriggerError(vm, "Expected string value");
        return nullptr;
    }

    return wrenGetSlotString(vm, slot);
}

static Size GetSlotIndexSafe(WrenVM *vm, int slot, Size len)
{
    Size idx = GetSlotIntegerSafe<Size>(vm, slot);

    if (idx >= 0 && idx < len) {
        return idx;
    } else if (idx < 0 && idx >= -len) {
        return len + idx;
    } else {
        TriggerError(vm, "Index is out-of-bound");
        return -1;
    }
}

static inline LocalDate GetSlotDateSafe(WrenVM *vm, int slot)
{
    switch (wrenGetSlotType(vm, slot)) {
        case WREN_TYPE_FOREIGN: {
            const mco_WrenRunner &runner = *(mco_WrenRunner *)wrenGetUserData(vm);

            if (!wrenForeignIsClass(vm, slot, runner.date_class)) [[unlikely]] {
                TriggerError(vm, "Expected Date or null");
                return {};
            }

            return *(LocalDate *)wrenGetSlotForeign(vm, slot);
        } break;
        case WREN_TYPE_NULL: return {};

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
            if (value < 0.0 || value >= 10.0) [[unlikely]] {
                TriggerError(vm, "Mode must be between 0 and 9");
                return 0;
            }

            return '0' + (char)value;
        } break;
        case WREN_TYPE_STRING: {
            const char *value = wrenGetSlotString(vm, slot);
            if (!value[0] || value[1]) [[unlikely]] {
                TriggerError(vm, "Mode must be one character");
                return 0;
            }

            return value[0];
        } break;
        case WREN_TYPE_NULL: return 0;

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
            LocalDate *date = (LocalDate *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(LocalDate));
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
            LocalDate *date = (LocalDate *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(LocalDate)); \
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
        LocalDate *date = (LocalDate *)wrenGetSlotForeign(vm, 0);

        date->st.year = GetSlotIntegerSafe<int16_t>(vm, 1);
        date->st.month = GetSlotIntegerSafe<int8_t>(vm, 2);
        date->st.day = GetSlotIntegerSafe<int8_t>(vm, 3);

        if (!date->IsValid()) {
            TriggerError(vm, "Date is not valid");
        }
    })
    ELSE_IF_METHOD("==(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 == date2);
    })
    ELSE_IF_METHOD("!=(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 != date2);
    })
    ELSE_IF_METHOD("<(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 < date2);
    })
    ELSE_IF_METHOD("<=(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 <= date2);
    })
    ELSE_IF_METHOD(">(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 > date2);
    })
    ELSE_IF_METHOD(">=(_)", [](WrenVM *vm) {
        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        LocalDate date2 = GetSlotDateSafe(vm, 1);

        wrenSetSlotBool(vm, 0, date1 >= date2);
    })
    ELSE_IF_METHOD("-(_)", [](WrenVM *vm) {
        const mco_WrenRunner &runner = *(const mco_WrenRunner *)wrenGetUserData(vm);

        LocalDate date1 = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        if (!date1.IsValid()) [[unlikely]] {
            TriggerError(vm, "Cannot compute on invalid date");
            return;
        }

        switch (wrenGetSlotType(vm, 1)) {
            case WREN_TYPE_FOREIGN: {
                LocalDate date2 = GetSlotDateSafe(vm, 1);
                if (!date2.IsValid()) [[unlikely]] {
                    TriggerError(vm, "Cannot compute days between invalid dates");
                    return;
                }

                wrenSetSlotDouble(vm, 0, (double)(date1 - date2));
            } break;
            case WREN_TYPE_NUM: {
                int16_t days = GetSlotIntegerSafe<int16_t>(vm, 1);

                wrenSetSlotHandle(vm, 0, runner.date_class);
                LocalDate *ret = (LocalDate *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(date1));
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

        LocalDate date = *(LocalDate *)wrenGetSlotForeign(vm, 0);
        if (!date.IsValid()) [[unlikely]] {
            TriggerError(vm, "Cannot compute on invalid date");
            return;
        }

        int16_t days = GetSlotIntegerSafe<int16_t>(vm, 1);

        wrenSetSlotHandle(vm, 0, runner.date_class);
        LocalDate *ret = (LocalDate *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(date));
        *ret = date + days;
    })
    ELSE_IF_GET_NUM("year", LocalDate, obj.st.year)
    ELSE_IF_GET_NUM("month", LocalDate, obj.st.month)
    ELSE_IF_GET_NUM("day", LocalDate, obj.st.day)
    ELSE_IF_METHOD("toString", [](WrenVM *vm) {
        LocalDate date = *(LocalDate *)wrenGetSlotForeign(vm, 0);

        char buf[64];
        Fmt(buf, "%1", date);
        wrenSetSlotString(vm, 0, buf);
    })

    return nullptr;
}

template <typename T>
static void MakeArrayMutable(ProxyArray<T> *array)
{
    if (!array->copies.len) {
        array->copies.Append(array->values);
        array->values = array->copies;
    }
}

static void MarkStaysAsMutated(WrenVM *vm)
{
    mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);
    MakeArrayMutable(runner->stays_arr);
}

static inline mco_Stay *GetMutableStay(ProxyArrayObject<mco_Stay> *obj)
{
    ProxyArray<mco_Stay> *array = obj->array;

    MakeArrayMutable(array);
    return &array->copies[obj->idx];
}

static WrenForeignMethodFn BindProxyArrayMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_METHOD("iterate(_)", [](WrenVM *vm) {
        const ProxyArray<char> &arr = *(const ProxyArray<char> *)wrenGetSlotForeign(vm, 0);

        Size idx;
        switch (wrenGetSlotType(vm, 1)) {
            case WREN_TYPE_NULL: { idx = -1; } break;
            case WREN_TYPE_NUM: { idx = (Size)wrenGetSlotDouble(vm, 1); } break;

            default: {
                TriggerError(vm, "Iterator must be null or number");
                return;
            } break;
        }

        if (++idx < arr.values.len) {
            wrenSetSlotDouble(vm, 0, (double)idx);
        } else {
            wrenSetSlotBool(vm, 0, false);
        }
    })
    ELSE_IF_METHOD("count", [](WrenVM *vm) {
        const ProxyArray<char> &arr = *(const ProxyArray<char> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, (double)arr.values.len);
    })

    return nullptr;
}

static WrenForeignMethodFn BindStayArrayMethod(const char *signature)
{
    const auto get_idx_value = [](WrenVM *vm) {
        ProxyArray<mco_Stay> *arr = (ProxyArray<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIndexSafe(vm, 1, arr->values.len);

        if (idx >= 0) [[likely]] {
            wrenSetSlotHandle(vm, 0, arr->vars[idx]);
        }
    };

    if (false) {}

    ELSE_IF_METHOD("[_]", get_idx_value)
    ELSE_IF_METHOD("iteratorValue(_)", get_idx_value)

    // Generic methods (count, iterator)
    return BindProxyArrayMethod(signature);
}

static WrenForeignMethodFn BindDiagnosisArrayMethod(const char *signature)
{
    const auto get_idx_value = [](WrenVM *vm) {
        const ProxyArray<drd_DiagnosisCode> &arr =
            *(const ProxyArray<drd_DiagnosisCode> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIndexSafe(vm, 1, arr.values.len);

        if (idx >= 0) [[likely]] {
            wrenSetSlotString(vm, 0, arr.values[idx].str);
        }
    };

    if (false) {}

    ELSE_IF_METHOD("add(_)", [](WrenVM *vm) {
        ProxyArray<drd_DiagnosisCode> &arr = *(ProxyArray<drd_DiagnosisCode> *)wrenGetSlotForeign(vm, 0);
        const char *str = GetSlotStringSafe(vm, 1);

        drd_DiagnosisCode new_diag = drd_DiagnosisCode::Parse(str, (int)ParseFlag::End);
        if (!new_diag.IsValid()) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (!std::any_of(arr.values.begin(), arr.values.end(),
                         [&](drd_DiagnosisCode diag) { return diag == new_diag; })) {
            if (!arr.copies.len) {
                arr.copies.Append(arr.values);
            }
            arr.copies.Append(new_diag);
            arr.values = arr.copies;

            MarkStaysAsMutated(vm);
        }
    })
    ELSE_IF_METHOD("remove(_)", [](WrenVM *vm) {
        ProxyArray<drd_DiagnosisCode> &arr = *(ProxyArray<drd_DiagnosisCode> *)wrenGetSlotForeign(vm, 0);
        const char *str = GetSlotStringSafe(vm, 1);

        drd_DiagnosisCode remove_diag = drd_DiagnosisCode::Parse(str, (int)ParseFlag::End);
        if (!remove_diag.IsValid()) {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        arr.copies.RemoveFrom(0);
        for (drd_DiagnosisCode diag: arr.values) {
            if (diag == remove_diag) {
                MarkStaysAsMutated(vm);
            } else {
                arr.copies.Append(diag);
            }
        }
        arr.values = arr.copies;
    })

    ELSE_IF_METHOD("[_]", get_idx_value)
    ELSE_IF_METHOD("iteratorValue(_)", get_idx_value)

    // Generic methods (count, iterator)
    return BindProxyArrayMethod(signature);
}

static WrenForeignMethodFn BindProcedureArrayMethod(const char *signature)
{
    const auto get_idx_value = [](WrenVM *vm) {
        const ProxyArray<mco_ProcedureRealisation> &arr =
            *(const ProxyArray<mco_ProcedureRealisation> *)wrenGetSlotForeign(vm, 0);
        Size idx = GetSlotIndexSafe(vm, 1, arr.values.len);

        if (idx >= 0) [[likely]] {
            wrenSetSlotString(vm, 0, arr.values[idx].proc.str);
        }
    };

    if (false) {}

    // Unlike diagnoses, we don't try to prevent duplicates. You can use a code
    // twice if the same procedure is done twice, even the same day!
    ELSE_IF_METHOD("add(_,_,_,_,_)", [](WrenVM *vm) {
        ProxyArray<mco_ProcedureRealisation> &arr =
            *(ProxyArray<mco_ProcedureRealisation> *)wrenGetSlotForeign(vm, 0);
        const char *str = GetSlotStringSafe(vm, 1);
        LocalDate date = GetSlotDateSafe(vm, 2);
        int8_t phase = GetSlotIntegerSafe<int8_t>(vm, 3);
        int activities_dec = GetSlotIntegerSafe<int>(vm, 4);
        int8_t extension = GetSlotIntegerSafe<int8_t>(vm, 5);

        mco_ProcedureRealisation new_proc = {};

        new_proc.proc = drd_ProcedureCode::Parse(str, (int)ParseFlag::End);
        if (!new_proc.proc.IsValid()) {
            TriggerError(vm, "Invalid procedure code");
            return;
        }
        new_proc.phase = phase;
        new_proc.count = 1;
        new_proc.date = date;
        new_proc.extension = extension;

        if (activities_dec) {
            if (!arr.copies.len) {
                arr.copies.Append(arr.values);
            }

            while (activities_dec) {
                new_proc.activity = (int8_t)(activities_dec % 10);
                activities_dec /= 10;

                arr.copies.Append(new_proc);
            }
            arr.values = arr.copies;

            MarkStaysAsMutated(vm);
        }
    })
    ELSE_IF_METHOD("remove(_)", [](WrenVM *vm) {
        ProxyArray<mco_ProcedureRealisation> &arr =
            *(ProxyArray<mco_ProcedureRealisation> *)wrenGetSlotForeign(vm, 0);
        const char *str = GetSlotStringSafe(vm, 1);

        drd_ProcedureCode remove_proc = drd_ProcedureCode::Parse(str, (int)ParseFlag::End);
        if (!remove_proc.IsValid()) {
            TriggerError(vm, "Invalid procedure code");
            return;
        }

        arr.copies.RemoveFrom(0);
        for (const mco_ProcedureRealisation &proc: arr.values) {
            if (proc.proc == remove_proc) {
                MarkStaysAsMutated(vm);
            } else {
                arr.copies.Append(proc);
            }
        }
        arr.values = arr.copies;
    })

    ELSE_IF_METHOD("[_]", get_idx_value)
    ELSE_IF_METHOD("iteratorValue(_)", get_idx_value)

    // Generic methods (count, iterator)
    return BindProxyArrayMethod(signature);
}

static WrenForeignMethodFn BindMcoStayMethod(const char *signature)
{
    if (false) {}

    ELSE_IF_GET_NUM("admin_id", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].admin_id)
    ELSE_IF_GET_NUM("bill_id", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].bill_id)
    ELSE_IF_GET_NUM("sex", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].sex)
    ELSE_IF_METHOD("sex=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (array->values[obj->idx].sex != new_value) {
            GetMutableStay(obj)->sex = new_value;
        }
    })
    ELSE_IF_GET_DATE("birthdate", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].birthdate)
    ELSE_IF_METHOD("birthdate=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        LocalDate new_date = GetSlotDateSafe(vm, 1);

        if (array->values[obj->idx].birthdate != new_date) {
            GetMutableStay(obj)->birthdate = new_date;
        }
    })
    ELSE_IF_GET_DATE("entry_date", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].entry.date)
    ELSE_IF_METHOD("entry_date=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        LocalDate new_date = GetSlotDateSafe(vm, 1);

        if (array->values[obj->idx].entry.date != new_date) {
            GetMutableStay(obj)->entry.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("entry_mode", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].entry.mode)
    ELSE_IF_METHOD("entry_mode=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        char new_value = GetSlotModeSafe(vm, 1);

        if (array->values[obj->idx].entry.mode != new_value) {
            GetMutableStay(obj)->entry.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("entry_origin", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].entry.origin)
    ELSE_IF_METHOD("entry_origin=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        char new_value = GetSlotModeSafe(vm, 1);

        if (array->values[obj->idx].entry.origin != new_value) {
            GetMutableStay(obj)->entry.origin = new_value;
        }
    })
    ELSE_IF_GET_DATE("exit_date", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].exit.date)
    ELSE_IF_METHOD("exit_date=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        LocalDate new_date = GetSlotDateSafe(vm, 1);

        if (array->values[obj->idx].exit.date != new_date) {
            GetMutableStay(obj)->exit.date = new_date;
        }
    })
    ELSE_IF_GET_MODE("exit_mode", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].exit.mode)
    ELSE_IF_METHOD("exit_mode=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        char new_value = GetSlotModeSafe(vm, 1);

        if (array->values[obj->idx].exit.mode != new_value) {
            GetMutableStay(obj)->exit.mode = new_value;
        }
    })
    ELSE_IF_GET_MODE("exit_destination", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].exit.destination)
    ELSE_IF_METHOD("exit_destination=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        char new_value = GetSlotModeSafe(vm, 1);

        if (array->values[obj->idx].exit.destination != new_value) {
            GetMutableStay(obj)->exit.destination = new_value;
        }
    })
    ELSE_IF_GET_NUM("unit", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].unit.number)
    ELSE_IF_METHOD("unit=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].unit.number != new_value) {
            GetMutableStay(obj)->unit = drd_UnitCode(new_value);
        }
    })
    ELSE_IF_GET_NUM("bed_authorization", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].bed_authorization)
    ELSE_IF_METHOD("bed_authorization=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int8_t new_value = GetSlotIntegerSafe<int8_t>(vm, 1);

        if (array->values[obj->idx].bed_authorization != new_value) {
            GetMutableStay(obj)->bed_authorization = new_value;
        }
    })
    ELSE_IF_GET_NUM("session_count", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].session_count)
    ELSE_IF_METHOD("session_count=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].session_count != new_value) {
            GetMutableStay(obj)->session_count = new_value;
        }
    })
    ELSE_IF_GET_NUM("igs2", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].igs2)
    ELSE_IF_METHOD("igs2=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].igs2 != new_value) {
            GetMutableStay(obj)->igs2 = new_value;
        }
    })
    ELSE_IF_GET_DATE("last_menstrual_period", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].last_menstrual_period)
    ELSE_IF_METHOD("last_menstrual_period=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        LocalDate new_date = GetSlotDateSafe(vm, 1);

        if (array->values[obj->idx].last_menstrual_period != new_date) {
            GetMutableStay(obj)->last_menstrual_period = new_date;
        }
    })
    ELSE_IF_GET_NUM("gestational_age", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].gestational_age)
    ELSE_IF_METHOD("gestational_age=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].gestational_age != new_value) {
            GetMutableStay(obj)->gestational_age = new_value;
        }
    })
    ELSE_IF_GET_NUM("newborn_weight", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].newborn_weight)
    ELSE_IF_METHOD("newborn_weight=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].newborn_weight != new_value) {
            GetMutableStay(obj)->newborn_weight = new_value;
        }
    })
    ELSE_IF_GET_NUM("dip_count", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].dip_count)
    ELSE_IF_METHOD("dip_count=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        int16_t new_value = GetSlotIntegerSafe<int16_t>(vm, 1);

        if (array->values[obj->idx].dip_count != new_value) {
            GetMutableStay(obj)->dip_count = new_value;
        }
    })
    ELSE_IF_GET_STRING("main_diagnosis", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].main_diagnosis.str)
    ELSE_IF_METHOD("main_diagnosis=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        const char *new_value = GetSlotStringSafe(vm, 1);
        drd_DiagnosisCode new_diag = drd_DiagnosisCode::Parse(new_value, (int)ParseFlag::End);
        if (!new_diag.IsValid()) [[unlikely]] {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (array->values[obj->idx].main_diagnosis != new_diag) {
            GetMutableStay(obj)->main_diagnosis = new_diag;
        }
    })
    ELSE_IF_GET_STRING("linked_diagnosis", ProxyArrayObject<mco_Stay>, obj.array->values[obj.idx].linked_diagnosis.str)
    ELSE_IF_METHOD("linked_diagnosis=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        const char *new_value = GetSlotStringSafe(vm, 1);
        drd_DiagnosisCode new_diag = drd_DiagnosisCode::Parse(new_value, (int)ParseFlag::End);
        if (!new_diag.IsValid()) [[unlikely]] {
            TriggerError(vm, "Invalid diagnosis code");
            return;
        }

        if (array->values[obj->idx].linked_diagnosis != new_diag) {
            GetMutableStay(obj)->linked_diagnosis = new_diag;
        }
    })
    ELSE_IF_METHOD("confirmed", [](WrenVM *vm) {
        const ProxyArrayObject<mco_Stay> &obj = *(ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, !!(obj.array->values[obj.idx].flags & (int)mco_Stay::Flag::Confirmed));
    })
    ELSE_IF_METHOD("confirmed=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        bool new_value = GetSlotIntegerSafe<int>(vm, 1);
        uint32_t new_flags = ApplyMask(array->values[obj->idx].flags,
                                       (int)mco_Stay::Flag::Confirmed, new_value);

        if (new_flags != array->values[obj->idx].flags) {
            GetMutableStay(obj)->flags = new_flags;
        }
    })
    ELSE_IF_METHOD("ucd", [](WrenVM *vm) {
        const ProxyArrayObject<mco_Stay> &obj = *(ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, !!(obj.array->values[obj.idx].flags & (int)mco_Stay::Flag::UCD));
    })
    ELSE_IF_METHOD("ucd=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        bool new_value = GetSlotIntegerSafe<int>(vm, 1);
        uint32_t new_flags = ApplyMask(array->values[obj->idx].flags,
                                       (int)mco_Stay::Flag::UCD, new_value);

        if (new_flags != array->values[obj->idx].flags) {
            GetMutableStay(obj)->flags = new_flags;
        }
    })
    ELSE_IF_METHOD("raac", [](WrenVM *vm) {
        const ProxyArrayObject<mco_Stay> &obj = *(ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        wrenSetSlotDouble(vm, 0, !!(obj.array->values[obj.idx].flags & (int)mco_Stay::Flag::RAAC));
    })
    ELSE_IF_METHOD("raac=(_)", [](WrenVM *vm) {
        ProxyArrayObject<mco_Stay> *obj = (ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);
        ProxyArray<mco_Stay> *array = obj->array;

        bool new_value = GetSlotIntegerSafe<int>(vm, 1);
        uint32_t new_flags = ApplyMask(array->values[obj->idx].flags,
                                       (int)mco_Stay::Flag::RAAC, new_value);

        if (new_flags != array->values[obj->idx].flags) {
            GetMutableStay(obj)->flags = new_flags;
        }
    })

    ELSE_IF_METHOD("other_diagnoses", [](WrenVM *vm) {
        mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);
        const ProxyArrayObject<mco_Stay> &obj = *(ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);

        wrenSetSlotHandle(vm, 0, runner->other_diagnosis_arrays[obj.idx]->var);
    })
    ELSE_IF_METHOD("procedures", [](WrenVM *vm) {
        mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);
        const ProxyArrayObject<mco_Stay> &obj = *(ProxyArrayObject<mco_Stay> *)wrenGetSlotForeign(vm, 0);

        wrenSetSlotHandle(vm, 0, runner->procedure_arrays[obj.idx]->var);
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
        wrenSetSlotHandle(vm, 0, runner.stays_arr->var);
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
    } else if (!is_static && TestStr(class_name, "StayArray")) {
        return BindStayArrayMethod(signature);
    } else if (!is_static && TestStr(class_name, "DiagnosisArray")) {
        return BindDiagnosisArrayMethod(signature);
    } else if (!is_static && TestStr(class_name, "ProcedureArray")) {
        return BindProcedureArrayMethod(signature);
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
    vm_alloc.Reset();

    // Init Wren VM
    {
        WrenConfiguration config;
        wrenInitConfiguration(&config);

        // Use fast bump allocator and avoid GC as much as possible for
        // maximum performance. Release everything at once at the end!
        config.reallocateFn = [](void *mem, size_t old_size, size_t new_size, void *udata) {
            mco_WrenRunner *runner = (mco_WrenRunner *)udata;

            K_ASSERT(old_size <= K_SIZE_MAX && new_size <= K_SIZE_MAX);
            if (new_size > old_size) {
                runner->vm_alloc.Resize(&mem, (Size)old_size, (Size)new_size);
            }
            return mem;
        };

        // Default issues stack-trace like errors, hack around it to show (when possible)
        // a single error message to the user.
        first_error = true;
        config.errorFn = [](WrenVM *vm, WrenErrorType, const char *, int, const char *msg) {
            mco_WrenRunner *runner = (mco_WrenRunner *)wrenGetUserData(vm);

            if (runner->first_error) {
                LogError("%1", msg);
                runner->first_error = false;
            }
        };

        config.bindForeignClassFn = BindForeignClass;
        config.bindForeignMethodFn = BindForeignMethod;

        // Limit execution time and space, and disable GC
        config.maxRunOps = 200000;
        config.maxHeapSize = Kibibytes(max_results) * 2;
        config.initialHeapSize = 0;

        // We don't need to free this because all allocations go through the
        // WrenMemory allocator above.
        vm = wrenNewVM(&config);
    }

    wrenSetUserData(vm, this);

    // Run init code
    {
        WrenInterpretResult ret = wrenInterpret(vm, "mco", InitCode);
        K_ASSERT(ret == WREN_RESULT_SUCCESS);
    }
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, "mco", "Date", 0);
    date_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "McoStay", 0);
    stay_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "DiagnosisArray", 0);
    diagnosis_array_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "ProcedureArray", 0);
    procedure_array_class = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "McoResult", 0);
    wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(ResultObject));
    result_obj = (ResultObject *)wrenGetSlotForeign(vm, 0);
    result_obj->var = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "StayArray", 0);
    wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(ProxyArray<char>));
    stays_arr = (ProxyArray<mco_Stay> *)wrenGetSlotForeign(vm, 0);
    stays_arr->var = wrenGetSlotHandle(vm, 0);
    wrenGetVariable(vm, "mco", "MCO", 0);
    mco_class = wrenGetSlotHandle(vm, 0);
    mco_build = wrenMakeCallHandle(vm, "build(_)");
    expression_call = wrenMakeCallHandle(vm, "call()");
    {
        WrenInterpretResult ret = wrenInterpret(vm, "mco", VarCode);
        K_ASSERT(ret == WREN_RESULT_SUCCESS);
    }

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
    BlockAllocator new_other_diagnoses_alloc { 2048 * K_SIZE(drd_DiagnosisCode) };
    BlockAllocator new_procedures_alloc { 2048 * K_SIZE(mco_ProcedureRealisation) };

    K_DEFER {
        new_other_diagnoses_alloc.GiveTo(&out_stay_set->array_alloc);
        new_procedures_alloc.GiveTo(&out_stay_set->array_alloc);
    };

    HeapArray<drd_DiagnosisCode> new_other_diagnoses(&new_other_diagnoses_alloc);
    HeapArray<mco_ProcedureRealisation> new_procedures(&new_procedures_alloc);
    Size stays_count = 0;

    for (const mco_Result &result: results) {
        InitProxyArrays(result.stays.len - other_diagnosis_arrays.len);

        stays_arr->values = result.stays;
        stays_arr->copies.RemoveFrom(0);
        result_obj->result = &result;
        result_obj->pricing = {};

        for (Size i = 0; i < result.stays.len; i++) {
            const mco_Stay &stay = result.stays[i];

            other_diagnosis_arrays[i]->values = stay.other_diagnoses;
            other_diagnosis_arrays[i]->copies.RemoveFrom(0);
            procedure_arrays[i]->values = stay.procedures;
            procedure_arrays[i]->copies.RemoveFrom(0);
        }

        wrenEnsureSlots(vm, 1);
        wrenSetSlotHandle(vm, 0, expression_var);
        if (wrenCall(vm, expression_call) != WREN_RESULT_SUCCESS)
            return -1;

        if (wrenGetSlotType(vm, 0) != WREN_TYPE_BOOL || wrenGetSlotBool(vm, 0)) {
            if (stays_arr->copies.len) {
                if (!out_stay_set) [[unlikely]] {
                    LogError("Cannot mutate stays");
                    return -1;
                }

                for (Size i = 0; i < result.stays.len; i++) {
                    if (other_diagnosis_arrays[i]->copies.len) {
                        new_other_diagnoses.Append(other_diagnosis_arrays[i]->values);
                        stays_arr->copies[i].other_diagnoses = new_other_diagnoses.TrimAndLeak();
                    }

                    if (procedure_arrays[i]->copies.len) {
                        new_procedures.Append(procedure_arrays[i]->values);
                        stays_arr->copies[i].procedures = new_procedures.TrimAndLeak();
                    }
                }

                out_stay_set->stays.Append(stays_arr->copies);
            } else {
                out_results->Append(&result);
                if (out_mono_results) {
                    for (Size i = 0; i < result.stays.len; i++) {
                        const mco_Result &mono_result = mono_results[stays_count + i];
                        K_ASSERT(mono_result.stays[0].bill_id == result.stays[0].bill_id);

                        out_mono_results->Append(&mono_result);
                    }
                }
            }
        }

        stays_count += result.stays.len;
    }

    return stays_count;
}

void mco_WrenRunner::InitProxyArrays(Size count)
{
    wrenEnsureSlots(vm, 1);

    for (Size i = 0; i < count; i++) {
        wrenSetSlotHandle(vm, 0, stay_class);
        {
            ProxyArrayObject<mco_Stay> *stay_obj =
                (ProxyArrayObject<mco_Stay> *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(ProxyArrayObject<mco_Stay>));
            WrenHandle *stay_var = wrenGetSlotHandle(vm, 0);

            stay_obj->array = stays_arr;
            stay_obj->idx = stays_arr->vars.len;
            stays_arr->vars.Append(stay_var);
        }

        wrenSetSlotHandle(vm, 0, diagnosis_array_class);
        {
            ProxyArray<drd_DiagnosisCode> *arr =
                (ProxyArray<drd_DiagnosisCode> *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(ProxyArray<drd_DiagnosisCode>));

            arr->var = wrenGetSlotHandle(vm, 0);
            other_diagnosis_arrays.Append(arr);
        }

        wrenSetSlotHandle(vm, 0, procedure_array_class);
        {
            ProxyArray<mco_ProcedureRealisation> *arr =
                (ProxyArray<mco_ProcedureRealisation> *)wrenSetSlotNewForeign(vm, 0, 0, K_SIZE(ProxyArray<mco_ProcedureRealisation>));

            arr->var = wrenGetSlotHandle(vm, 0);
            procedure_arrays.Append(arr);
        }
    }
}

mco_FilterRunner::~mco_FilterRunner()
{
    if (wren) {
        delete wren;
    }
}

bool mco_FilterRunner::Init(const char *filter)
{
    // Newlines are significant in Wren
    Span<const char> filter2 = TrimStr(filter);

    // NOTE: We hack around the fact that Wren expressions cannot contain multiple
    // statements by turn filter into a function body when there are newlines. It's
    // not very elegant, but it does the work. An alternative would be to compile
    // filter as a script, and then to remove the popping bytecode at the end
    // to make sure the last expression value is available.

    filter_buf.Clear();
    if (memchr(filter2.ptr, '\n', filter2.len)) {
        Fmt(&filter_buf, "MCO.filter {\n%1\n}", filter2);
    } else {
        Fmt(&filter_buf, "%1", filter2);
    }

    return ResetRunner();
}

// XXX: Parallelize filtering, see old mco_Filter() API
bool mco_FilterRunner::Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                               HeapArray<const mco_Result *> *out_results,
                               HeapArray<const mco_Result *> *out_mono_results,
                               mco_StaySet *out_stay_set)
{
    K_DEFER_NC(out_guard, results_len = out_results->len,
                           mono_results_len = out_mono_results ? out_mono_results->len : 0,
                           stays_len = out_stay_set ? out_stay_set->stays.len : 0) {
        out_results->RemoveFrom(results_len);
        if (out_mono_results) {
            out_mono_results->RemoveFrom(mono_results_len);
        }
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

    out_guard.Disable();
    return true;
}

bool mco_FilterRunner::ResetRunner()
{
    if (wren) {
        delete wren;
    }

    wren = new mco_WrenRunner;
    wren_count = 16384;

    return wren->Init(filter_buf.ptr, wren_count);
}

}
