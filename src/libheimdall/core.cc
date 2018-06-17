// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "core.hh"
#include "data.hh"
#include "opengl.hh"
#include "render.hh"
#include "runner.hh"

// Ideas:
// - Multiple / Task-oriented concept trees
// - Magic shift, to filter concept under the cursor and pick and choose concepts in right panel
// - Negative coordinates
// - Cursor-centered zoom (needs negative coordinates first)
// - Relative time setting (use first period X, etc.)
// - Ctrl + click on element = instant zoom to pertinent level
// - One pixel mode (height 1 pixel) for dense view

enum class VisColor {
    Event,
    Alert,
    Plot,
    Limit
};

static ImU32 GetVisColor(VisColor color, float alpha = 1.0f)
{
    switch (color) {
        case VisColor::Event: { return ImGui::ColorConvertFloat4ToU32(ImVec4(0.100f, 0.400f, 0.750f, alpha)); } break;
        case VisColor::Alert: { return ImGui::ColorConvertFloat4ToU32(ImVec4(0.724f, 0.107f, 0.076f, alpha)); } break;
        case VisColor::Plot: { return ImGui::GetColorU32(ImGuiCol_PlotLines, alpha); } break;
        case VisColor::Limit: { return ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.7f, 0.03f, 0.4f * alpha)); } break;
    }
    DebugAssert(false);
}

static bool DetectAnomaly(const Element &elmt)
{
    switch (elmt.type) {
        case Element::Type::Event: { return false; } break;
        case Element::Type::Measure: {
            return ((!std::isnan(elmt.u.measure.min) && elmt.u.measure.value < elmt.u.measure.min) ||
                    (!std::isnan(elmt.u.measure.max) && elmt.u.measure.value > elmt.u.measure.max));
        } break;
        case Element::Type::Period: { return false; } break;
    }
    DebugAssert(false);
}

static void DrawPeriods(float x_offset, float y_min, float y_max, float time_zoom, float alpha,
                        Span<const Element *const> periods, double align_offset)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    for (const Element *elmt: periods) {
        DebugAssert(elmt->type == Element::Type::Period);

        ImRect rect {
            x_offset + (float)elmt->time * time_zoom, y_min,
            x_offset + (float)(elmt->time + elmt->u.period.duration) * time_zoom, y_max
        };
        // Make sure it's at least one pixel wide
        rect.Max.x = std::max(rect.Min.x + 1.0f, rect.Max.x);

        if (ImGui::ItemAdd(rect, 0)) {
            ImVec4 color = style.Colors[ImGuiCol_Border];
            color.w *= style.Alpha * alpha;

            draw->AddRectFilled(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(color));

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%g | %s [until %g]",
                            elmt->time - align_offset, elmt->concept,
                            elmt->time - align_offset + elmt->u.period.duration);
                ImGui::EndTooltip();
            }
        }
    }
}

static void TextMeasure(const Element &elmt, double align_offset)
{
    DebugAssert(elmt.type == Element::Type::Measure);

    DEFER_N_DISABLED(style_guard) {
        ImGui::PopStyleColor();
    };
    if (DetectAnomaly(elmt)) {
        ImGui::PushStyleColor(ImGuiCol_Text, GetVisColor(VisColor::Alert));
        style_guard.enable();
    }

    if (!std::isnan(elmt.u.measure.min) && !std::isnan(elmt.u.measure.max)) {
        ImGui::Text("%g | %s = %.2f [%.2f ; %.2f]",
                    elmt.time - align_offset, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.min, elmt.u.measure.max);
    } else if (!std::isnan(elmt.u.measure.min)) {
        ImGui::Text("%g | %s = %.2f [min = %.2f]",
                    elmt.time - align_offset, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.min);
    } else if (!std::isnan(elmt.u.measure.max)) {
        ImGui::Text("%g | %s = %.2f [max = %.2f]",
                    elmt.time - align_offset, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.max);
    } else {
        ImGui::Text("%g | %s = %.2f",
                    elmt.time - align_offset, elmt.concept, elmt.u.measure.value);
    }
}

static void DrawEventsBlock(ImRect rect, float alpha, Span<const Element *const> events,
                            double align_offset)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImRect bb {
        rect.Min.x - 10.0f, std::max(rect.Min.y, rect.Max.y - 20.0f),
        rect.Max.x + 10.0f, rect.Max.y
    };

    if (ImGui::ItemAdd(bb, 0)) {
        Size anomalies = 0;
        ImU32 color;
        for (const Element *elmt: events) {
            anomalies += DetectAnomaly(*elmt);
        }
        color = GetVisColor(anomalies ? VisColor::Alert : VisColor::Event, alpha);

        if (rect.GetWidth() >= 1.0f) {
            ImVec2 points[] = {
                { rect.Min.x, bb.Min.y },
                { rect.Max.x, bb.Min.y },
                { rect.Max.x + 10.0f, bb.Max.y },
                { rect.Min.x - 10.0f, bb.Max.y }
            };
            draw->AddConvexPolyFilled(points, ARRAY_SIZE(points), color);
        } else {
            ImVec2 points[] = {
                { rect.Min.x, bb.Min.y },
                { rect.Min.x + 10.0f, bb.Max.y },
                { rect.Min.x - 10.0f, bb.Max.y }
            };
            draw->AddTriangleFilled(points[0], points[1], points[2], color);
        }

        if (events.len > 1) {
            char len_str[32];
            Fmt(len_str, "%1", events.len);

            ImVec2 text_bb;
            {
                text_bb = bb.GetCenter();
                ImVec2 text_size = ImGui::CalcTextSize(len_str);
                text_bb.x -= text_size.x / 2.0f + 1.0f;
                text_bb.y -= text_size.y / 2.0f - 2.0f;
            }

            draw->AddText(text_bb, ImGui::GetColorU32(ImGuiCol_Text, alpha), len_str, nullptr);
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        for (const Element *elmt: events) {
            if (elmt->type == Element::Type::Measure) {
                TextMeasure(*elmt, align_offset);
            } else {
                ImGui::Text("%g | %s", elmt->time - align_offset, elmt->concept);
            }
        }
        ImGui::EndTooltip();
    }
}

static void DrawEvents(float x_offset, float y_min, float y_max, float time_zoom, float alpha,
                       Span<const Element *const> events, double align_offset)
{
    if (!events.len)
        return;

    ImRect rect {
        x_offset + ((float)events[0]->time * time_zoom), y_min,
        x_offset + ((float)events[0]->time * time_zoom), y_max
    };
    Size first_block_event = 0;
    for (Size i = 0; i < events.len; i++) {
        const Element *elmt = events[i];

        float event_pos = x_offset + ((float)elmt->time * time_zoom);
        if (event_pos - rect.Max.x >= 16.0f) {
            DrawEventsBlock(rect, alpha, events.Take(first_block_event, i - first_block_event),
                            align_offset);

            rect.Min.x = event_pos;
            first_block_event = i;
        }
        rect.Max.x = event_pos;
    }
    if (first_block_event < events.len) {
        DrawEventsBlock(rect, alpha, events.Take(first_block_event, events.len - first_block_event),
                        align_offset);
    }
}

template <typename Fun>
void DrawLine(InterpolationMode interpolation, Fun f)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    switch (interpolation) {
        case InterpolationMode::Linear: {
            ImU32 prev_color = 0;
            ImVec2 prev_point;
            f(0, &prev_point, &prev_color);

            for (Size i = 1;; i++) {
                ImU32 color = 0;
                ImVec2 point;
                if (!f(i, &point, &color))
                    break;

                if (LIKELY(!std::isnan(prev_point.y) && !std::isnan(point.y))) {
                    draw->AddLine(prev_point, point, prev_color, 1.0f);
                }

                prev_color = color;
                prev_point = point;
            }
        } break;

        case InterpolationMode::LOCF: {
            ImU32 prev_color = 0;
            ImVec2 prev_point;
            f(0, &prev_point, &prev_color);

            for (Size i = 1;; i++) {
                ImU32 color = 0;
                ImVec2 point;
                if (!f(i, &point, &color))
                    break;

                if (LIKELY(!std::isnan(prev_point.y) && !std::isnan(point.y))) {
                    ImVec2 points[] = {
                        prev_point,
                        ImVec2(point.x, prev_point.y),
                        point
                    };
                    draw->AddPolyline(points, ARRAY_SIZE(points), prev_color, false, 1.0f);
                }

                prev_color = color;
                prev_point = point;
            }
        } break;

        case InterpolationMode::Spline: {
            // TODO: Implement Akima spline interpolation
            // See http://www.iue.tuwien.ac.at/phd/rottinger/node60.html
        } break;

        case InterpolationMode::Disable: {
            // Name speaks for itself
        } break;
    }
}

static void DrawMeasures(float x_offset, float y_min, float y_max, float time_zoom, float alpha,
                         Span<const Element *const> measures, double align_offset,
                         double min, double max, InterpolationMode interpolation)
{
    if (!measures.len)
        return;
    DebugAssert(measures[0]->type == Element::Type::Measure);

    ImDrawList *draw = ImGui::GetWindowDrawList();

    float y_scaler;
    if (max > min) {
        y_scaler  = (y_max - y_min - 4.0f) / (float)(max - min);;
    } else {
        DebugAssert(!(min > max));
        y_max = (y_max + y_min) / 2.0f;
        y_scaler = 1.0f;
    }

    const auto ComputeCoordinates = [&](double time, double value) {
        return ImVec2 {
            x_offset + ((float)time * time_zoom),
            y_max - 4.0f - y_scaler * (float)(value - min)
        };
    };
    const auto GetColor = [&](const Element *elmt) {
        return DetectAnomaly(*elmt) ? GetVisColor(VisColor::Alert, alpha)
                                    : GetVisColor(VisColor::Plot, alpha);
    };

    // Draw limits
    DrawLine(interpolation, [&](Size i, ImVec2 *out_point, ImU32 *out_color) {
        if (i >= measures.len)
            return false;
        DebugAssert(measures[i]->type == Element::Type::Measure);
        if (!std::isnan(measures[i]->u.measure.min)) {
            *out_point = ComputeCoordinates(measures[i]->time, measures[i]->u.measure.min);
            *out_color = GetVisColor(VisColor::Limit, alpha);
        } else {
            out_point->y = NAN;
        }
        return true;
    });
    DrawLine(interpolation, [&](Size i, ImVec2 *out_point, ImU32 *out_color) {
        if (i >= measures.len)
            return false;
        if (!std::isnan(measures[i]->u.measure.max)) {
            *out_point = ComputeCoordinates(measures[i]->time, measures[i]->u.measure.max);
            *out_color = GetVisColor(VisColor::Limit, alpha);
        } else {
            out_point->y = NAN;
        }
        return true;
    });

    // Draw line
    DrawLine(interpolation, [&](Size i, ImVec2 *out_point, ImU32 *out_color) {
        if (i >= measures.len)
            return false;
        *out_point = ComputeCoordinates(measures[i]->time, measures[i]->u.measure.value);
        *out_color = GetColor(measures[i]);
        return true;
    });

    // Draw points
    for (const Element *elmt: measures) {
        ImU32 color = GetColor(elmt);
        ImVec2 point = ComputeCoordinates(elmt->time, elmt->u.measure.value);
        ImRect point_bb = {
            point.x - 3.0f, point.y - 3.0f,
            point.x + 3.0f, point.y + 3.0f
        };

        if (ImGui::ItemAdd(point_bb, 0)) {
            draw->AddCircleFilled(point, 3.0f, color);

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                TextMeasure(*elmt, align_offset);
                ImGui::EndTooltip();
            }
        }
    }
}

struct LineData {
    const Entity *entity;
    Span<const char> path;
    Span<const char> title;
    bool leaf;
    bool deployed;
    bool select;
    int depth;
    float text_alpha;
    float elements_alpha;
    float height;
    double align_offset;
    HeapArray<const Element *> elements;
};

enum class LineInteraction {
    None,
    Click,
    Select,
    Menu
};

static LineInteraction DrawLineFrame(ImRect bb, float tree_width, const LineData &line)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // Layout
    float y = (bb.Min.y + bb.Max.y) / 2.0f - 9.0f;
    ImVec2 text_size = ImGui::CalcTextSize(line.title.ptr, line.title.end());
    ImRect select_bb(bb.Min.x + 2.0f, y + 2.0f, bb.Min.x + 14.0f, y + 16.0f);
    ImRect deploy_bb(bb.Min.x + 16.0f + (float)line.depth * 12.0f - 3.0f, y,
                     bb.Min.x + 16.0f + (float)line.depth * 12.0f + 23.0f + text_size.x, y + 16.0f);
    ImRect full_bb(select_bb.Min.x, deploy_bb.Min.y, deploy_bb.Max.x, deploy_bb.Max.y);

    LineInteraction interaction = LineInteraction::None;

    // Select
    if (ImGui::ItemAdd(select_bb, 0) && line.select) {
        ImGui::RenderCheckMark(ImVec2(bb.Min.x + 2.0f, y + 2.0f),
                               ImGui::GetColorU32(ImGuiCol_CheckMark), 10.0f);
    }
    if (ImGui::IsItemClicked()) {
        interaction = LineInteraction::Select;
    }

    // Deploy
    if (ImGui::ItemAdd(deploy_bb, 0)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, line.text_alpha));
        DEFER { ImGui::PopStyleColor(1); };

        if (!line.leaf) {
            ImGui::RenderArrow(ImVec2(bb.Min.x + 16.0f + (float)line.depth * 12.0f, y),
                               line.deployed ? ImGuiDir_Down : ImGuiDir_Right);
        }

        ImVec4 text_rect {
            bb.Min.x + 16.0f + (float)line.depth * 12.0f + 20.0f, bb.Min.y,
            bb.Min.x + tree_width, bb.Max.y
        };
        draw->AddText(nullptr, 0.0f, ImVec2(text_rect.x, y),
                      ImGui::GetColorU32(ImGuiCol_Text),
                      line.title.ptr, line.title.end(), 0.0f, &text_rect);
    }
    if (!line.leaf && ImGui::IsItemClicked()) {
        interaction = LineInteraction::Click;
    }

    // Menu
    ImGui::ItemAdd(full_bb, 0);
    if (ImGui::IsItemClicked(1)) {
        interaction = LineInteraction::Menu;
    }

    // Support line
    if (ImGui::ItemAdd(bb, 0)) {
        const ImGuiStyle &style = ImGui::GetStyle();

        if (line.path == "/") {
            draw->AddLine(ImVec2(bb.Min.x, bb.Min.y - style.ItemSpacing.y + 1.0f),
                          ImVec2(bb.Max.x, bb.Min.y - style.ItemSpacing.y + 1.0f),
                          ImGui::GetColorU32(ImGuiCol_Separator));
        }

        draw->AddLine(ImVec2(bb.Min.x, bb.Max.y),
                      ImVec2(bb.Max.x, bb.Max.y),
                      ImGui::GetColorU32(ImGuiCol_Separator));
    }

    return interaction;
}

static void DrawLineElements(ImRect bb, float tree_width,
                             const InterfaceState &state, double time_offset, const LineData &line)
{
    if (line.elements_alpha == 0.0f)
        return;

    // Split elements
    HeapArray<const Element *> events;
    HeapArray<const Element *> periods;
    HeapArray<const Element *> measures;
    double measures_min = DBL_MAX, measures_max = -DBL_MAX;
    double min_min = DBL_MAX, max_max = -DBL_MAX;
    for (const Element *elmt: line.elements) {
        switch (elmt->type) {
            case Element::Type::Event: { events.Append(elmt); } break;
            case Element::Type::Measure: {
                if (line.leaf && state.settings.plot_measures) {
                    if (!std::isnan(elmt->u.measure.min)) {
                        min_min = std::min(min_min, elmt->u.measure.min);
                    }
                    if (!std::isnan(elmt->u.measure.max)) {
                        max_max = std::max(max_max, elmt->u.measure.max);
                    }
                    measures_min = std::min(measures_min, elmt->u.measure.value);
                    measures_max = std::max(measures_max, elmt->u.measure.value);
                    measures.Append(elmt);
                } else {
                    events.Append(elmt);
                }
            } break;
            case Element::Type::Period: { periods.Append(elmt); } break;
        }
    }

    if (min_min < max_max) {
        if (min_min < DBL_MAX && max_max > -DBL_MAX) {
            measures_min = std::min(min_min - (max_max - min_min) * 0.05, measures_min);
            measures_max = std::max(max_max + (max_max - min_min) * 0.05, measures_max);
        } else if (min_min < DBL_MAX) {
            measures_min = std::min(min_min - (measures_max - min_min) * 0.05, measures_min);
        } else {
            measures_max = std::max(max_max + (max_max - measures_min) * 0.05, measures_max);
        }
    }

    // Draw elements
    float x_offset = bb.Min.x + tree_width + 15.0f - (float)(time_offset * state.time_zoom);
    DrawPeriods(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha, periods,
                line.align_offset);
    DrawEvents(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha, events,
               line.align_offset);
    DrawMeasures(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha,
                 measures, line.align_offset, measures_min, measures_max, state.settings.interpolation);
}

static bool FindConceptAndAlign(const Entity &ent, const HashSet<Span<const char>> &align_concepts,
                                double *out_offset = nullptr)
{
    if (align_concepts.table.count) {
        for (const Element &elmt: ent.elements) {
            if (align_concepts.Find(elmt.concept)) {
                *out_offset = elmt.time;
                return true;
            }
        }
        return false;
    } else {
        *out_offset = 0.0;
        return true;
    }
}

static float ComputeElementHeight(const InterfaceSettings &settings, Element::Type type)
{
    if (settings.plot_measures && type == Element::Type::Measure) {
        return settings.plot_height;
    } else {
        return 20.0f;
    }
}

static ImRect ComputeEntitySize(const InterfaceState &state, const EntitySet &entity_set,
                                const ConceptSet *concept_set, const Entity &ent)
{
    ImGuiStyle &style = ImGui::GetStyle();

    HashMap<Span<const char>, float> line_heights;
    float min_x = 0.0f, max_x = 0.0f, height = 0.0f;

    double align_offset;
    if (FindConceptAndAlign(ent, state.align_concepts, &align_offset)) {
        for (const Element &elmt: ent.elements) {
            min_x = std::min((float)(elmt.time - align_offset), min_x);
            max_x = std::max((float)(elmt.time - align_offset), max_x);

            Span<const char> path;
            {
                if (elmt.concept[0] == '/') {
                    path = elmt.concept;
                    while (path.len > 1 && path.ptr[--path.len] != '/');
                } else if (concept_set) {
                    const Concept *concept = concept_set->concepts_map.Find(elmt.concept);
                    if (!concept) {
                        const char *src_name = *entity_set.sources.Find(elmt.source_id);
                        concept = concept_set->concepts_map.Find(src_name);
                        if (!concept)
                            continue;
                    }
                    path = concept->path;
                } else {
                    continue;
                }
            }
            DebugAssert(path.len > 0);

            bool fully_deployed = false;
            {
                Span<const char> partial_path = {path.ptr, 1};
                for (;;) {
                    height += line_heights.Append(partial_path, 20.0f).second *
                              (20.0f + style.ItemSpacing.y);
                    fully_deployed = state.deploy_paths.Find(partial_path);

                    if (!fully_deployed || partial_path.len == path.len)
                        break;
                    while (++partial_path.len < path.len && partial_path.ptr[partial_path.len] != '/');
                }
            }

            if (fully_deployed) {
                float new_height = ComputeElementHeight(state.settings, elmt.type) + style.ItemSpacing.y;
                std::pair<float *, bool> ret = line_heights.Append(elmt.concept, 0.0f);
                if (new_height > *ret.first) {
                    height += new_height - *ret.first;
                    *ret.first = new_height;
                }
            }
        }
    }

    return ImRect(min_x, 0.0f, max_x, height);
}

static bool DrawEntities(ImRect bb, float tree_width, double time_offset,
                         InterfaceState &state, const EntitySet &entity_set,
                         const ConceptSet *concept_set)
{
    if (!entity_set.entities.len)
        return true;

    const ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindow *win = ImGui::GetCurrentWindow();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(bb.Min, bb.Max);
    DEFER { draw->PopClipRect(); };

    bool cache_refreshed = false;
    if (!state.size_cache_valid ||
            state.lines_top.len != entity_set.entities.len ||
            state.prev_concept_set != concept_set) {
        state.minimum_x_unscaled = 0.0f;
        state.total_width_unscaled = 0.0f;
        state.total_height = 0.5f;

        state.lines_top.SetCapacity(entity_set.entities.len);
        state.lines_top.len = entity_set.entities.len;
        for (Size i = 0; i < state.scroll_to_idx; i++) {
            state.lines_top[i] = state.total_height;

            ImRect ent_size = ComputeEntitySize(state, entity_set, concept_set,
                                                entity_set.entities[i]);
            state.minimum_x_unscaled = std::min(state.minimum_x_unscaled, ent_size.Min.x);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.Max.x);
            state.total_height += ent_size.Max.y;
        }
        state.scroll_y = state.total_height - state.scroll_offset_y;
        for (Size i = state.scroll_to_idx; i < entity_set.entities.len; i++) {
            state.lines_top[i] = state.total_height;

            ImRect ent_size = ComputeEntitySize(state, entity_set, concept_set,
                                                entity_set.entities[i]);
            state.minimum_x_unscaled = std::min(state.minimum_x_unscaled, ent_size.Min.x);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.Max.x);
            state.total_height += ent_size.Max.y;
        }

        state.prev_concept_set = concept_set;
        state.size_cache_valid = true;
        cache_refreshed = true;
    }

    Size render_idx = entity_set.entities.len;
    float render_offset = 0.0f;
    for (Size i = 1; i < state.lines_top.len; i++) {
        if (state.lines_top[i] >= state.scroll_y) {
            if (!cache_refreshed) {
                state.scroll_to_idx = i;
                state.scroll_offset_y = state.lines_top[i] - state.scroll_y;
            }
            render_idx = i - 1;
            render_offset = state.lines_top[i - 1] + style.ItemSpacing.y - state.scroll_y;
            break;
        }
    }

    HeapArray<LineData> lines;
    {
        float base_y = render_offset;
        float y = base_y;
        for (Size i = render_idx; i < entity_set.entities.len &&
                                  y < win->ClipRect.Max.y; i++) {
            const Entity &ent = entity_set.entities[i];

            double align_offset = 0.0f;
            if (!FindConceptAndAlign(ent, state.align_concepts, &align_offset))
                continue;

            Size prev_lines_len = lines.len;
            HashMap<Span<const char>, Size> lines_map;

            for (const Element &elmt: ent.elements) {
                Span<const char> path;
                Span<const char> title;
                {
                    title = elmt.concept;
                    if (elmt.concept[0] == '/') {
                        path = title;
                        // FIXME: Check name does not end with '/'
                        while (path.len > 1 && path.ptr[--path.len] != '/');
                        title.ptr += path.len + 1;
                        title.len -= path.len + 1;
                    } else if (concept_set) {
                        const Concept *concept = concept_set->concepts_map.Find(elmt.concept);
                        if (!concept) {
                            const char *src_name = *entity_set.sources.Find(elmt.source_id);
                            concept = concept_set->concepts_map.Find(src_name);
                            if (!concept)
                                continue;
                        }
                        path = concept->path;
                    } else {
                        continue;
                    }
                }
                DebugAssert(path.len > 0);

                bool fully_deployed = false;
                int tree_depth = 0;
                {
                    Size name_offset = 1;
                    Span<const char> partial_path = {path.ptr, 1};
                    for (;;) {
                        LineData *line;
                        {
                            std::pair<Size *, bool> ret = lines_map.Append(partial_path, lines.len);
                            if (!ret.second) {
                                line = &lines[*ret.first];
                                tree_depth = line->depth + 1;
                            } else {
                                line = lines.AppendDefault();
                                line->entity = &ent;
                                line->path = partial_path;
                                if (partial_path.len > 1) {
                                    line->title = MakeSpan(partial_path.ptr + name_offset,
                                                           partial_path.len - name_offset);
                                } else {
                                    line->title = ent.id;
                                }
                                line->leaf = false;
                                line->deployed = state.deploy_paths.Find(partial_path);
                                line->depth = tree_depth++;
                                line->text_alpha = 1.0f;
                                line->elements_alpha = line->deployed ? state.settings.deployed_alpha : 1.0f;
                                line->height = 20.0f;
                                line->align_offset = align_offset;
                                y += line->height + style.ItemSpacing.y;
                            }
                            fully_deployed = line->deployed;
                        }
                        line->elements.Append(&elmt);

                        if (!fully_deployed || partial_path.len == path.len)
                            break;
                        name_offset = partial_path.len + (partial_path.len > 1);
                        while (++partial_path.len < path.len && partial_path.ptr[partial_path.len] != '/');
                    }
                }

                if (fully_deployed) {
                    LineData *line;
                    {
                        std::pair<Size *, bool> ret = lines_map.Append(elmt.concept, lines.len);
                        if (!ret.second) {
                            line = &lines[*ret.first];
                        } else {
                            line = lines.AppendDefault();
                            line->entity = &ent;
                            line->path = path;
                            line->title = title;
                            line->leaf = true;
                            line->depth = tree_depth;
                            line->select = state.select_concepts.Find(title);
                            line->text_alpha = 1.0f;
                            line->elements_alpha = 1.0f;
                            line->height = 0.0f;
                            line->align_offset = align_offset;
                            y += style.ItemSpacing.y;
                        }

                        float new_height = ComputeElementHeight(state.settings, elmt.type);
                        if (new_height > line->height) {
                            y += new_height - line->height;
                            line->height = new_height;
                        }
                    }
                    line->elements.Append(&elmt);
                }
            }

            // Try to stabilize highlighted entity if any
            if (g_io->input.mouseover && !state.grab_canvas && !cache_refreshed &&
                    g_io->input.y >= base_y && g_io->input.y < y && !ImGui::IsPopupOpen("tree_menu")) {
                state.highlight_idx = i;
                state.scroll_to_idx = i;
                state.scroll_offset_y = base_y - style.ItemSpacing.y;
            }
            if (i != state.highlight_idx && state.settings.highlight_current) {
                for (Size j = prev_lines_len; j < lines.len; j++) {
                    lines[j].text_alpha *= 0.05f;
                    lines[j].elements_alpha *= 0.05f;
                }
            }

            base_y = y;
        }
    }

    std::sort(lines.begin(), lines.end(),
              [](const LineData &line1, const LineData &line2) {
        return MultiCmp((int)(line1.entity - line2.entity),
                        CmpStr(line1.path, line2.path),
                        (int)line1.leaf - (int)line2.leaf,
                        CmpStr(line1.title, line2.title)) < 0;
    });

    // Draw elements
    {
        draw->PushClipRect(ImVec2(win->ClipRect.Min.x + tree_width, win->ClipRect.Min.y),
                           win->ClipRect.Max, true);
        DEFER { draw->PopClipRect(); };

        float y = render_offset;
        for (const LineData &line: lines) {
            ImRect bb(win->ClipRect.Min.x, y + style.ItemSpacing.y,
                      win->ClipRect.Max.x, y + style.ItemSpacing.y + line.height);
            DrawLineElements(bb, tree_width, state, time_offset + line.align_offset, line);
            y = bb.Max.y;
        }
    }

    // Draw frames (header, support line)
    Span<const char> deploy_path = {};
    Span<const char> select_concept = {};
    {
        const Entity *ent = nullptr;
        float ent_offset_y = 0.0f;

        float y = render_offset;
        for (Size i = 0; i < lines.len && y < win->ClipRect.Max.y; i++) {
            const LineData &line = lines[i];

            if (ent != line.entity) {
                ent = line.entity;
                ent_offset_y = y;
            }

            ImRect bb(win->ClipRect.Min.x, y + style.ItemSpacing.y,
                      win->ClipRect.Max.x, y + style.ItemSpacing.y + line.height);
            LineInteraction interaction = DrawLineFrame(bb, tree_width, line);

            switch (interaction) {
                case LineInteraction::None: {} break;

                case LineInteraction::Click: {
                    state.scroll_to_idx = ent - entity_set.entities.ptr;
                    state.scroll_offset_y = ent_offset_y - style.ItemSpacing.y;
                    deploy_path = line.path;
                } break;

                case LineInteraction::Select: {
                    select_concept = line.title;
                } break;

                case LineInteraction::Menu: {
                    if (!line.select) {
                        select_concept = line.title;
                    }
                    ImGui::OpenPopup("tree_menu");
                } break;
            }

            y = bb.Max.y;
        }
    }

    if (deploy_path.len) {
        std::pair<Span<const char> *, bool> ret = state.deploy_paths.Append(deploy_path);
        if (!ret.second) {
            state.deploy_paths.Remove(ret.first);
        }

        state.size_cache_valid = false;
    } else if (select_concept.len) {
        std::pair<Span<const char> *, bool> ret = state.select_concepts.Append(select_concept);
        if (!ret.second) {
            state.select_concepts.Remove(ret.first);
        }
    }

    return !cache_refreshed;
}

static void DrawTimeScale(ImRect bb, double time_offset, float time_zoom, float grid_alpha,
                          bool highlight_zero)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    int precision = (int)log10f(1.0f / time_step);
    float min_text_delta = 25.0f + 10.0f * fabsf(log10f(1.0f / time_step));

    // TODO: Avoid overdraw (left of screen)
    float x = bb.Min.x - (float)time_offset * time_zoom;
    float time = 0.0f;
    {
        int test = (int)std::ceil(min_text_delta / (time_step * time_zoom));
        while (x > bb.Min.x) {
            x -= time_step * time_zoom * test;
            time -= time_step * test;
        }
    }

    float prev_text_x = x - min_text_delta - 1.0f;
    while (x < bb.Max.x + 30.0f) {
        bool show_text = false;
        if (x - prev_text_x >= min_text_delta) {
            show_text = true;
            prev_text_x = x;
        }

        if (x >= bb.Min.x) {
            float x_exact = round(x);

            if (show_text) {
                draw->AddLine(ImVec2(x_exact, bb.Min.y + 2.0f), ImVec2(x_exact, bb.Max.y - ImGui::GetFontSize() - 4.0f),
                              ImGui::GetColorU32(ImGuiCol_Text));
                if (grid_alpha > 0.0f) {
                    if (highlight_zero && std::abs(time) < 0.00001) {
                        draw->AddLine(ImVec2(x_exact, 0.0f), ImVec2(x_exact, bb.Min.y + 2.0f),
                                      GetVisColor(VisColor::Limit, 0.7f));
                    } else {
                        draw->AddLine(ImVec2(x_exact, 0.0f), ImVec2(x_exact, bb.Min.y + 2.0f),
                                      ImGui::GetColorU32(ImGuiCol_Text, grid_alpha));
                    }
                }

                char time_str[32];
                ImVec2 text_size;
                if (std::abs(time) < 0.000001) {
                    Fmt(time_str, "%1", FmtDouble(0.0, precision));
                } else {
                    Fmt(time_str, "%1", FmtDouble(time, precision));
                }
                text_size = ImGui::CalcTextSize(time_str);

                draw->AddText(ImVec2(x - text_size.x / 2.0f, bb.Max.y - ImGui::GetFontSize() - 2.0f),
                              ImGui::GetColorU32(ImGuiCol_Text), time_str);
            } else {
                draw->AddLine(ImVec2(x_exact, bb.Min.y + 2.0f), ImVec2(x_exact, bb.Max.y - ImGui::GetFontSize() - 8.0f),
                              ImGui::GetColorU32(ImGuiCol_Text));
                if (grid_alpha > 0.0f) {
                    draw->AddLine(ImVec2(x_exact, 0.0f), ImVec2(x_exact, bb.Min.y + 2.0f),
                                  ImGui::GetColorU32(ImGuiCol_Text, grid_alpha * 0.5f));
                }
            }
        }

        x += time_step * time_zoom;
        time += time_step;
    }
}

static float AdjustScrollAfterZoom(float stable_x, double prev_zoom, double new_zoom)
{
    double stable_time = stable_x / prev_zoom;
    return (float)(stable_time * (new_zoom - prev_zoom));
}

static bool DrawView(InterfaceState &state,
                     const EntitySet &entity_set, const ConceptSet *concept_set)
{
    ImGuiWindow *win = ImGui::GetCurrentWindow();

    // Layout
    float scale_height = 16.0f + ImGui::GetFontSize();
    ImRect scale_rect = win->ClipRect;
    ImRect entity_rect = win->ClipRect;
    scale_rect.Min.x = ImMin(scale_rect.Min.x + state.settings.tree_width + 15.0f, scale_rect.Max.x);
    scale_rect.Min.y = ImMin(scale_rect.Max.y - scale_height, scale_rect.Max.y);
    entity_rect.Max.y -= scale_height;

    // Copy ImGui scroll changes
    /*float prev_scroll_y = state.scroll_y;
    state.scroll_x = ImGui::GetScrollX() + state.imgui_delta_x;
    state.scroll_y = ImGui::GetScrollY() + state.imgui_delta_y;*/

    // Handle controls
    float entities_mouse_x = (state.scroll_x + (float)g_io->input.x - win->ClipRect.Min.x - (state.settings.tree_width + 15.0f));
    if (ImGui::IsMouseHoveringWindow()) {
        if (g_io->input.buttons & MaskEnum(RunIO::Button::Left)) {
            if (state.grab_canvas) {
                state.scroll_x += state.grab_canvas_x - (float)g_io->input.x;
                state.scroll_y += state.grab_canvas_y - (float)g_io->input.y;
            } else if (entity_rect.Contains(ImVec2((float)g_io->input.x, (float)g_io->input.y))) {
                state.grab_canvas = true;
            }

            state.grab_canvas_x = (float)g_io->input.x;
            state.grab_canvas_y = (float)g_io->input.y;
        } else {
            state.grab_canvas = false;
        }

        if (g_io->input.keys.Test((int)RunIO::Key::Control) && g_io->input.wheel_y) {
            double (*animator)(double relative_time) = nullptr;
            if (state.time_zoom.animation.Running(g_io->time.monotonic)) {
                state.scroll_x += AdjustScrollAfterZoom(entities_mouse_x, state.time_zoom, state.time_zoom.end_value);
                state.time_zoom = state.time_zoom.end_value;
                animator = TweenOutQuad;
            } else {
                animator = TweenInOutQuad;
            }

            float new_zoom;
            {
                float multiplier = ((g_io->input.keys.Test((int)RunIO::Key::Shift)) ? 2.0736f : 1.2f);
                if (g_io->input.wheel_y > 0) {
                    new_zoom = state.time_zoom * (float)g_io->input.wheel_y * multiplier;
                } else {
                    new_zoom = state.time_zoom / -(float)g_io->input.wheel_y / multiplier;
                }
                new_zoom = ImClamp(new_zoom, 0.00001f, 1000000.0f);
            }

            state.time_zoom = MakeAnimatedValue(state.time_zoom, new_zoom, g_io->time.monotonic,
                                                g_io->time.monotonic + 0.05, animator);
        }
    }

    // Update time zoom
    double time_offset;
    {
        double prev_zoom = state.time_zoom;
        state.time_zoom.Update(g_io->time.monotonic);
        state.scroll_x += AdjustScrollAfterZoom(entities_mouse_x, prev_zoom, state.time_zoom);

        time_offset = state.scroll_x / state.time_zoom;
    }

    // Render time scale and entities
    bool valid_frame;
    {
        DrawTimeScale(scale_rect, time_offset, state.time_zoom, state.settings.grid_alpha,
                      state.align_concepts.table.count);
        valid_frame = DrawEntities(entity_rect, state.settings.tree_width, time_offset,
                                   state, entity_set, concept_set);
    }

#if 1
    // Scrollbar limits
    {
        /*float min_x = std::min((float)(state.minimum_x_unscaled * state.time_zoom), scroll_x);
        float max_x = std::max((float)(state.total_width_unscaled * state.time_zoom) + state.settings.tree_width + 20.0f, scroll_x) +
                      win->ClipRect.Max.x - 4.0f;

        state.extra_scroll_left = std::max(0.0f, -min_x);
        state.extra_scroll_right = std::max(0.0f, max_x);

        float width = state.settings.tree_width + 20.0f +
                      state.extra_scroll_left + state.extra_scroll_right +
                      state.total_width_unscaled * (float)state.time_zoom;

        ImGui::SetCursorPos(ImVec2(width, state.total_height + scale_height));*/

        state.imgui_delta_x = std::min(0.0f, state.minimum_x_unscaled * state.time_zoom);
        state.imgui_delta_x = std::min(state.imgui_delta_x, state.scroll_x);
        state.imgui_delta_y = std::min(0.0f, state.scroll_y);

        float width = (state.total_width_unscaled - state.minimum_x_unscaled) * (float)state.time_zoom;
        width += std::max(state.minimum_x_unscaled * (float)state.time_zoom - state.scroll_x, 0.0f);

        float height = state.total_height;
        height += std::max(-state.scroll_y, 0.0f);

        ImGui::SetCursorPos(ImVec2(state.settings.tree_width + 20.0f + width, height + scale_height));
    }
#else
    state.scroll_x_delta = std::min(state.minimum_x_unscaled * state.time_zoom, scroll_x);
    // state.scroll_x_expand = std::max(0.0f, state.scroll_x_expand + scroll_x - state.scroll_x_delta - ImGui::GetScrollX());
    {
        float max_x = state.settings.tree_width + 20.0f + (state.total_width_unscaled - state.minimum_x_unscaled) * (float)state.time_zoom;
        max_x = std::max(max_x, win->ClipRect.Max.x - 4.0f - state.scroll_x_delta);
        //max_x = std::max(max_x, win->ClipRect.Max.x - 4.0f + state.scroll_x_expand);
        /*float max_x = std::max(win->ClipRect.Max.x - 4.0f - state.scroll_x_delta,
                               state.settings.tree_width + 20.0f + state.total_width_unscaled * (float)state.time_zoom);*/
        ImGui::SetCursorPos(ImVec2(max_x, state.total_height + scale_height));
    }
#endif
    ImGui::ItemSize(ImVec2(0.0f, 0.0f));

    // Sync scroll state
    /*ImGui::SetScrollX(state.scroll_x + state.imgui_delta_x);
    if (state.scroll_y != prev_scroll_y) {
        ImGui::SetScrollY(state.scroll_y);
    }*/

    return valid_frame;
}

bool Step(InterfaceState &state, const EntitySet &entity_set, Span<const ConceptSet> concept_sets)
{
    if (!StartRender())
        return false;

    // Menu
    float menu_height = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        ImGui::PushItemWidth(100.0f);
        ImGui::ShowStyleSelector("##StyleSelector");
        if (ImGui::Checkbox("Highlight entity", &state.settings.highlight_current)) {
            state.new_settings.highlight_current = state.settings.highlight_current;
        }
        ImGui::Checkbox("Other settings", &state.show_settings);

        ImGui::Text("             Framerate: %.1f (%.3f ms/frame)             ",
                    ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

        ImGui::Combo("Views", &state.concept_set_idx,
                     [](void *udata, int idx, const char **out_text) {
            Span<const ConceptSet> &concept_sets = *(Span<const ConceptSet> *)udata;
            *out_text = concept_sets[idx].name;
            return true;
        }, &concept_sets, (int)concept_sets.len);

        menu_height = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    // Main view
    bool valid_frame;
    {
        ImVec2 view_pos = ImVec2(0, menu_height);
        ImVec2 view_size = ImGui::GetIO().DisplaySize;
        view_size.y -= menu_height;
        ImGuiWindowFlags view_flags = ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_HorizontalScrollbar |
                                      ImGuiWindowFlags_AlwaysHorizontalScrollbar;
        ImGui::SetNextWindowPos(view_pos);
        ImGui::SetNextWindowSize(view_size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        DEFER { ImGui::PopStyleVar(1); };

        ImGui::Begin("View", nullptr, view_flags);
        {
            const ConceptSet *concept_set = nullptr;
            if (state.concept_set_idx >= 0 && state.concept_set_idx < concept_sets.len) {
                concept_set = &concept_sets[state.concept_set_idx];
            }
            valid_frame = DrawView(state, entity_set, concept_set);
        }

        if (ImGui::BeginPopup("tree_menu")) {
            if (ImGui::MenuItem("Align", nullptr, state.align_concepts.table.count)) {
                if (state.align_concepts.table.count) {
                    state.align_concepts.Clear();
                } else {
                    // FIXME: Add iterator to HashTable and related containers
                    for (Size i = 0; i < state.select_concepts.table.capacity; i++) {
                        if (!state.select_concepts.table.IsEmpty(i)) {
                            Span<const char> concept = state.select_concepts.table.data[i].value;
                            state.align_concepts.Append(concept);
                        }
                    }
                }
                state.size_cache_valid = false;
            }
            ImGui::Separator();
            ImGui::MenuItem("Create View");
            if (ImGui::BeginMenu("Add to view")) {
                for (const ConceptSet &concept_set: concept_sets) {
                    ImGui::MenuItem(concept_set.name);
                }
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Remove from view");
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    // Settings
    if (state.show_settings) {
        ImGui::Begin("Settings", &state.show_settings);

        if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat("Tree width", &state.new_settings.tree_width, 100.0f, 400.0f);
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat("Plot height", &state.new_settings.plot_height, 20.0f, 100.0f);
        }
        if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat("Grid opacity", &state.new_settings.grid_alpha, 0.0f, 1.0f);
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat("Parent opacity", &state.new_settings.deployed_alpha, 0.0f, 1.0f);
            ImGui::Checkbox("Highlight entity", &state.new_settings.highlight_current);
        }
        if (ImGui::CollapsingHeader("Plots", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Draw plots", &state.new_settings.plot_measures);
            ImGui::Combo("Interpolation", (int *)&state.new_settings.interpolation,
                     interpolation_mode_names, ARRAY_SIZE(interpolation_mode_names));
        }

        if (ImGui::Button("Apply")) {
            state.size_cache_valid &= !(state.new_settings.plot_height != state.settings.plot_height ||
                                        state.new_settings.plot_measures != state.settings.plot_measures);
            state.settings = state.new_settings;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.new_settings = state.settings;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            state.new_settings = InterfaceSettings();
            state.size_cache_valid &= !(state.new_settings.plot_height != state.settings.plot_height ||
                                        state.new_settings.plot_measures != state.settings.plot_measures);
            state.settings = state.new_settings;
        }

        ImGui::End();
    }

    Render();
    // FIXME: This is a hack to work around the fact that ImGui::SetScroll() functions
    // are off by one frame. We need to take over ImGui layout completely, because we
    // do know the window size!
    if (valid_frame) {
        SwapGLBuffers();
    }

    if (!g_io->main.run) {
        ReleaseRender();
    }

    return true;
}
