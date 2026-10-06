#include <catch2/catch_test_macros.hpp>

#include <string>

#include "update/Sha256.h"

using hushrig::Sha256;

TEST_CASE ("SHA-256 de texto vazio")
{
    REQUIRE (Sha256::hashHex ("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE ("SHA-256 de abc")
{
    REQUIRE (Sha256::hashHex ("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE ("SHA-256 de mensagem com mais de um bloco")
{
    REQUIRE (Sha256::hashHex ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
             == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE ("SHA-256 incremental gera o mesmo resultado")
{
    Sha256 sha;
    sha.update ("ab", 2);
    sha.update ("c", 1);
    REQUIRE (sha.finishHex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
