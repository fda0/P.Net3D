BK_GLTF_Spec :: struct
{
  height: float; // automatically adjust scale so model is equal height (meters) on Z axis
  disable_z0: bool; // automatically move model to start at z = 0

  scale: float;
  rot: Quat;
  move: V3;

  name: string; // If empty name will be taken from file name.
};

BK_GLTF_Load :: (path: string, spec: BK_GLTF_Spec)
{
}
