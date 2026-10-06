#pragma once

#include <array>
#include <cctype>
#include <string>

namespace hushrig
{
/** Lê "v1.2.3" ou "1.2.3" (sufixos como "-beta" são ignorados). Devolve false se inválido. */
inline bool parseVersion (const std::string& text, std::array<int, 3>& out)
{
    size_t i = (! text.empty() && (text[0] == 'v' || text[0] == 'V')) ? 1 : 0;

    for (int part = 0; part < 3; ++part)
    {
        if (i >= text.size() || ! std::isdigit (static_cast<unsigned char> (text[i])))
            return false;

        long value = 0;
        while (i < text.size() && std::isdigit (static_cast<unsigned char> (text[i])))
        {
            value = value * 10 + (text[i] - '0');
            if (value > 1000000)
                return false;
            ++i;
        }
        out[static_cast<size_t> (part)] = static_cast<int> (value);

        if (part < 2)
        {
            if (i >= text.size() || text[i] != '.')
                return false;
            ++i;
        }
    }
    return true;
}

/** true somente se `latest` for estritamente maior que `current`. Versão inválida => false. */
inline bool isNewerVersion (const std::string& latest, const std::string& current)
{
    std::array<int, 3> a {}, b {};
    if (! parseVersion (latest, a) || ! parseVersion (current, b))
        return false;
    return a > b;
}
} // namespace hushrig
