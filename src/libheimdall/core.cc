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

static void DrawPeriods(float x_offset, float y_min, float y_max, float time_zoom,
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
            color.w *= style.Alpha;

            draw->AddRectFilled(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(color));

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s [%f]", elmt->concept, elmt->time);
                ImGui::EndTooltip();
            }
        }
    }
}

static void DrawEventsBlock(ImRect rect, Span<const Element *const> events)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImRect bb {
        rect.Min.x - 10.0f, rect.Min.y,
        rect.Max.x + 10.0f, rect.Max.y
    };

    if (ImGui::ItemAdd(bb, 0)) {
        Size anomalies = 0;
        ImU32 color;
        for (const Element *elmt: events) {
            anomalies += DetectAnomaly(*elmt);
        }
        color = GetVisColor(anomalies ? VisColor::Alert : VisColor::Event);

        if (rect.GetWidth() >= 1.0f) {
            ImVec2 points[] = {
                { rect.Min.x, rect.Min.y },
                { rect.Max.x, rect.Min.y },
                { rect.Max.x + 10.0f, rect.Max.y },
                { rect.Min.x - 10.0f, rect.Max.y }
            };
            draw->AddConvexPolyFilled(points, ARRAY_SIZE(points), color);
        } else {
            ImVec2 points[] = {
                { rect.Min.x, rect.Min.y },
                { rect.Min.x + 10.0f, rect.Max.y },
                { rect.Min.x - 10.0f, rect.Max.y }
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

            draw->AddText(text_bb, ImGui::GetColorU32(ImGuiCol_Text), len_str, nullptr);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            for (const Element *elmt: events) {
                if (elmt->type == Element::Type::Measure) {
                    ImGui::Text("%s = %.2f [%f]", elmt->concept, elmt->u.measure.value, elmt->time);
                } else {
                    ImGui::Text("%s [%f]", elmt->concept, elmt->time);
                }
            }
            ImGui::EndTooltip();
        }
    }
}

static void DrawEvents(float x_offset, float y_min, float y_max, float time_zoom,
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
            DrawEventsBlock(rect, events.Take(first_block_event, i - first_block_event));

            rect.Min.x = event_pos;
            first_block_event = i;
        }
        rect.Max.x = event_pos;
    }
    if (first_block_event < events.len) {
        DrawEventsBlock(rect, events.Take(first_block_event, events.len - first_block_event));
    }
}

static void DrawMeasures(float x_offset, float y_min, float y_max, float time_zoom,
                         Span<const Element *const> measures, double min, double max)
{
    if (!measures.len)
        return;

    ImDrawList *draw = ImGui::GetWindowDrawList();

    float y_scaler = (y_max - y_min - 4.0f) / (float)(max - min);
    if (std::isinf(y_scaler)) {
        y_scaler = 0.5f;
    }
    const auto compute_coordinates = [&](const Element *elmt) {
        return ImVec2 {
            x_offset + ((float)elmt->time * time_zoom),
            y_max - 4.0f - y_scaler * (float)(elmt->u.measure.value - min)
        };
    };

    // Draw line
    {
        ImVec2 prev_point = compute_coordinates(measures[0]);
        for (Size i = 1; i < measures.len; i++) {
            const Element *elmt = measures[i];
            DebugAssert(elmt->type == Element::Type::Measure);

            ImVec2 point = compute_coordinates(elmt);
            draw->AddLine(prev_point, point, GetVisColor(VisColor::Plot));
            prev_point = point;
        }
    }

    // Draw points
    for (const Element *elmt: measures) {
        ImU32 color = DetectAnomaly(*elmt) ? GetVisColor(VisColor::Alert)
                                           : GetVisColor(VisColor::Plot);
        ImVec2 point = compute_coordinates(elmt);
        draw->AddCircleFilled(point, 3.0f, color);
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

static bool DrawEntityLine(ImRect bb, float tree_width,
                           const InterfaceState &state, double time_offset, const LineData &line)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // Line header
    bool deploy_click;
    {
        if (!line.leaf) {
            ImRect triangle_bb(bb.Min.x + (float)line.depth * 12.0f - 3.0f, bb.Min.y + 2.0f,
                               bb.Min.x + (float)line.depth * 12.0f + 13.0f, bb.Min.y + 18.0f);
            if (ImGui::ItemAdd(triangle_bb, 0)) {
                ImGui::RenderTriangle(ImVec2(bb.Min.x + (float)line.depth * 12.0f, bb.Min.y),
                                      line.deployed ? ImGuiDir_Down : ImGuiDir_Right);
            }
            deploy_click = ImGui::IsItemClicked();
        } else {
            deploy_click = false;
        }

        ImVec4 text_rect {
            bb.Min.x + (float)line.depth * 12.0f + 20.0f, bb.Min.y,
            bb.Min.x + tree_width, bb.Max.y
        };
        draw->AddText(nullptr, 0.0f, ImVec2(text_rect.x, text_rect.y),
                      ImGui::GetColorU32(ImGuiCol_Text), line.title.ptr, line.title.end(),
                      0.0f, &text_rect);
    }

    // Split elements
    HeapArray<const Element *> events;
    HeapArray<const Element *> periods;
    HeapArray<const Element *> measures;
    double measures_min = FLT_MAX, measures_max = -FLT_MAX;
    for (const Element *elmt: line.elements) {
        switch (elmt->type) {
            case Element::Type::Event: { events.Append(elmt); } break;
            case Element::Type::Measure: {
                if (line.leaf && state.plot_measures) {
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

    // Draw elements
    {
        float x_offset = bb.Min.x + tree_width + 15.0f - (float)(time_offset * state.time_zoom);

        DrawPeriods(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, periods);
        DrawEvents(x_offset, bb.Min.y, bb.Max.y, state.time_zoom, events);
        DrawMeasures(x_offset, bb.Min.y, bb.Max.y, state.time_zoom,
                     measures, measures_min, measures_max);
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

static ImVec2 ComputeEntitySize(const EntitySet &entity_set, const Entity &ent,
                                const HashSet<Span<const char>> &deployed_paths)
{
    ImGuiStyle &style = ImGui::GetStyle();

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
        DebugAssert(path.len > 0);

        bool fully_deployed = false;
        {
            Span<const char> partial_path = {path.ptr, 1};
            for (;;) {
                lines_set.Append(partial_path);
                fully_deployed = deployed_paths.Find(partial_path);

                if (!fully_deployed || partial_path.len == path.len)
                    break;
                while (++partial_path.len < path.len && partial_path.ptr[partial_path.len] != '/');
            }
        }

        if (fully_deployed) {
            lines_set.Append(elmt.concept);
        }
    }

    // TODO: Move spacing calculation to parents
    return ImVec2(max_x, (float)lines_set.count * (20.0f + style.ItemSpacing.y));
}

static void DrawEntities(ImRect bb, float tree_width, double time_offset,
                         InterfaceState &state, const EntitySet &entity_set)
{
    if (!entity_set.entities.len)
        return;

    const ImGuiStyle &style = ImGui::GetStyle();
    ImGuiWindow *win = ImGui::GetCurrentWindow();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(bb.Min, bb.Max);
    DEFER { draw->PopClipRect(); };

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
            ImGui::SetCursorPosY(state.lines_top[i - 1] + style.ItemSpacing.y);
            break;
        }
    }

    // TODO: Set to moused over element instead?
    state.scroll_to_idx = start_render_idx;
    state.scroll_offset_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;

    Span<const char> deploy_path =  {};
    for (Size i = start_render_idx; i < entity_set.entities.len &&
                                    ImGui::GetCursorScreenPos().y <= win->ClipRect.Max.y; i++) {
        const Entity &ent = entity_set.entities[i];

        HeapArray<LineData> lines;
        HashMap<Span<const char>, Size> lines_map;

        float entity_offset_y = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y -
                                style.ItemSpacing.y;
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
                } else {
                    const Concept *concept = entity_set.concepts_map.Find(elmt.concept);
                    if (concept) {
                        path = concept->path;
                    } else {
                        path = entity_set.sources.Find(elmt.source_id)->default_path;
                    }
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
                            line = lines.Append();
                            line->path = partial_path;
                            if (partial_path.len > 1) {
                                line->title = MakeSpan(partial_path.ptr + name_offset,
                                                       partial_path.len - name_offset);
                                name_offset = partial_path.len + 1;
                            } else {
                                line->title = ent.id;
                            }
                            line->leaf = false;
                            line->deployed = state.deploy_paths.Find(partial_path);
                            line->depth = tree_depth++;
                        }
                        fully_deployed = line->deployed;
                    }
                    if (!line->deployed || state.keep_deployed) {
                        line->elements.Append(&elmt);
                    }

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
                        line = lines.Append();
                        line->path = path;
                        line->title = title;
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

            ImRect bb(win->ClipRect.Min.x, ImGui::GetCursorScreenPos().y + style.ItemSpacing.y,
                      win->ClipRect.Max.x, ImGui::GetCursorScreenPos().y + style.ItemSpacing.y + 20.0f);
            if (DrawEntityLine(bb, tree_width, state, time_offset, line)) {
                state.scroll_to_idx = i;
                state.scroll_offset_y = entity_offset_y;
                deploy_path = line.path;
            }
            ImGui::SetCursorScreenPos(bb.Max);
        }
    }

    if (deploy_path.len) {
        std::pair<Span<const char> *, bool> ret = state.deploy_paths.Append(deploy_path);
        if (!ret.second) {
            state.deploy_paths.Remove(ret.first);
        }

        state.size_cache_valid = false;
    }
}

static void DrawTimeScale(ImRect bb, double time_offset, float time_zoom)
{
    ImDrawList *draw = ImGui::GetWindowDrawList();

    // draw->PushClipRect(bb.Min, bb.Max, true);
    // DEFER { draw->PopClipRect(); };

    // float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    float time_step = 10.0f / powf(10.0f, floorf(log10f(time_zoom)));
    int precision = (int)log10f(1.0f / time_step);
    float min_text_delta = 20.0f + 10.0f * fabsf(log10f(1.0f / time_step));

    // TODO: Avoid overdraw (left of screen)
    float x = bb.Min.x - (float)time_offset * time_zoom, time = 0.0f;
    float prev_text_x = x - min_text_delta - 1.0f;
    while (x < bb.Max.x + 30.0f) {
        if (x - prev_text_x >= min_text_delta) {
            draw->AddLine(ImVec2(x, bb.Min.y + 2.0f), ImVec2(x, bb.Max.y - ImGui::GetFontSize() - 4.0f),
                          ImGui::GetColorU32(ImGuiCol_Text));

            char time_str[32];
            ImVec2 text_size;
            Fmt(time_str, "%1", FmtDouble(time, precision));
            text_size = ImGui::CalcTextSize(time_str);

            draw->AddText(ImVec2(x - text_size.x / 2.0f, bb.Max.y - ImGui::GetFontSize() - 2.0f),
                          ImGui::GetColorU32(ImGuiCol_Text), time_str);
            prev_text_x = x;
        } else {
            draw->AddLine(ImVec2(x, bb.Min.y + 2.0f), ImVec2(x, bb.Max.y - ImGui::GetFontSize() - 8.0f),
                          ImGui::GetColorU32(ImGuiCol_Text));
        }

        x += time_step * time_zoom;
        time += time_step;
    }
}

static void DrawView(InterfaceState &state, const EntitySet &entity_set)
{
    ImGuiWindow *win = ImGui::GetCurrentWindow();

    // Layout settings
    float tree_width = 200.0f;
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

    // Render entities
    ImRect entity_rect = win->ClipRect;
    entity_rect.Max.y -= scale_height;
    DrawEntities(entity_rect, tree_width, time_offset, state, entity_set);

    // Render time scale
    ImRect scale_rect = win->ClipRect;
    scale_rect.Min.x = ImMin(scale_rect.Min.x + tree_width + 15.0f, scale_rect.Max.x);
    scale_rect.Min.y = ImMin(scale_rect.Max.y - scale_height, scale_rect.Max.y);
    DrawTimeScale(scale_rect, time_offset, state.time_zoom);

    // Help ImGui compute scrollbar and layout
    ImGui::SetCursorPos(ImVec2(tree_width + 20.0f + state.total_width_unscaled * (float)state.time_zoom,
                               state.total_height + scale_height));
    ImGui::ItemSize(ImVec2(0.0f, 0.0f));
}

bool Step(InterfaceState &state, const EntitySet &entity_set)
{
    if (!StartRender())
        return false;

    if (!g_io->main.iteration_count) {
        ImGui::StyleColorsDark();
    }

    // Menu
    float menu_height = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        //LogInfo("Framerate: %1 (%2 ms/frame)",
        //        FmtDouble(ImGui::GetIO().Framerate, 1), FmtDouble(1000.0f / ImGui::GetIO().Framerate, 3));

        ImGui::Checkbox("Plots", &state.plot_measures);
        ImGui::Checkbox("Keep deployed", &state.keep_deployed);
        ImGui::Text("             Framerate: %.1f (%.3f ms/frame)",
                    ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        menu_height = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    // Main view
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
        DrawView(state, entity_set);
        ImGui::End();
    }

    Render();
    SwapGLBuffers();

    if (!g_io->main.run) {
        ReleaseRender();
    }

    return true;
}
