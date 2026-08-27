/* SPDX-License-Identifier: MPL-2.0 */
#include <facetwire/core_content.h>
#include <facetwire/runtime.h>

#if defined(NDEBUG)
#undef NDEBUG
#endif

#include <assert.h>
#include <string.h>

static const int static_interface_token = 41;
static int static_unload_count;

static int view_equal(fw_string_view value, const char *expected) {
    const size_t length = strlen(expected);
    return value.length == length && value.data != NULL &&
           memcmp(value.data, expected, length) == 0;
}

static fw_string_view view_from_cstr(const char *value) {
    const fw_string_view result = {value, strlen(value)};
    return result;
}

static const fw_capability_descriptor_v1 static_capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.render"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"),
        0u,
    },
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.static.only"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.diagnostic"),
        0u,
    },
};

static const fw_plugin_descriptor_v1 static_descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.test.static"),
    FW_STRING_VIEW_LITERAL("Static discovery test plugin"),
    FW_STRING_VIEW_LITERAL("FacetWire"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    static_capabilities,
    2u,
};

static const fw_plugin_descriptor_v1 *FW_CALL get_static_descriptor(void) {
    return &static_descriptor;
}

static fw_status FW_CALL load_static(const fw_host_api_v1 *host,
                                     fw_plugin_handle *out_handle) {
    assert(host != NULL);
    assert(out_handle != NULL);
    *out_handle = (fw_plugin_handle)&static_descriptor;
    return FW_STATUS_OK;
}

static void FW_CALL unload_static(fw_plugin_handle handle) {
    assert(handle == (fw_plugin_handle)&static_descriptor);
    ++static_unload_count;
}

static fw_status FW_CALL query_static_interface(
    fw_plugin_handle handle,
    fw_string_view interface_id,
    uint32_t minimum_version,
    const void **out_interface) {
    assert(handle == (fw_plugin_handle)&static_descriptor);
    if (out_interface == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    *out_interface = NULL;
    if (!view_equal(interface_id, "org.facetwire.test.echo/1") ||
        minimum_version > 1u) {
        return FW_STATUS_NOT_FOUND;
    }
    *out_interface = &static_interface_token;
    return FW_STATUS_OK;
}

static const fw_plugin_api_v1 static_api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    get_static_descriptor,
    load_static,
    unload_static,
    query_static_interface,
};

static const fw_plugin_api_v1 *FW_CALL query_static(
    fw_abi_version requested_abi) {
    return requested_abi.major == FW_ABI_VERSION_MAJOR ? &static_api : NULL;
}

static const fw_capability_descriptor_v1 duplicate_capabilities[] = {
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.duplicate"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.diagnostic"),
        0u,
    },
    {
        sizeof(fw_capability_descriptor_v1),
        FW_STRING_VIEW_LITERAL("test.duplicate"),
        FW_STRING_VIEW_LITERAL("facetwire.capability.renderer"),
        0u,
    },
};

static const fw_plugin_descriptor_v1 duplicate_descriptor = {
    sizeof(fw_plugin_descriptor_v1),
    FW_ABI_VERSION_INIT,
    FW_STRING_VIEW_LITERAL("org.facetwire.test.duplicate"),
    FW_STRING_VIEW_LITERAL("Invalid duplicate capability plugin"),
    FW_STRING_VIEW_LITERAL("FacetWire"),
    FW_STRING_VIEW_LITERAL("0.1.0"),
    duplicate_capabilities,
    2u,
};

static const fw_plugin_descriptor_v1 *FW_CALL get_duplicate_descriptor(void) {
    return &duplicate_descriptor;
}

static const fw_plugin_api_v1 duplicate_api = {
    sizeof(fw_plugin_api_v1),
    FW_ABI_VERSION_INIT,
    get_duplicate_descriptor,
    load_static,
    NULL,
    NULL,
};

static const fw_plugin_api_v1 *FW_CALL query_duplicate(
    fw_abi_version requested_abi) {
    return requested_abi.major == FW_ABI_VERSION_MAJOR ? &duplicate_api : NULL;
}

int main(int argc, char **argv) {
    const fw_host_api_v1 host = {
        sizeof(fw_host_api_v1), FW_ABI_VERSION_INIT, NULL, NULL};
    const fw_runtime_config_v1 config = {
        sizeof(fw_runtime_config_v1), &host, 4u};
    const fw_plugin_descriptor_v1 *descriptor = NULL;
    const void *interface_value = (const void *)1;
    fw_capability_match_v1 match = {
        sizeof(fw_capability_match_v1), 0u, NULL, NULL,
        FW_PLUGIN_SOURCE_UNKNOWN};
    fw_runtime *runtime = NULL;
    size_t plugin_index = 99u;

    assert(fw_runtime_create(&config, &runtime) == FW_STATUS_OK);
    assert(strcmp(FW_CONTENT_TYPE_TEXT, "text") == 0);
    assert(strcmp(FW_VIDEO_RENDERER_CAPABILITY_ID,
                  "facetwire.renderer.video") == 0);
    assert(fw_runtime_register_static(runtime, query_duplicate, NULL) ==
           FW_STATUS_INVALID_PLUGIN);
    assert(fw_runtime_plugin_count(runtime) == 0u);

    assert(fw_runtime_register_static(runtime, query_static, &descriptor) ==
           FW_STATUS_OK);
    assert(descriptor == &static_descriptor);
    assert(fw_runtime_plugin_source_at(runtime, 0u) ==
           FW_PLUGIN_SOURCE_STATIC);
    assert(fw_runtime_plugin_source_at(runtime, 1u) ==
           FW_PLUGIN_SOURCE_UNKNOWN);

    assert(fw_runtime_find_plugin(
        runtime, view_from_cstr("org.facetwire.test.static"),
        &plugin_index, &descriptor) == FW_STATUS_OK);
    assert(plugin_index == 0u && descriptor == &static_descriptor);
    assert(fw_runtime_find_plugin(
        runtime, view_from_cstr("org.facetwire.test.missing"),
        NULL, NULL) == FW_STATUS_NOT_FOUND);

    assert(fw_runtime_find_capability(
        runtime, view_from_cstr("test.render"), 0u, &match) ==
        FW_STATUS_OK);
    assert(match.plugin_index == 0u && match.plugin == &static_descriptor);
    assert(match.capability == &static_capabilities[0]);
    assert(match.source == FW_PLUGIN_SOURCE_STATIC);
    assert(fw_runtime_find_capability(
        runtime, view_from_cstr("test.render"), 1u, &match) ==
        FW_STATUS_NOT_FOUND);

    assert(fw_runtime_select_capability(
        runtime, view_from_cstr("test.render"),
        (fw_string_view){NULL, 0u}, &match) == FW_STATUS_OK);
    assert(match.plugin == &static_descriptor);
    assert(fw_runtime_select_capability(
        runtime, view_from_cstr("test.dynamic.only"),
        view_from_cstr("org.facetwire.test.static"), &match) ==
        FW_STATUS_NOT_FOUND);
    assert(match.plugin == NULL && match.capability == NULL);
    assert(fw_runtime_select_capability(
        runtime, view_from_cstr("test.render"),
        view_from_cstr("org.facetwire.test.missing"), NULL) ==
        FW_STATUS_INVALID_ARGUMENT);

    assert(fw_runtime_query_interface(
        runtime, view_from_cstr("org.facetwire.test.static"),
        view_from_cstr("org.facetwire.test.echo/1"), 1u,
        &interface_value) == FW_STATUS_OK);
    assert(interface_value == &static_interface_token);
    interface_value = (const void *)1;
    assert(fw_runtime_query_interface(
        runtime, view_from_cstr("org.facetwire.test.static"),
        view_from_cstr("org.facetwire.test.echo/1"), 2u,
        &interface_value) == FW_STATUS_NOT_FOUND);
    assert(interface_value == NULL);
    assert(fw_runtime_unload_dynamic(
        runtime, view_from_cstr("org.facetwire.test.static")) ==
        FW_STATUS_UNSUPPORTED);

    if (argc == 2) {
        const fw_string_view relative_path =
            FW_STRING_VIEW_LITERAL("facetwire_dynamic_test_plugin");
        const fw_string_view dynamic_path = {argv[1], strlen(argv[1])};

        assert(fw_runtime_load_dynamic(runtime, relative_path, NULL) ==
               FW_STATUS_INVALID_ARGUMENT);
        assert(fw_runtime_load_dynamic(runtime, dynamic_path, &descriptor) ==
               FW_STATUS_OK);
        assert(view_equal(descriptor->id, "org.facetwire.test.dynamic"));
        assert(fw_runtime_plugin_count(runtime) == 2u);
        assert(fw_runtime_plugin_source_at(runtime, 1u) ==
               FW_PLUGIN_SOURCE_NATIVE_DYNAMIC);

        match.struct_size = sizeof(match);
        assert(fw_runtime_find_capability(
            runtime, view_from_cstr("test.render"), 1u, &match) ==
            FW_STATUS_OK);
        assert(match.plugin_index == 1u);
        assert(match.source == FW_PLUGIN_SOURCE_NATIVE_DYNAMIC);
        assert(fw_runtime_select_capability(
            runtime, view_from_cstr("test.render"),
            view_from_cstr("org.facetwire.test.dynamic"), &match) ==
            FW_STATUS_OK);
        assert(match.plugin_index == 1u);

        assert(fw_runtime_query_interface(
            runtime, view_from_cstr("org.facetwire.test.dynamic"),
            view_from_cstr("org.facetwire.test.echo/1"), 1u,
            &interface_value) == FW_STATUS_OK);
        assert(interface_value != NULL);
        assert(fw_runtime_unload_dynamic(
            runtime, view_from_cstr("org.facetwire.test.dynamic")) ==
            FW_STATUS_OK);
        assert(fw_runtime_plugin_count(runtime) == 1u);
        assert(fw_runtime_find_plugin(
            runtime, view_from_cstr("org.facetwire.test.dynamic"),
            NULL, NULL) == FW_STATUS_NOT_FOUND);

        assert(fw_runtime_load_dynamic(runtime, dynamic_path, NULL) ==
               FW_STATUS_OK);
    } else {
        assert(argc == 1);
#if defined(_WIN32)
        assert(fw_runtime_load_dynamic(
            runtime, view_from_cstr("C:\\facetwire-disabled.dll"), NULL) ==
            FW_STATUS_UNSUPPORTED);
#else
        assert(fw_runtime_load_dynamic(
            runtime, view_from_cstr("/facetwire-disabled.so"), NULL) ==
            FW_STATUS_UNSUPPORTED);
#endif
    }

    fw_runtime_destroy(runtime);
    assert(static_unload_count == 1);
    return 0;
}
