#run build();
build :: ()
{
  modify_print_style();
  set_build_options_dc(.{do_output=false});
  make_directory_if_it_does_not_exist("build");

  // Parse command-line arguments
  build_sdl := false;
  after_dash := false;
  options := get_build_options();
  for options.compile_time_command_line
  {
    if it == {
      case "-"; after_dash = true;
      case "sdl"; build_sdl = true;
      case "kill"; LogAndRun("raddbg", "--ipc", "kill_all"); // @todo use OS to kill processes
      case; log_error("Unknow argument: %\n", it);
    }
  }

  if build_sdl
  {
    err := false;
    log("- building sdl3 base -\n");
    if !err err = Run("cmake", "-S", ".", "-B", "build/win", work_dir="libs/SDL");
    if !err err = Run("cmake", "--build", "build/win", work_dir="libs/SDL");

    log("- building sdl3 net -\n");
    if !err err = Run("cmake", "-S", ".", "-B", "build/win", "-DSDL3_DIR=../SDL/build/win", work_dir="libs/SDL_net");
    if !err err = Run("cmake", "--build", "build/win", work_dir="libs/SDL_net");

    log("- building sdl3 image -\n");
    if !err err = Run("cmake", "-S", ".", "-B", "build/win", "-DSDL3_DIR=../SDL/build/win",
                      "-DSDLIMAGE_VENDORED=OFF", work_dir="libs/SDL_image");
    if !err err = Run("cmake", "--build", "build/win", work_dir="libs/SDL_image");

    SDL :: #import,file "jsrc/bindings/make/sdl3.jai";
    if !err err = !SDL.GenerateBindings(.{targets = .BASE,
                                          optimizations = .DEBUG,
                                          input_dir = "libs/",
                                          output_filename = "jsrc/bindings/generated/sdl3_base.jai"});
    if !err err = !SDL.GenerateBindings(.{targets = .BASE | .NET,
                                          optimizations = .DEBUG,
                                          input_dir = "libs/",
                                          output_filename = "jsrc/bindings/generated/sdl3_base+net.jai"});
    if !err err = !SDL.GenerateBindings(.{targets = .BASE | .IMAGE,
                                          optimizations = .DEBUG,
                                          input_dir = "libs/",
                                          output_filename = "jsrc/bindings/generated/sdl3_base+image.jai"});

    if err
    {
      log_error("Encountered error when building SDL3.");
      exit(1);
    }
    else exit(0);
  }

  // @todo put these workspaces on separate threads
  // @todo someway to auto-launch baker in the background?
  // @todo baker should have an option to be long-running background process in the future (for hot-reloading)
  Build(.{workspace_name = "Baker",
          output_name = "Baker",
          entry_file = "jsrc/baker/baker_entry.jai",
          icon_path = "res/ico/cook.ico"});

  Build(.{workspace_name = "Game",
          output_name = "Treasure",
          entry_file = "jsrc/game/game_sdl_entry.jai",
          icon_path = "res/ico/coin.ico"});
}

BuildConfig :: struct
{
  workspace_name: string;
  output_name: string;
  entry_file: string;
  icon_path: string;
};

Build :: (using config: BuildConfig)
{
  w := compiler_create_workspace(workspace_name);
  options := get_build_options(w);
  options.output_path = "build";
  options.output_executable_name = output_name;
  set_build_options(options, w);
  compiler_begin_intercept(w);
  add_build_file(entry_file, w);
  MessageLoop(w, config);
}

MessageLoop :: (w: Workspace, using config: BuildConfig)
{
  while true
  {
    message := compiler_wait_for_message();

    if message.kind == .PHASE
    {
      phase := message.(*Message_Phase);
      if phase.phase == .POST_WRITE_EXECUTABLE
      {
        if icon_path
        {
          exe_filename := tprint("%/%", get_build_options(message.workspace).output_path, phase.executable_name);
          #if OS == .WINDOWS
          {
            #import "Windows_Resources";
            success := set_icon_by_filename(exe_filename, icon_path);
            if !success then log_error("Failed to set the icon for exe '%' to filename '%'.\n", exe_filename, icon_path);
          }
        }
      }
    }

    if message.kind == .COMPLETE break;
  }
}

Run :: (args: ..string, work_dir := "") -> error:bool
{
  return run_command(..args, working_directory=work_dir).exit_code != 0;
}

LogAndRun :: (args: ..string)
{
  log("Custom command from build.jai: %\n", join(..args, " "));
  run_command(..args);
}

translate_precompiled_shader_headers :: ()
{
  // 0. (future) Trigger shader precompilation.
  // 1. Scan gen/ directory
  // 2. Do a simple string transformation to generate .jai files.
  // In the future steps 1 & 2 would be done only after shader precompilation - not for all game compilations.
}

modify_print_style :: ()
{
  using context.print_style.default_format_struct;
  use_newlines_if_long_form = true;
  indentation_width = 2;
}

#import "Compiler";
#import "Basic";
#import "File";
#import "String";
#import "Process";
