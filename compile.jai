#run build();
build :: ()
{
  modify_print_style();
  set_build_options_dc(.{do_output=false});
  make_directory_if_it_does_not_exist("build");

  // Parse command-line arguments
  clean_sdl := false;
  build_sdl := false;


  after_dash := false;
  options := get_build_options();
  for options.compile_time_command_line
  {
    if it == {
      case "clean_sdl"; clean_sdl = true;
      case "sdl";       build_sdl = true;
      case "kill"; LogAndRun("raddbg --ipc kill_all"); // @todo use OS to kill processes
      case "-"; after_dash = true;
      case; log_error("Unknow argument: %", it);
    }
  }

  if clean_sdl
  {
    log("- deleting SDL build directories -");
    delete_directory("libs/SDL/build");
    delete_directory("libs/SDL_net/build");
    delete_directory("libs/SDL_image/build");
    delete_directory("libs/SDL_ttf/build");
  }

  if build_sdl
  {
    error_message = "Encountered an error when building SDL3.";

    log("- building SDL3 base -");
    workdir := "libs/SDL";
    Check(Run("cmake -S . -B build/win", workdir));
    Check(Run("cmake --build build/win", workdir));

    log("- building SDL net -");
    workdir = "libs/SDL_net";
    Check(Run("cmake -S . -B build/win -DSDL3_DIR=../SDL/build/win", workdir));
    Check(Run("cmake --build build/win", workdir));

    log("- building SDL image -");
    workdir = "libs/SDL_image";
    Check(Run("cmake -S . -B build/win -DSDL3_DIR=../SDL/build/win -DSDLIMAGE_VENDORED=OFF", workdir));
    Check(Run("cmake --build build/win", workdir));

    log("- generating SDL bindings -");
    SDL :: #import,file "jsrc/bindings/sdl3.jai";
    Check(SDL.GenerateBindings(.{targets = .BASE,
                                 optimizations = .DEBUG,
                                 base_directory = "libs/",
                                 output_filename = "sdl3_base.jai"}));
    Check(SDL.GenerateBindings(.{targets = .BASE | .NET,
                                 optimizations = .DEBUG,
                                 base_directory = "libs/",
                                 output_filename = "sdl3_base+net.jai"}));
    Check(SDL.GenerateBindings(.{targets = .BASE | .IMAGE,
                                 optimizations = .DEBUG,
                                 base_directory = "libs/",
                                 output_filename = "sdl3_base+image.jai"}));
    exit(0);
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
            if !success then log_error("Failed to set the icon for exe '%' to filename '%'.", exe_filename, icon_path);
          }
        }
      }
    }

    if message.kind == .COMPLETE break;
  }
}

Run :: (args: string, work_dir := "") -> success:bool
{
  return run_command(..break_command_into_strings(args), working_directory=work_dir).exit_code == 0;
}

LogAndRun :: (args: string)
{
  log("Custom command from build.jai: %", args);
  run_command(..break_command_into_strings(args));
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

error_message: string;
Check :: (success: bool, loc := #caller_location)
{
  if !success
  {
    log_error("% - %", error_message, loc);
    exit(1);
  }
}

#import "Compiler";
#import "Basic";
#import "File";
#import "String";
#import "Process";
