build :: () {
  make_directory_if_it_does_not_exist("build");

  w := compiler_create_workspace("Game");
  options := get_build_options(w);
  options.output_path = "build";
  options.output_executable_name = "Treasure";
  set_build_options(options, w);

  if false {
    format := *context.print_style.default_format_struct;
    format.use_newlines_if_long_form = true;
    format.indentation_width = 2;
    print("Workspace: %\nOptions: %\n", w, options);
  }

  add_build_file(tprint("%jsrc/game_sdl_entry.jai", #filepath), w);
  set_build_options_dc(.{do_output=false});
}
#run build();

#import "Basic";
#import "File";
#import "Compiler";
