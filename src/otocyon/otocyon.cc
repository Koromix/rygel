// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "../core/libcc/libcc.hh"
#include "../../vendor/raylib/src/raylib.h"
#include "../../vendor/raylib/src/raymath.h"
#include "../../vendor/raylib/src/rlgl.h"
#include "math.hh"

namespace RG {

static BucketArray<Texture> textures;
static HashMap<const char *, Texture *> textures_map;

static struct {
    int width;
    int height;
} screen;

static struct {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    bool fire = false;
} commands;

static struct {
    float width;
    float height;
} world;

static struct {
    Vector2 pos = { 300.0f, 300.0f };
} camera;

static struct {
    Vector2 pos = { 300.0f, 300.0f };
    Vector2 speed = { 0.0f, 0.0f };
    float angle;
} ship;

struct Projectile {
    Vector2 pos;
    Vector2 speed;
};
static HeapArray<Projectile> projectiles;

static BlockAllocator frame_alloc(Megabytes(4));

static void ReleaseAssets();
static bool InitAssets()
{
    RG_DEFER_N(out_guard) { ReleaseAssets(); };

    Span<const AssetInfo> assets = GetPackedAssets();

    for (const AssetInfo &asset: assets) {
        const char *ext = GetPathExtension(asset.name).ptr;

        if (TestStr(ext, ".png") || TestStr(ext, ".jpg")) {
            Image img = LoadImageFromMemory(ext + 1, asset.data.ptr, (int)asset.data.len);
            if (!img.data) {
                LogError("Failed to load '%1'", asset.name);
                return false;
            }
            RG_DEFER { UnloadImage(img); };

            Texture tex = LoadTextureFromImage(img);
            if (tex.id <= 0) {
                LogError("Failed to create texture for '%1'", asset.name);
                return false;
            }
            SetTextureFilter(tex, FILTER_BILINEAR);

            Texture *ptr = textures.Append(tex);
            textures_map.Set(asset.name, ptr);
        } else {
            LogError("Ignoring unknown asset type for '%1'", asset.name);
        }
    }

    out_guard.Disable();
    return true;
}

static void ReleaseAssets()
{
    for (Texture &tex: textures) {
        UnloadTexture(tex);
    }
}

static void InitWorld()
{
    world.width = 3000.0f;
    world.height = 1400.0f;
}

static void Input()
{
    commands.up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    commands.down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
    commands.left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
    commands.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    commands.fire = IsMouseButtonDown(0);
}

static float FixSmooth(float from, float to, float t1, float t2, float width) {
    float delta = t2 - t1;

    float a = abs(from - to) / (delta * width);
    float b = t1 / delta;
    float f = sqrtf(a - b) / 160.0f;

    float value = (1 - f) * from + f * to;
    return value;
}

static void Follow(Vector2 pos, float t1, float t2) {
    t1 /= 2.0f;
    t2 /= 2.0f;

    if (camera.pos.x < pos.x - screen.width * t1) {
        camera.pos.x = FixSmooth(camera.pos.x, pos.x, t1, t2, (float)screen.width);
    } else if (camera.pos.x > pos.x + screen.width * t1) {
        camera.pos.x = FixSmooth(camera.pos.x, pos.x, t1, t2, (float)screen.width);
    }
    if (camera.pos.y < pos.y - screen.height * t1) {
        camera.pos.y = FixSmooth(camera.pos.y, pos.y, t1, t2, (float)screen.height);
    } else if (camera.pos.y > pos.y + screen.height * t1) {
        camera.pos.y = FixSmooth(camera.pos.y, pos.y, t1, t2, (float)screen.height);
    }

    camera.pos.x = std::clamp(camera.pos.x, 0.0f, world.width);
    camera.pos.y = std::clamp(camera.pos.y, 0.0f, world.height);
}

static void Update()
{
    Follow(ship.pos, 0.3f, 0.6f);

    // Ship
    {
        // Point to mouse
        Vector2 mouse = GetMousePosition();
        mouse.x -= screen.width / 2.0f - camera.pos.x;
        mouse.y -= screen.height / 2.0f - camera.pos.y;
        ship.angle = atan2(-mouse.y + ship.pos.y, mouse.x - ship.pos.x);

        // Thrust
        if (commands.up || commands.down) {
            float main_accel =  0.01f * commands.up +
                               -0.006f * commands.down;

            ship.speed.x += main_accel * cosf(ship.angle);
            ship.speed.y -= main_accel * sinf(ship.angle);
        }
        if (commands.left || commands.right) {
            float side_accel = -0.006f * commands.left +
                                0.006f * commands.right;

            ship.speed.x += side_accel * cosf(ship.angle - PI / 2.0f);
            ship.speed.y -= side_accel * sinf(ship.angle - PI / 2.0f);
        }

        // Gravity
        ship.speed.y += 0.005f;

        // Speed limit
        {
            float speed = sqrtf(ship.speed.x * ship.speed.x + ship.speed.y * ship.speed.y);

            if (speed > 2.5f) {
                ship.speed.x *= 2.5f / speed;
                ship.speed.y *= 2.5f / speed;
            }
        }

        // Apply speed
        ship.pos = Vector2Add(ship.pos, ship.speed);

        // Borders
        if (ship.pos.x - 20.0f < 0.0f) {
            ship.pos.x = 20.0f;
            ship.speed.x *= -0.5f;
            ship.speed.y *= 0.5f;
        }
        if (ship.pos.x + 20.0f > world.width) {
            ship.pos.x = world.width - 20.0f;
            ship.speed.x *= -0.5f;
            ship.speed.y *= 0.5f;
        }
        if (ship.pos.y - 20.0f < 0.0f) {
            ship.pos.y = 20.0f;
            ship.speed.x *= 0.5f;
            ship.speed.y *= -0.5f;
        }
        if (ship.pos.y + 20.0f > world.height) {
            ship.pos.y = world.height - 20.0f;
            ship.speed.x *= 0.5f;
            ship.speed.y *= -0.5f;
        }

        // Fire
        if (commands.fire) {
            Projectile pj = {};

            pj.pos.x = ship.pos.x + 20.0f * cosf(ship.angle);
            pj.pos.y = ship.pos.y - 20.0f * sinf(ship.angle);
            pj.speed.x = 2.5f * cosf(ship.angle);
            pj.speed.y = -2.5f * sinf(ship.angle);

            projectiles.Append(pj);
        }
    }

    // Projectiles
    {
        Size end = 0;
        for (Size i = 0; i < projectiles.len; i++) {
            Projectile &pj = projectiles[i];

            if (pj.pos.x < 0.0f || pj.pos.x > world.width || pj.pos.y < 0.0f || pj.pos.y > world.height)
                continue;

            pj.pos = Vector2Add(pj.pos, pj.speed);

            projectiles[end++] = pj;
        }
        projectiles.RemoveFrom(end);
    }
}

static void Draw()
{
    // Draw background (with parallax)
    {
        const Texture &tex = *textures_map.FindValue("backgrounds/lonely.jpg", nullptr);

        float pan_width = screen.width + world.width / 16.0f;
        float pan_height = screen.height + world.height / 16.0f;

        float ratio1 = pan_width / pan_height;
        float ratio2 = (float)tex.width / (float)tex.height;
        float factor = (ratio1 > ratio2) ? (pan_width / tex.width) : (pan_height / tex.height);

        float orig_x = -camera.pos.x / 16.0f - (tex.width * factor - pan_width) / 2.0f;
        float orig_y = -camera.pos.y / 16.0f - (tex.height * factor - pan_height) / 2.0f;

        DrawTexturePro(tex, { 0.0f, 0.0f, (float)tex.width, (float)tex.height },
                            { orig_x, orig_y, (float)tex.width * factor, (float)tex.height * factor },
                            { 0.0f, 0.0f }, 0.0f, WHITE);
    }

    // Draw game
    {
        rlPushMatrix();
        RG_DEFER { rlPopMatrix(); };

        rlTranslatef((float)screen.width / 2.0f - camera.pos.x, (float)screen.height / 2.0f - camera.pos.y, 0.0f);

        // Draw ship
        for (const Projectile &pj: projectiles) {
            float angle = atan2(pj.speed.y, pj.speed.x);

            const Color middle = { 46, 191, 116, 255 };
            const Color extrem = { 46, 191, 116, 0 };

            rlPushMatrix();
            rlTranslatef(pj.pos.x, pj.pos.y, 0.0f);
            rlRotatef(RadToDeg(angle) + 90.0f, 0, 0, 1);
            DrawRectangleGradientH(-3, -5, 3, 10, extrem, middle);
            DrawRectangleGradientH(0, -5, 3, 10, middle, extrem);
            rlPopMatrix();
        }

        // Draw ship
        {
            const Texture &tex = *textures_map.FindValue("sprites/ship.png", nullptr);
            DrawTexturePro(tex, { 0.0f, 0.0f, (float)tex.width, (float)tex.height },
                                { ship.pos.x, ship.pos.y, (float)(tex.width / 2), (float)(tex.height / 2) },
                                { (float)tex.width / 4, (float)tex.height / 4 }, -RadToDeg(ship.angle), WHITE);
        }
    }

    // HUD
    {
        const Texture &tex = *textures_map.FindValue("sprites/health.png", nullptr);
        DrawTexturePro(tex, { 0.0f, 0.0f, (float)tex.width, (float)tex.height },
                            { (float)screen.width - tex.width / 2 - 10.0f, (float)screen.height - tex.height / 2 - 10.0f,
                              (float)(tex.width / 2), (float)(tex.height / 2) }, { 0.0f, 0.0f }, 0.0f, WHITE);

        HeapArray<char> buf(&frame_alloc);
        Fmt(&buf, "Speed: %1\n", FmtDouble(sqrtf(ship.speed.x * ship.speed.x + ship.speed.y * ship.speed.y) * 100, 0));
        Fmt(&buf, "Projectiles: %1\n", projectiles.len);
        DrawText(buf.ptr, 10, 10, 20, WHITE);
    }
}

int Main(int argc, char **argv)
{
    InitWindow(1280, 720, "Otocyon");
    RG_DEFER { CloseWindow(); };

    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

    if (!InitAssets())
        return 1;
    RG_DEFER { ReleaseAssets(); };

    InitWorld();

    static double time = GetTime();
    static double updates = 1.0;

    while (!WindowShouldClose()) {
        screen.width = GetScreenWidth();
        screen.height = GetScreenHeight();

        Input();

        while (updates >= 1.0f) {
            updates -= 1.0f;
            Update();
        }

        BeginDrawing();
        Draw();
        EndDrawing();

        // Stabilize world time and physics
        {
            double prev_time = time;
            time = GetTime();

            updates += (time - prev_time) * 480.0;

            // Don't try to catch up if a long pause happens, such as when the window gets moved
            // or if the OS is busy.
            if (updates > 100) {
                updates = 0;
            }
        }

        frame_alloc.ReleaseAll();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
