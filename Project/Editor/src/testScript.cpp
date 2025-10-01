#pragma once
#include "pch.h"

#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

#ifdef _WIN32
    #include <Windows.h>
    #define MAX_PATH_LENGTH MAX_PATH
    #define load_library(path) LoadLibraryW(path)
    #define get_export GetProcAddress
    #define unload_library FreeLibrary
    typedef HMODULE lib_handle;
#else
    #include <dlfcn.h>
    #include <limits.h>
    #define MAX_PATH_LENGTH PATH_MAX
    #define load_library(path) dlopen(path, RTLD_LAZY)
    #define get_export dlsym
    #define unload_library dlclose
    typedef void* lib_handle;
#endif

// Function pointers
hostfxr_initialize_for_runtime_config_fn init_fptr = nullptr;
hostfxr_get_runtime_delegate_fn get_delegate_fptr = nullptr;
hostfxr_close_fn close_fptr = nullptr;

bool load_hostfxr() {
    // Get the path to hostfxr
    char_t buffer[MAX_PATH_LENGTH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    
    if (get_hostfxr_path(buffer, &buffer_size, nullptr) != 0) {
        return false;
    }
    
    // Load hostfxr and get exports
    lib_handle lib = load_library(buffer);
    if (!lib) {
        return false;
    }
    
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)
        get_export(lib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)
        get_export(lib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)
        get_export(lib, "hostfxr_close");
    
    return init_fptr && get_delegate_fptr && close_fptr;
}

bool init_dotnet(const char_t* config_path) {
    // Initialize the runtime
    hostfxr_handle cxt = nullptr;
    int rc = init_fptr(config_path, nullptr, &cxt);
    if (rc != 0 || cxt == nullptr) {
        return false;
    }

    // Get a function pointer to load assemblies and call methods
    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;
    rc = get_delegate_fptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        (void**)&load_assembly_and_get_function_pointer
    );

    close_fptr(cxt);

    if (rc != 0 || load_assembly_and_get_function_pointer == nullptr) {
        return false;
    }

    // Load your C# assembly and get the function pointer
    typedef int (*hello_fn)(int);
    hello_fn hello = nullptr;

    rc = load_assembly_and_get_function_pointer(
        L"D:\\GAM300\\Project\\ScriptTest\\bin\\Debug\\net8.0\\ScriptTest.dll",
        L"GameScripts.ScriptTest, ScriptTest",
        L"HelloFromCSharp",
        nullptr,
        nullptr,
        (void**)&hello
    );

    if (rc != 0 || hello == nullptr) {
        return false;
    }

    // Call the C# function!
    int result = hello(42);
    printf("Result from C#: %d\n", result); // Should print 84

    return true;
}