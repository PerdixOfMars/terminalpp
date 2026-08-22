#include "terminalpp/detail/parser.hpp"

#include "terminalpp/ansi/control_characters.hpp"
#include "terminalpp/ansi/csi.hpp"
#include "terminalpp/ansi/mouse.hpp"
#include "terminalpp/ansi/protocol.hpp"
#include "terminalpp/detail/ascii.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <limits>

namespace terminalpp::detail {

namespace {

constexpr bool is_csi_extension_character(byte input)
{
    return input == terminalpp::detail::ascii::question_mark
        || input == terminalpp::detail::ascii::greater_than
        || input == terminalpp::detail::ascii::exclamation_mark
        || input == terminalpp::detail::ascii::less_than;
}

std::optional<std::uint32_t> parse_number(byte_storage const &argument)
{
    if (argument.empty())
    {
        return {};
    }

    std::uint32_t result = 0;
    auto const *const begin = reinterpret_cast<char const *>(argument.data());
    auto const *const end = begin + argument.size();
    auto const [ptr, ec] = std::from_chars(begin, end, result);

    if (ec != std::errc{} || ptr != end)
    {
        return {};
    }

    return result;
}

mouse::event_type action_from_button(mouse::button button, bool is_release)
{
    if (is_release)
    {
        return button == mouse::button::scrollwheel_up
                || button == mouse::button::scrollwheel_down
            ? mouse::event_type::no_button_change
            : mouse::event_type::button_up;
    }

    switch (button)
    {
        case mouse::button::left:
            return mouse::event_type::left_button_down;
        case mouse::button::middle:
            return mouse::event_type::middle_button_down;
        case mouse::button::right:
            return mouse::event_type::right_button_down;
        case mouse::button::scrollwheel_up:
            return mouse::event_type::scrollwheel_up;
        case mouse::button::scrollwheel_down:
            return mouse::event_type::scrollwheel_down;
        default:
            return mouse::event_type::no_button_change;
    }
}

mouse::button button_from_code(std::uint16_t code)
{
    auto const base_code = static_cast<std::uint16_t>(code & ~std::uint16_t{60});

    switch (base_code)
    {
        case 0:
            return mouse::button::left;
        case 1:
            return mouse::button::middle;
        case 2:
            return mouse::button::right;
        case 64:
            return mouse::button::scrollwheel_up;
        case 65:
            return mouse::button::scrollwheel_down;
        case 66:
            return mouse::button::scrollwheel_right;
        case 67:
            return mouse::button::scrollwheel_left;
        case 128:
            return mouse::button::button_8;
        case 129:
            return mouse::button::button_9;
        case 130:
            return mouse::button::button_10;
        case 131:
            return mouse::button::button_11;
        default:
            return mouse::button::unknown;
    }
}

terminalpp::vk_modifier modifiers_from_code(std::uint16_t code)
{
    auto result = terminalpp::vk_modifier::none;

    if ((code & 4) != 0)
    {
        result |= terminalpp::vk_modifier::shift;
    }

    if ((code & 8) != 0)
    {
        result |= terminalpp::vk_modifier::meta;
    }

    if ((code & 16) != 0)
    {
        result |= terminalpp::vk_modifier::ctrl;
    }

    return result;
}

std::optional<mouse::event> make_sgr_mouse_event(
    std::vector<byte_storage> const &arguments,
    byte command)
{
    if (arguments.size() != 3)
    {
        return {};
    }

    auto const code = parse_number(arguments[0]);
    auto const x = parse_number(arguments[1]);
    auto const y = parse_number(arguments[2]);

    if (!code || !x || !y || *x == 0 || *y == 0
        || *code > std::numeric_limits<std::uint16_t>::max()
        || *x > std::numeric_limits<coordinate_type>::max()
        || *y > std::numeric_limits<coordinate_type>::max())
    {
        return {};
    }

    auto const button_code = static_cast<std::uint16_t>(*code);
    auto const button = button_from_code(button_code);
    auto const is_release = command == terminalpp::detail::ascii::lowercase_m;

    return mouse::event{
        .action_ = action_from_button(button, is_release),
        .position_ = {static_cast<coordinate_type>(*x - 1),
                      static_cast<coordinate_type>(*y - 1)},
        .button_ = button,
        .button_code_ = button_code,
        .modifiers_ = modifiers_from_code(button_code),
        .is_motion_ = (button_code & 32) != 0,
        .is_release_ = is_release};
}

}  // namespace

parser::parser() : state_(state::idle)
{
}

std::optional<terminalpp::token> parser::parser::operator()(byte input)
{
    switch (state_)
    {
        case state::idle:
            return parse_idle(input);
        case state::cr:
            return parse_cr(input);
        case state::lf:
            return parse_lf(input);
        case state::escape:
            return parse_escape(input);
        case state::arguments:
            return parse_arguments(input);
        case state::mouse0:
            return parse_mouse0(input);
        case state::mouse1:
            return parse_mouse1(input);
        case state::mouse2:
            return parse_mouse2(input);
        default:
            assert(!"state out of range");
    }

    return {};
}

std::optional<terminalpp::token> parser::parser::parse_idle(byte input)
{
    if (input == terminalpp::detail::ascii::esc)
    {
        state_ = state::escape;
        meta_ = false;
        extender_ = '\0';
        argument_ = {};
        arguments_ = {};
        return {};
    }
    else if (input == terminalpp::detail::ascii::cr)
    {
        state_ = state::cr;
        return terminalpp::token{
            terminalpp::virtual_key{
                                    terminalpp::vk::enter,
                                    terminalpp::vk_modifier::none,
                                    1, '\n'_tb}
        };
    }
    else if (input == terminalpp::detail::ascii::lf)
    {
        state_ = state::lf;
        return terminalpp::token{
            terminalpp::virtual_key{
                                    terminalpp::vk::enter,
                                    terminalpp::vk_modifier::none,
                                    1, '\n'_tb}
        };
    }
    else if (input == terminalpp::ansi::control8::csi)
    {
        state_ = state::arguments;
        meta_ = false;
        initializer_ = terminalpp::ansi::control7::csi[1];
        extender_ = '\0';
        argument_ = {};
        arguments_ = {};
        return {};
    }
    else if (input == terminalpp::ansi::control8::ss3)
    {
        state_ = state::arguments;
        meta_ = false;
        initializer_ = terminalpp::ansi::control7::ss3[1];
        extender_ = '\0';
        argument_ = {};
        arguments_ = {};
        return {};
    }
    else
    {
        return terminalpp::token{
            terminalpp::virtual_key{
                                    static_cast<vk>(input),
                                    terminalpp::vk_modifier::none,
                                    1, {input}}
        };
    }
}

std::optional<terminalpp::token> parser::parse_cr(byte input)
{
    state_ = state::idle;

    if (input == terminalpp::detail::ascii::lf
        || input == terminalpp::detail::ascii::nul)
    {
        return {};
    }
    else
    {
        return parse_idle(input);
    }
}

std::optional<terminalpp::token> parser::parse_lf(byte input)
{
    state_ = state::idle;

    if (input == terminalpp::detail::ascii::cr)
    {
        return {};
    }
    else
    {
        return parse_idle(input);
    }
}

std::optional<terminalpp::token> parser::parse_escape(byte input)
{
    if (input == terminalpp::detail::ascii::esc)
    {
        meta_ = true;
    }
    else
    {
        initializer_ = input;
        state_ = state::arguments;
    }
    return {};
}

std::optional<terminalpp::token> parser::parse_arguments(byte input)
{
    if (isdigit(input) != 0)  // TODO: depends on initiator.
    {
        argument_.push_back(input);
    }
    else if (input == terminalpp::ansi::ps)
    {
        arguments_.push_back(argument_);
        argument_ = {};
    }
    else if (
        input == terminalpp::ansi::csi::mouse_tracking
        && initializer_ == terminalpp::ansi::control7::csi[1] && extender_ == 0)
    {
        state_ = state::mouse0;
    }
    else if (is_csi_extension_character(input))
    {
        extender_ = input;
    }
    else
    {
        // construct and return a control sequence.
        arguments_.push_back(argument_);
        state_ = state::idle;

        if (initializer_ == terminalpp::ansi::control7::csi[1]
            && extender_ == terminalpp::detail::ascii::less_than
            && (input == terminalpp::detail::ascii::uppercase_m
                || input == terminalpp::detail::ascii::lowercase_m))
        {
            if (auto mouse_event = make_sgr_mouse_event(arguments_, input))
            {
                return terminalpp::token{*mouse_event};
            }
        }

        return terminalpp::token{
            terminalpp::control_sequence{
                                         initializer_, input, meta_, arguments_, extender_}
        };
    }

    return {};
}

std::optional<terminalpp::token> parser::parse_mouse0(byte input)
{
    static constexpr struct
    {
        byte ansi_mouse_event;
        mouse::event_type mouse_event;
    } mouse_event_table[] = {
        {ansi::mouse::left_button_down,   mouse::event_type::left_button_down },
        {ansi::mouse::middle_button_down,
         mouse::event_type::middle_button_down                                },
        {ansi::mouse::right_button_down,  mouse::event_type::right_button_down},
        {ansi::mouse::button_up,          mouse::event_type::button_up        },
        {ansi::mouse::no_button_change,   mouse::event_type::no_button_change },
        {ansi::mouse::scrollwheel_up,     mouse::event_type::scrollwheel_up   },
        {ansi::mouse::scrollwheel_down,   mouse::event_type::scrollwheel_down },
    };

    if (auto const *result = std::ranges::find(
            mouse_event_table,
            input - ansi::mouse::mouse_value_offset,
            [](auto const &entry) { return entry.ansi_mouse_event; });
        result != std::cend(mouse_event_table))
    {
        mouse_event_type_ = result->mouse_event;
    }
    else
    {
        mouse_event_type_ = mouse::event_type::no_button_change;
    }

    state_ = state::mouse1;
    return {};
}

std::optional<terminalpp::token> parser::parse_mouse1(byte input)
{
    // In addition to the offset described above, ANSI co-ordinates are
    // 1-based, whereas Terminal++ is 0-based, which means an extra offset
    // is required.
    mouse_coordinate_.x_ = static_cast<coordinate_type>(
        (input - ansi::mouse::mouse_value_offset) - 1);
    state_ = state::mouse2;

    return {};
}

std::optional<terminalpp::token> parser::parse_mouse2(byte input)
{
    mouse_coordinate_.y_ = static_cast<coordinate_type>(
        (input - ansi::mouse::mouse_value_offset) - 1);
    state_ = state::idle;

    return {terminalpp::token{terminalpp::mouse::event{
        .action_ = mouse_event_type_,
        .position_ = mouse_coordinate_,
        .button_ = mouse::button::none,
        .button_code_ = std::nullopt,
        .modifiers_ = terminalpp::vk_modifier::none,
        .is_motion_ = false,
        .is_release_ = false}}};
}

}  // namespace terminalpp::detail
