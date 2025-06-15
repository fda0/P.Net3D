#run build();
build :: ()
{
  set_build_options_dc(.{do_output=false});
  make_directory_if_it_does_not_exist("build");

  // Parse command-line arguments
  after_dash := false;
  options := get_build_options();
  for options.compile_time_command_line
  {
    if it == "-" then after_dash = true;

    if it == "kill"    then LogAndRun("raddbg", "--ipc", "kill_all");
    else if after_dash then log_error("Unknow argument: %\n", it);
  }

  Build(.{workspace_name = "Baker",
          output_name = "Baker",
          entry_file = tprint("%jsrc/baker/baker_entry.jai", #filepath),
          icon_path = "res/ico/cook.ico"});

  Build(.{workspace_name = "Game",
          output_name = "Treasure",
          entry_file = tprint("%jsrc/game/game_sdl_entry.jai", #filepath),
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

#import "Compiler";
#import "Basic";
#import "File";
#import "String";
#import "Process";
