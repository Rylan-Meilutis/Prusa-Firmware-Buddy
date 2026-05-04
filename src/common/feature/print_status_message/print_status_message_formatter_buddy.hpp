#pragma once

#include <utils/string_builder.hpp>

#include "print_status_message.hpp"

class PrintStatusMessageFormatterBuddy {

public:
    using Message = PrintStatusMessage;

    static void format(StringBuilder &target, const Message &msg);
};
