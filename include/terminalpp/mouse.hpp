#pragma once

#include "terminalpp/point.hpp"
#include "terminalpp/virtual_key.hpp"

#include <iosfwd>
#include <optional>

namespace terminalpp::mouse {

enum class button : byte
{
    none,
    left,
    middle,
    right,
    unknown,
    scrollwheel_up,
    scrollwheel_down,
    scrollwheel_right,
    scrollwheel_left,
    button_8,
    button_9,
    button_10,
    button_11,
};

enum class event_type : byte
{
    left_button_down,
    middle_button_down,
    right_button_down,
    button_up,
    no_button_change,
    scrollwheel_up,
    scrollwheel_down,
};

//* =========================================================================
/// \brief A structure that encapsulates a mouse event.
//* =========================================================================
struct TERMINALPP_EXPORT event
{
    //* =====================================================================
    /// \brief The type of action that caused this event.
    //* =====================================================================
    event_type action_ = event_type::no_button_change;

    //* =====================================================================
    /// \brief The position of the mouse in this event.
    //* =====================================================================
    point position_;

    button button_ = button::none;
    std::optional<std::uint16_t> button_code_;
    terminalpp::vk_modifier modifiers_ = terminalpp::vk_modifier::none;
    bool is_motion_ = false;
    bool is_release_ = false;

    //* =====================================================================
    /// \brief Relational operators for events
    //* =====================================================================
    [[nodiscard]] constexpr friend auto operator<=>(
        event const &lhs, event const &rhs) noexcept = default;
};

//* =========================================================================
/// \brief Streaming output operator
//* =========================================================================
TERMINALPP_EXPORT
std::ostream &operator<<(std::ostream &out, event const &ev);

}  // namespace terminalpp::mouse
