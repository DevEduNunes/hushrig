#include <catch2/catch_test_macros.hpp>

#include "dsp/ChainOrder.h"

using hushrig::ChainOrder;

TEST_CASE ("ChainOrder comeca na ordem padrao")
{
    REQUIRE (ChainOrder {}.toString() == "0,1,2,3,4");
}

TEST_CASE ("ChainOrder move pedais mantendo uma permutacao")
{
    ChainOrder o;
    o.move (0, 3);
    REQUIRE (o.toString() == "1,2,3,0,4");
    o.move (4, 0);
    REQUIRE (o.toString() == "4,1,2,3,0");
    o.move (2, 2);
    o.move (-1, 2);
    o.move (1, 9);
    REQUIRE (o.toString() == "4,1,2,3,0");
    REQUIRE (ChainOrder::isValid (o.slots));
}

TEST_CASE ("ChainOrder le texto e rejeita lixo")
{
    REQUIRE (ChainOrder::fromString ("4,3,2,1,0").toString() == "4,3,2,1,0");
    for (const char* bad : { "", "0,1,2,3", "0,1,2,3,3", "0,1,2,3,4,5", "a,b,c,d,e", "0,1,2,3,7", "01234", "0,,2,3,4" })
        REQUIRE (ChainOrder::fromString (bad).toString() == "0,1,2,3,4");
}
