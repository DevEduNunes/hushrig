#include <catch2/catch_test_macros.hpp>

#include "update/VersionCompare.h"

using hushrig::isNewerVersion;

TEST_CASE ("Versao maior e detectada")
{
    REQUIRE (isNewerVersion ("v0.2.0", "0.1.0"));
    REQUIRE (isNewerVersion ("1.0.0", "0.9.9"));
    REQUIRE (isNewerVersion ("0.1.10", "0.1.9"));
}

TEST_CASE ("Versao igual ou menor nao e atualizacao")
{
    REQUIRE_FALSE (isNewerVersion ("v0.1.0", "0.1.0"));
    REQUIRE_FALSE (isNewerVersion ("0.1.0", "0.1.1"));
    REQUIRE_FALSE (isNewerVersion ("0.9.0", "1.0.0"));
}

TEST_CASE ("Versao invalida nunca e atualizacao")
{
    REQUIRE_FALSE (isNewerVersion ("", "0.1.0"));
    REQUIRE_FALSE (isNewerVersion ("latest", "0.1.0"));
    REQUIRE_FALSE (isNewerVersion ("1.2", "0.1.0"));
    REQUIRE_FALSE (isNewerVersion ("v1.0.0", "abc"));
}

TEST_CASE ("Sufixo de pre-release e ignorado")
{
    REQUIRE (isNewerVersion ("v0.2.0-beta", "0.1.0"));
}
