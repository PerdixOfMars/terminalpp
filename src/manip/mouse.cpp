#include "terminalpp/detail/element_difference.hpp"
#include "terminalpp/terminal.hpp"

namespace terminalpp {

namespace {

void write_dec_private_mode(
    terminalpp::behaviour const &beh,
    terminal::write_function const &write_fn,
    std::span<terminalpp::byte const> mode,
    std::span<terminalpp::byte const> operation)
{
    detail::dec_pm(beh, write_fn);
    write_fn(mode);
    write_fn(operation);
}

}  // namespace

// ==========================================================================
// ENABLE_MOUSE::OPERATOR()
// ==========================================================================
void enable_mouse::operator()(
    terminalpp::behaviour const &beh,
    terminalpp::terminal_state &state,
    terminal::write_function const &write_fn) const
{
    if (beh.supports_button_event_mouse_tracking
        && beh.supports_sgr_mouse_encoding)
    {
        write_dec_private_mode(
            beh, write_fn, ansi::dec_pm::sgr_mouse_encoding, ansi::dec_pm::set);
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::cell_motion_mouse_tracking,
            ansi::dec_pm::set);
    }
    else if (
        beh.supports_basic_mouse_tracking && beh.supports_sgr_mouse_encoding)
    {
        write_dec_private_mode(
            beh, write_fn, ansi::dec_pm::sgr_mouse_encoding, ansi::dec_pm::set);
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::basic_mouse_tracking,
            ansi::dec_pm::set);
    }
    else if (beh.supports_basic_mouse_tracking)
    {
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::basic_mouse_tracking,
            ansi::dec_pm::set);
    }
}

// ==========================================================================
// DISABLE_MOUSE::OPERATOR()
// ==========================================================================
void disable_mouse::operator()(
    terminalpp::behaviour const &beh,
    terminalpp::terminal_state &state,
    terminal::write_function const &write_fn) const
{
    if (beh.supports_button_event_mouse_tracking
        && beh.supports_sgr_mouse_encoding)
    {
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::cell_motion_mouse_tracking,
            ansi::dec_pm::reset);
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::sgr_mouse_encoding,
            ansi::dec_pm::reset);
    }
    else if (
        beh.supports_basic_mouse_tracking && beh.supports_sgr_mouse_encoding)
    {
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::basic_mouse_tracking,
            ansi::dec_pm::reset);
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::sgr_mouse_encoding,
            ansi::dec_pm::reset);
    }
    else if (beh.supports_basic_mouse_tracking)
    {
        write_dec_private_mode(
            beh,
            write_fn,
            ansi::dec_pm::basic_mouse_tracking,
            ansi::dec_pm::reset);
    }
}

}  // namespace terminalpp
