#pragma once

#include "common/Common.h"
#include "console/Console.h"

#include "math/AABB.h"
#include "map/Common.h"
#include <optional>
#include <unordered_map>

namespace chisel
{
    using SelectionID = uint32_t;

    class Selectable
    {
    public:
        Selectable();
        virtual ~Selectable();

        SelectionID GetSelectionID() const { return m_id; }
        bool IsSelected() const { return m_selected; }

        virtual std::optional<AABB> SelectionBounds() const = 0;
        virtual void SelectionTransform(const mat4x4& matrix) = 0;
        virtual void SelectionDelete() = 0;
        virtual void SelectionAlignToGrid(vec3 gridSize) = 0;
        virtual void SelectionSetVolume(Volume volume) = 0;
    protected:
        friend class Selection;

        void SetSelected(bool selected) { m_selected = selected; }
        static Selectable* Find(SelectionID id);
    private:
        static SelectionID s_lastId;
        static std::unordered_map<SelectionID, Selectable*> s_map;

        SelectionID m_id = 0;
        bool m_selected = false;
    };

    extern class Selection
    {
    public:
        Selection();

        bool Empty();
        void Select(Selectable* ent);
        void Unselect(Selectable* ent);
        void Toggle(Selectable* ent);
        void Clear();
        Selectable* Find(SelectionID id);

        Selectable** begin() { return m_selection.size() > 0 ? &m_selection.front() : nullptr; }
        Selectable** end()   { return m_selection.size() > 0 ? &m_selection.back() + 1 : nullptr; }
    private:
        std::vector<Selectable*> m_selection;
    } Selection;
}