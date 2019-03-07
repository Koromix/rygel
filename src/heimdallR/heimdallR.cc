// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libheimdall/libheimdall.hh"
#include "../wrappers/Rcc.hh"
#include <thread>

struct Instance {
    ~Instance();

    EntitySet entity_set;
    int last_source_id = 0;

    HeapArray<ConceptSet> concept_sets;

    bool run = false;
    std::thread run_thread;
    std::mutex lock;
};

// [[Rcpp::export(name = 'heimdall')]]
SEXP R_Heimdall()
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = new Instance;
    DEFER_N(inst_guard) { delete inst; };

    inst_guard.disable();
    return Rcpp::XPtr<Instance>(inst, true);
}

template <typename Fun>
int AddElements(Instance *inst, const Rcpp::String &source, Rcpp::DataFrame values_df,
                Rcpp::CharacterVector keys, Fun func)
{
    // FIXME: Use guard to restore stuff in case of error

    std::lock_guard<std::mutex> lock(inst->lock);

    struct {
        Rcpp::CharacterVector entity;
        Rcpp::CharacterVector concept;
        Rcpp::NumericVector time;
    } values;
    values.entity = values_df[(const char *)keys["entity"]];
    values.concept = values_df[(const char *)keys["concept"]];
    values.time = values_df[(const char *)keys["time"]];

    inst->last_source_id++;
    {
        const char *src_name = DuplicateString(source.get_cstring(), &inst->entity_set.str_alloc).ptr;
        inst->entity_set.sources.Append(inst->last_source_id, src_name);
    }

    HashMap<const char *, Size> entities_map;
    for (Size i = 0; i < inst->entity_set.entities.len; i++) {
        const Entity &ent = inst->entity_set.entities[i];
        entities_map.Append(ent.id, i);
    }

    for (Size i = 0; i < values_df.nrow(); i++) {
        Entity *entity;
        {
            Size idx = entities_map.FindValue(values.entity[i], -1);
            if (idx == -1) {
                entity = inst->entity_set.entities.AppendDefault();
                entity->id = DuplicateString((const char *)values.entity[i], &inst->entity_set.str_alloc).ptr;

                entities_map.Append(entity->id, inst->entity_set.entities.len - 1);
            } else {
                entity = &inst->entity_set.entities[idx];
            }
        }

        Element elmt;
        elmt.source_id = inst->last_source_id;
        elmt.concept = DuplicateString((const char *)values.concept[i], &inst->entity_set.str_alloc).ptr;
        elmt.time = values.time[i];
        func(elmt, i);
        entity->elements.Append(elmt);
    }

    // TODO: Delay sort (diry flag, sort in run). Might also want to only sort
    // elements in changed entities.
    std::sort(inst->entity_set.entities.begin(), inst->entity_set.entities.end(),
              [](const Entity &ent1, const Entity &ent2) { return strcmp(ent1.id, ent2.id) < 0; });
    for (Entity &entity: inst->entity_set.entities) {
        std::stable_sort(entity.elements.begin(), entity.elements.end(),
                         [](const Element &elmt1, const Element &elmt2) {
            return elmt1.time < elmt2.time;
        });
    }

    return inst->last_source_id;
}

// [[Rcpp::export(name = 'heimdall.add_events')]]
void R_HeimdallAddEvents(SEXP inst_xp, Rcpp::String source, Rcpp::DataFrame values_df,
                         Rcpp::CharacterVector keys)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    AddElements(inst, source, values_df, keys, [&](Element &elmt, Size) {
        elmt.type = Element::Type::Event;
    });
}

// [[Rcpp::export(name = 'heimdall.add_measures')]]
void R_HeimdallAddMeasures(SEXP inst_xp, Rcpp::String source, Rcpp::DataFrame values_df,
                           Rcpp::CharacterVector keys)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    struct {
        Rcpp::NumericVector value;
        Rcpp::NumericVector min;
        Rcpp::NumericVector max;
    } values;
    values.value = values_df[(const char *)keys["value"]];
    if (keys.containsElementNamed("min")) {
        values.min = values_df[(const char *)keys["min"]];
        values.max = values_df[(const char *)keys["max"]];
    }

    AddElements(inst, source, values_df, keys, [&](Element &elmt, Size i) {
        elmt.type = Element::Type::Measure;
        elmt.u.measure.value = values.value[i];
        if (values.min.size()) {
            elmt.u.measure.min = values.min[i];
            elmt.u.measure.max = values.max[i];
        } else {
            elmt.u.measure.min = NAN;
            elmt.u.measure.max = NAN;
        }
    });
}

// [[Rcpp::export(name = 'heimdall.add_periods')]]
void R_HeimdallAddPeriods(SEXP inst_xp, Rcpp::String source, Rcpp::DataFrame periods_df,
                          Rcpp::CharacterVector keys)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    struct {
        Rcpp::NumericVector duration;
    } periods;
    periods.duration = periods_df[(const char *)keys["duration"]];

    AddElements(inst, source, periods_df, keys, [&](Element &elmt, Size i) {
        elmt.type = Element::Type::Period;
        elmt.u.period.duration = periods.duration[i];
        if (std::isnan(elmt.u.period.duration) || elmt.u.period.duration < 0.0)
            Rcpp::stop("Duration must be zero or a positive number");
    });
}

// [[Rcpp::export(name = 'heimdall.set_concepts')]]
void R_HeimdallSetConcepts(SEXP inst_xp, std::string name, Rcpp::DataFrame concepts_df)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    struct {
        Rcpp::CharacterVector name;
        Rcpp::CharacterVector path;
    } concepts;
    concepts.name = concepts_df["name"];
    concepts.path = concepts_df["path"];

    ConceptSet *concept_set = nullptr;
    for (Size i = 0; i < inst->concept_sets.len; i++) {
        if (inst->concept_sets[i].name == name) {
            concept_set = &inst->concept_sets[i];
            break;
        }
    }

    if (concept_set) {
        concept_set->paths.Clear();
        concept_set->paths_set.Clear();
        concept_set->concepts_map.Clear();
        concept_set->str_alloc.ReleaseAll();
    } else {
        concept_set = inst->concept_sets.AppendDefault();
    }
    concept_set->name = DuplicateString(name.c_str(), &concept_set->str_alloc).ptr;

    for (Size i = 0; i < concepts_df.nrow(); i++) {
        if (((const char *)concepts.path[i])[0] != '/')
            Rcpp::stop("Paths must start with '/'");

        const char *path = concept_set->paths_set.FindValue(concepts.path[i], nullptr);
        if (!path) {
            path = DuplicateString((const char *)concepts.path[i], &inst->entity_set.str_alloc).ptr;
            concept_set->paths.Append(path);
            concept_set->paths_set.Append(path);
        }

        Concept concept;
        concept.name = DuplicateString((const char *)concepts.name[i], &inst->entity_set.str_alloc).ptr;
        concept.path = path;
        if (!concept_set->concepts_map.Append(concept).second) {
            LogError("Concept '%1' already exists", concept.name);
        }
    }
}

// [[Rcpp::export(name = 'heimdall.run')]]
void R_HeimdallRun(SEXP inst_xp)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    if (!inst->run) {
        // Previous instance is done (or it is shutting down, just wait for a bit)
        if (inst->run_thread.joinable()) {
            inst->run_thread.join();
        }

        inst->run = true;
        inst->run_thread = std::thread([=]() {
            DEFER { inst->run = false; };

            gui_Window window;
            if (!window.Init(HEIMDALL_NAME))
                rcc_StopWithLastError();
            if (!window.InitImGui())
                rcc_StopWithLastError();

            InterfaceState render_state = {};

            while (inst->run) {
                if (!window.Prepare())
                    break;

                std::lock_guard<std::mutex> locker(inst->lock);
                if (!StepHeimdall(window, render_state, inst->concept_sets, inst->entity_set))
                    break;
            }
        });
    }
}

// [[Rcpp::export(name = 'heimdall.run_sync')]]
void R_HeimdallRunSync(SEXP inst_xp)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);

    if (inst->run)
        Rcpp::stop("Async run in progress");

    gui_Window window;
    if (!window.Init(HEIMDALL_NAME))
        rcc_StopWithLastError();
    if (!window.InitImGui())
        rcc_StopWithLastError();

    InterfaceState render_state = {};

    for (;;) {
        if (!window.Prepare())
            break;
        if (!StepHeimdall(window, render_state, inst->concept_sets, inst->entity_set))
            break;
    }
}

static void StopInstance(Instance *inst)
{
    if (inst->run_thread.joinable()) {
        inst->run = false;
        inst->run_thread.join();
    }
}
Instance::~Instance() { /* StopInstance(this); */ }

// [[Rcpp::export(name = 'heimdall.stop')]]
void R_HeimdallStop(SEXP inst_xp)
{
    RCC_SETUP_LOG_HANDLER();

    Instance *inst = (Instance *)rcc_GetPointerSafe(inst_xp);
    StopInstance(inst);
}
