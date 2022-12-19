/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2018 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#define GLFW_INCLUDE_NONE
#include "imgui_helper.h"
#include <math.h>

#include "imgui.h"
#include <fstream>

#include <DirectXMath.h>
#include <windows.h>


namespace ImGuiH {

template <class T>
inline T imguih_clamp(const T u, const T min, const T max)
{
    T o = (u < min) ? min : u;
    o = (o > max) ? max : o;
    return o;
}

float getDPIScale()
{
  // Cached DPI scale, so that this doesn't change after the first time code calls getDPIScale.
  // A negative value indicates that the value hasn't been computed yet.
  static float cached_dpi_scale = -1.0f;

  if(cached_dpi_scale < 0.0f)
  {
    // Compute the product of the monitor DPI scale and any DPI scale
    // set in the NVPRO_DPI_SCALE variable.
    cached_dpi_scale = 1.0f;

    auto activeWindow = GetActiveWindow();
    HMONITOR monitor = MonitorFromWindow(activeWindow, MONITOR_DEFAULTTONEAREST);

    MONITORINFOEX monitorInfoEx;
    monitorInfoEx.cbSize = sizeof(monitorInfoEx);
    GetMonitorInfo(monitor, &monitorInfoEx);
    auto cxLogical = monitorInfoEx.rcMonitor.right - monitorInfoEx.rcMonitor.left;
    auto cyLogical = monitorInfoEx.rcMonitor.bottom - monitorInfoEx.rcMonitor.top;

    // Get the physical width and height of the monitor
    DEVMODE devMode;
    devMode.dmSize = sizeof(devMode);
    devMode.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfoEx.szDevice, ENUM_CURRENT_SETTINGS, &devMode);
    auto cxPhysical = devMode.dmPelsWidth;
    auto cyPhysical = devMode.dmPelsHeight;

    // Calculate the scaling factor
    float horizontalScale = static_cast<float>(((double)cxPhysical / (double)cxLogical));
    float verticalScale = static_cast<float>(((double)cyPhysical / (double)cyLogical));

    cached_dpi_scale = horizontalScale;

    cached_dpi_scale = (cached_dpi_scale > 0.0f ? cached_dpi_scale : 1.0f);
  }

  return cached_dpi_scale;
}

//--------------------------------------------------------------------------------------------------
// Setting a dark style for the GUI
// The colors were coded in sRGB color space, set the useLinearColor
// flag to convert to linear color space.
void setStyle(bool useLinearColor)
{
  typedef ImVec4 (*srgbFunction)(float, float, float, float);
  srgbFunction passthrough = [](float r, float g, float b, float a) -> ImVec4 { return ImVec4(r, g, b, a); };
  srgbFunction toLinear    = [](float r, float g, float b, float a) -> ImVec4 {
    auto toLinearScalar = [](float u) -> float {
      return u <= 0.04045 ? 25 * u / 323.f : powf((200 * u + 11) / 211.f, 2.4f);
    };
    return ImVec4(toLinearScalar(r), toLinearScalar(g), toLinearScalar(b), a);
  };
  srgbFunction srgb = useLinearColor ? toLinear : passthrough;

  ImGui::StyleColorsDark();

  ImGuiStyle& style                  = ImGui::GetStyle();
  style.WindowRounding               = 0.0f;
  style.WindowBorderSize             = 0.0f;
  style.ColorButtonPosition          = ImGuiDir_Right;
  style.FrameRounding                = 2.0f;
  style.FrameBorderSize              = 1.0f;
  style.GrabRounding                 = 4.0f;
  style.IndentSpacing                = 12.0f;
  style.Colors[ImGuiCol_WindowBg]    = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_MenuBarBg]   = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarBg] = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_PopupBg]     = srgb(0.135f, 0.135f, 0.135f, 1.0f);
  style.Colors[ImGuiCol_Border]      = srgb(0.4f, 0.4f, 0.4f, 0.5f);
  style.Colors[ImGuiCol_FrameBg]     = srgb(0.05f, 0.05f, 0.05f, 0.5f);

  // Normal
  ImVec4                normal_color = srgb(0.465f, 0.465f, 0.525f, 1.0f);
  std::vector<ImGuiCol> to_change_nrm;
  to_change_nrm.push_back(ImGuiCol_Header);
  to_change_nrm.push_back(ImGuiCol_SliderGrab);
  to_change_nrm.push_back(ImGuiCol_Button);
  to_change_nrm.push_back(ImGuiCol_CheckMark);
  to_change_nrm.push_back(ImGuiCol_ResizeGrip);
  to_change_nrm.push_back(ImGuiCol_TextSelectedBg);
  to_change_nrm.push_back(ImGuiCol_Separator);
  to_change_nrm.push_back(ImGuiCol_FrameBgActive);
  for(auto c : to_change_nrm)
  {
    style.Colors[c] = normal_color;
  }

  // Active
  ImVec4                active_color = srgb(0.365f, 0.365f, 0.425f, 1.0f);
  std::vector<ImGuiCol> to_change_act;
  to_change_act.push_back(ImGuiCol_HeaderActive);
  to_change_act.push_back(ImGuiCol_SliderGrabActive);
  to_change_act.push_back(ImGuiCol_ButtonActive);
  to_change_act.push_back(ImGuiCol_ResizeGripActive);
  to_change_act.push_back(ImGuiCol_SeparatorActive);
  for(auto c : to_change_act)
  {
    style.Colors[c] = active_color;
  }

  // Hovered
  ImVec4                hovered_color = srgb(0.565f, 0.565f, 0.625f, 1.0f);
  std::vector<ImGuiCol> to_change_hover;
  to_change_hover.push_back(ImGuiCol_HeaderHovered);
  to_change_hover.push_back(ImGuiCol_ButtonHovered);
  to_change_hover.push_back(ImGuiCol_FrameBgHovered);
  to_change_hover.push_back(ImGuiCol_ResizeGripHovered);
  to_change_hover.push_back(ImGuiCol_SeparatorHovered);
  for(auto c : to_change_hover)
  {
    style.Colors[c] = hovered_color;
  }


  style.Colors[ImGuiCol_TitleBgActive]    = srgb(0.465f, 0.465f, 0.465f, 1.0f);
  style.Colors[ImGuiCol_TitleBg]          = srgb(0.125f, 0.125f, 0.125f, 1.0f);
  style.Colors[ImGuiCol_Tab]              = srgb(0.05f, 0.05f, 0.05f, 0.5f);
  style.Colors[ImGuiCol_TabHovered]       = srgb(0.465f, 0.495f, 0.525f, 1.0f);
  style.Colors[ImGuiCol_TabActive]        = srgb(0.282f, 0.290f, 0.302f, 1.0f);
  style.Colors[ImGuiCol_ModalWindowDimBg] = srgb(0.465f, 0.465f, 0.465f, 0.350f);

  //Colors_ext[ImGuiColExt_Warning] = srgb (1.0f, 0.43f, 0.35f, 1.0f);

  ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
}

//
// Local, return true if the filename exist
//
static bool fileExists(const char* filename)
{
  std::ifstream stream;
  stream.open(filename);
  return stream.is_open();
}

//--------------------------------------------------------------------------------------------------
// Looking for TTF fonts, first on the VULKAN SDK, then Windows default fonts
//

void setFonts(FontMode fontmode)
{
  ImGuiIO&    io             = ImGui::GetIO();
  const float high_dpi_scale = getDPIScale();


  // Nicer fonts
  ImFont* font = nullptr;
  if(fontmode == FONT_MONOSPACED_SCALED)
  {
    if(font == nullptr)
    {
      const std::string p = R"(C:/Windows/Fonts/consola.ttf)";
      if(fileExists(p.c_str()))
        font = io.Fonts->AddFontFromFileTTF(p.c_str(), 12.0f * high_dpi_scale);
    }
    if(font == nullptr)
    {
      const std::string p = "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf";
      if(fileExists(p.c_str()))
        font = io.Fonts->AddFontFromFileTTF(p.c_str(), 12.0f * high_dpi_scale);
    }
  }
  else if(fontmode == FONT_PROPORTIONAL_SCALED)
  {
    if(font == nullptr)
    {
      const std::string p = R"(C:/Windows/Fonts/segoeui.ttf)";
      if(fileExists(p.c_str()))
        font = io.Fonts->AddFontFromFileTTF(p.c_str(), 16.0f * high_dpi_scale);
    }
    if(font == nullptr)
    {
      const std::string p = "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf";
      if(fileExists(p.c_str()))
        font = io.Fonts->AddFontFromFileTTF(p.c_str(), 16.0f * high_dpi_scale);
    }
  }

  if(font == nullptr)
  {
    ImFontConfig font_config = ImFontConfig();
    font_config.SizePixels   = 13.0f * high_dpi_scale;  // 13 is the default font size
    io.Fonts->AddFontDefault(&font_config);
  }
}

void tooltip(const char* description, bool questionMark /*= false*/, float timerThreshold /*= 0.5f*/)
{
  bool passTimer = GImGui->HoveredIdTimer >= timerThreshold && GImGui->ActiveIdTimer == 0.0f;
  if(questionMark)
  {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    passTimer = true;
  }

  if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && passTimer)
  {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}


// ------------------------------------------------------------------------------------------------

namespace {

template <typename TScalar, ImGuiDataType type, uint8_t dim>
bool show_slider_control_scalar(TScalar* value, TScalar* min, TScalar* max, const char* format)
{
  static const char* visible_labels[] = {"x:", "y:", "z:", "w:"};

  if(dim == 1)
    return ImGui::SliderScalar("##hidden", type, &value[0], &min[0], &max[0], format);

  float indent  = ImGui::GetCursorPos().x;
  bool  changed = false;
  for(uint8_t c = 0; c < dim; ++c)
  {
    ImGui::PushID(c);
    if(c > 0)
    {
      ImGui::NewLine();
      ImGui::SameLine(indent);
    }
    ImGui::Text("%s", visible_labels[c]);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    changed |= ImGui::SliderScalar("##hidden", type, &value[c], &min[c], &max[c], format);
    ImGui::PopID();
  }
  return changed;
}


}  // namespace

template <>
bool Control::show_slider_control<float>(float* value, float& min, float& max, const char* format)
{
  return show_slider_control_scalar<float, ImGuiDataType_Float, 1>(value, &min, &max, format ? format : "%.3f");
}

template <>
bool Control::show_slider_control<DirectX::XMFLOAT2>(DirectX::XMFLOAT2* value, DirectX::XMFLOAT2& min, DirectX::XMFLOAT2& max, const char* format)
{
  return show_slider_control_scalar<float, ImGuiDataType_Float, 2>(&value->x, &min.x, &max.x, format ? format : "%.3f");
}

template <>
bool Control::show_slider_control<DirectX::XMFLOAT3>(DirectX::XMFLOAT3* value, DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max, const char* format)
{
  return show_slider_control_scalar<float, ImGuiDataType_Float, 3>(&value->x, &min.x, &max.x, format ? format : "%.3f");
}

template <>
bool Control::show_slider_control<DirectX::XMFLOAT4>(DirectX::XMFLOAT4* value, DirectX::XMFLOAT4& min, DirectX::XMFLOAT4& max, const char* format)
{
  return show_slider_control_scalar<float, ImGuiDataType_Float, 4>(&value->x, &min.x, &max.x, format ? format : "%.3f");
}

template <>
bool Control::show_drag_control<float>(float* value, float speed, float& min, float& max, const char* format)
{
  return show_drag_control_scalar<float, ImGuiDataType_Float, 1>(value, speed, &min, &max, format ? format : "%.3f");
}

template <>
bool Control::show_drag_control<DirectX::XMFLOAT2>(DirectX::XMFLOAT2* value, float speed, DirectX::XMFLOAT2& min, DirectX::XMFLOAT2& max, const char* format)
{
  return show_drag_control_scalar<float, ImGuiDataType_Float, 2>(&value->x, speed, &min.x, &max.x, format ? format : "%.3f");
}

template <>
bool Control::show_drag_control<DirectX::XMFLOAT3>(DirectX::XMFLOAT3* value, float speed, DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max, const char* format)
{
  return show_drag_control_scalar<float, ImGuiDataType_Float, 3>(&value->x, speed, &min.x, &max.x, format ? format : "%.3f");
}

template <>
bool Control::show_drag_control<DirectX::XMFLOAT4>(DirectX::XMFLOAT4* value, float speed, DirectX::XMFLOAT4& min, DirectX::XMFLOAT4& max, const char* format)
{
  return show_drag_control_scalar<float, ImGuiDataType_Float, 4>(&value->x, speed, &min.x, &max.x, format ? format : "%.3f");
}


template <>
bool Control::show_slider_control<int>(int* value, int& min, int& max, const char* format)
{
  return show_slider_control_scalar<int, ImGuiDataType_S32, 1>(value, &min, &max, format ? format : "%d");
}

template <>
bool Control::show_slider_control<DirectX::XMINT2>(DirectX::XMINT2* value, DirectX::XMINT2& min, DirectX::XMINT2& max, const char* format)
{
  return show_slider_control_scalar<int, ImGuiDataType_S32, 2>(&value->x, &min.x, &max.x, format ? format : "%d");
}

template <>
bool Control::show_slider_control<DirectX::XMINT3>(DirectX::XMINT3* value, DirectX::XMINT3& min, DirectX::XMINT3& max, const char* format)
{
  return show_slider_control_scalar<int, ImGuiDataType_S32, 3>(&value->x, &min.x, &max.x, format ? format : "%d");
}

template <>
bool Control::show_slider_control<DirectX::XMINT4>(DirectX::XMINT4* value, DirectX::XMINT4& min, DirectX::XMINT4& max, const char* format)
{
  return show_slider_control_scalar<int, ImGuiDataType_S32, 4>(&value->x, &min.x, &max.x, format ? format : "%d");
}

template <>
bool Control::show_drag_control<int>(int* value, float speed, int& min, int& max, const char* format)
{
  return show_drag_control_scalar<int, ImGuiDataType_S32, 1>(value, speed, &min, &max, format ? format : "%d");
}

template <>
bool Control::show_drag_control<DirectX::XMINT2>(DirectX::XMINT2* value, float speed, DirectX::XMINT2& min, DirectX::XMINT2& max, const char* format)
{
  return show_drag_control_scalar<int, ImGuiDataType_S32, 2>(&value->x, speed, &min.x, &max.x, format ? format : "%d");
}

template <>
bool Control::show_drag_control<DirectX::XMINT3>(DirectX::XMINT3* value, float speed, DirectX::XMINT3& min, DirectX::XMINT3& max, const char* format)
{
  return show_drag_control_scalar<int, ImGuiDataType_S32, 3>(&value->x, speed, &min.x, &max.x, format ? format : "%d");
}

template <>
bool Control::show_drag_control<DirectX::XMINT4>(DirectX::XMINT4* value, float speed, DirectX::XMINT4& min, DirectX::XMINT4& max, const char* format)
{
  return show_drag_control_scalar<int, ImGuiDataType_S32, 4>(&value->x, speed, &min.x, &max.x, format ? format : "%d");
}


template <>
bool Control::show_slider_control<uint32_t>(uint32_t* value, uint32_t& min, uint32_t& max, const char* format)
{
  return show_slider_control_scalar<uint32_t, ImGuiDataType_U32, 1>(value, &min, &max, format ? format : "%d");
}

template <>
bool Control::show_slider_control<size_t>(size_t* value, size_t& min, size_t& max, const char* format)
{
  return show_slider_control_scalar<size_t, ImGuiDataType_U64, 1>(value, &min, &max, format ? format : "%d");
}

template <>
bool Control::show_drag_control<size_t>(size_t* value, float speed, size_t& min, size_t& max, const char* format)
{
  return show_drag_control_scalar<size_t, ImGuiDataType_U64, 1>(value, speed, &min, &max, format ? format : "%d");
}

// Static member declaration
ImGuiID Panel::dockspaceID{0};

void Panel::Begin(Side side /*= Side::Right*/, float alpha /*= 0.5f*/, char* name /*= nullptr*/)
{
  // Keeping the unique ID of the dock space
  dockspaceID = ImGui::GetID("DockSpace");

  // The dock need a dummy window covering the entire viewport.
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  // All flags to dummy window
  ImGuiWindowFlags host_window_flags = 0;
  host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
  host_window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
  host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  host_window_flags |= ImGuiWindowFlags_NoBackground;

  // Starting dummy window
  char label[32];
  ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin(label, nullptr, host_window_flags);
  ImGui::PopStyleVar(3);

  // The central node is transparent, so that when UI is draw after, the image is visible
  // Auto Hide Bar, no title of the panel
  // Center is not dockable, that is for the scene
  ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar
                                      | ImGuiDockNodeFlags_NoDockingInCentralNode;

  // Default panel/window is name setting
  std::string dock_name("Settings");
  if(name != nullptr)
    dock_name = name;

  // Building the splitting of the dock space is done only once
  if(!ImGui::DockBuilderGetNode(dockspaceID))
  {
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, dockspaceFlags | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

    ImGuiID dock_main_id = dockspaceID;

    // Slitting all 4 directions, targetting (320 pixel * DPI) panel width, (180 pixel * DPI) panel height.
    float ratioTemp = 320.0f * getDPIScale() / viewport->WorkSize[0];
    ratioTemp = ratioTemp < 0.01f ? 0.01f : ratioTemp;
    const float xRatio = imguih_clamp<float>(320.0f * getDPIScale() / viewport->WorkSize[0], 0.01f, 0.499f);
    const float yRatio = imguih_clamp<float>(180.0f * getDPIScale() / viewport->WorkSize[1], 0.01f, 0.499f);
    ImGuiID     id_left, id_right, id_up, id_down;

    // Note, for right, down panels, we use the n / (1 - n) formula to correctly split the space remaining from the left, up panels.
    id_left  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, xRatio, nullptr, &dock_main_id);
    id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, xRatio / (1 - xRatio), nullptr, &dock_main_id);
    id_up    = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, yRatio, nullptr, &dock_main_id);
    id_down  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, yRatio / (1 - yRatio), nullptr, &dock_main_id);

    ImGui::DockBuilderDockWindow(side == Side::Left ? dock_name.c_str() : "Dock_left", id_left);
    ImGui::DockBuilderDockWindow(side == Side::Right ? dock_name.c_str() : "Dock_right", id_right);
    ImGui::DockBuilderDockWindow("Dock_up", id_up);
    ImGui::DockBuilderDockWindow("Dock_down", id_down);
    ImGui::DockBuilderDockWindow("Scene", dock_main_id);  // Center

    ImGui::DockBuilderFinish(dock_main_id);
  }

  // Setting the panel to blend with alpha
  ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(col.x, col.y, col.z, alpha));

  ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);
  ImGui::PopStyleColor();
  ImGui::End();

  // The panel
  if(alpha < 1)
    ImGui::SetNextWindowBgAlpha(alpha);  // For when the panel becomes a floating window
  ImGui::Begin(dock_name.c_str());
}

Control::Style Control::style{};

}  // namespace ImGuiH
