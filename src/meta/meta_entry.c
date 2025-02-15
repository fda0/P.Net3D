#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_math.h"
#include "base_printer.h"
#include "game_render.h"

#include "meta_obj.h"
#include "meta_entry.h"
#include "meta_print_parse.c"
#include "meta_obj.c"

#define CGLTF_IMPLEMENTATION
#pragma warning(push, 2)
#include "cgltf.h"
#pragma warning(pop)

static U8 arena_memory_buffer[Megabyte(64)];

int main()
{
    // init
    M.log_filter = ~(U32)(M_LogObjDebug);
    //M.log_filter = ~(U32)(0);
    M.tmp = Arena_MakeInside(arena_memory_buffer, sizeof(arena_memory_buffer));

    // work
    {
        ArenaScope tmp_scope = Arena_PushScope(M.tmp);

        Printer pr_out = Pr_Alloc(M.tmp, Megabyte(1));
        M_ParseObj("../res/models/teapot.obj", &pr_out, (M_ModelSpec){.scale = 10.f, .rot_x = 0.25f});
        M_ParseObj("../res/models/flag.obj", &pr_out, (M_ModelSpec){.scale = 0.1f, .rot_x = 0.25f, .rot_z = 0.25f});
        M_SaveFile("../src/gen/gen_models.h", Pr_S8(&pr_out));

        Arena_PopScope(tmp_scope);
    }

    {
        cgltf_options options = {0};
        cgltf_data *data = 0;
        cgltf_result result = cgltf_parse_file(&options, "../res/models/Worker.gltf", &data);
        if (result == cgltf_result_success)
        {
            bool test = true;

            ForU64(node_index, data->nodes_count)
            {
                cgltf_node *node = data->nodes + node_index;
                cgltf_mesh* mesh = node->mesh;
                if (mesh)
                {
                    ForU64(primitive_index, mesh->primitives_count)
                    {
                        cgltf_primitive *primitive = mesh->primitives + primitive_index;
                        ForU64(attribute_index, primitive->attributes_count)
                        {
                            cgltf_attribute *attribute = primitive->attributes + attribute_index;
                            (void)attribute;
                            int a = 1;
                            a += 1;
                            if (test) break;
                        }
                        if (test) break;
                    }
                    if (test) break;
                }
                if (test) break;
            }

#if 0
            ForU64(mesh_index, data->meshes_count)
            {
                cgltf_mesh *mesh = data->meshes + mesh_index;
                ForU64(primitive_index, mesh->primitives_count)
                {
                    cgltf_primitive *primitive = mesh->primitives + primitive_index;
                    if (primitive->type != cgltf_primitive_type_triangles)
                    {
                        M_LOG(M_LogGltf, "[gltf] Found primitive type that isn't triangle! (type: %u)");
                        continue;
                    }

                    int a = 1;
                    a += 1;
                }
            }
#endif

            cgltf_free(data);
        }
    }


    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
