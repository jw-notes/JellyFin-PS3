// release_asset.h on a real GitHub "latest release" response
// (fixtures/github/latest_release.json, vortigauntlet's v3.2.3), and on the
// shapes that could trip it: several assets, no exact name, no digest, and
// URLs inside the nested "uploader" object.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../source/net/release_asset.h"

static int g_n, g_fail;
#define CHECK(c) do { g_n++; if (!(c)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    b[n] = '\0'; fclose(f);
    return b;
}

int main(void) {
    release_asset a;

    char *real = slurp("fixtures/github/latest_release.json");
    CHECK(real != NULL);
    if (real) {
        CHECK(release_asset_find(real, "JellyFin-PS3.pkg", &a));
        CHECK(strcmp(a.name, "JellyFin-PS3.pkg") == 0);
        CHECK(strcmp(a.url, "https://github.com/vortigauntlet/JellyFin-PS3-LosslessAudio/releases/download/v3.2.3/JellyFin-PS3.pkg") == 0);
        CHECK(a.size == 3032928);
        CHECK(strcmp(a.sha256, "f368370a773fccddd624a89323aed32210677f818381dd21a4b056ecca130eb5") == 0);
        free(real);
    }

    // The uploader's URLs come first in each asset; they must not be taken.
    const char *two =
        "{\"tag_name\":\"v9\",\"assets\":["
        "{\"url\":\"https://api/x\",\"name\":\"notes.txt\",\"uploader\":{\"url\":\"https://u\",\"name\":\"JellyFin-PS3.pkg\"},"
        "\"size\":10,\"browser_download_url\":\"https://dl/notes.txt\"},"
        "{\"url\":\"https://api/y\",\"uploader\":{\"html_url\":\"https://u2\"},\"name\":\"JellyFin-PS3.pkg\","
        "\"size\":123456,\"digest\":\"sha256:" "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" "\","
        "\"browser_download_url\":\"https:\\/\\/dl\\/v9\\/JellyFin-PS3.pkg\"}]}";
    CHECK(release_asset_find(two, "JellyFin-PS3.pkg", &a));
    CHECK(strcmp(a.url, "https://dl/v9/JellyFin-PS3.pkg") == 0);
    CHECK(a.size == 123456);
    CHECK(strlen(a.sha256) == 64);

    // No exact name: the first .pkg is taken; no digest: sha256 is empty.
    const char *other =
        "{\"assets\":[{\"name\":\"readme.md\",\"browser_download_url\":\"https://dl/r\"},"
        "{\"name\":\"JellyFin-PS3-v4.pkg\",\"size\":7,\"browser_download_url\":\"https://dl/p\"}]}";
    CHECK(release_asset_find(other, "JellyFin-PS3.pkg", &a));
    CHECK(strcmp(a.name, "JellyFin-PS3-v4.pkg") == 0);
    CHECK(a.sha256[0] == '\0');
    CHECK(a.size == 7);

    // Nothing usable.
    CHECK(!release_asset_find("{\"assets\":[]}", "JellyFin-PS3.pkg", &a));
    CHECK(!release_asset_find("{\"assets\":[{\"name\":\"a.zip\",\"browser_download_url\":\"https://x\"}]}", NULL, &a));
    CHECK(!release_asset_find("{\"message\":\"Not Found\"}", NULL, &a));
    CHECK(!release_asset_find("{\"assets\":[{\"name\":\"x.pkg\",\"browser_download_url\":\"http://plain\"}]}", NULL, &a));
    CHECK(!release_asset_find("{\"assets\":[{\"name\":\"x.pkg\"", NULL, &a));     // truncated

    printf("release asset: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
