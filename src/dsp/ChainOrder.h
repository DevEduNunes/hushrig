#pragma once

#include <algorithm>
#include <array>
#include <string>

namespace hushrig
{
// Os ids sao estaveis (aparecem no estado salvo); a ordem padrao esta em ChainOrder::slots.
enum class Pedal : int { overdrive = 0, eq, chorus, delay, reverb, amp };

constexpr int kNumPedals = 6;

/** Ordem dos pedais na cadeia. Sempre uma permutacao valida; serializa como "0,5,1,2,3,4". */
struct ChainOrder
{
    // Padrao: overdrive > amp > eq > chorus > delay > reverb
    std::array<int, kNumPedals> slots { 0, 5, 1, 2, 3, 4 };

    static bool isValid (const std::array<int, kNumPedals>& s)
    {
        std::array<bool, kNumPedals> seen {};
        for (int v : s)
        {
            if (v < 0 || v >= kNumPedals || seen[static_cast<size_t> (v)])
                return false;
            seen[static_cast<size_t> (v)] = true;
        }
        return true;
    }

    /** Move o pedal da posicao `from` para `to`, empurrando os demais. */
    void move (int from, int to)
    {
        if (from < 0 || from >= kNumPedals || to < 0 || to >= kNumPedals || from == to)
            return;
        if (from < to)
            std::rotate (slots.begin() + from, slots.begin() + from + 1, slots.begin() + to + 1);
        else
            std::rotate (slots.begin() + to, slots.begin() + from, slots.begin() + from + 1);
    }

    std::string toString() const
    {
        std::string s;
        for (size_t i = 0; i < slots.size(); ++i)
            s += (i ? "," : "") + std::to_string (slots[i]);
        return s;
    }

    /** Texto invalido (ou vazio) devolve a ordem padrao. */
    static ChainOrder fromString (const std::string& text)
    {
        ChainOrder order;
        std::array<int, kNumPedals> parsed {};
        size_t count = 0, pos = 0;

        while (pos <= text.size() && count < parsed.size() + 1)
        {
            const auto comma = text.find (',', pos);
            const auto token = text.substr (pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (token.size() != 1 || token[0] < '0' || token[0] > '9' || count >= parsed.size())
                return order;
            parsed[count++] = token[0] - '0';
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }

        if (count == parsed.size() && isValid (parsed))
            order.slots = parsed;
        return order;
    }
};
} // namespace hushrig
