main :: ()
{
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

  print("Hello from Baker\n");
}

#load "baker_gltf_models.jai";
#import "Basic";
#load "../game/base_math.jai";
