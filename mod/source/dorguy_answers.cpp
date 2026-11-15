#include <common.h>
#include <msl/string.h>

#include "dorguy_answers.h"
#include "options_menu.h"

namespace mod::dorguy_answers {

#define RED "<col c00000ff>"
#define NORMAL "<col 000000ff>"  // back to the box's dark text

/*
    The Dorguys' quizzes in 7-2 (stg7.txt). Each answer list is a select box; the script after it
    (relF 0x19f6c0-0x1a4de4) compares the choice with a fixed number, so each question has one right
    answer, given here in red. The TV show question (stg7_2_116) takes any answer, so it isn't listed.

    A "</col>" right before an answer's line break swallowed the break, so the next answer joined the
    red one's line (user, 2026-11-13: "Queen JaydesQueen Deejays"). The colour is now only ever set at the
    start of a line: red on the right answer's, the box's dark colour on the line after it.
*/
static const struct
{
    const char * key;
    const char * text;
} sAnswers[] = {
    // Dorguy the First (relF 0x19f6c0, 0x19f850, 0x19f9d8)
    {"stg7_2_014", "<select 0 -1 310 80>\n" RED "Queen Jaydes\n" NORMAL "Queen Deejays\nQueen Dayjes"},
    {"stg7_2_016", "<select 0 -1 350 80>\nGorguy the Thirst\n" RED "Dorguy the First\n" NORMAL "Borguy the 64th"},
    {"stg7_2_018", "<select 0 -1 190 80>\n5 times\n" RED "6 times\n" NORMAL "7 times"},
    // Dorguy the Second (relF 0x1a4a34-0x1a4de4: each right answer adds one to the score)
    {"stg7_2_075", "<select 0 -1 150 80>\n1\n2\n" RED "3"},
    {"stg7_2_077", "<select 0 -1 150 80>\n1\n" RED "2\n" NORMAL "3"},
    {"stg7_2_079", "<select 0 -1 250 80>\n" RED "Blue\n" NORMAL "Red\nSchmorange"},
    {"stg7_2_081", "<select 0 -1 280 80>\n" RED "4\n" NORMAL "5\n1,000,000,000"},
    {"stg7_2_083", "<select 0 -1 200 80>\n" RED "Triangles\n" NORMAL "Squares\nDonuts"},
};

const char * message(const char * msgName)
{
    if (msl::string::strncmp(msgName, "stg7_2_", 7) != 0 || !options_menu::get(options_menu::OPTION_DORGUY_ANSWERS))
        return nullptr;
    for (auto & answer : sAnswers)
        if (msl::string::strcmp(msgName, answer.key) == 0)
            return answer.text;
    return nullptr;
}

}
