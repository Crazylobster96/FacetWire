/* SPDX-License-Identifier: MPL-2.0 */
#include "dynamic_library.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING) && defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define FW_NATIVE_DYNAMIC_BACKEND 1
#elif defined(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING) && defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX && !TARGET_OS_IPHONE
#include <dlfcn.h>
#define FW_NATIVE_DYNAMIC_BACKEND 2
#endif
#elif defined(FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING) && \
    (defined(__linux__) || defined(__unix__))
#include <dlfcn.h>
#define FW_NATIVE_DYNAMIC_BACKEND 2
#endif

static int fw_path_is_absolute(fw_string_view path) {
    if (path.data == NULL || path.length == 0u ||
        memchr(path.data, '\0', path.length) != NULL) {
        return 0;
    }
#if defined(_WIN32)
    if (path.length >= 3u &&
        ((path.data[0] >= 'A' && path.data[0] <= 'Z') ||
         (path.data[0] >= 'a' && path.data[0] <= 'z')) &&
        path.data[1] == ':' &&
        (path.data[2] == '\\' || path.data[2] == '/')) {
        return 1;
    }
    return path.length >= 2u &&
           ((path.data[0] == '\\' && path.data[1] == '\\') ||
            (path.data[0] == '/' && path.data[1] == '/'));
#else
    return path.data[0] == '/';
#endif
}

fw_status fw_native_library_open(fw_string_view absolute_path,
                                 fw_native_library *out_library,
                                 fw_plugin_query_fn *out_query) {
    if (out_library == NULL || out_query == NULL) {
        return FW_STATUS_INVALID_ARGUMENT;
    }
    out_library->handle = NULL;
    *out_query = NULL;
    if (!fw_path_is_absolute(absolute_path)) {
        return FW_STATUS_INVALID_ARGUMENT;
    }

#if FW_NATIVE_DYNAMIC_BACKEND == 1
    {
        int wide_length;
        wchar_t *wide_path;
        HMODULE module;
        FARPROC symbol;
        fw_plugin_query_fn query = NULL;

        if (absolute_path.length > (size_t)INT_MAX) {
            return FW_STATUS_RESOURCE_LIMIT;
        }
        wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          absolute_path.data,
                                          (int)absolute_path.length,
                                          NULL, 0);
        if (wide_length <= 0) {
            return FW_STATUS_INVALID_ARGUMENT;
        }
        wide_path = (wchar_t *)calloc((size_t)wide_length + 1u,
                                      sizeof(*wide_path));
        if (wide_path == NULL) {
            return FW_STATUS_OUT_OF_MEMORY;
        }
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                absolute_path.data,
                                (int)absolute_path.length,
                                wide_path, wide_length) != wide_length) {
            free(wide_path);
            return FW_STATUS_INVALID_ARGUMENT;
        }
        module = LoadLibraryExW(wide_path, NULL,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        free(wide_path);
        if (module == NULL) {
            return FW_STATUS_NOT_FOUND;
        }
        symbol = GetProcAddress(module, FW_PLUGIN_QUERY_SYMBOL);
        if (symbol == NULL || sizeof(query) != sizeof(symbol)) {
            FreeLibrary(module);
            return FW_STATUS_INVALID_PLUGIN;
        }
        memcpy(&query, &symbol, sizeof(query));
        out_library->handle = (void *)module;
        *out_query = query;
        return FW_STATUS_OK;
    }
#elif FW_NATIVE_DYNAMIC_BACKEND == 2
    {
        char *path;
        void *module;
        void *symbol;
        fw_plugin_query_fn query = NULL;

        path = (char *)malloc(absolute_path.length + 1u);
        if (path == NULL) {
            return FW_STATUS_OUT_OF_MEMORY;
        }
        memcpy(path, absolute_path.data, absolute_path.length);
        path[absolute_path.length] = '\0';
        module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        free(path);
        if (module == NULL) {
            return FW_STATUS_NOT_FOUND;
        }
        symbol = dlsym(module, FW_PLUGIN_QUERY_SYMBOL);
        if (symbol == NULL || sizeof(query) != sizeof(symbol)) {
            dlclose(module);
            return FW_STATUS_INVALID_PLUGIN;
        }
        memcpy(&query, &symbol, sizeof(query));
        out_library->handle = module;
        *out_query = query;
        return FW_STATUS_OK;
    }
#else
    (void)absolute_path;
    return FW_STATUS_UNSUPPORTED;
#endif
}

void fw_native_library_close(fw_native_library *library) {
    if (library == NULL || library->handle == NULL) {
        return;
    }
#if FW_NATIVE_DYNAMIC_BACKEND == 1
    FreeLibrary((HMODULE)library->handle);
#elif FW_NATIVE_DYNAMIC_BACKEND == 2
    dlclose(library->handle);
#endif
    library->handle = NULL;
}
