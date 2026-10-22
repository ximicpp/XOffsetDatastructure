// Map state demo: raylib window front end. OWNER: client thread.
//
// One frame: pump the socket; at frame start install a pending snapshot
// (SnapshotHolder::replace destroys the old buffer); request the current
// camera rect when nothing is outstanding and 100 ms have passed; read the
// live snapshot into presentation data (DrawItem: screen position + colour);
// draw scene grid, sites (read from the catalog buffer), units, labels and
// hover text (names read with name.view() from the snapshot buffer); present.
// No reference into a snapshot survives the frame it was taken in.
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include "client_core.hpp"

#include "raylib.h"
#include "rlgl.h"

namespace mapdemo::client {
namespace {

constexpr int kWinW = int(kViewWidth);
constexpr int kWinH = int(kViewHeight);
constexpr float kGrid = 256.0f;

Color rgba(std::uint32_t v) {
    return Color{static_cast<unsigned char>(v >> 24), static_cast<unsigned char>(v >> 16),
                 static_cast<unsigned char>(v >> 8), static_cast<unsigned char>(v)};
}

// raylib wants NUL-terminated text; names are string_views into the buffer.
const char* cstr(std::string_view s, char (&out)[96]) {
    std::size_t n = s.size() < sizeof out - 1 ? s.size() : sizeof out - 1;
    std::memcpy(out, s.data(), n);
    out[n] = '\0';
    return out;
}

Rect camera_rect(Point origin) {
    return Rect{origin, Point{origin.x + kViewWidth, origin.y + kViewHeight}};
}

Point clamp_origin(Point o, const Rect& scene) {
    Rect r = clamp_view(camera_rect(o), scene);
    return r.min;
}

// --auto-drag: still for 4 s, drag toward the north-west corner over 2 s, still again.
constexpr int kDragStart = 240;
constexpr int kDragFrames = 120;

void draw_grid(const Rect& cam, const Rect& scene) {
    const Color line{205, 199, 184, 255};
    const Color label{120, 112, 96, 255};
    float x0 = std::floor(cam.min.x / kGrid) * kGrid;
    for (float x = x0; x <= cam.max.x; x += kGrid) {
        if (x < scene.min.x || x > scene.max.x) continue;
        int sx = int(x - cam.min.x);
        DrawLine(sx, 0, sx, kWinH, line);
        DrawText(TextFormat("%d", int(x)), sx + 3, kWinH - 14, 10, label);
    }
    float y0 = std::floor(cam.min.y / kGrid) * kGrid;
    for (float y = y0; y <= cam.max.y; y += kGrid) {
        if (y < scene.min.y || y > scene.max.y) continue;
        int sy = int(y - cam.min.y);
        DrawLine(0, sy, kWinW, sy, line);
        DrawText(TextFormat("%d", int(y)), kWinW - 34, sy + 3, 10, label);
    }
    // Scene border (visible when the camera touches an edge).
    DrawRectangleLinesEx(Rectangle{scene.min.x - cam.min.x, scene.min.y - cam.min.y,
                                   scene.max.x - scene.min.x, scene.max.y - scene.min.y},
                         3.0f, Color{90, 80, 60, 255});
}

void draw_sites(const MapCatalog& catalog, const Rect& cam) {
    char name[96];
    for (const SiteData& s : catalog.sites) {
        if (!rules::contains(Rect{{cam.min.x - 80, cam.min.y - 20}, {cam.max.x + 20, cam.max.y + 20}}, s.pos)) continue;
        float sx = s.pos.x - cam.min.x, sy = s.pos.y - cam.min.y;
        DrawRectangle(int(sx) - 6, int(sy) - 6, 12, 12, Color{40, 70, 140, 255});
        DrawRectangleLines(int(sx) - 8, int(sy) - 8, 16, 16, Color{20, 35, 80, 255});
        DrawText(cstr(s.name.view(), name), int(sx) + 11, int(sy) - 6, 14, Color{20, 35, 80, 255});
    }
}

} // namespace

int run_gui(const Options& opt) {
    Session s(opt);
    s.connect();

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(kWinW, kWinH, "XOffset map state demo");
    SetTargetFPS(60);

    const Rect scene = s.scene();
    Point cam_origin = clamp_origin(Point{(scene.min.x + scene.max.x - kViewWidth) / 2,
                                          (scene.min.y + scene.max.y - kViewHeight) / 2}, scene);
    const Point drag_from = cam_origin;
    const Point drag_to = clamp_origin(Point{scene.min.x + 256.0f, scene.min.y + 192.0f}, scene);

    const std::uint64_t request_period_ns = std::uint64_t(1e9 / kViewRequestHz);
    std::uint64_t last_request_ns = 0;
    std::size_t next_shot = 0;
    std::vector<DrawItem> items;
    int frame = 0;
    bool server_gone = false;

    while (!WindowShouldClose()) {
        s.pump(0);
        if (s.bye()) {
            server_gone = true;
            break;
        }
        // ---- frame start: replace the snapshot only here
        if (s.install_pending() && opt.check_on()) s.verify_direct_read();

        // ---- camera: mouse drag or scripted drag
        if (opt.auto_drag) {
            if (frame >= kDragStart && frame < kDragStart + kDragFrames) {
                float t = float(frame - kDragStart + 1) / float(kDragFrames);
                cam_origin = Point{drag_from.x + (drag_to.x - drag_from.x) * t, drag_from.y + (drag_to.y - drag_from.y) * t};
            }
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            cam_origin = Point{cam_origin.x - d.x, cam_origin.y - d.y};
        }
        cam_origin = clamp_origin(cam_origin, scene);
        const Rect cam = camera_rect(cam_origin);

        // ---- request the latest camera rect (one outstanding, 10 Hz)
        std::uint64_t now = net::now_ns();
        if (s.can_request() && now - last_request_ns >= request_period_ns) {
            s.send_view(cam, net::ViewCategory::Normal);
            last_request_ns = now;
        }

        // ---- field read + presentation data (merged metric)
        const bool have = !s.holder().empty();
        if (have) {
            metrics::Stopwatch sw;
            build_draw_items(s.holder().view(), cam, items);
            s.note_read_present(sw.us());
        } else {
            items.clear();
        }

        // ---- draw
        metrics::Stopwatch draw_sw;
        BeginDrawing();
        ClearBackground(Color{244, 241, 232, 255});
        draw_grid(cam, scene);
        draw_sites(s.catalog(), cam);
        for (const DrawItem& d : items) DrawCircleV(Vector2{d.sx, d.sy}, 3.0f, rgba(d.rgba));

        const Vector2 mouse = GetMousePosition();
        const VisibleUnit* hover = nullptr;
        unsigned in_view[4] = {0, 0, 0, 0};   // Patrol, Hold, Alert, out of range
        unsigned marked = 0, marked_long = 0; // id % 100 == 0 units and those with the long name
        if (have) {
            char name[96];
            float best = 8.0f * 8.0f;
            for (const VisibleUnit& u : s.holder().view().units) {
                if (!rules::contains(cam, u.pos)) continue;
                float sx = u.pos.x - cam.min.x, sy = u.pos.y - cam.min.y;
                ++in_view[static_cast<std::uint32_t>(u.behavior) < 3 ? static_cast<std::uint32_t>(u.behavior) : 3];
                if (u.id % kEventIdModulo == 0) {
                    ++marked;
                    if (u.name.view().size() > 10) ++marked_long;
                    DrawText(cstr(u.name.view(), name), int(sx) + 5, int(sy) - 5, 10, Color{50, 45, 40, 255});
                    DrawCircleLines(int(sx), int(sy), 5.0f, Color{50, 45, 40, 255});
                }
                float dx = sx - mouse.x, dy = sy - mouse.y;
                if (dx * dx + dy * dy < best) {
                    best = dx * dx + dy * dy;
                    hover = &u;
                }
            }
            if (hover) {
                const VisibleUnit& u = *hover;
                float sx = u.pos.x - cam.min.x, sy = u.pos.y - cam.min.y;
                DrawCircleLines(int(sx), int(sy), 7.0f, BLACK);
                const char* text = TextFormat("%s  id %u  hp %d  %s  (%.1f, %.1f)", cstr(u.name.view(), name), u.id,
                                              u.hp, behavior_name(u.behavior), u.pos.x, u.pos.y);
                int w = MeasureText(text, 14);
                int tx = int(sx) + 12, ty = int(sy) + 10;
                if (tx + w + 10 > kWinW) tx = kWinW - w - 10;
                if (ty + 24 > kWinH) ty = kWinH - 24;
                DrawRectangle(tx - 4, ty - 4, w + 8, 22, Color{255, 255, 255, 235});
                DrawRectangleLines(tx - 4, ty - 4, w + 8, 22, DARKGRAY);
                DrawText(text, tx, ty, 14, BLACK);
            }
        }

        // ---- HUD
        DrawRectangle(8, 8, 520, 122, Color{20, 20, 20, 200});
        if (have) {
            const ViewSnapshot& v = s.holder().view();
            DrawText(TextFormat("tick %" PRIu64 "   visible %u   snapshot %u B", std::uint64_t(v.tick),
                                unsigned(v.units.size()), unsigned(s.last_wire_bytes())),
                     16, 14, 16, RAYWHITE);
        } else {
            DrawText("waiting for the first snapshot...", 16, 14, 16, RAYWHITE);
        }
        DrawText(TextFormat("view rtt %.2f ms (frame-paced)   load_verified %.1f us   %d fps", s.last_rtt_us() / 1000.0,
                            s.last_load_us(), GetFPS()),
                 16, 36, 16, RAYWHITE);
        DrawText(TextFormat("camera (%d, %d)   drag with the left mouse button", int(cam.min.x), int(cam.min.y)), 16, 58,
                 14, Color{200, 200, 200, 255});
        DrawCircle(22, 88, 5, rgba(behavior_rgba(Behavior::Patrol)));
        DrawText("Patrol", 32, 81, 14, RAYWHITE);
        DrawCircle(102, 88, 5, rgba(behavior_rgba(Behavior::Hold)));
        DrawText("Hold", 112, 81, 14, RAYWHITE);
        DrawCircle(166, 88, 5, rgba(behavior_rgba(Behavior::Alert)));
        DrawText("Alert", 176, 81, 14, RAYWHITE);
        DrawCircle(232, 88, 5, rgba(behavior_rgba(static_cast<Behavior>(99))));
        DrawText("out of range", 242, 81, 14, RAYWHITE);
        DrawText(TextFormat("in view: %u patrol  %u hold  %u alert  %u other   labelled %u (%u long names)", in_view[0],
                            in_view[1], in_view[2], in_view[3], marked, marked_long),
                 16, 103, 14, Color{200, 200, 200, 255});

        s.recorder().add("c.draw_us", draw_sw.us());

        // In-process screenshot of this frame (before the buffer swap; not timed).
        bool shot = next_shot < opt.screenshot_at.size() && opt.screenshot_at[next_shot] == frame;
        if (shot) {
            rlDrawRenderBatchActive();
            char file[64];
            std::snprintf(file, sizeof file, "mapdemo_f%05d.png", frame);
            TakeScreenshot(file);
            std::fprintf(stdout, "screenshot: %s (tick %" PRIu64 ", camera %d,%d)\n", file,
                         have ? std::uint64_t(s.holder().view().tick) : 0, int(cam.min.x), int(cam.min.y));
            std::fflush(stdout);
            while (next_shot < opt.screenshot_at.size() && opt.screenshot_at[next_shot] <= frame) ++next_shot;
        }

        // Read a snapshot that arrived while drawing (it is installed at the next
        // frame start), so the window's view rtt is not padded by the vsync wait.
        s.pump(0);

        metrics::Stopwatch present_sw;
        EndDrawing();
        s.recorder().add("c.present_us", present_sw.us());

        ++frame;
        if (!opt.screenshot_at.empty() && next_shot >= opt.screenshot_at.size()) break;   // scripted run done
    }
    CloseWindow();
    if (server_gone) std::fprintf(stdout, "client: server said Bye\n");
    bool ok = s.finish();
    s.recorder().print_line(stdout, {"c.view_rtt_us", "c.load_us", "c.read_present_us", "c.draw_us", "c.present_us",
                                     "c.snap_bytes", "c.visible_count"});
    if (opt.check_on()) s.checks().print(stdout);
    return ok ? 0 : 1;
}

} // namespace mapdemo::client
