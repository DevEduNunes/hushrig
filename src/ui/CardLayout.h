#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

namespace hushrig
{
// Os ids são estáveis (a ordem escolhida pelo usuário é salva com eles).
enum class Card : int { latency = 0, meters, knobs, record, pedals, amp };

constexpr int kNumCards = 6;

struct CardPlacement
{
    int card = 0;
    int column = 0;
    int x = 0, y = 0, w = 0, h = 0; // relativos ao canto superior esquerdo da área de cards
};

/**
 * Distribui os cards em colunas, na ordem escolhida, conforme a largura disponível.
 * Header-only e sem dependência do JUCE, para ser testável isoladamente.
 */
struct CardLayout
{
    struct Result
    {
        std::vector<CardPlacement> cards;
        int columns = 1;
        int contentHeight = 0;
    };

    static std::vector<int> defaultOrder()
    {
        std::vector<int> order (kNumCards);
        for (int i = 0; i < kNumCards; ++i)
            order[static_cast<size_t> (i)] = i;
        return order;
    }

    static bool isValid (const std::vector<int>& order)
    {
        if (static_cast<int> (order.size()) != kNumCards)
            return false;

        std::array<bool, kNumCards> seen {};
        for (const int v : order)
        {
            if (v < 0 || v >= kNumCards || seen[static_cast<size_t> (v)])
                return false;
            seen[static_cast<size_t> (v)] = true;
        }
        return true;
    }

    static std::string toString (const std::vector<int>& order)
    {
        std::string out;
        for (size_t i = 0; i < order.size(); ++i)
            out += (i == 0 ? "" : ",") + std::to_string (order[i]);
        return out;
    }

    /** Lê "0,1,2,3,4,5"; qualquer coisa inválida volta à ordem padrão. */
    static std::vector<int> fromString (const std::string& text)
    {
        std::vector<int> parsed;
        size_t pos = 0;

        while (pos <= text.size())
        {
            const size_t comma = text.find (',', pos);
            const std::string token = text.substr (pos, comma == std::string::npos ? std::string::npos : comma - pos);

            if (token.empty() || token.find_first_not_of ("0123456789") != std::string::npos || token.size() > 3)
                return defaultOrder();

            parsed.push_back (std::atoi (token.c_str()));

            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }

        return isValid (parsed) ? parsed : defaultOrder();
    }

    /** Quantas colunas cabem: pelo menos 1, no máximo maxCols. */
    static int columnsFor (int width, int hGap, int minColW, int maxCols)
    {
        return std::clamp ((width + hGap) / (minColW + hGap), 1, std::max (1, maxCols));
    }

    /**
     * Preenche as colunas em sequência, da esquerda para a direita, equilibrando a altura:
     * um card muda de coluna quando passaria da altura média (contando só a metade dele).
     * `heights` é indexado pelo id do card.
     */
    static Result flow (const std::vector<int>& order, const std::array<int, kNumCards>& heights,
                        int areaWidth, int hGap, int vGap, int minColW, int maxCols)
    {
        Result result;
        result.columns = columnsFor (areaWidth, hGap, minColW, maxCols);

        const int n = result.columns;
        const int colW = std::max (1, (areaWidth - hGap * (n - 1)) / n);

        double total = vGap * (static_cast<double> (order.size()) - 1.0);
        for (const int id : order)
            total += heights[static_cast<size_t> (id)];
        const double target = total / n;

        int column = 0;
        int columnHeight = 0;

        for (const int id : order)
        {
            const int h = heights[static_cast<size_t> (id)];
            const int top = columnHeight > 0 ? columnHeight + vGap : 0;

            if (column < n - 1 && columnHeight > 0 && top + h / 2.0 > target)
            {
                ++column;
                columnHeight = 0;
            }

            const int y = columnHeight > 0 ? columnHeight + vGap : 0;
            result.cards.push_back ({ id, column, column * (colW + hGap), y, colW, h });
            columnHeight = y + h;
            result.contentHeight = std::max (result.contentHeight, columnHeight);
        }

        return result;
    }
};
} // namespace hushrig
