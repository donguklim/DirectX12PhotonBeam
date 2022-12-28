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

/// \nodoc (keyword to exclude this file from automatic README.md generation)

//#ifndef NV_IMGUI_INCLUDED
#define NV_IMGUI_INCLUDED

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>


#include <algorithm>
#include <limits>
#include <array>
#include <climits>
#include <functional>
#include <string>
#include <vector>


enum ImGui_Extra
{
  ImGuiPlotType_Area = ImGuiPlotType_Histogram + 1
};


namespace ImGuiH {

enum FontMode
{
  FONT_DEFAULT_SCALED,
  FONT_PROPORTIONAL_SCALED,
  FONT_MONOSPACED_SCALED,
};

//--------------------------------------------------------------------------------------------------
//
// Setting common style across samples
//
void setStyle(bool useLinearColor = false);

//--------------------------------------------------------------------------------------------------
//
// Setting nicer default fonts
//
void setFonts(FontMode fontmode = FONT_PROPORTIONAL_SCALED);



// Display a tooltip for the previous item
void tooltip(const char* description, bool questionMark = false, float timerThreshold = 0.5f);



//--------------------------------------------------------------------------------------------------
// Creating a window panel
// - Panel will be on the left or the right side of the window.
// - It fills the height of the window, and stays on the side it was created.
class Panel /*static*/
{
  static ImGuiID dockspaceID;

public:
  // Side where the panel will be
  enum class Side
  {
    Left,
    Right,
  };


  // Starting the panel, equivalent to ImGui::Begin for a window. Need ImGui::end()
  static void Begin(Side side = Side::Right, float alpha = 0.5f, char* name = nullptr);

  // Mirror begin but can use directly End()
  static void End() { ImGui::End(); }

  // Return the position and size of the central display
  static void CentralDimension(ImVec2& pos, ImVec2& size)
  {
    auto dock_main = ImGui::DockBuilderGetCentralNode(dockspaceID);
    if(dock_main)
    {
      pos  = dock_main->Pos;
      size = dock_main->Size;
    }
  }
};


//--------------------------------------------------------------------------------------------------
// GUI control elements which have a 'property page' look.
// - Name on the left, justified on the right
// - Value filling the right side
// - Support for 1..4 components, int, float, bool
// - Tooltip over all param name for the description
// - Default values  (reset to default)
// - Min / Max for all value widgets
//
// Controls:
//    Slider    - Slider with bounded values
//    Drag      - Infinite values, but min or max values can be set
//    Checkbox  - Simple [x]
//    Selection - Combobox with simple vector of string or fct callback
//    Text      - Text edit box to enter/modify text
//    Color     - Color picker
//    Button    - param + button, or just a button when param name is empty
//    Group     - Grouping a list of elements (collapsible)
//    Info      - Simple info text
class Control /*static*/
{
public:
  // Influences the rendering of elements.
  enum class Flags
  {
    Normal   = 0,       // Normal display
    Disabled = 1 << 0,  // Grayed out
  };

  struct Style
  {
    float ctrlPerc    = 0.7f;   // Percentage the value control takes in the pane
    float ctrlMaxSize = 500.f;  // Max pixel size control will take
    float ctrlMinSize = 50.f;   // Minimum size of the control
    float dragMinSize = 150.f;  // Minimum size to be on one line
  };

  static Style style;

public:
  // Slider - used for bounded values
  template <typename TValue, typename TTooltip>
  static bool Slider(const std::string& label,        // Name of the parameter
                     TTooltip           description,  // Tooltip info
                     TValue*            value,        // value
                     const void*        default_value = nullptr,
                     Flags              flags         = Flags::Normal,
                     TValue             min           = TValue(0),
                     TValue             max           = TValue(1),
                     const char*        format        = nullptr)
  {
    return show_numeric_control(label, description, value, default_value, flags,
                                [&] { return show_slider_control<TValue>(value, min, max, format); });
  }

  // Drag - used for unbound float values. Min and max (or only one) can be specified.
  template <typename TValue, typename TTooltip>
  static bool Drag(const std::string& label,
                   TTooltip           description,
                   TValue*            value,
                   const void*        default_value = nullptr,
                   Flags              flags         = Flags::Normal,
                   TValue             min           = std::numeric_limits<TValue>::lowest(),
                   TValue             max           = std::numeric_limits<TValue>::max(),
                   float              speed         = std::is_integral<TValue>::value ? 0.25f : 0.01f,
                   const char*        format        = nullptr)
  {
    return show_numeric_control(label, description, value, default_value, flags,
                                [&] { return show_drag_control<TValue>(value, speed, min, max, format); });
  }
  // Checkbox  -  to toggle options on and off.
  template <typename TTooltip>
  static bool Checkbox(const std::string& label, TTooltip description, bool* value, const bool* default_value = nullptr, Flags flags = Flags::Normal);

  // Combobox(1)  -  to select between options.
  template <typename Tinteger, typename TTooltip>
  static bool Selection(const std::string&                   label,
                        TTooltip                             description,
                        Tinteger*                            index,
                        const void*                          default_index,
                        Flags                                flags,
                        std::function<const char*(Tinteger)> get_value);

  // Combobox(2)  -  to select between options.
  template <typename Tinteger, typename TTooltip>
  static bool Selection(const std::string&       label,
                        TTooltip                 description,
                        Tinteger*                index,
                        const void*              default_index,
                        Flags                    flags,
                        std::vector<std::string> values)
  {
    return Selection<Tinteger>(label, description, index, default_index, flags,
                               [&](Tinteger i) { return i < values.size() ? values[i].c_str() : nullptr; });
  }

  // Text  -  to input text
  template <typename TTooltip>
  static bool Text(const std::string& label,
                   TTooltip           description,
                   char*              value_buffer,
                   size_t             value_buffer_size,
                   const char*        default_value,
                   Flags              flags = Flags::Normal);

  // Pick  -  to pick a color.
  template <typename TTooltip>
  static bool Color(const std::string& label,
                    TTooltip           description,
                    float*             value_float3,
                    const float*       default_value_float3 = nullptr,
                    Flags              flags                = Flags::Normal);

  // Button  -  to start an action or for navigation.
  template <typename TTooltip>
  static bool button(const std::string& label, const std::string& button_text, TTooltip description, Flags flags = Flags::Normal);

  // Group  -  Create a collapsible group that can nest other elements.
  template <typename TReturn>
  static TReturn Group(const std::string& name, bool expanded_by_default, std::function<TReturn(void)> show_content);

  // Info  -  Shows an text label only without interaction possibility.
  template <typename TTooltip>
  static void Info(const std::string& label, TTooltip description, const char* info_text, Flags flags = Flags::Normal);
  template <typename TTooltip>
  static void Info(const std::string& label, TTooltip description, const std::string& info_text, Flags flags = Flags::Normal)
  {
    Info(label, description, info_text.c_str(), flags);
  }


  template <typename TTooltip>
  static bool Custom(const std::string& label, TTooltip description, std::function<bool()> show_content, Flags flags = Flags::Normal);

private:
  template <typename TTooltip>
  static void show_property_label(const std::string& text, TTooltip description, Flags flags);

  template <typename TValue>
  static bool show_slider_control(TValue* value, TValue& min, TValue& max, const char* format);

  template <typename TValue>
  static bool show_drag_control(TValue* value, float speed, TValue& min, TValue& max, const char* format);

  template <typename TValue, typename TTooltip>
  static bool show_numeric_control(const std::string&        label,
                                   TTooltip                  description,
                                   TValue*                   value,
                                   const void*               default_value,
                                   Flags                     flags,
                                   std::function<bool(void)> show_numeric_control);

  template <typename TScalar, ImGuiDataType type, uint8_t dim>
  static bool show_drag_control_scalar(TScalar* value, float speed, TScalar* min, TScalar* max, const char* format);

  template <typename TScalar, ImGuiDataType type, uint8_t dim>
  static bool show_slider_control_scalar(TScalar* value, TScalar* min, TScalar* max, const char* format);
};


//////////////////////////////////////////////////////////////////////////
// Template IMPLEMENTATION
//


//
// Displaying the parameter name
// - When string ends with \n, the parameters will be on next line instead of on the right
//
template <typename TTooltip>
void ImGuiH::Control::show_property_label(const std::string& text, TTooltip description, Flags flags)
{
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  }

  if(text.back() == '\n')
  {
    ImGui::TextWrapped("%s", text.c_str());
  }
  else
  {
    // Indenting the text to be right justified
    float              current_indent = ImGui::GetCursorPos().x;
    const ImGuiStyle&  imstyle        = ImGui::GetStyle();
    const ImGuiWindow* window         = ImGui::GetCurrentWindow();

    float control_width = std::min((ImGui::GetWindowWidth() - imstyle.IndentSpacing) * style.ctrlPerc, style.ctrlMaxSize);
    control_width -= window->ScrollbarSizes.x;
    control_width = std::max(control_width, style.ctrlMinSize);

    float  available_width     = ImGui::GetContentRegionAvail().x;
    float  avaiable_text_width = available_width - control_width - imstyle.ItemInnerSpacing.x;
    ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size(), false, avaiable_text_width);
    float  indent    = current_indent + available_width - control_width - text_size.x - imstyle.ItemInnerSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::NewLine();
    ImGui::SameLine(indent);
    ImGui::PushTextWrapPos(indent + avaiable_text_width);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SameLine();
  }

  if(flags == Flags::Disabled)
    ImGui::PopStyleColor();

  ImGuiH::tooltip(description, false, 0.0f);
}

//
// Add the value controls (sliders, drag)
// - handle default value
//
template <typename TValue, typename TTooltip>
bool ImGuiH::Control::show_numeric_control(const std::string&        label,
                                           TTooltip                  description,
                                           TValue*                   value,
                                           const void*               default_value,
                                           Flags                     flags,
                                           std::function<bool(void)> show_numeric_control)
{
  ImGui::PushID(value);
  show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  bool changed = show_numeric_control();

  if(default_value && ImGui::BeginPopupContextItem("item context menu"))
  {
    if(ImGui::Selectable("set default"))
    {
      changed = *value != *(reinterpret_cast<const TValue*>(default_value));
      *value  = *(reinterpret_cast<const TValue*>(default_value));
    }
    ImGui::EndPopup();
  }

  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }

  ImGui::PopID();
  return changed;
}

//
//
template <typename TTooltip>
void ImGuiH::Control::Info(const std::string& label, TTooltip description, const char* info_text, Flags flags)
{
  ImGui::PushID(&info_text);
  if(label.empty() == false)
    show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::TextWrapped("%s", info_text);

  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }

  if(label.empty())
    ImGuiH::tooltip(description, false, 0.0f);

  ImGui::PopID();
}

template <typename TReturn>
TReturn ImGuiH::Control::Group(const std::string& name, bool expanded_by_default, std::function<TReturn(void)> show_content)
{
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec4      color     = ImGui::GetStyle().Colors[ImGuiCol_Text];
  color.x *= 0.6f;
  color.y *= 0.6f;
  color.z *= 0.6f;
  const ImU32 draw_color      = ImColor(color);
  const float draw_line_width = 1.0f;

  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

  TReturn      ret       = static_cast<TReturn>(0);
  const ImVec2 start_top = ImGui::GetCursorScreenPos();
  ImGui::PushID(&show_content);
  if(ImGui::TreeNodeEx(name.c_str(), expanded_by_default ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
  {
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    const ImVec2 start = ImGui::GetCursorScreenPos();
    ret                = show_content();
    const ImVec2 end   = ImGui::GetCursorScreenPos();

    draw_list->AddLine(ImVec2(start.x - draw_line_width * 0.5f, (start.y + start_top.y) * 0.5f),
                       ImVec2(end.x - draw_line_width * 0.5f, end.y), draw_color, draw_line_width);

    ImGui::TreePop();
  }
  else
  {
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
  }
  ImGui::PopID();
  ImGui::Spacing();
  return ret;
}

template <typename TTooltip>
bool ImGuiH::Control::button(const std::string& label, const std::string& button_text, TTooltip description, Flags flags /*= Flags::Normal*/)
{
  ImGui::PushID(&description);

  // show the left side label with tool tips
  if(!label.empty())
    show_property_label(label, description, flags);

  bool label_fits = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(button_text.c_str()).x
                     - ImGui::GetStyle().ItemInnerSpacing.x * 2.0f)
                    > 0.0f;

  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  bool pressed = ImGui::Button((label.empty() || label_fits ? button_text.c_str() : "..."),
                               ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }

  // show the tool tip when hovering over the button in case there is no left side label
  if(label.empty())
    ImGuiH::tooltip(description, false, 0.0f);

  ImGui::PopID();
  return pressed;
}

template <typename TTooltip>
bool ImGuiH::Control::Color(const std::string& label, TTooltip description, float* value_float3, const float* default_value_float3, Flags flags)
{
  ImGui::PushID(value_float3);
  show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  bool changed = ImGui::ColorEdit3("##hidden", &value_float3[0],
                                   ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha);

  if(default_value_float3 && ImGui::BeginPopupContextItem("item context menu"))
  {
    if(ImGui::Selectable("set default"))
    {
      changed = value_float3[0] != default_value_float3[0] || value_float3[1] != default_value_float3[1]
                || value_float3[2] != default_value_float3[2];

      value_float3[0] = default_value_float3[0];
      value_float3[1] = default_value_float3[1];
      value_float3[2] = default_value_float3[2];
    }
    ImGui::EndPopup();
  }
  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }
  ImGui::PopID();
  return changed;
}


// Combo-box to select between options.
template <typename Tinteger, typename TTooltip>
bool ImGuiH::Control::Selection(const std::string&                   label,
                                TTooltip                             description,
                                Tinteger*                            index,
                                const void*                          default_index,
                                Flags                                flags,
                                std::function<const char*(Tinteger)> get_value)
{
  return show_numeric_control(label, description, index, default_index, flags, [&] {
    bool valid   = false;
    bool changed = false;
    if(ImGui::BeginCombo("##hidden", get_value(*index)))
    {
      Tinteger i = 0;
      while(true)
      {
        const char* option = get_value(i);
        if(!option)
          break;

        valid |= (i == *index);  // check if current selection is a valid option
        if(ImGui::Selectable(option, i == *index))
        {
          *index  = i;
          changed = true;
          valid   = true;
        }
        i++;
      }
      ImGui::EndCombo();

      if(!valid && default_index)
      {
        *index  = *reinterpret_cast<const Tinteger*>(default_index);
        changed = true;
      }
    }
    return changed;
  });
}

template <typename TTooltip>
bool ImGuiH::Control::Text(const std::string& label, TTooltip description, char* value_buffer, size_t value_buffer_size, const char* default_value, Flags flags)
{
  //assuming that this is unique, but constant from frame to frame
  int id = *static_cast<int*>((void*)&label);
  id ^= *static_cast<int*>((void*)&description);
  id ^= *static_cast<int*>((void*)&default_value);

  ImGui::PushID(id);
  show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  bool changed = ImGui::InputText("##hidden", value_buffer, value_buffer_size, ImGuiInputTextFlags_EnterReturnsTrue);
  if(default_value && ImGui::BeginPopupContextItem("item context menu"))
  {
    if(ImGui::Selectable("set default"))
    {
      changed = strcmp(value_buffer, default_value) != 0;
      memset(value_buffer, '\0', value_buffer_size);
      strncpy(value_buffer, default_value, std::min(value_buffer_size, strlen(default_value)));
    }
    ImGui::EndPopup();
  }
  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }
  ImGui::PopID();
  return changed;
}

template <typename TTooltip>
bool ImGuiH::Control::Checkbox(const std::string& label, TTooltip description, bool* value, const bool* default_value /*= nullptr*/, Flags flags /*= Flags::Normal*/)
{
  ImGui::PushID(value);
  show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  bool changed = ImGui::Checkbox("##hidden", value);
  if(default_value && ImGui::BeginPopupContextItem("item context menu"))
  {
    if(ImGui::Selectable("set default"))
    {
      changed = *value != *default_value;
      *value  = *default_value;
    }
    ImGui::EndPopup();
  }
  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }
  ImGui::PopID();
  return changed;
}


template <typename TScalar, ImGuiDataType type, uint8_t dim>
bool Control::show_drag_control_scalar(TScalar* value, float speed, TScalar* min, TScalar* max, const char* format)
{
  static const char* visible_labels[] = {"x:", "y:", "z:", "w:"};

  if(ImGui::GetContentRegionAvail().x > style.dragMinSize)
    return ImGui::DragScalarN("##hidden", type, &value[0], dim, speed, &min[0], &max[0], format);

  if(dim == 1)
    return ImGui::DragScalar("##hidden", type, &value[0], speed, &min[0], &max[0], format);

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
    changed |= ImGui::DragScalar("##hidden", type, &value[c], speed, &min[c], &max[c], format);
    ImGui::PopID();
  }
  return changed;
}

template <typename TScalar, ImGuiDataType type, uint8_t dim>
bool Control::show_slider_control_scalar(TScalar* value, TScalar* min, TScalar* max, const char* format)
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

template <typename TTooltip>
bool ImGuiH::Control::Custom(const std::string& label, TTooltip description, std::function<bool()> show_content, Flags flags /*= Flags::Normal*/)
{
  //ImGui::PushID(value);
  show_property_label(label, description, flags);
  if(flags == Flags::Disabled)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
  }
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

  bool changed = show_content();

  if(flags == Flags::Disabled)
  {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
  }
  // ImGui::PopID();
  return changed;
}

}  // namespace ImGui



