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
    Plot
};

static ImU32 GetVisColor(VisColor color)
{
    switch (color) {
        case VisColor::Event: { return ImGui::ColorConvertFloat4ToU32(ImVec4(0.100f, 0.400f, 0.750f, 1.0f)); } break;
        case VisColor::Alert: { return ImGui::ColorConvertFloat4ToU32(ImVec4(0.724f, 0.107f, 0.076f, 1.0f)); } break;
        case VisColor::Plot: { return ImGui::GetColorU32(ImGuiCol_PlotLines); } break;
    }
    Assert(false);
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
    Assert(false);
}

static void DrawEvents(float start_x, float end_x, float y, Span<const Element *const> events)
{
    ImRect bb(start_x - 10.0f, y, end_x + 10.0f, y + 20.0f);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->ChannelsSetCurrent(1);

    Size anomalies = 0;
    ImU32 color;
    for (const Element *elmt: events) {
        anomalies += DetectAnomaly(*elmt);
    }
    color = GetVisColor(anomalies ? VisColor::Alert : VisColor::Event);

    if (end_x - start_x >= 1.0f) {
        ImVec2 points[] = {
            { start_x, y },
            { end_x, y },
            { end_x + 10.0f, y + 20.0f },
            { start_x - 10.0f, y + 20.0f }
        };
        draw->AddConvexPolyFilled(points, ARRAY_SIZE(points), color);
    } else {
        draw->AddTriangleFilled(ImVec2(start_x, y),
                                ImVec2(start_x + 10.0f, y + 20.0f),
                                ImVec2(start_x - 10.0f, y + 20.0f),
                                color);
    }

    if (events.len > 1) {
        char len_str[32];
        Fmt(len_str, "%1", events.len);
        ImVec2 text_size = ImGui::CalcTextSize(len_str);
        ImVec2 text_bb = bb.GetCenter();
        text_bb.x -= text_size.x / 2.0f + 1.0f;
        text_bb.y -= text_size.y / 2.0f - 2.0f;
        draw->AddText(text_bb, ImGui::GetColorU32(ImGuiCol_Text), len_str, nullptr);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        for (const Element *elmt: events) {
            if (elmt->type == Element::Type::Measure) {
                ImGui::Text("%s = %.2f [%f]", elmt->concept, elmt->u.measure.value,
                            elmt->time);
            } else {
                ImGui::Text("%s [%f]", elmt->concept, elmt->time);
            }
        }
        ImGui::EndTooltip();
    }
}

static void DrawMeasures(float start_x, float y, float time_zoom,
                         Span<const Element *const> measures)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->ChannelsSetCurrent(2);

    // TODO: Check min / max homogeneity across measures
    double min_value = FLT_MAX, max_value = -FLT_MAX;
    if (false && !std::isnan(measures[0]->u.measure.min) &&
                 !std::isnan(measures[0]->u.measure.max)) {
        double normal_range = measures[0]->u.measure.max - measures[0]->u.measure.min;
        min_value = measures[0]->u.measure.min - (0.05 * normal_range);
        max_value = measures[0]->u.measure.max + (0.05 * normal_range);
    } else {
        for (const Element *elmt: measures) {
            min_value = std::min(min_value, elmt->u.measure.value);
            max_value = std::max(max_value, elmt->u.measure.value);
        }
    }

    float scaler = 16.0f / (float)(max_value - min_value);
    if (std::isinf(scaler)) {
        scaler = 0.5f;
    }

    double start_time = measures[0]->time;
    ImVec2 p_prev;
    for (Size i = 0; i < measures.len; i++) {
        const Element *elmt = measures[i];
        DebugAssert(elmt->type == Element::Type::Measure);

        ImVec2 p(start_x + ((float)(elmt->time - start_time) * time_zoom),
                 y + 16.0f - scaler * (float)(elmt->u.measure.value - min_value));
        if (i) {
            draw->AddLine(p_prev, p, GetVisColor(VisColor::Plot));
        }
        p_prev = p;
    }
    for (const Element *elmt: measures) {
        ImU32 color = DetectAnomaly(*elmt) ? GetVisColor(VisColor::Alert)
                                           : GetVisColor(VisColor::Plot);
        ImVec2 p(start_x + ((float)(elmt->time - start_time) * time_zoom),
                 y + 16.0f - scaler * (float)(elmt->u.measure.value - min_value));
        draw->AddCircleFilled(p, 3.0f, color);
    }
}

static void DrawPeriod(float start_x, float end_x, float y, const Element &elmt)
{
    DebugAssert(elmt.type == Element::Type::Period);

    end_x = std::max(end_x, start_x + 1.0f);
    ImRect bb(ImVec2(start_x, y), ImVec2(end_x, y + 20.0f));
    if (!ImGui::ItemAdd(bb, 0))
        return;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->ChannelsSetCurrent(0);

    const ImGuiStyle *style = &ImGui::GetStyle();
    ImVec4 color = style->Colors[ImGuiCol_Border];
    color.w *= style->Alpha;
    draw->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(color));

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s [%f]", elmt.concept, elmt.time);
        ImGui::EndTooltip();
    }
}

struct LineData {
    Span<const char> path;
    Span<const char> title;
    bool leaf;
    bool deployed;
    int depth;
    HeapArray<const Element *> elements;
};

static bool RenderLine(const InterfaceState &state, const LineData &line)
{
    DebugAssert(line.elements.len);

    ImVec2 base_pos = ImGui::GetCursorScreenPos();

    bool ret;
    if (!line.leaf) {
        ImRect bb(base_pos.x + (float)line.depth * 5.0f, base_pos.y + 5.0f,
                  base_pos.x + (float)line.depth * 5.0f + 10.0f, base_pos.y + 15.0f);
        if (ImGui::ItemAdd(bb, 0)) {
            ImGui::RenderTriangle(ImVec2(base_pos.x + (float)line.depth * 5.0f, base_pos.y),
                                  line.deployed ? ImGuiDir_Down : ImGuiDir_Right);
        }
        ret = ImGui::IsItemClicked();
    } else {
        ret = false;
    }

    HeapArray<const Element *> events_acc;
    float events_start_x, events_end_x;
    HeapArray<const Element *> measures_acc;
    float first_measure_x;

    float elmt_pos;
    for (const Element *elmt: line.elements) {
        elmt_pos = base_pos.x + 200.0f + (float)elmt->time * state.time_zoom;

        if (events_acc.len && elmt_pos - events_end_x >= 16.0f) {
            DrawEvents(events_start_x, events_end_x, base_pos.y, events_acc);
            events_acc.Clear(256);
        }

        switch (elmt->type) {
            case Element::Type::Event: {
                if (!events_acc.len) {
                    events_start_x = elmt_pos;
                }
                events_end_x = elmt_pos;
                events_acc.Append(elmt);
            } break;

            case Element::Type::Measure: {
                if (state.plot_measures && line.leaf) {
                    if (!measures_acc.len) {
                        first_measure_x = elmt_pos;
                    }
                    measures_acc.Append(elmt);
                } else {
                    if (!events_acc.len) {
                        events_start_x = elmt_pos;
                    }
                    events_end_x = elmt_pos;
                    events_acc.Append(elmt);
                }
            } break;

            case Element::Type::Period: {
                float end_pos = elmt_pos + ((float)elmt->u.period.duration * state.time_zoom);
                DrawPeriod(elmt_pos, end_pos, base_pos.y, *elmt);
            } break;
        }
    }
    if (events_acc.len) {
        DrawEvents(events_start_x, events_end_x, base_pos.y, events_acc);
    }
    if (measures_acc.len) {
        DrawMeasures(first_measure_x, base_pos.y, state.time_zoom, measures_acc);
    }

    ImGui::ItemSize(ImVec2(events_end_x - base_pos.x, 20.0f));
    if (ImGui::ItemAdd(ImRect(base_pos.x, base_pos.y,
                              ImGui::GetWindowWidth(), base_pos.y + 20.0f), 0)) {
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddText(ImVec2(base_pos.x + (float)line.depth * 5.0f + 20.0f, base_pos.y),
                      ImGui::GetColorU32(ImGuiCol_Text), line.title.ptr, line.title.end());
        if (line.path == "/") {
            draw->AddLine(ImVec2(base_pos.x, base_pos.y - 3.0f),
                          ImVec2(ImGui::GetWindowWidth(), base_pos.y - 3.0f),
                          ImGui::GetColorU32(ImGuiCol_Separator));
        }
        draw->AddLine(ImVec2(base_pos.x, base_pos.y + 20.0f),
                      ImVec2(ImGui::GetWindowWidth(), base_pos.y + 20.0f),
                      ImGui::GetColorU32(ImGuiCol_Separator));
    }

    return ret;
}

static ImVec2 ComputeEntitySize(const EntitySet &entity_set, const Entity &ent,
                                const HashSet<Span<const char>> &deployed_paths)
{
    HashSet<Span<const char>> lines_set;
    float max_x = 0.0f;

    for (const Element &elmt: ent.elements) {
        max_x = std::max((float)elmt.time, max_x);

        Span<const char> path;
        {
            if (elmt.concept[0] == '/') {
                path = elmt.concept;
                while (path.len > 1 && path.ptr[--path.len] != '/');
            } else {
                const Concept *concept = entity_set.concepts_map.Find(elmt.concept);
                if (concept) {
                    path = concept->path;
                } else {
                    path = entity_set.sources.Find(elmt.source_id)->default_path;
                }
            }
        }

        bool fully_deployed = false;
        {
            Span<const char> partial_path = {path.ptr, 0};
            while (partial_path.len < path.len) {
                while (partial_path.len < path.len && partial_path.ptr[partial_path.len++] != '/');

                lines_set.Append(partial_path);
                fully_deployed = deployed_paths.Find(partial_path);

                if (!fully_deployed)
                    break;
            }
        }

        if (fully_deployed) {
            lines_set.Append(elmt.concept);
        }
    }

    return ImVec2(max_x, (float)lines_set.count * 24.0f);
}

static void RenderEntities(InterfaceState &state, const EntitySet &entity_set)
{
    if (!entity_set.entities.len)
        return;

    ImGui::GetWindowDrawList()->ChannelsSplit(3);

    if (!state.size_cache_valid || state.lines_top.len != entity_set.entities.len) {
        state.total_width_unscaled = 0.0f;
        state.total_height = 0.0f;

        state.lines_top.SetCapacity(entity_set.entities.len);
        state.lines_top.len = entity_set.entities.len;
        for (Size i = 0; i < state.scroll_to_idx; i++) {
            state.lines_top[i] = state.total_height;

            ImVec2 ent_size = ComputeEntitySize(entity_set, entity_set.entities[i], state.deploy_paths);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.x);
            state.total_height += ent_size.y;
        }
        ImGui::SetScrollY(state.total_height - state.scroll_offset_y);
        for (Size i = state.scroll_to_idx; i < entity_set.entities.len; i++) {
            state.lines_top[i] = state.total_height;

            ImVec2 ent_size = ComputeEntitySize(entity_set, entity_set.entities[i], state.deploy_paths);
            state.total_width_unscaled = std::max(state.total_width_unscaled, ent_size.x);
            state.total_height += ent_size.y;
        }

        state.size_cache_valid = true;
    }

    Size start_render_idx = 0;
    for (Size i = 1; i < state.lines_top.len; i++) {
        if (state.lines_top[i] >= ImGui::GetScrollY()) {
            start_render_idx = i - 1;
            ImGui::SetCursorPosY(4.0f + state.lines_top[i - 1]);
            break;
        }
    }

    // TODO: Set to moused over element instead?
    state.scroll_to_idx = start_render_idx;
    state.scroll_offset_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;

    // TODO: Use real screen height to stop rendering
    Span<const char> deploy_path =  {};
    float max_render_y = 1500.0f;
    for (Size i = start_render_idx; i < entity_set.entities.len &&
                                    ImGui::GetCursorScreenPos().y < max_render_y; i++) {
        const Entity &ent = entity_set.entities[i];

        HeapArray<LineData> lines;
        HashMap<Span<const char>, Size> lines_map;

        float entity_offset_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y - 4.0f;
        for (const Element &elmt: ent.elements) {
            Span<const char> path;
            {
                if (elmt.concept[0] == '/') {
                    path = elmt.concept;
                    while (path.len > 1 && path.ptr[--path.len] != '/');
                } else {
                    const Concept *concept = entity_set.concepts_map.Find(elmt.concept);
                    if (concept) {
                        path = concept->path;
                    } else {
                        path = entity_set.sources.Find(elmt.source_id)->default_path;
                    }
                }
            }

            bool fully_deployed = false;
            int tree_depth = 0;
            {
                Span<const char> partial_path = {path.ptr, 0};
                while (partial_path.len < path.len) {
                    while (partial_path.len < path.len && partial_path.ptr[partial_path.len++] != '/');

                    LineData *line;
                    {
                        std::pair<Size *, bool> ret = lines_map.Append(partial_path, lines.len);
                        if (!ret.second) {
                            line = &lines[*ret.first];
                            tree_depth = line->depth + 1;
                        } else {
                            line = lines.Append();
                            line->path = partial_path;
                            if (partial_path.len > 1) {
                                line->title = partial_path;
                            } else {
                                line->title = ent.id;
                            }
                            line->leaf = false;
                            line->deployed = state.deploy_paths.Find(partial_path);
                            line->depth = tree_depth++;
                        }
                        fully_deployed = line->deployed;
                    }
                    line->elements.Append(&elmt);

                    if (!line->deployed)
                        break;
                }
            }

            if (fully_deployed) {
                LineData *line;
                {
                    std::pair<Size *, bool> ret = lines_map.Append(elmt.concept, lines.len);
                    if (!ret.second) {
                        line = &lines[*ret.first];
                    } else {
                        line = lines.Append();
                        line->path = path;
                        line->title = elmt.concept;
                        line->leaf = true;
                        line->depth = tree_depth;
                    }
                }
                line->elements.Append(&elmt);
            }
        }

        std::sort(lines.begin(), lines.end(),
                  [](const LineData &line1, const LineData &line2) {
            return MultiCmp(CmpStr(line1.path, line2.path),
                            (int)line1.leaf - (int)line2.leaf,
                            CmpStr(line1.title, line2.title)) < 0;
        });

        for (LineData &line: lines) {
            // FIXME: Don't do this all the time
            std::stable_sort(line.elements.begin(), line.elements.end(),
                             [](const Element *elmt1, const Element *elmt2) {
                return elmt1->time < elmt2->time;
            });

            if (RenderLine(state, line)) {
                state.scroll_to_idx = i;
                state.scroll_offset_y = entity_offset_y;
                deploy_path = line.path;
            }
        }
    }

    if (deploy_path.len) {
        std::pair<Span<const char> *, bool> ret = state.deploy_paths.Append(deploy_path);
        if (!ret.second) {
            state.deploy_paths.Remove(ret.first);
        }

        state.size_cache_valid = false;
    }

    ImGui::GetWindowDrawList()->ChannelsMerge();

    {
        float cursor_x = ImGui::GetCursorPosX();
        ImGui::SetCursorPos(ImVec2(200.0f + state.total_width_unscaled * (float)state.time_zoom + 10.0f,
                                   state.total_height));
        ImGui::SetCursorPos(ImVec2(cursor_x, state.total_height));
    }
}

void RenderTimeScale(const InterfaceState &state)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImVec2 base_pos = ImGui::GetCursorScreenPos();

    // FIXME: Should not need to erase the background, this is beyond ugly
    {
        ImVec2 p0(base_pos.x, base_pos.y);
        ImVec2 p1(base_pos.x + ImGui::GetWindowWidth() + ImGui::GetScrollMaxX(), base_pos.y + 28.0f);
        //ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.329f, 0.329f, 0.447f, 1.0f));
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.137f, 0.137f, 0.137f, 1.0f));
        draw->AddRectFilled(p0, p1, bg);
    }

    float time_step = 10.0f / powf(10.0f, floorf(log10f(state.time_zoom)));
    int precision = (int)log10f(1.0f / time_step);
    float min_text_delta = 20.0f + 10.0f * std::abs(log10f(1.0f / time_step));

    // TODO: Limit to visible area (left and right) to avoid overdraw
    float x = base_pos.x + 200.0f, time = 0.0f;
    float prev_text_x = -min_text_delta - 1.0f;
    while (x < 3000.0f) {
        if (x - prev_text_x >= min_text_delta) {
            draw->AddLine(ImVec2(x, base_pos.y + 2.0f), ImVec2(x, base_pos.y + 10.0f),
                          ImGui::GetColorU32(ImGuiCol_Text));

            char time_str[32];
            Fmt(time_str, "%1", FmtDouble(time, precision));
            ImVec2 text_size = ImGui::CalcTextSize(time_str);

            draw->AddText(ImVec2(x - text_size.x / 2.0f, base_pos.y + 12.0f),
                          ImGui::GetColorU32(ImGuiCol_Text), time_str);
            prev_text_x = x;
        } else {
            draw->AddLine(ImVec2(x, base_pos.y + 6.0f), ImVec2(x, base_pos.y + 10.0f),
                          ImGui::GetColorU32(ImGuiCol_Text));
        }

        x += time_step * state.time_zoom;
        time += time_step;
    }

    // TODO: ItemSize(), ItemAdd()
}

bool Step(InterfaceState &state, const EntitySet &entity_set)
{
    if (!StartRender())
        return false;

    if (!g_io->main.iteration_count) {
        ImGui::StyleColorsDark();
    }

    float menu_height = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        //LogInfo("Framerate: %1 (%2 ms/frame)",
        //        FmtDouble(ImGui::GetIO().Framerate, 1), FmtDouble(1000.0f / ImGui::GetIO().Framerate, 3));

        ImGui::Checkbox("Plots", &state.plot_measures);
        ImGui::Text("             Framerate: %.1f (%.3f ms/frame)",
                    ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        menu_height = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    // Init main / render window
    ImVec2 desktop_pos = ImVec2(0, menu_height);
    ImVec2 desktop_size = ImGui::GetIO().DisplaySize;
    desktop_size.y -= menu_height;
    ImGuiWindowFlags desktop_flags = ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_HorizontalScrollbar |
                                     ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    ImGui::SetNextWindowPos(desktop_pos);
    ImGui::SetNextWindowSize(desktop_size);
    ImGui::Begin("View", nullptr, ImVec2(), 0.0f, desktop_flags);

    if (ImGui::IsMouseHoveringWindow() &&
            g_io->input.keys.Test((int)RunIO::Key::Control) && g_io->input.wheel_y) {
        float multiplier = ((g_io->input.keys.Test((int)RunIO::Key::Shift)) ? 1.5f : 1.1f);
        if (g_io->input.wheel_y > 0) {
            float new_zoom = state.time_zoom * (float)g_io->input.wheel_y * multiplier;
            state.time_zoom = std::min(new_zoom, 100000.0f);
        } else {
            float new_zoom = state.time_zoom / -(float)g_io->input.wheel_y / multiplier;
            state.time_zoom = std::max(new_zoom, 0.00001f);
        }
    }

    RenderEntities(state, entity_set);

    {
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        cursor_pos.y = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() + ImGui::GetScrollY() - 62.0f;
        ImGui::SetCursorPos(cursor_pos);
    }
    RenderTimeScale(state);

    ImGui::End();

    Render();
    SwapGLBuffers();
    if (!g_io->main.run) {
        ReleaseRender();
    }

    return true;
}
