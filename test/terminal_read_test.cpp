#include "fakes/fake_channel.hpp"
#include "terminalpp/terminal.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

using namespace terminalpp::literals;  // NOLINT
using testing::ValuesIn;

namespace {

class terminal_read_test_base
{
public:
    explicit terminal_read_test_base(
        terminalpp::behaviour const &behaviour = terminalpp::behaviour{})
      : terminal_{channel_, behaviour}
    {
    }

protected:
    fake_channel channel_;
    terminalpp::terminal terminal_;
};

using token_test_data = std::tuple<
    terminalpp::byte_storage,  // Input byte sequence
    terminalpp::token_storage  // Expected output
    >;

terminalpp::mouse::event mouse_event(
    terminalpp::mouse::event_type action,
    terminalpp::point position,
    terminalpp::mouse::button button,
    std::uint16_t button_code,
    terminalpp::vk_modifier modifiers = terminalpp::vk_modifier::none,
    bool is_motion = false,
    bool is_release = false)
{
    return terminalpp::mouse::event{
        .action_ = action,
        .position_ = position,
        .button_ = button,
        .button_code_ = button_code,
        .modifiers_ = modifiers,
        .is_motion_ = is_motion,
        .is_release_ = is_release};
}

terminalpp::control_sequence sgr_mouse_fallback(
    terminalpp::byte command, std::vector<terminalpp::byte_storage> arguments)
{
    return terminalpp::control_sequence{
        .initiator = '[',
        .command = command,
        .meta = false,
        .arguments = std::move(arguments),
        .extender = '<'};
}

class a_terminal_reading_input_tokens
  : public testing::TestWithParam<token_test_data>,
    public terminal_read_test_base
{
};

TEST_P(a_terminal_reading_input_tokens, tokenizes_the_results)
{
    using std::get;

    auto const &[input_sequence, expected_result] = GetParam();

    std::vector<terminalpp::token> result;
    terminal_.async_read([&result](terminalpp::tokens tokens) {
        result.insert(result.end(), tokens.begin(), tokens.end());
    });

    channel_.receive(input_sequence);
    ASSERT_EQ(expected_result, result);
}

token_test_data const token_test_data_table[] = {
    {""_tb,                       {}                                                  },

    // Single characters yield virtual keys
    {"z"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::lowercase_z,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = {'z'_tb}}}                                                       },
    {"a"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::lowercase_a,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = {'a'_tb}}}                                                       },
    {"J"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::uppercase_j,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = {'J'_tb}}}                                                       },
    {"X"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::uppercase_x,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = {'X'_tb}}}                                                       },

    {"AB"_tb,
     {
         terminalpp::virtual_key{
             .key = terminalpp::vk::uppercase_a,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = {'A'_tb}},
         terminalpp::virtual_key{
             .key = terminalpp::vk::uppercase_b,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = {'B'_tb}},
     }                                                                                },

    // Partial commands yield nothing, including partial but otherwise
    // complete-looking mouse commands.
    {"\x1B"_tb,                   {}                                                  },
    {"\x1B["_tb,                  {}                                                  },
    {"\x1B[M"_tb,                 {}                                                  },

    // Protocol-encoded commands are converted to control sequences
    {"\x1B[S"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'S',
         .meta = false,
         .arguments = {""_tb}}}                                                       },
    {"\x9BS"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'S',
         .meta = false,
         .arguments = {""_tb}}}                                                       },
    {"\x1B[22;33S"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'S',
         .meta = false,
         .arguments = {"22"_tb, "33"_tb}}}                                            },
    {"\x9B"_tb
     "22;33S"_tb,            {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'S',
         .meta = false,
         .arguments = {"22"_tb, "33"_tb}}}                      },
    {"\x1B?M"_tb,
     {terminalpp::control_sequence{
         .initiator = '?',
         .command = 'M',
         .meta = false,
         .arguments = {""_tb}}}                                                       },

    // Doubly-escaped protocol-encoded commands are converted to control
    // sequences with the meta flag.
    {"\x1B\x1B[S"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'S',
         .meta = true,
         .arguments = {""_tb}}}                                                       },
    {"\x1B\x1B[T"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'T',
         .meta = true,
         .arguments = {""_tb}}}                                                       },

    // Extended commands are converted to extended control sequences
    {"\x1B[?6n"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'n',
         .meta = false,
         .arguments = {"6"_tb},
         .extender = '?'}}                                                            },
    {"\x1B[>5c"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'c',
         .meta = false,
         .arguments = {"5"_tb},
         .extender = '>'}}                                                            },
    {"\x1B[!p"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'p',
         .meta = false,
         .arguments = {""_tb},
         .extender = '!'}}                                                            },
    {"\x9B!p"_tb,
     {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'p',
         .meta = false,
         .arguments = {""_tb},
         .extender = '!'}}                                                            },

    // ANSI mouse events are converted to the respective structure
    {"\x1B[M"_tb,                 {}                                                  },
    {"\x1B[M @B"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::left_button_down,
         {31, 33},
         terminalpp::mouse::button::left,
         0)}                                                                          },
    {"\x1B[M!X4"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::middle_button_down,
         {55, 19},
         terminalpp::mouse::button::middle,
         1)}                                                                          },
    {"\x1B[<0;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::left_button_down,
         {31, 33},
         terminalpp::mouse::button::left,
         0)}                                                                          },
    {"\x1B[<0;32;34m"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::button_up,
         {31, 33},
         terminalpp::mouse::button::left,
         0,
         terminalpp::vk_modifier::none,
         false,
         true)}                                                                       },
    {"\x1B[<28;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::left_button_down,
         {31, 33},
         terminalpp::mouse::button::left,
         28,
         terminalpp::vk_modifier::shift | terminalpp::vk_modifier::ctrl
             | terminalpp::vk_modifier::meta)}                                        },
    {"\x1B[<32;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::left_button_down,
         {31, 33},
         terminalpp::mouse::button::left,
         32,
         terminalpp::vk_modifier::none,
         true)}                                                                       },
    {"\x1B[<64;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::scrollwheel_up,
         {31, 33},
         terminalpp::mouse::button::scrollwheel_up,
         64)}                                                                         },
    {"\x1B[<65;32;34m"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::scrollwheel_down,
         65,
         terminalpp::vk_modifier::none,
         false,
         true)}                                                                       },
    {"\x1B[<66;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::scrollwheel_right,
         66)}                                                                         },
    {"\x1B[<67;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::scrollwheel_left,
         67)}                                                                         },
    {"\x1B[<160;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::button_8,
         160,
         terminalpp::vk_modifier::none,
         true)}                                                                       },
    {"\x1B[<129;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::button_9,
         129)}                                                                        },
    {"\x1B[<130;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::button_10,
         130)}                                                                        },
    {"\x1B[<131;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::button_11,
         131)}                                                                        },
    {"\x1B[<132;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::button_8,
         132,
         terminalpp::vk_modifier::shift)}                                             },
    {"\x1B[<192;32;34M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::no_button_change,
         {31, 33},
         terminalpp::mouse::button::unknown,
         192)}                                                                        },
    {"\x9B<000;032;034M"_tb,
     {mouse_event(
         terminalpp::mouse::event_type::left_button_down,
         {31, 33},
         terminalpp::mouse::button::left,
         0)}                                                                          },
    {"\x1B[<a;32;34M"_tb,
     {sgr_mouse_fallback('M', {"a"_tb, "32"_tb, "34"_tb})}                            },
    {"\x1B[<0;0;34M"_tb,          {sgr_mouse_fallback('M', {"0"_tb, "0"_tb, "34"_tb})}},
    {"\x1B[<0;32;0M"_tb,          {sgr_mouse_fallback('M', {"0"_tb, "32"_tb, "0"_tb})}},
    {"\x1B[<0;32M"_tb,            {sgr_mouse_fallback('M', {"0"_tb, "32"_tb})}        },
    {"\x1B[<0;32;34;1M"_tb,
     {sgr_mouse_fallback('M', {"0"_tb, "32"_tb, "34"_tb, "1"_tb})}                    },
    {"\x1B[<0;;34M"_tb,           {sgr_mouse_fallback('M', {"0"_tb, ""_tb, "34"_tb})} },
    {"\x1B[<-1;32;34M"_tb,
     {sgr_mouse_fallback('M', {"-1"_tb, "32"_tb, "34"_tb})}                           },
    {"\x1B[<65536;32;34M"_tb,
     {sgr_mouse_fallback('M', {"65536"_tb, "32"_tb, "34"_tb})}                        },
    {"\x1B[<0;2147483648;34M"_tb,
     {sgr_mouse_fallback('M', {"0"_tb, "2147483648"_tb, "34"_tb})}                    },

    // Cursor keys are converted to the respective virtual keys.
    {"\x1B[A"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'A',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[B"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_down,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'B',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[C"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_right,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'C',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[D"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_left,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'D',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },

    {"\x1B[1A"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'A',
                 .meta = false,
                 .arguments = {"1"_tb}}}}                                             },
    {"\x1B[2A"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 2,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'A',
                 .meta = false,
                 .arguments = {"2"_tb}}}}                                             },
    {"\x1B[3A"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 3,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'A',
                 .meta = false,
                 .arguments = {"3"_tb}}}}                                             },

    {"\x1B\x1B[A"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::meta,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'A',
                 .meta = true,
                 .arguments = {""_tb}}}}                                              },

    // Other special keyboard keys are converted to the respective virtual keys
    {"\x1B[1~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::home,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"1"_tb}}}}                                             },
    {"\x1B[1;3H"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::home,
         .modifiers = terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'H',
                 .meta = false,
                 .arguments = {"1"_tb, "3"_tb}}}}                                     },
    {"\x1B[1;10H"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::home,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::shift,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'H',
                 .meta = false,
                 .arguments = {"1"_tb, "10"_tb}}}}                                    },
    {"\x1B[2~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ins,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"2"_tb}}}}                                             },
    {"\x1B[3~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::del,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"3"_tb}}}}                                             },
    {"\x1B[4~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::end,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"4"_tb}}}}                                             },
    {"\x1B[5~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::pgup,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"5"_tb}}}}                                             },
    {"\x1B[6~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::pgdn,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"6"_tb}}}}                                             },

    // Keypad commands can have modifiers
    {"\x1B[6;5~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::pgdn,
         .modifiers = terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"6"_tb, "5"_tb}}}}                                     },
    {"\x1B\x1B[6~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::pgdn,
         .modifiers = terminalpp::vk_modifier::meta,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = true,
                 .arguments = {"6"_tb}}}}                                             },

    // Alternative sequences for control keys
    {"\x1B[H"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::home,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'H',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[F"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::end,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'F',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[1;3F"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::end,
         .modifiers = terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'F',
                 .meta = false,
                 .arguments = {"1"_tb, "3"_tb}}}}                                     },
    {"\x1B[1;10F"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::end,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::shift,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'F',
                 .meta = false,
                 .arguments = {"1"_tb, "10"_tb}}}}                                    },
    {"\x1BOA"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_up,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'A',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOB"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_down,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'B',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOC"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_right,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'C',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOD"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::cursor_left,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'D',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },

    // All forms of tab key should yield the same virtual key
    {"\t"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = '\t'_tb}}                                                        },
    {"\x1B[I"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'I',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B[7I"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 7,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'I',
                 .meta = false,
                 .arguments = {"7"_tb}}}}                                             },
    {"\x9BI"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'I',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOI"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'I',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x8FI"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::ht,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'I',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },

    {"\x1B[Z"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::bt,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = 'Z',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },

    // Various forms of carriage return should be converted to the respective
    // virtual keys.
    {"\r\n"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::enter,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = '\n'_tb}}                                                        },
    // Including this common but incorrect form.
    {"\n\r"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::enter,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = '\n'_tb}}                                                        },
    {"\r\0"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::enter,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence = '\n'_tb}}                                                        },
    {"\r\0\r\0"_tb,
     {
         terminalpp::virtual_key{
             .key = terminalpp::vk::enter,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = '\n'_tb},
         terminalpp::virtual_key{
             .key = terminalpp::vk::enter,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = '\n'_tb},
     }                                                                                },
    {"\n\n"_tb,
     {
         terminalpp::virtual_key{
             .key = terminalpp::vk::enter,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = '\n'_tb},
         terminalpp::virtual_key{
             .key = terminalpp::vk::enter,
             .modifiers = terminalpp::vk_modifier::none,
             .repeat_count = 1,
             .sequence = '\n'_tb},
     }                                                                                },
    {"\x1BOM"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::enter,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'M',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },

    // Function keys are converted to the respective virtual keys
    {"\x1B[11~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f1,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"11"_tb}}}}                                            },
    {"\x1B[12~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f2,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"12"_tb}}}}                                            },
    {"\x1B[13~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f3,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"13"_tb}}}}                                            },
    {"\x1B[14~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f4,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"14"_tb}}}}                                            },
    {"\x1B[15~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f5,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"15"_tb}}}}                                            },
    // Sic.  The protocol really skips over 16~ and goes to 17~ for f6.
    {"\x1B[17~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f6,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"17"_tb}}}}                                            },
    {"\x1B[18~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f7,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"18"_tb}}}}                                            },
    {"\x1B[19~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f8,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"19"_tb}}}}                                            },
    {"\x1B[20~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f9,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"20"_tb}}}}                                            },
    {"\x1B[21~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f10,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"21"_tb}}}}                                            },
    // Sic.  The protocol really skips over 22~ and goes to 23~ for f11.
    {"\x1B[23~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f11,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"23"_tb}}}}                                            },
    {"\x1B[24~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb}}}}                                            },

    // Function keys may have modifiers
    {"\x1B[24;0~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "0"_tb}}}}                                    },
    {"\x1B[24;2~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::shift,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "2"_tb}}}}                                    },
    {"\x1B[24;3~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "3"_tb}}}}                                    },
    {"\x1B[24;4~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::shift | terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "4"_tb}}}}                                    },
    {"\x1B[24;5~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "5"_tb}}}}                                    },
    {"\x1B[24;6~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::shift | terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "6"_tb}}}}                                    },
    {"\x1B[24;7~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::alt | terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "7"_tb}}}}                                    },
    {"\x1B[24;8~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::shift
                    | terminalpp::vk_modifier::alt
                    | terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "8"_tb}}}}                                    },
    {"\x1B[24;9~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::meta,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "9"_tb}}}}                                    },
    {"\x1B[24;10~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::shift,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "10"_tb}}}}                                   },
    {"\x1B[24;11~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "11"_tb}}}}                                   },
    {"\x1B[24;12~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::meta
                    | terminalpp::vk_modifier::shift
                    | terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "12"_tb}}}}                                   },
    {"\x1B[24;13~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "13"_tb}}}}                                   },
    {"\x1B[24;14~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::meta
                    | terminalpp::vk_modifier::shift
                    | terminalpp::vk_modifier::ctrl,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "14"_tb}}}}                                   },
    {"\x1B[24;15~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers = terminalpp::vk_modifier::meta
                    | terminalpp::vk_modifier::ctrl
                    | terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "15"_tb}}}}                                   },
    {"\x1B[24;16~"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f12,
         .modifiers =
             terminalpp::vk_modifier::meta | terminalpp::vk_modifier::shift
             | terminalpp::vk_modifier::ctrl | terminalpp::vk_modifier::alt,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = '[',
                 .command = '~',
                 .meta = false,
                 .arguments = {"24"_tb, "16"_tb}}}}                                   },

    // We can also parse alternative SS3 fkey sequences
    {"\x1BOP"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f1,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'P',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOQ"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f2,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'Q',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOR"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f3,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'R',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1BOS"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f4,
         .modifiers = terminalpp::vk_modifier::none,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'S',
                 .meta = false,
                 .arguments = {""_tb}}}}                                              },
    {"\x1B\x1BOS"_tb,
     {terminalpp::virtual_key{
         .key = terminalpp::vk::f4,
         .modifiers = terminalpp::vk_modifier::meta,
         .repeat_count = 1,
         .sequence =
             terminalpp::control_sequence{
                 .initiator = 'O',
                 .command = 'S',
                 .meta = true,
                 .arguments = {""_tb}}}}                                              },
};

INSTANTIATE_TEST_SUITE_P(
    reading_terminal_bytes_in_a_terminal_yields_tokens,
    a_terminal_reading_input_tokens,
    ValuesIn(token_test_data_table));

namespace {

using partial_token_test_data = std::tuple<
    terminalpp::byte_storage,  // First input byte sequence
    terminalpp::byte_storage,  // Second input byte sequence
    terminalpp::token_storage  // Expected output
    >;

class a_terminal_reading_partial_input_tokens
  : public testing::TestWithParam<partial_token_test_data>,
    public terminal_read_test_base
{
};

}  // namespace

TEST_P(a_terminal_reading_partial_input_tokens, tokenizes_the_results)
{
    using std::get;

    auto const &[first_input_sequence, second_input_sequence, expected_result] =
        GetParam();

    std::vector<terminalpp::token> result;
    auto const append_to_result = [&result](terminalpp::tokens tokens) {
        result.insert(result.end(), tokens.begin(), tokens.end());
    };

    terminal_.async_read(append_to_result);
    channel_.receive(first_input_sequence);

    terminal_.async_read(append_to_result);
    channel_.receive(second_input_sequence);

    ASSERT_EQ(expected_result, result);
}

partial_token_test_data const partial_token_test_data_table[] = {
    {"\x1B"_tb,
     "[X"_tb,  {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'X',
         .meta = false,
         .arguments = {""_tb}}} },
    {"\x9B"_tb,
     "X"_tb,   {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'X',
         .meta = false,
         .arguments = {""_tb}}}  },
    {"\x1B["_tb,
     "X"_tb,   {terminalpp::control_sequence{
         .initiator = '[',
         .command = 'X',
         .meta = false,
         .arguments = {""_tb}}}  },
    {"\x1B[M"_tb,
     "!X4"_tb, {terminalpp::mouse::event{
         .action_ = terminalpp::mouse::event_type::middle_button_down,
         .position_ = {55, 19},
         .button_ = terminalpp::mouse::button::middle,
         .button_code_ = 1,
         .modifiers_ = terminalpp::vk_modifier::none,
         .is_motion_ = false,
         .is_release_ = false}}},
};

INSTANTIATE_TEST_SUITE_P(
    reading_split_packets_of_ansi_tokens_parses_to_tokens,
    a_terminal_reading_partial_input_tokens,
    ValuesIn(partial_token_test_data_table));

}  // namespace
