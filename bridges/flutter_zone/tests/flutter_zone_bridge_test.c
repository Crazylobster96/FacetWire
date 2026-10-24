/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire_flutter_zone_bridge.h>
#include <facetwire/flutter_zone_profile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define CHECK(condition) do {                                             \
    if (!(condition)) {                                                   \
        fprintf(stderr, "bridge check failed at line %d\n", __LINE__);  \
        return 1;                                                         \
    }                                                                     \
} while (0)

static fw_string_view view(const char *text) {
    fw_string_view result = {text, strlen(text)};
    return result;
}

static int run(const char *profile_path, const char *legacy_path) {
    const char *identity = "org.facetwire.reference.status-tile-renderer";
    unsigned char bytes[FW_FLUTTER_ZONE_PROFILE_MAX_BYTES];
    size_t length = 123u;
    fw_status status;
    memset(bytes, 0x5a, sizeof(bytes));
    status = fw_flutter_zone_profile_load(view(profile_path), view(identity),
                                          bytes, sizeof(bytes), &length);
    if (status != FW_STATUS_OK)
        fprintf(stderr, "initial bridge status %d for %s\n", (int)status, profile_path);
    CHECK(status == FW_STATUS_OK);
    CHECK(length > 0u && length < sizeof(bytes));
    CHECK(memcmp(bytes, "{\"schema\":\"facetwire.declarative-zone.v1\"",
                 strlen("{\"schema\":\"facetwire.declarative-zone.v1\"")) == 0);
    CHECK(bytes[length] == 0x5a);

    length = 123u;
    status = fw_flutter_zone_profile_load(view(profile_path), view("wrong"),
                                          bytes, sizeof(bytes), &length);
    CHECK(status == FW_STATUS_INVALID_PLUGIN && length == 0u);
    CHECK(bytes[0] == '{');

    length = 123u;
    status = fw_flutter_zone_profile_load(view(profile_path), view(identity),
                                          bytes, 1u, &length);
    CHECK(status == FW_STATUS_BUFFER_TOO_SMALL && length == 0u);

    length = 123u;
    status = fw_flutter_zone_profile_load(
        view(legacy_path), view("org.facetwire.test.dynamic"),
        bytes, sizeof(bytes), &length);
    CHECK(status == FW_STATUS_UNSUPPORTED && length == 0u);

    length = 123u;
    status = fw_flutter_zone_profile_load(view("relative.dll"), view(identity),
                                          bytes, sizeof(bytes), &length);
    CHECK(status != FW_STATUS_OK && length == 0u);
    CHECK(fw_flutter_zone_profile_load(view(profile_path), view(identity),
                                        bytes, sizeof(bytes), NULL) ==
          FW_STATUS_INVALID_ARGUMENT);
    CHECK(fw_flutter_zone_profile_load(view(profile_path), view(identity),
                                        NULL, sizeof(bytes), &length) ==
          FW_STATUS_INVALID_ARGUMENT && length == 0u);
    return 0;
}

#if defined(_WIN32)
static char *utf8(const wchar_t *wide) {
    int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                     wide, -1, NULL, 0, NULL, NULL);
    char *result;
    if (needed <= 0) return NULL;
    result = (char *)malloc((size_t)needed);
    if (result == NULL) return NULL;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            wide, -1, result, needed, NULL, NULL) != needed) {
        free(result);
        return NULL;
    }
    return result;
}
int wmain(int argc, wchar_t **argv) {
    char *profile_path;
    char *legacy_path;
    int result;
    if (argc != 3) return 2;
    profile_path = utf8(argv[1]);
    legacy_path = utf8(argv[2]);
    if (profile_path == NULL || legacy_path == NULL) {
        free(profile_path);
        free(legacy_path);
        return 2;
    }
    result = run(profile_path, legacy_path);
    free(profile_path);
    free(legacy_path);
    return result;
}
#else
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    return run(argv[1], argv[2]);
}
#endif
