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
#include "math.h"

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
            Image img = LoadImageFromMemory(ext + 1, asset.data.ptr, asset.data.len);
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

static inline float RadToDeg(double angle)
{
    return (float)(angle * 180.0 / PI);
}

static void Input()
{
    commands.up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    commands.down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
    commands.left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
    commands.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    commands.fire = IsMouseButtonDown(0);
}

static void Update()
{
    // Ship
    {
        // Point to mouse
        Vector2 mouse = GetMousePosition();
        ship.angle = atan2(-mouse.y + ship.pos.y, mouse.x - ship.pos.x);

        // Thrust
        if (commands.up || commands.down) {
            float main_accel =  0.02f * commands.up +
                               -0.012f * commands.down;

            ship.speed.x += main_accel * cosf(ship.angle);
            ship.speed.y -= main_accel * sinf(ship.angle);
        }
        if (commands.left || commands.right) {
            float side_accel = -0.012f * commands.left +
                                0.012f * commands.right;

            ship.speed.x += side_accel * cosf(ship.angle - PI / 2.0f);
            ship.speed.y -= side_accel * sinf(ship.angle - PI / 2.0f);
        }

        // Gravity
        ship.speed.y += 0.01f;

        // Apply speed
        ship.pos = Vector2Add(ship.pos, ship.speed);

        // Borders
        if (ship.pos.x - 20.0f < 0.0f) {
            ship.pos.x = 20.0f;
            ship.speed.x *= -0.5f;
            ship.speed.y *= 0.5f;
        }
        if (ship.pos.x + 20.0f > screen.width) {
            ship.pos.x = screen.width - 20.0f;
            ship.speed.x *= -0.5f;
            ship.speed.y *= 0.5f;
        }
        if (ship.pos.y - 20.0f < 0.0f) {
            ship.pos.y = 20.0f;
            ship.speed.x *= 0.5f;
            ship.speed.y *= -0.5f;
        }
        if (ship.pos.y + 20.0f > screen.height) {
            ship.pos.y = screen.height - 20.0f;
            ship.speed.x *= 0.5f;
            ship.speed.y *= -0.5f;
        }

        // Fire
        if (commands.fire) {
            Projectile pj = {};

            pj.pos.x = ship.pos.x + 20.0f * cosf(ship.angle);
            pj.pos.y = ship.pos.y - 20.0f * sinf(ship.angle);
            pj.speed.x = 5.0f * cosf(ship.angle);
            pj.speed.y = -5.0f * sinf(ship.angle);

            projectiles.Append(pj);
        }
    }

    // Projectiles
    {
        Size end = 0;
        for (Size i = 0; i < projectiles.len; i++) {
            Projectile &pj = projectiles[i];

            if (pj.pos.x < 0.0f || pj.pos.x > screen.width || pj.pos.y < 0.0f || pj.pos.y > screen.height)
                continue;

            pj.pos = Vector2Add(pj.pos, pj.speed);

            projectiles[end++] = pj;
        }
        projectiles.RemoveFrom(end);
    }
}

static void Draw()
{
    // Draw background
    {
        const Texture &tex = *textures_map.FindValue("backgrounds/lonely.jpg", nullptr);
        DrawTexturePro(tex, { 0.0f, 0.0f, (float)tex.width, (float)tex.height },
                            { 0.0f, 0.0f, (float)screen.width, (float)screen.height },
                            { 0.0f, 0.0f }, 0.0f, WHITE);
    }

    // Draw projectiles
    for (const Projectile &pj: projectiles) {
        float angle = atan2(pj.speed.y, pj.speed.x);

        const Color middle = { 46, 191, 116, 255 };
        const Color extrem = { 46, 191, 116, 0 };

        rlPushMatrix();
        rlTranslatef(pj.pos.x, pj.pos.y, 0);
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

    // HUD
    {
        const Texture &tex = *textures_map.FindValue("sprites/health.png", nullptr);
        DrawTexturePro(tex, { 0.0f, 0.0f, (float)tex.width, (float)tex.height },
                            { (float)screen.width - tex.width / 2 - 10.0f, (float)screen.height - tex.height / 2 - 10.0f,
                              (float)(tex.width / 2), (float)(tex.height / 2) }, { 0.0f, 0.0f }, 0.0f, WHITE);
    }

    // Debug
    {
        const char *text = Fmt(&frame_alloc, "Projectiles: %1", projectiles.len).ptr;
        DrawText(text, 10, 10, 20, WHITE);
    }
}

int Main(int argc, char **argv)
{
    InitWindow(1280, 720, "Otocyon");
    RG_DEFER { CloseWindow(); };

    if (!InitAssets())
        return 1;
    RG_DEFER { ReleaseAssets(); };

    static double time = GetTime();
    static double updates = 1.0;

    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

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

            updates += (time - prev_time) * 240.0;
        }

        frame_alloc.ReleaseAll();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
