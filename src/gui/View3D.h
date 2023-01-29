#pragma once

#include "chisel/Tools.h"
#include "gui/Window.h"
#include "chisel/Selection.h"
#include "chisel/Handles.h"
#include "input/Input.h"
#include "input/Keyboard.h"
#include "platform/Cursor.h"
#include "common/Time.h"
#include "console/ConVar.h"

#include "core/Camera.h"
#include "core/Transform.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include "gui/IconsMaterialCommunity.h"
#include "render/Render.h"

static constexpr float CHISEL_PI = 3.14159265358979323846f;

/// Convert a value from degrees to radians
constexpr float DegreesToRadians(float inV)
{
	return inV * (CHISEL_PI / 180.0f);
}

/// Convert a value from radians to degrees
constexpr float RadiansToDegrees(float inV)
{
	return inV * (180.0f / CHISEL_PI);
}

inline float AngleNormalize(float angle)
{
	angle = fmodf(angle, 360.0f);
	if (angle > +180) { angle -= 360; }
	if (angle < -180) { angle += 360; }
	return angle;
}

namespace chisel
{
    static ConVar<float> sensitivity ("sensitivity",   6.0f, "Mouse sensitivity");
    static ConVar<float> sv_maxspeed ("sv_maxspeed",   700.0f, "Max speed");

    static ConVar<float> cl_pitchup  ("cl_pitchup",   +89.0f, "Set the max pitch value.");
    static ConVar<float> cl_pitchdown("cl_pitchdown", -89.0f, "Set the min pitch value.");

    static ConVar<float> m_pitch("m_pitch", 0.022f, "Mouse pitch factor.");
    static ConVar<float> m_yaw  ("m_yaw",   0.022f, "Mouse yaw factor.");

    struct View3D : public GUI::Window
    {
        View3D(auto... args) : GUI::Window(args..., ImGuiWindowFlags_MenuBar) {}

        using Tool = Handles::Tool;

        Tool  activeTool    = Tool::Translate;
        Space space         = Space::World;
        Rect  viewport;
        bool  allowAxisFlip = true;

        bool showGrid       = true;
        bool gridSnap       = true;
        vec3 gridSize       = vec3(64.0f);
        bool gridUniform    = true;

        bool  popupOpen     = false;

    // Virtual Methods //

        virtual void DrawHandles(mat4x4& view, mat4x4& proj) {}
        virtual void OnPostDraw() {}

    // Draw Modes //

        enum class DrawMode {
            Shaded, Depth
        };

        static inline const char* drawModes[] = { "Shaded", "Depth" };

        DrawMode drawMode = DrawMode::Shaded;

        auto GetTexture(DrawMode mode)
        {
            switch (mode) {
                default: case DrawMode::Shaded:
                    return Tools.rt_SceneView->GetTexture();
                case DrawMode::Depth:
                    return Tools.rt_SceneView->GetDepthTexture();
            }
        }

    // UI //

        bool BeginMenu(const char* label, bool enabled = true)
        {
            bool open = ImGui::BeginMenu(label, enabled);

            // Prevent click on viewport when clicking menu
            if (open && ImGui::IsWindowHovered())
                popupOpen = true;
            return open;
        }

        void NoPadding() {
            // Set window padding to 0
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        }

        void ResetPadding() {
            ImGui::PopStyleVar();
        }

        void PreDraw() override
        {
            NoPadding();
        }

        void PostDraw() override final
        {
            ResetPadding();

            if (!visible)
                return;

            // HACK: Set hovered window to NULL,
            // this fixes mouse over with ImGui docking
            ImGuiContext& g = *ImGui::GetCurrentContext();
            ImGuiWindow* hovered = g.HoveredWindow;
            g.HoveredWindow = NULL;

            Handles.Begin(viewport, allowAxisFlip);

            Camera& camera = Tools.editorCamera.camera;

            // Get camera matrices
            mat4x4 view = camera.ViewMatrix();
            mat4x4 proj = camera.ProjMatrix();

            DrawHandles(view, proj);

            // HACK: Reset hovered window
            g.HoveredWindow = hovered;

            // Begin scene view extra rendering
            render::Render& r = Tools.Render;
            r.SetRenderTarget(Tools.rt_SceneView);

            // Draw grid
            if (showGrid)
                Handles.DrawGrid(r, Tools.sh_Grid);

            OnPostDraw();
        }

        void Draw() override
        {
            popupOpen = false;
            ResetPadding();

        // Menu Bar //
            if (ImGui::BeginMenuBar())
            {
                // Left side
                CoordinateSpacePicker();
                GridMenu();

                // Right side
                ImGui::SameLine(ImGui::GetWindowWidth() - 90);
                if (BeginMenu(ICON_MC_IMAGE_MULTIPLE " " ICON_MC_MENU_DOWN))
                {
                    for (int i = 0; i < sizeof(drawModes) / sizeof(const char*); i++)
                    {
                        if (ImGui::MenuItem(drawModes[i], "", drawMode == DrawMode(i)))
                            drawMode = DrawMode(i);
                    }
                    ImGui::EndMenu();
                }

                // Right side
                ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                if (BeginMenu(ICON_MC_VIDEO " " ICON_MC_MENU_DOWN))
                {
                    Camera& camera = Tools.editorCamera.camera;
                    ImGui::TextUnformatted("Scene Camera");
                    ImGui::InputFloat("FOV", &camera.fieldOfView);
                    ImGui::InputFloat("Speed (hu/s)", &sv_maxspeed.value);
                    ImGui::InputFloat("Sensitivity", &sensitivity.value);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            NoPadding();

            // Return if window is collapsed
            if (!CheckResize()) {
                return;
            }

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetContentRegionAvail();
            ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

            // Copy from scene view render target into viewport
            ImGui::GetWindowDrawList()->AddImage(
                GetTexture(drawMode),
                pos, max,
                ImVec2(0, 0), ImVec2(1, 1)
            );

            viewport = Rect(pos.x, pos.y, size.x, size.y);

            Toolbar();

            // If mouse is over viewport,
            if (ImGui::IsMouseHoveringRect(pos, max))
            {
                Camera& camera = Tools.editorCamera.camera;

                // Left-click: Select (or transform selection)
                if (Mouse.GetButtonDown(Mouse::Left) && !popupOpen && (Selection.Empty() || !Handles.IsMouseOver()))
                {
                    ImVec2 absolute = ImGui::GetMousePos();
                    uint2 mouse = uint2(absolute.x - pos.x, absolute.y - pos.y);
                    Tools.PickObject(mouse);
                }

                if (!ImGui::GetIO().KeyCtrl && ImGui::IsWindowHovered())
                {
                    // Right-click and hold (or press Z) to mouselook
                    // TODO: Make Z toggle instead of hold
                    if (Mouse.GetButtonDown(Mouse::Right) || Keyboard.GetKeyDown(Key::Z))
                    {
                        Cursor.SetMode(Cursor::Locked);
                        Cursor.SetVisible(false);
                    }

                    if (Mouse.GetButton(Mouse::Right) || Keyboard.GetKey(Key::Z))
                    {
                        int2 mouse = Mouse.GetMotion();
                        if (camera.rightHanded)
                            mouse.x = -mouse.x;

                        camera.yaw   = AngleNormalize(camera.yaw + DegreesToRadians(mouse.x * sensitivity * m_yaw));
                        camera.pitch = std::clamp(camera.pitch - DegreesToRadians(mouse.y * sensitivity * m_pitch), DegreesToRadians(cl_pitchdown), DegreesToRadians(cl_pitchup));
                    }

                    if (Mouse.GetButtonUp(Mouse::Right) || Keyboard.GetKeyUp(Key::Z))
                    {
                        Cursor.SetMode(Cursor::Normal);
                        Cursor.SetVisible(true);
                    }

                    // WASD. TODO: Virtual axes, arrow keys
                    float w = Keyboard.GetKey(Key::W) ? 1.f : 0.f;
                    float s = Keyboard.GetKey(Key::S) ? 1.f : 0.f;
                    float a = Keyboard.GetKey(Key::A) ? 1.f : 0.f;
                    float d = Keyboard.GetKey(Key::D) ? 1.f : 0.f;

                    camera.position += camera.forward() * (w-s) * sv_maxspeed.value * float(Time.deltaTime);
                    camera.position += camera.right() * (d-a) * sv_maxspeed.value * float(Time.deltaTime);
                }
            }
        }

        // Returns true if window is not collapsed
        bool CheckResize()
        {
            // If window resized: resize render target
            ImVec2 size = ImGui::GetContentRegionAvail();
            if (size.x != width || size.y != height)
            {
                // Window is collapsed or too small
                if (size.x == 0 || size.y == 0) {
                    return false;
                }

                width = uint(size.x);
                height = uint(size.y);

                // Resize framebuffer or viewport
                Tools.ResizeViewport(width, height);

                return true;
            }

            return true;
        }

    // Coordinate Space Picker //

        void CoordinateSpacePicker()
        {
            const char* items[] = {
                ICON_MC_WEB " World",
                ICON_MC_CUBE_OUTLINE " Local"
            };
            int i = int(space);
            auto label = std::string(items[i]) + " " ICON_MC_MENU_DOWN;
            if (BeginMenu(label.c_str())) {
                for (size_t j = 0; j < std::size(items); j++) {
                    if (ImGui::MenuItem(items[j], "", space == Space(j))) {
                        space = Space(j);
                    }
                }
                ImGui::Checkbox("Axis Flip", &allowAxisFlip);
                ImGui::EndMenu();
            }
        }

    // Grid Menu //

        void GridMenu()
        {
            static bool showingGrid = true;

            char gridTabName[128];
            const bool isGridUniform = gridUniform || (gridSize.x == gridSize.y && gridSize.y == gridSize.z);
            if (isGridUniform)
                snprintf(gridTabName, sizeof(gridTabName), ICON_MC_GRID " %g " ICON_MC_MENU_DOWN, gridSize.x);
            else
                snprintf(gridTabName, sizeof(gridTabName), ICON_MC_GRID " %g %g %g " ICON_MC_MENU_DOWN, gridSize.x, gridSize.y, gridSize.z);
            if (BeginMenu(gridTabName))
            {
                ImGui::TextUnformatted("Grid");
                const char* label = showGrid
                    ? (ICON_MC_GRID " Show Grid")
                    : (ICON_MC_GRID_OFF " Show Grid");
                ImGui::Checkbox(label, &showGrid);
                ImGui::Checkbox(ICON_MC_MAGNET " Snap to Grid", &gridSnap);
                ImGui::Checkbox(ICON_MC_LINK " Uniform Grid Size", &gridUniform);
                if (gridUniform) {
                    if (ImGui::InputFloat("Grid Size", &gridSize.x))
                        gridSize = gridSize.xxx;
                } else {
                    ImGui::InputFloat3("Grid Size", &gridSize.x);
                }
                ImGui::EndMenu();
            }
            else
            {
                showingGrid = showGrid;
            }
        }

    // Toolbar //

        void Toolbar()
        {
            ImGui::Spacing();

            ImGui::Indent(5.f);
            ToolbarButton(ICON_MC_ARROW_ALL, Tool::Translate);
            ToolbarButton(ICON_MC_AUTORENEW, Tool::Rotate);
            ToolbarButton(ICON_MC_RESIZE, Tool::Scale);
            ToolbarButton(ICON_MC_ALPHA_U_BOX_OUTLINE, Tool::Universal);
            ImGui::Unindent(5.f);

        }

        void ToolbarButton(const char* label, Tool tool)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
            ImGuiCol col = activeTool == tool ? ImGuiCol_TabActive : ImGuiCol_WindowBg;
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(col));

            if (ImGui::Button(label)) {
                activeTool = tool;
            }
            if (ImGui::IsItemHovered()) {
                popupOpen = true;
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

    };
}
