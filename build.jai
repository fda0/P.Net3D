build :: () {
  make_directory_if_it_does_not_exist("build");

  w := compiler_create_workspace("Game");
  options := get_build_options(w);

  args := options.compile_time_command_line;
  for args
  {
    if it == "kill"  print_and_run("raddbg", "--ipc", "kill_all");
    else if it_index > 1  print("Unknow argument used: %\n", it);
  }

  options.output_path = "build";
  options.output_executable_name = "Treasure";
  set_build_options(options, w);

  if false {
    format := *context.print_style.default_format_struct;
    format.use_newlines_if_long_form = true;
    format.indentation_width = 2;
    print("Workspace: %\nOptions: %\n", w, options);
  }

  add_build_file(tprint("%jsrc/game/game_sdl_entry.jai", #filepath), w);
  set_build_options_dc(.{do_output=false});
}
#run build();

print_and_run :: (args: ..string)
{
  print("Run before compilation: %\n", join(..args, " "));
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
