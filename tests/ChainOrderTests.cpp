#include <catch2/catch_test_macros.hpp>

#include "dsp/ChainOrder.h"

using hushrig::ChainOrder;

TEST_CASE ("ChainOrder comeca na ordem padrao (amp depois do overdrive)")
{
    REQUIRE (ChainOrder {}.toString() == "0,5,1,2,3,4");
}

TEST_CASE ("ChainOrder move pedais mantendo uma permutacao")
{
    ChainOrder o;
    o.move (0, 3);
    REQUIRE (o.toString() == "5,1,2,0,3,4");
    o.move (5, 0);
    REQUIRE (o.toString() == "4,5,1,2,0,3");
    o.move (2, 2);
    o.move (-1, 2);
    o.move (1, 9);
    REQUIRE (o.toString() == "4,5,1,2,0,3");
    REQUIRE (ChainOrder::isValid (o.slots));
}

TEST_CASE ("ChainOrder le texto e rejeita lixo")
{
    REQUIRE (ChainOrder::fromString ("4,3,2,1,0,5").toString() == "4,3,2,1,0,5");
    for (const char* bad : { "", "0,1,2,3,4", "0,1,2,3,4,4", "0,1,2,3,4,5,6", "a,b,c,d,e,f", "0,1,2,3,4,7", "012345", "0,,2,3,4,5" })
        REQUIRE (ChainOrder::fromString (bad).toString() == "0,5,1,2,3,4");
}
