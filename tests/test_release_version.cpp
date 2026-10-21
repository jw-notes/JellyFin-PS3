#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "release_version.h"
#include "update_check.h"

int main() {
    // The build's own release is up to date; anything later is an update.
    assert(strcmp(APP_VERSION, "3.3.4") == 0);
    assert(release_version_cmp("v3.3.4", APP_VERSION) == 0);
    assert(release_version_cmp("V3.3.4", APP_VERSION) == 0);
    assert(release_version_cmp("v3.3.5", APP_VERSION) > 0);
    assert(release_version_cmp("v3.4.0", APP_VERSION) > 0);
    assert(release_version_cmp("v4.0", APP_VERSION) > 0);
    // Older releases, including the upstream one this is based on, are not.
    assert(release_version_cmp("v3.2.3", APP_VERSION) < 0);
    assert(release_version_cmp("v3.3.1", APP_VERSION) < 0);
    assert(release_version_cmp("v3.3.2", APP_VERSION) < 0);
    assert(release_version_cmp("v3.3.3", APP_VERSION) < 0);
    assert(release_version_cmp("Testing", APP_VERSION) < 0);
    assert(release_version_cmp("3.2", "3.2.0") == 0);
    assert(release_version_cmp("999999999999999999999999", "3.2.3") > 0);
    assert(release_version_cmp("3.2.3", "999999999999999999999999") < 0);
    puts("release version: this release is current; later releases are updates, earlier ones are not");
}
