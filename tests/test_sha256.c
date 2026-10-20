// sha256.h against the FIPS 180-4 / NIST test vectors, fed in every chunk
// size so the incremental path (an update streamed off the network) is covered.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/util/sha256.h"

static int g_n, g_fail;
#define CHECK(c) do { g_n++; if (!(c)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void hash_chunks(const void *data, size_t n, size_t chunk, char hex[65]) {
    sha256_ctx c; uint8_t d[32];
    sha256_init(&c);
    const unsigned char *p = (const unsigned char *)data;
    for (size_t off = 0; off < n; off += chunk)
        sha256_update(&c, p + off, n - off < chunk ? n - off : chunk);
    sha256_final(&c, d);
    sha256_hex(d, hex);
}

int main(void) {
    static const struct { const char *msg, *hex; } V[] = {
        { "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
        { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
          "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" },
    };
    const size_t chunks[] = { 1, 3, 63, 64, 65, 4096 };
    for (size_t v = 0; v < sizeof V / sizeof V[0]; v++)
        for (size_t c = 0; c < sizeof chunks / sizeof chunks[0]; c++) {
            char hex[65];
            hash_chunks(V[v].msg, strlen(V[v].msg), chunks[c], hex);
            CHECK(strcmp(hex, V[v].hex) == 0);
        }
    // One million 'a's: crosses many blocks and a 64-bit length.
    char *m = (char *)malloc(1000000);
    memset(m, 'a', 1000000);
    char hex[65];
    hash_chunks(m, 1000000, 65536, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
    hash_chunks(m, 1000000, 777, hex);
    CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
    free(m);
    printf("sha256: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
