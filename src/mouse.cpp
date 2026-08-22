#include "terminalpp/mouse.hpp"

#include <iostream>
#include <utility>

namespace terminalpp::mouse {

namespace {

std::ostream &operator<<(std::ostream &out, button const &btn)
{
    switch (btn)
    {
        case button::none:
            return out << "none";
        case button::left:
            return out << "left";
        case button::middle:
            return out << "middle";
        case button::right:
            return out << "right";
        case button::unknown:
            return out << "unknown";
        case button::scrollwheel_up:
            return out << "scrollwheel-up";
        case button::scrollwheel_down:
            return out << "scrollwheel-down";
        case button::scrollwheel_right:
            return out << "scrollwheel-right";
        case button::scrollwheel_left:
            return out << "scrollwheel-left";
        case button::button_8:
            return out << "button-8";
        case button::button_9:
            return out << "button-9";
        case button::button_10:
            return out << "button-10";
        case button::button_11:
            return out << "button-11";
        default:
            return out << "unknown";
    }
}

void output_pipe(std::ostream &out, bool &pipe)
{
    if (std::exchange(pipe, true))
    {
        out << "|";
    }
}

std::ostream &output_modifiers(
    std::ostream &out, terminalpp::vk_modifier const &modifiers)
{
    bool pipe = false;

    if ((modifiers & terminalpp::vk_modifier::shift)
        == terminalpp::vk_modifier::shift)
    {
        output_pipe(out, pipe);
        out << "shift";
    }

    if ((modifiers & terminalpp::vk_modifier::ctrl)
        == terminalpp::vk_modifier::ctrl)
    {
        output_pipe(out, pipe);
        out << "ctrl";
    }

    if ((modifiers & terminalpp::vk_modifier::meta)
        == terminalpp::vk_modifier::meta)
    {
        output_pipe(out, pipe);
        out << "meta";
    }

    return out;
}

}  // namespace

// ==========================================================================
// OPERATOR<<
// ==========================================================================
std::ostream &operator<<(std::ostream &out, event const &ev)
{
    out << "mouse_event[" << ev.position_ << ", ";

    switch (ev.action_)
    {
        case event_type::left_button_down:
            out << "lmb";
            break;
        case event_type::middle_button_down:
            out << "mmb";
            break;
        case event_type::right_button_down:
            out << "rmb";
            break;
        case event_type::button_up:
            out << "up";
            break;
        case event_type::no_button_change:
            out << "no-change";
            break;
        case event_type::scrollwheel_down:
            out << "sdn";
            break;
        case event_type::scrollwheel_up:
            out << "sup";
            break;
        default:
            out << "unk";
            break;
    }

    if (ev.button_ != button::none)
    {
        out << ", button:" << ev.button_;
    }

    if (ev.button_code_)
    {
        out << ", code:" << *ev.button_code_;
    }

    if (ev.modifiers_ != terminalpp::vk_modifier::none)
    {
        out << ", mods:";
        output_modifiers(out, ev.modifiers_);
    }

    if (ev.is_motion_)
    {
        out << ", motion";
    }

    if (ev.is_release_)
    {
        out << ", release";
    }

    return out << "]";
}

}  // namespace terminalpp::mouse
