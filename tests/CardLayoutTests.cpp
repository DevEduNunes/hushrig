#include <catch2/catch_test_macros.hpp>

#include "ui/CardLayout.h"

using hushrig::CardLayout;

namespace
{
// latência, níveis, controles, gravação, pedais, amp
const std::array<int, hushrig::kNumCards> kHeights { 118, 108, 196, 84, 276, 208 };

const hushrig::CardPlacement& find (const CardLayout::Result& r, hushrig::Card card)
{
    for (const auto& p : r.cards)
        if (p.card == static_cast<int> (card))
            return p;
    FAIL ("card ausente");
    return r.cards.front();
}
} // namespace

TEST_CASE ("Ordem dos cards: serializacao e validacao", "[cards]")
{
    CHECK (CardLayout::fromString ("0,1,2,3,4,5") == CardLayout::defaultOrder());
    CHECK (CardLayout::fromString ("5,4,3,2,1,0") == std::vector<int> { 5, 4, 3, 2, 1, 0 });
    CHECK (CardLayout::toString ({ 2, 0, 1, 3, 4, 5 }) == "2,0,1,3,4,5");

    for (const char* bad : { "", "abc", "0,1,2", "0,1,2,3,4,4", "0,1,2,3,4,6", "0,1,2,3,4,5,6", "0,,1,2,3,4", "-1,0,1,2,3,4" })
        CHECK (CardLayout::fromString (bad) == CardLayout::defaultOrder());
}

TEST_CASE ("Colunas conforme a largura", "[cards]")
{
    CHECK (CardLayout::columnsFor (300, 20, 640, 3) == 1);
    CHECK (CardLayout::columnsFor (680, 20, 640, 3) == 1);
    CHECK (CardLayout::columnsFor (1380, 20, 640, 3) == 2);
    CHECK (CardLayout::columnsFor (2100, 20, 640, 3) == 3);
    CHECK (CardLayout::columnsFor (5000, 20, 640, 3) == 3);
}

TEST_CASE ("Duas colunas: controles a esquerda, pedais e amp a direita", "[cards]")
{
    const auto r = CardLayout::flow (CardLayout::defaultOrder(), kHeights, 1380, 20, 12, 640, 3);

    REQUIRE (r.columns == 2);
    CHECK (find (r, hushrig::Card::latency).column == 0);
    CHECK (find (r, hushrig::Card::record).column == 0);
    CHECK (find (r, hushrig::Card::pedals).column == 1);
    CHECK (find (r, hushrig::Card::amp).column == 1);
    CHECK (find (r, hushrig::Card::pedals).y == 0);
    CHECK (find (r, hushrig::Card::amp).y == 276 + 12);
    CHECK (find (r, hushrig::Card::pedals).x == 1380 / 2 + 10);
}

TEST_CASE ("Uma coluna empilha tudo na ordem", "[cards]")
{
    const auto order = std::vector<int> { 5, 4, 0, 1, 2, 3 };
    const auto r = CardLayout::flow (order, kHeights, 700, 20, 12, 640, 3);

    REQUIRE (r.columns == 1);
    int y = 0;
    for (size_t i = 0; i < order.size(); ++i)
    {
        const int id = order[i];
        const auto& p = r.cards[i];
        CHECK (p.card == id);
        CHECK (p.column == 0);
        CHECK (p.y == y);
        CHECK (p.w == 700);
        y += kHeights[static_cast<size_t> (id)] + 12;
    }
    CHECK (r.contentHeight == y - 12);
}

TEST_CASE ("Nenhum card se sobrepoe e todos aparecem", "[cards]")
{
    for (const int width : { 700, 1000, 1400, 2000, 2600 })
    {
        const auto r = CardLayout::flow (CardLayout::defaultOrder(), kHeights, width, 20, 12, 640, 3);
        REQUIRE (r.cards.size() == hushrig::kNumCards);

        for (size_t i = 0; i < r.cards.size(); ++i)
            for (size_t j = i + 1; j < r.cards.size(); ++j)
            {
                const auto& a = r.cards[i];
                const auto& b = r.cards[j];
                const bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
                CHECK (apart);
            }

        for (const auto& p : r.cards)
            CHECK (p.x + p.w <= width);
    }
}
