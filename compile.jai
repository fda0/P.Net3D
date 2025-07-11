LOG_RUN :: false;
BUILD_DIRECTORY :: "build";

#run build();
build :: ()
{
  modify_print_style();
  set_build_options_dc(.{do_output=false});

  // Parse command-line arguments
  kill_processes := false;
  clean_targets: enum_flags {
    BUILD;
    SDL;
    ALL :: ~0;
  };
  build_targets: enum_flags {
    TREASURE;
    BAKER;
    SDL;
    CGLTF;
    ALL :: ~0;
  };

  options := get_build_options();
  for options.compile_time_command_line
  {
    if it == {
      case "kill";        kill_processes = true;
      case "clean_sdl";   clean_targets |= .SDL;
      case "clean_build"; clean_targets |= .BUILD;
      case "clean_all";   clean_targets |= .ALL;
      case "treasure";    build_targets |= .TREASURE;
      case "baker";       build_targets |= .BAKER;
      case "sdl";         build_targets |= .SDL;
      case "cgltf";       build_targets |= .CGLTF;
      case "build_all";   build_targets |= .ALL;
      case; log_error("Unknown argument: %", it); exit(1);
    }
  }

  if build_targets == 0
  {
    build_targets = .TREASURE | .BAKER;
    log("No build targets provided. Adding defaults: treasure baker");
  }

  if kill_processes
  {
    Run("taskkill /im Treasure.exe", silent=true);
    Run("taskkill /im Baker.exe", silent=true);
  }

  if clean_targets & .BUILD
  {
    log("- deleting build directory -");
    delete_directory("build");
  }
  make_directory_if_it_does_not_exist(BUILD_DIRECTORY);

  if clean_targets & .SDL
  {
    log("- deleting SDL build directories -");
    delete_directory("libs/SDL/build");
    delete_directory("libs/SDL_net/build");
    delete_directory("libs/SDL_image/build");
    delete_directory("libs/SDL_ttf/build");
  }

  if build_targets & .SDL
  {
    error_message = "Encountered an error when building SDL3 & generating bindings.";

    stage1 := "cmake -S . -B build/win";
    stage2 := "cmake --build build/win";
    stage1b := join(stage1, " -DSDL3_DIR=../SDL/build/win");

    workdir := "libs/SDL";
    log("- building % -", workdir);
    Check(Run(stage1, workdir));
    Check(Run(stage2, workdir));

    workdir = "libs/SDL_net";
    log("- building % -", workdir);
    Check(Run(stage1b, workdir));
    Check(Run(stage2, workdir));

    workdir = "libs/SDL_image";
    log("- building % -", workdir);
    Check(Run(join(stage1b, " -DSDLIMAGE_VENDORED=OFF"), workdir));
    Check(Run(stage2, workdir));

    log("- generating SDL bindings -");
    SDL :: #import,file "jsrc/bindings/generate_bindings_sdl3.jai";
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
  }

  // copy SDL dll dependencies into build directory if missing
  {
    copies: [] struct {dst, src: string;} = .[
      .{"build/SDL3.dll", "libs/SDL/build/win/Debug/SDL3.dll"},
      .{"build/SDL3_net.dll", "libs/SDL_net/build/win/Debug/SDL3_net.dll"},
      .{"build/SDL3_image.dll", "libs/SDL_image/build/win/Debug/SDL3_image.dll"},
    ];
    for copies if !file_exists(it.dst) then copy_file(it.src, it.dst);
  }

  if build_targets & .CGLTF
  {
    CGLTF :: #import,file "jsrc/bindings/build_cgltf.jai";
    CGLTF.Build();
  }

  if build_targets & .BAKER
  {
    Build(.{workspace_name = "Baker",
            output_name = "Baker",
            entry_file = "jsrc/baker/baker_entry.jai",
            icon_path = "res/ico/cook.ico"});
  }

  if build_targets & .TREASURE
  {
    Build(.{workspace_name = "Treasure",
            output_name = "Treasure",
            entry_file = "jsrc/game/game_sdl_entry.jai",
            icon_path = "res/ico/coin.ico"});
  }

  MessageLoop();
}

BuildConfig :: struct
{
  workspace_name: string;
  output_name: string;
  entry_file: string;
  icon_path: string;
};

BuildInProgress :: struct
{
  w: Workspace;
  using config: BuildConfig;
};

builds_in_progress: [..] BuildInProgress;

Build :: (using config: BuildConfig)
{
  w := compiler_create_workspace(workspace_name);
  options := get_build_options(w);
  options.output_path = BUILD_DIRECTORY;
  options.output_executable_name = output_name;
  options.text_output_flags = .OUTPUT_TIMING_INFO;

  modules_path: [..] string;
  array_add(*modules_path, "libs/");
  array_add(*modules_path, ..options.import_path);
  options.import_path = modules_path;

  set_build_options(options, w);
  compiler_begin_intercept(w, .SKIP_ALL);
  add_build_file(entry_file, w);

  array_add(*builds_in_progress, .{w, config});
}

MessageLoop :: ()
{
  finished_builds := 0;
  while finished_builds < builds_in_progress.count
  {
    message := compiler_wait_for_message();
    if message.kind == .COMPLETE then finished_builds += 1;
    if message.kind == .PHASE
    {
      phase := message.(*Message_Phase);
      if phase.phase == .POST_WRITE_EXECUTABLE
      {
        // Find config that matches this workspace
        config: BuildConfig;
        for builds_in_progress
        {
          if it.w == message.workspace
          {
            config = it.config;
            break;
          }
        }

        using config;
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
  }
}

Run :: (args: string, work_dir := "", silent := false) -> success:bool
{
  if LOG_RUN log("Run(`%`, `%`);", args, work_dir);
  return run_command(..break_command_into_strings(args), working_directory=work_dir, capture_and_return_output=silent).exit_code == 0;
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
#import "File_Utilities";
#import "String";
#import "Process";
