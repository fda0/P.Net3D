// Binding generation itself is taken from https://github.com/overlord-systems/jai-sdl3
// It was modified to make it easier to build it with custom SDL3 revisions and "satellite" libraries like SDL_net or SDL_image.

BindingsConfig :: struct
{
 targets: enum_flags {BASE; NET; IMAGE;};
 optimizations: enum {DEBUG; RELEASE;};
 base_directory: string;
 output_filename: string;
};

GenerateBindings :: (using config: BindingsConfig) -> bool
{
 opt_dir := ifx optimizations == .DEBUG then "Debug" else "Release";
 base_dir := base_directory;
 if base_dir.count > 0 && !(ends_with(base_dir, "/") || ends_with(base_dir, "\\"))
 {
  base_dir = tprint("%/", base_dir);
 }

 output_path := join(base_dir, output_filename);

 footer_builder: String_Builder;
 opts: Generate_Bindings_Options;
 opts.header = HEADER;
 opts.strip_flags = .FUNCTIONS_WITH_VALIST;

 AddTarget :: (dir_name: string, lib_name: string) #expand
 {
  build_path   := tprint("%/build/win/%/", dir_name, opt_dir);
  lib_path     := join(base_dir, build_path);
  include_path := tprint("%1%2/include", base_dir, dir_name);
  source_file  := tprint("%1%2/include/%/%.h", base_dir, dir_name, lib_name, dir_name);
  array_add(*opts.libnames,      lib_name);
  array_add(*opts.libpaths,      lib_path);
  array_add(*opts.include_paths, include_path);
  array_add(*opts.source_files,  source_file);
  print_to_builder(*footer_builder, "%1 :: #library,no_dll \"%2%3\";\n", to_lower_copy(lib_name), build_path, lib_name);
 }

 if targets & .BASE  then AddTarget("SDL", "SDL3");
 if targets & .NET   then AddTarget("SDL_net", "SDL3_net");
 if targets & .IMAGE then AddTarget("SDL_image", "SDL3_image");

 // @todo revisit this comment bellow:
 // DLL provided by user not in bin, so use our own footer not generated library declaration
 opts.footer = builder_to_string(*footer_builder);
 opts.generate_library_declarations = false;

 opts.visitor = sdl3_visitor;
 opts.generate_compile_time_struct_checks = false;
 return generate_bindings(opts, output_path);
}

sdl3_visitor :: (decl: *Declaration, parent_decl: *Declaration) -> Declaration_Visit_Result {

 if parent_decl return .RECURSE;

 if array_find(DECLARATIONS_TO_OMIT, decl.name) {
  decl.decl_flags |= .OMIT_FROM_OUTPUT;
  return .STOP;
 }

 return .RECURSE;
}

#import "Basic";
#import "Compiler";
#import "BuildCpp";
#import "Check";
#import "Bindings_Generator";
#import "File";
#import "String";

DECLARATIONS_TO_OMIT :: string.[
 "_m_prefetch",
 "__debugbreak",

 /*
    Those are force inlined functions defined in the headers, so they don't appear in the library and so bindings generator complains.

    One way is to automatically remove inline, compile them into a DLL, and turn them into #c_call jai procs. However, this loses inline.

    The performant way is to rewrite them in pure jai code, however that introduces overhead of ensuring they are updated.
    */
 "SDL_size_mul_check_overflow",
 "SDL_size_mul_check_overflow_builtin",
 "SDL_size_add_check_overflow",
 "SDL_size_add_check_overflow_builtin",
 "SDL_SwapFloat",
 "SDL_MostSignificantBitIndex32",
 "SDL_HasExactlyOneBitSet32",
 "SDL_RectToFRect",
 "SDL_PointInRect",
 "SDL_RectEmpty",
 "SDL_RectsEqual",
 "SDL_PointInRectFloat",
 "SDL_RectEmptyFloat",
 "SDL_RectsEqualEpsilon",
 "SDL_RectsEqualFloat",
];

/*
The header contains things not yet supported by bindings generator like the SDL_WindowFlags set of enums, as they use macros calling system macros.

These should rarely changed, but last checked for SDL 3.2.14
*/
HEADER :: #string EOF

/*
File=SDL_video.h
*/

// From https://wiki.libsdl.org/SDL3/SDL_WindowFlags
SDL_WINDOW_FULLSCREEN           : SDL_WindowFlags : 0x0000000000000001;    /**< window is in fullscreen mode */
SDL_WINDOW_OPENGL               : SDL_WindowFlags : 0x0000000000000002;    /**< window usable with OpenGL context */
SDL_WINDOW_OCCLUDED             : SDL_WindowFlags : 0x0000000000000004;    /**< window is occluded */
SDL_WINDOW_HIDDEN               : SDL_WindowFlags : 0x0000000000000008;    /**< window is neither mapped onto the desktop nor shown in the taskbar/dock/window list; SDL_ShowWindow() is required for it to become visible */
SDL_WINDOW_BORDERLESS           : SDL_WindowFlags : 0x0000000000000010;    /**< no window decoration */
SDL_WINDOW_RESIZABLE            : SDL_WindowFlags : 0x0000000000000020;    /**< window can be resized */
SDL_WINDOW_MINIMIZED            : SDL_WindowFlags : 0x0000000000000040;    /**< window is minimized */
SDL_WINDOW_MAXIMIZED            : SDL_WindowFlags : 0x0000000000000080;    /**< window is maximized */
SDL_WINDOW_MOUSE_GRABBED        : SDL_WindowFlags : 0x0000000000000100;    /**< window has grabbed mouse input */
SDL_WINDOW_INPUT_FOCUS          : SDL_WindowFlags : 0x0000000000000200;    /**< window has input focus */
SDL_WINDOW_MOUSE_FOCUS          : SDL_WindowFlags : 0x0000000000000400;    /**< window has mouse focus */
SDL_WINDOW_EXTERNAL             : SDL_WindowFlags : 0x0000000000000800;    /**< window not created by SDL */
SDL_WINDOW_MODAL                : SDL_WindowFlags : 0x0000000000001000;    /**< window is modal */
SDL_WINDOW_HIGH_PIXEL_DENSITY   : SDL_WindowFlags : 0x0000000000002000;    /**< window uses high pixel density back buffer if possible */
SDL_WINDOW_MOUSE_CAPTURE        : SDL_WindowFlags : 0x0000000000004000;    /**< window has mouse captured (unrelated to MOUSE_GRABBED) */
SDL_WINDOW_MOUSE_RELATIVE_MODE  : SDL_WindowFlags : 0x0000000000008000;    /**< window has relative mode enabled */
SDL_WINDOW_ALWAYS_ON_TOP        : SDL_WindowFlags : 0x0000000000010000;    /**< window should always be above others */
SDL_WINDOW_UTILITY              : SDL_WindowFlags : 0x0000000000020000;    /**< window should be treated as a utility window, not showing in the task bar and window list */
SDL_WINDOW_TOOLTIP              : SDL_WindowFlags : 0x0000000000040000;    /**< window should be treated as a tooltip and does not get mouse or keyboard focus, requires a parent window */
SDL_WINDOW_POPUP_MENU           : SDL_WindowFlags : 0x0000000000080000;    /**< window should be treated as a popup menu, requires a parent window */
SDL_WINDOW_KEYBOARD_GRABBED     : SDL_WindowFlags : 0x0000000000100000;    /**< window has grabbed keyboard input */
SDL_WINDOW_VULKAN               : SDL_WindowFlags : 0x0000000010000000;    /**< window usable for Vulkan surface */
SDL_WINDOW_METAL                : SDL_WindowFlags : 0x0000000020000000;    /**< window usable for Metal view */
SDL_WINDOW_TRANSPARENT          : SDL_WindowFlags : 0x0000000040000000;    /**< window with transparent buffer */
SDL_WINDOW_NOT_FOCUSABLE        : SDL_WindowFlags : 0x0000000080000000;    /**< window should not be focusable */

/**
 * Used to indicate that you don't care what the window position is.
 *
 * If you _really_ don't care, SDL_WINDOWPOS_UNDEFINED is the same, but always
 * uses the primary display instead of specifying one.
 *
 * \param X the SDL_DisplayID of the display to use.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// Original: #define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
SDL_WINDOWPOS_UNDEFINED_DISPLAY :: inline (display_id: SDL_DisplayID) -> SDL_DisplayID {
    return SDL_WINDOWPOS_UNDEFINED_MASK | display_id;
}

/**
 * Used to indicate that you don't care what the window position/display is.
 *
 * This always uses the primary display.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// #define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)
SDL_WINDOWPOS_UNDEFINED :: #bake_arguments SDL_WINDOWPOS_UNDEFINED_DISPLAY(display_id = 0);

/**
 * A macro to test if the window position is marked as "undefined."
 *
 * \param X the window position value.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// #define SDL_WINDOWPOS_ISUNDEFINED(X)    (((X)&0xFFFF0000) == SDL_WINDOWPOS_UNDEFINED_MASK)
SDL_WINDOWPOS_ISUNDEFINED :: inline (display_id: SDL_DisplayID) -> bool {
    return (display_id & 0xFFFF0000) == SDL_WINDOWPOS_UNDEFINED_MASK;
}


/**
 * Used to indicate that the window position should be centered.
 *
 * SDL_WINDOWPOS_CENTERED is the same, but always uses the primary display
 * instead of specifying one.
 *
 * \param X the SDL_DisplayID of the display to use.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// #define SDL_WINDOWPOS_CENTERED_DISPLAY(X)  (SDL_WINDOWPOS_CENTERED_MASK|(X))
SDL_WINDOWPOS_CENTERED_DISPLAY :: inline (display_id: SDL_DisplayID) -> SDL_DisplayID {
    return SDL_WINDOWPOS_CENTERED_MASK | display_id;
}

/**
 * Used to indicate that the window position should be centered.
 *
 * This always uses the primary display.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// #define SDL_WINDOWPOS_CENTERED         SDL_WINDOWPOS_CENTERED_DISPLAY(0)
SDL_WINDOWPOS_CENTERED :: #bake_arguments SDL_WINDOWPOS_CENTERED_DISPLAY(display_id = 0);

/**
 * A macro to test if the window position is marked as "centered."
 *
 * \param X the window position value.
 *
 * \since This macro is available since SDL 3.2.0.
 */
// #define SDL_WINDOWPOS_ISCENTERED(X)    \
            // (((X)&0xFFFF0000) == SDL_WINDOWPOS_CENTERED_MASK)
SDL_WINDOWPOS_ISCENTERED :: inline (pos: u32) -> bool {
    return (pos & 0xFFFF0000) == SDL_WINDOWPOS_CENTERED_MASK;
}
EOF
