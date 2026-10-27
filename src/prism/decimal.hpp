// Prism - Locale-free decimal parsing.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The console's C library does not parse JSON decimals the way nlohmann expects
// (a Home Assistant registry timestamp ended the app in the JSON lexer), so
// numbers are read here instead of through localeconv() and strtod().
#pragma once
#include <cstdint>
#include <limits>

namespace prism
{
// Reads [+-]digits[.digits][(e|E)[+-]digits] from text. end, when given, is set
// past the last character used, or to text when there is no number. Keeps 19
// significant digits, which is far more than any value Prism shows.
inline double parse_decimal(const char *text, char **end = nullptr)
{
    const char *p = text;
    const bool negative = *p == '-';
    if (*p == '-' || *p == '+')
        ++p;
    std::uint64_t mantissa = 0;
    int significant = 0, exponent = 0;
    bool any = false;
    for (; *p >= '0' && *p <= '9'; ++p, any = true)
        if (significant < 19)
        {
            mantissa = mantissa * 10 + static_cast<std::uint64_t>(*p - '0');
            significant += mantissa != 0;
        }
        else
            ++exponent;
    if (*p == '.')
        for (++p; *p >= '0' && *p <= '9'; ++p, any = true)
            if (significant < 19)
            {
                mantissa = mantissa * 10 + static_cast<std::uint64_t>(*p - '0');
                significant += mantissa != 0;
                --exponent;
            }
    if (!any)
    {
        if (end)
            *end = const_cast<char *>(text);
        return 0;
    }
    if (*p == 'e' || *p == 'E')
    {
        const char *q = p + 1;
        const bool down = *q == '-';
        if (*q == '-' || *q == '+')
            ++q;
        if (*q >= '0' && *q <= '9')
        {
            int power = 0;
            for (; *q >= '0' && *q <= '9'; ++q)
                if (power < 100000)
                    power = power * 10 + (*q - '0');
            exponent += down ? -power : power;
            p = q;
        }
    }
    if (end)
        *end = const_cast<char *>(p);
    double value = static_cast<double>(mantissa);
    if (value != 0 && exponent > 400)
        value = std::numeric_limits<double>::infinity();
    else if (value != 0 && exponent < -400)
        value = 0;
    else if (exponent != 0)
    {
        double scale = 1, base = 10;
        for (int n = exponent < 0 ? -exponent : exponent; n; n >>= 1, base *= base)
            if (n & 1)
                scale *= base;
        value = exponent < 0 ? value / scale : value * scale;
    }
    return negative ? -value : value;
}
} // namespace prism
