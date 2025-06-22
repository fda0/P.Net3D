main :: ()
{
  B.tex.format = .R8G8B8A8;

  PIE_InitBuilder();

  // Load .gltf models
  {
    rot_x := RotationAroundAxis(AxisV3(.X), 0.25);
    rot_y := RotationAroundAxis(AxisV3(.Y), 0.25);
    rot_z := RotationAroundAxis(AxisV3(.Z), 0.25);
    rot_xz := rot_z * rot_x;

    spec := BK_GLTF_Spec.{height = 1.7, rot = rot_xz};
    // @todo detect duplicated skeleton & animations and reuse it
    BK_GLTF_Load("../res/models/UltimateModularWomen/Worker.gltf", spec);
    BK_GLTF_Load("../res/models/UltimateModularWomen/Formal.gltf", spec);
    BK_GLTF_Load("../res/models/UltimateModularWomen/Casual.gltf", spec);

    BK_GLTF_Load("../res/models/UniversalAnimationLibrary[Standard]/Dude.glb", spec);

    BK_GLTF_Load("../res/models/Flag.glb", BK_GLTF_Spec.{height = 1.0, rot = rot_x});
    BK_GLTF_Load("../res/models/tree_low-poly/scene.gltf", BK_GLTF_Spec.{disable_z0 = true, height = 5.0, name = "Tree"});
  }

  // Load texture files
  {
    // @todo adjust after adding tone mapping
    spec_tiny := Color32_Grayf(0.05);
    spec_low := Color32_Grayf(0.1);
    spec_mid := Color32_Grayf(0.2);
    spec_big := Color32_Grayf(0.4);
    BK_TEX_LoadPBRs("Bricks071", spec_mid);
    BK_TEX_LoadPBRs("Bricks097", spec_mid);
    BK_TEX_LoadPBRs("Clay002", spec_big);
    BK_TEX_LoadPBRs("Grass004", spec_tiny);
    BK_TEX_LoadPBRs("Ground037", spec_tiny);
    BK_TEX_LoadPBRs("Ground068", spec_tiny);
    BK_TEX_LoadPBRs("Ground078", spec_tiny);
    BK_TEX_LoadPBRs("Leather011", spec_big);
    BK_TEX_LoadPBRs("PavingStones067", spec_low);
    BK_TEX_LoadPBRs("Tiles087", spec_big);
    BK_TEX_LoadPBRs("Tiles101", spec_big);
    BK_TEX_LoadPBRs("TestPBR001", spec_mid);
  }

  // Compile shaders
  {
    #import "dxc_compiler";

    base_dir := "../jsrc/shader/";
    source_flags := read_entire_file(join(base_dir, "shader_flags.h"));
    source_util := read_entire_file(join(base_dir, "shader_util.hlsl"));

    GetArgs :: (source_file: string, entry: string, shader_type: Shader_Type) -> Dxc_Shader_Compiler_Args #expand
    {
      source_path := join(base_dir, source_file);
      source := read_entire_file(source_path);

      full_source := join(source_flags, source_util, source);

      config := Dxc_Shader_Compiler_Args.{
        shader_name = entry,
        shader_source = full_source,
        shader_model_version = "6_0",
        shader_type = shader_type,
        entry_point = entry,
        // defines: [] string;
        // extra_args: [] string;
        // add_debug_info: bool;
        // warnings_as_error: bool;
        // skip_optimization: bool;
        // compile_as_spirv: bool;
        // use_spirv_1_4: bool;
      };
      return config;
    }

    world_vs := DXC_Compile(GetArgs("shader_world.hlsl", "WORLD_DxShaderVS", .Vertex));
    world_ps := DXC_Compile(GetArgs("shader_world.hlsl", "WORLD_DxShaderPS", .Fragment));
    ui_vs := DXC_Compile(GetArgs("shader_ui.hlsl", "UI_DxShaderVS", .Vertex));
    ui_ps := DXC_Compile(GetArgs("shader_ui.hlsl", "UI_DxShaderPS", .Fragment));
    // @todo Things like entrypoint name could be saved into .pie file too.
    PIE_ListSetArray(*array_add(*B.pie.shaders).data, world_vs);
    PIE_ListSetArray(*array_add(*B.pie.shaders).data, world_ps);
    PIE_ListSetArray(*array_add(*B.pie.shaders).data, ui_vs);
    PIE_ListSetArray(*array_add(*B.pie.shaders).data, ui_ps);
  }

  PIE_FinalizeBuilder();
  PIE_SaveToFile("data.pie");
}

BK_State :: struct
{
  pie: PIE_Builder;

  tex: struct
  {
    // bc7enc_compress_block_params params;
    // SDL_Surface *bc7_block_surf;
    format: PIE_TexFormat;
  };
};
B: BK_State;

BK_SetDefaultsPIEMaterial :: (pie_material: *PIE_Material)
{
  pie_material.params.diffuse = xx Color32_RGBi(128,128,128);
  pie_material.params.specular = xx Color32_RGBf(0.05, 0.02, 0.01);
  pie_material.params.roughness = 0.5;
}

Check :: (arg: bool, message := "", args: .. Any, loc := #caller_location) #no_debug #expand {
  if !arg && assert_helper(message, ..args, loc) then debug_break();
}

#load "pie_builder.jai";
#load "baker_gltf_models.jai";
#load "baker_textures.jai";
#import "Basic";
#import "String";
#import "Flat_Pool";
#import "File";
#import "sdl3_base+image";
#load "../game/base_utils.jai";
#load "../game/base_math.jai";
#load "../game/base_string.jai";
#load "../game/base_hash.jai";
#load "../game/base_array.jai";
#load "../game/pie_file_format.jai";
#load "../game/game_render_world_vertex.jai";
#load "../game/game_color.jai"; // @todo rename to base_color
