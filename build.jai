#run build();
build :: ()
{
  make_directory_if_it_does_not_exist("build");
  set_build_options_dc(.{do_output=false});

  options := get_build_options();
  // Parse command-line arguments
  after_dash := false;
  for options.compile_time_command_line
  {
    if it == "-" then after_dash = true;

    if it == "kill"    then print_and_run("raddbg", "--ipc", "kill_all");
    else if after_dash then print("Unknow argument: %\n", it);
  }

  build_game();
}

build_game :: ()
{
  w := compiler_create_workspace("Game");
  options := get_build_options(w);
  options.output_path = "build";
  options.output_executable_name = "Treasure";
  set_build_options(options, w);
  add_build_file(tprint("%jsrc/game/game_sdl_entry.jai", #filepath), w);
}

print_and_run :: (args: ..string)
{
  print("Custom run from build.jai: %\n", join(..args, " "));
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
