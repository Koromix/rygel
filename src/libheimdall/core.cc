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
            return ((!isnan(elmt.u.measure.min) && elmt.u.measure.value < elmt.u.measure.min) ||
                    (!isnan(elmt.u.measure.max) && elmt.u.measure.value > elmt.u.measure.max));
        } break;
        case Element::Type::Period: { return false; } break;
    }
    DebugAssert(false);
}

static void DrawPeriods(float x_offset, float y_min, float y_max, float time_zoom, float alpha,
                        Span<const Element *const> periods)
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
                ImGui::Text("%g | %s [until %g]", elmt->time, elmt->concept, elmt->time + elmt->u.period.duration);
                ImGui::EndTooltip();
            }
        }
    }
}

static void TextMeasure(const Element &elmt)
{
    DebugAssert(elmt.type == Element::Type::Measure);

    DEFER_N_DISABLED(style_guard) {
        ImGui::PopStyleColor();
    };
    if (DetectAnomaly(elmt)) {
        ImGui::PushStyleColor(ImGuiCol_Text, GetVisColor(VisColor::Alert));
        style_guard.enable();
    }

    if (!isnan(elmt.u.measure.min) && !isnan(elmt.u.measure.max)) {
        ImGui::Text("%g | %s = %.2f [%.2f ; %.2f]",
                    elmt.time, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.min, elmt.u.measure.max);
    } else if (!isnan(elmt.u.measure.min)) {
        ImGui::Text("%g | %s = %.2f [min = %.2f]",
                    elmt.time, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.min);
    } else if (!isnan(elmt.u.measure.max)) {
        ImGui::Text("%g | %s = %.2f [max = %.2f]",
                    elmt.time, elmt.concept, elmt.u.measure.value,
                    elmt.u.measure.max);
    } else {
        ImGui::Text("%g | %s = %.2f", elmt.time, elmt.concept, elmt.u.measure.value);
    }
}

static void DrawEventsBlock(ImRect rect, float alpha, Span<const Element *const> events)
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
                TextMeasure(*elmt);
            } else {
                ImGui::Text("%g | %s", elmt->time, elmt->concept);
            }
        }
        ImGui::EndTooltip();
    }
}

static void DrawEvents(float x_offset, float y_min, float y_max, float time_zoom, float alpha,
                       Span<const Element *const> events)
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
            DrawEventsBlock(rect, alpha, events.Take(first_block_event, i - first_block_event));

            rect.Min.x = event_pos;
            first_block_event = i;
        }
        rect.Max.x = event_pos;
    }
    if (first_block_event < events.len) {
        DrawEventsBlock(rect, alpha, events.Take(first_block_event, events.len - first_block_event));
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

                if (LIKELY(!isnan(prev_point.y) && !isnan(point.y))) {
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

                if (LIKELY(!isnan(prev_point.y) && !isnan(point.y))) {
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
                         Span<const Element *const> measures, double min, double max,
                         InterpolationMode interpolation)
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
        if (!isnan(measures[i]->u.measure.min)) {
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
        if (!isnan(measures[i]->u.measure.max)) {
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
                TextMeasure(*elmt);
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
    int depth;
    float text_alpha;
    float elements_alpha;
    float height;
    HeapArray<const Element *> elements;
};

static bool DrawLineFrame(ImRect bb, float tree_width, const LineData &line)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // Line header
    bool deploy_click;
    {
        float y = (bb.Min.y + bb.Max.y) / 2.0f;
        ImVec2 text_size = ImGui::CalcTextSize(line.title.ptr, line.title.end());
        ImRect deploy_bb(bb.Min.x + (float)line.depth * 12.0f - 3.0f, y - 9.0f,
                         bb.Min.x + (float)line.depth * 12.0f + 23.0f + text_size.x, y + 7.0f);

        if (ImGui::ItemAdd(deploy_bb, 0)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, line.text_alpha));
            DEFER { ImGui::PopStyleColor(1); };

            if (!line.leaf) {
                ImGui::RenderArrow(ImVec2(bb.Min.x + (float)line.depth * 12.0f, y - 9.0f),
                                   line.deployed ? ImGuiDir_Down : ImGuiDir_Right);
            }

            ImVec4 text_rect {
                bb.Min.x + (float)line.depth * 12.0f + 20.0f, bb.Min.y,
                bb.Min.x + tree_width, bb.Max.y
            };
            draw->AddText(nullptr, 0.0f, ImVec2(text_rect.x, y - 9.0f),
                          ImGui::GetColorU32(ImGuiCol_Text),
                          line.title.ptr, line.title.end(), 0.0f, &text_rect);
        }

        deploy_click = !line.leaf && ImGui::IsItemClicked();
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

    return deploy_click;
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
                    if (!isnan(elmt->u.measure.min)) {
                        min_min = std::min(min_min, elmt->u.measure.min);
                    }
                    if (!isnan(elmt->u.measure.max)) {
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
    DrawPeriods(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha, periods);
    DrawEvents(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha, events);
    DrawMeasures(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, line.elements_alpha,
                 measures, measures_min, measures_max, state.settings.interpolation);
}

static float ComputeElementHeight(const InterfaceSettings &settings, Element::Type type)
{
    if (settings.plot_measures && type == Element::Type::Measure) {
        return settings.plot_height;
    } else {
        return 20.0f;
    }
}

static ImVec2 ComputeEntitySize(const InterfaceState &state, const EntitySet &entity_set,
                                const ConceptSet *concept_set, const Entity &ent)
{
    ImGuiStyle &style = ImGui::GetStyle();

    HashMap<Span<const char>, float> line_heights;
    float max_x = 0.0f, height = 0.0f;

    for (const Element &elmt: ent.elements) {
        max_x = std::max((float)elmt.time, max_x);

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

    return ImVec2(max_x, height);
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
        state.total_width_unscaled = 0.0f;
        state.total_height = 0.5f;

        state.lines_top.SetCapacity(entity_set.entities.len);
        state.lines_top.len = entity_set.entities.len;
        for (Size i = 0; i < state.scroll_to_idx; i++) {
            state.lines_top[i] = state.total_height;

            ImVec2 ent_size = ComputeEntitySize(state, entity_set, concept_set,
                                                entity_set.entities[i]);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.x);
            state.total_height += ent_size.y;
        }
        ImGui::SetScrollY(state.total_height - state.scroll_offset_y);
        for (Size i = state.scroll_to_idx; i < entity_set.entities.len; i++) {
            state.lines_top[i] = state.total_height;

            ImVec2 ent_size = ComputeEntitySize(state, entity_set, concept_set,
                                                entity_set.entities[i]);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.x);
            state.total_height += ent_size.y;
        }

        state.prev_concept_set = concept_set;
        state.size_cache_valid = true;
        cache_refreshed = true;
    }

    Size render_idx = -1;
    float render_offset = 0.0f;
    for (Size i = 1; i < state.lines_top.len; i++) {
        if (state.lines_top[i] >= ImGui::GetScrollY()) {
            if (!cache_refreshed) {
                state.scroll_to_idx = i;
                state.scroll_offset_y = state.lines_top[i] - ImGui::GetScrollY();
            }
            render_idx = i - 1;
            ImGui::SetCursorPosY(state.lines_top[i - 1] + style.ItemSpacing.y);
            render_offset = ImGui::GetCursorScreenPos().y;
            break;
        }
    }
    DebugAssert(render_idx >= 0);

    HeapArray<LineData> lines;
    {
        float base_y = render_offset;
        float y = base_y;
        for (Size i = render_idx; i < entity_set.entities.len &&
                                  y < win->ClipRect.Max.y; i++) {
            const Entity &ent = entity_set.entities[i];

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
                            line->text_alpha = 1.0f;
                            line->elements_alpha = 1.0f;
                            line->height = 0.0f;
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

            if (!g_io->input.mouseover || (g_io->input.y < base_y || g_io->input.y >= y)) {
                for (Size i = prev_lines_len; i < lines.len; i++) {
                    lines[i].text_alpha *= 0.4f;
                    lines[i].elements_alpha *= 0.4f;
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
            DrawLineElements(bb, tree_width, state, time_offset, line);
            y = bb.Max.y;
        }
    }

    // Draw frames (header, support line)
    Span<const char> deploy_path = {};
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
            if (DrawLineFrame(bb, tree_width, line)) {
                state.scroll_to_idx = ent - entity_set.entities.ptr;
                // NOTE: I'm not sure I get why ent_offset_y does not work directly but
                // it's 5 in the morning. Fix this hack later.
                state.scroll_offset_y = ent_offset_y - style.ItemSpacing.y - ImGui::GetWindowPos().y;
                deploy_path = line.path;
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
    }

    return !cache_refreshed;
}

static void DrawTimeScale(ImRect bb, double time_offset, float time_zoom, float grid_alpha)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    int precision = (int)log10f(1.0f / time_step);
    float min_text_delta = 20.0f + 10.0f * fabsf(log10f(1.0f / time_step));

    // TODO: Avoid overdraw (left of screen)
    float x = bb.Min.x - (float)time_offset * time_zoom, time = 0.0f;
    float prev_text_x = x - min_text_delta - 1.0f;
    while (x < bb.Max.x + 30.0f) {
        if (x >= bb.Min.x) {
            float x_exact = round(x);
            if (x - prev_text_x >= min_text_delta) {
                draw->AddLine(ImVec2(x_exact, bb.Min.y + 2.0f), ImVec2(x_exact, bb.Max.y - ImGui::GetFontSize() - 4.0f),
                              ImGui::GetColorU32(ImGuiCol_Text));
                if (grid_alpha > 0.0f) {
                    draw->AddLine(ImVec2(x_exact, 0.0f), ImVec2(x_exact, bb.Min.y + 2.0f),
                                  ImGui::GetColorU32(ImGuiCol_Text, grid_alpha));
                }

                char time_str[32];
                ImVec2 text_size;
                Fmt(time_str, "%1", FmtDouble(time, precision));
                text_size = ImGui::CalcTextSize(time_str);

                draw->AddText(ImVec2(x - text_size.x / 2.0f, bb.Max.y - ImGui::GetFontSize() - 2.0f),
                              ImGui::GetColorU32(ImGuiCol_Text), time_str);
                prev_text_x = x;
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

static bool DrawView(InterfaceState &state,
                     const EntitySet &entity_set, const ConceptSet *concept_set)
{
    ImGuiWindow *win = ImGui::GetCurrentWindow();

    // Layout settings
    float scale_height = 16.0f + ImGui::GetFontSize();
    double time_offset = ImGui::GetScrollX() / state.time_zoom;

    // Deal with time zoom
    if (ImGui::IsMouseHoveringWindow() &&
            g_io->input.keys.Test((int)RunIO::Key::Control) && g_io->input.wheel_y) {
        double (*animator)(double relative_time) = nullptr;
        if (state.time_zoom.animation.Running(g_io->time.monotonic)) {
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

    // Run animations
    state.time_zoom.Update(g_io->time.monotonic);

    // Render time scale
    ImRect scale_rect = win->ClipRect;
    scale_rect.Min.x = ImMin(scale_rect.Min.x + state.settings.tree_width + 15.0f, scale_rect.Max.x);
    scale_rect.Min.y = ImMin(scale_rect.Max.y - scale_height, scale_rect.Max.y);
    DrawTimeScale(scale_rect, time_offset, state.time_zoom, state.settings.grid_alpha);

    // Render entities
    bool valid_frame;
    {
        ImRect entity_rect = win->ClipRect;
        entity_rect.Max.y -= scale_height;
        valid_frame = DrawEntities(entity_rect, state.settings.tree_width,
                                   time_offset, state, entity_set, concept_set);
    }

    // Help ImGui compute scrollbar and layout
    ImGui::SetCursorPos(ImVec2(state.settings.tree_width + 20.0f + state.total_width_unscaled *
                                                                   (float)state.time_zoom,
                               state.total_height + scale_height));
    ImGui::ItemSize(ImVec2(0.0f, 0.0f));

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
