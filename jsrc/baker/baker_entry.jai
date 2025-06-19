main :: ()
{
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

  PIE_FinalizeBuilder();
  PIE_SaveToFile("jai_data.pie");
}

BK_State :: struct
{
  pie: PIE_Builder;

  // tex: BAKER_TexState;
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
#load "../game/base_utils.jai";
#load "../game/base_math.jai";
#load "../game/base_string.jai";
#load "../game/base_hash.jai";
#load "../game/base_array.jai";
#load "../game/pie_file_format.jai";
#load "../game/game_render_world_vertex.jai";
#load "../game/game_color.jai"; // @todo rename to base_color
