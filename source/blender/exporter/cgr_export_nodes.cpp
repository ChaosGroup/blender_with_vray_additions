/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "CGR_config.h"
#include "cgr_export_nodes.h"

#include "utils/CGR_vrscene.h"
#include "utils/CGR_string.h"
#include "utils/CGR_blender_data.h"
#include "utils/cgr_json_plugins.h"

#include "vrscene_exporter/anim.h"
#include "vrscene_exporter/Node.h"

#include "common/blender_includes.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>

#include <string>
#include <vector>


typedef std::vector<std::string>       StringVector;
typedef AnimationCache<VRScene::Node>  NodesCache;


static int IsAnimated(Object *ob)
{
    // We are checking only 'transform' and visibilty here
    //
    if(ob->adt) {
        // ...
        return 1;
    }

    return 0;
}


static std::string WriteMtlMulti(FILE *outputFile, Object *ob)
{
    if(NOT(ob->totcol))
        return "MtlNoMaterial";

    StringVector mtls_list;
    StringVector ids_list;

    for(int a = 1; a <= ob->totcol; ++a) {
        Material *ma = give_current_material(ob, a);
        if(NOT(ma))
            continue;

        mtls_list.push_back(ma->id.name);
        ids_list.push_back(boost::lexical_cast<std::string>(a-1));
    }

    // No need for multi-material if only one slot
    // is used
    //
    if(mtls_list.size() == 1)
        return mtls_list[0];

    std::string plugName("MM");
    plugName.append(ob->id.name+2);

    fprintf(outputFile, "\nMtlMulti %s {", plugName.c_str());
    fprintf(outputFile, IND"mtls_list=List(%s);", boost::algorithm::join(mtls_list, ",").c_str());
    fprintf(outputFile, IND"ids_list=List(%s);", boost::algorithm::join(ids_list, ",").c_str());
    fprintf(outputFile, "\n}\n");

    return plugName;
}


static void WriteNode(FILE *outputFile, Object *ob, const VRScene::Node *node, const char *pluginName, int useAnimation=false, int frame=0)
{
    std::string plugName;

    char obName[MAX_ID_NAME] = "";
    char libFilename[FILENAME_MAX] = "";

    char interpStart[32] = "";
    char interpEnd[3]    = "";

    if(useAnimation) {
        sprintf(interpStart, "interpolate((%d,", frame);
        sprintf(interpEnd,   "))");
    }

    if(pluginName) {
        plugName = pluginName;
    }
    else {
        // Construct Node name
        //
        BLI_strncpy(obName, ob->id.name+2, MAX_ID_NAME);
        StripString(obName);

        plugName.append("OB");
        plugName.append(obName);

        if(ob->id.lib) {
            BLI_split_file_part(ob->id.lib->name+2, libFilename, FILE_MAX);
            BLI_replace_extension(libFilename, FILE_MAX, "");

            StripString(libFilename);

            plugName.append("LI");
            plugName.append(libFilename);
        }
    }

    // Move to Node.{h,cpp}
    //
    std::string materialName = WriteMtlMulti(outputFile, ob);

    fprintf(outputFile, "\nNode %s {", plugName.c_str());
    fprintf(outputFile, IND"objectID=%i;", node->getObjectID());
    fprintf(outputFile, IND"geometry=%s;", plugName.c_str());
    fprintf(outputFile, IND"material=%s;", materialName.c_str());
    fprintf(outputFile, IND"transform=%sTransformHex(\"%s\")%s;", interpStart, node->getTransform(), interpEnd);
    fprintf(outputFile, "\n}\n");
}


static void ProcessObjects(FILE *outputFile, NodesCache *nodesCache, Scene *sce, Main *main, int activeLayers, int altDInstances, int animation=false, int checkAnimated=ANIM_CHECK_NONE)
{
    EvaluationContext eval_ctx = {0};
    eval_ctx.for_render = true;

    Base *base = (Base*)sce->base.first;
    while(base) {
        Object *ob = base->object;
        base = base->next;

        // PRINT_INFO("Processgin '%s'...", ob->id.name);

        // Skip object here, but not in dupli!
        // Dupli could be particles and it's better to
        // have animated 'visible' param there
        //
        if(ob->restrictflag & OB_RESTRICT_RENDER)
            continue;

        if(activeLayers)
            if(NOT(ob->lay & sce->lay))
                continue;

        if(GEOM_TYPE(ob) || EMPTY_TYPE(ob)) {
            // Free duplilist if there is some for some reason
            FreeDupliList(ob);

            ob->duplilist = object_duplilist(&eval_ctx, sce, ob);

            for(DupliObject *dob = (DupliObject*)ob->duplilist->first; dob; dob = dob->next) {
                VRScene::Node *node = new VRScene::Node();
                node->init(sce, main, ob, dob);

                WriteNode(outputFile, ob, node, NULL, animation, sce->r.cfra);
            }

            FreeDupliList(ob);

            // TODO: Check particle systems for 'Render Emitter' prop

            if(NOT(EMPTY_TYPE(ob))) {
                VRScene::Node *node = new VRScene::Node();
                node->init(sce, main, ob, NULL);

                WriteNode(outputFile, ob, node, NULL, animation, sce->r.cfra);
            }
        }
        else if(ob->type == OB_LAMP) {

        }
#if 0
        MHash curHash = node->getHash();
        if(curHash) {
            if(NOT(animation)) {
                // ...
            }
            else {
                if(checkAnimated == ANIM_CHECK_NONE) {
                    // ...
                }
                else if(checkAnimated == ANIM_CHECK_HASH) {
                    std::string obName(ob->id.name);

                    MHash prevHash = nodesCache->getHash(obName);

                    if(NOT(curHash == prevHash)) {
                        // Write previous frame if hash is more then 'frame_step' back
                        // If 'prevHash' is 0 than previous call was for the first frame
                        // and no need to export
                        if(prevHash) {
                            int cacheFrame = nodesCache->getFrame(obName);
                            int prevFrame  = sce->r.cfra - sce->r.frame_step;

                            if(cacheFrame < prevFrame) {
                                // ...
                            }
                        }

                        // Write current frame data
                        // ...

                        // This will free previous data and store new pointer
                        nodesCache->update(obName, curHash, sce->r.cfra, node);
                    }
                }
                else if(checkAnimated == ANIM_CHECK_SIMPLE) {
                    if(IsAnimated(ob)) {
                        // ...
                    }
                }
            }
        }
#endif
    } // while(base)
}


void ExportNodes(FILE *outputFile, Scene *sce, Main *main, int activeLayers, int altDInstances)
{
    PRINT_INFO("ExportNodes()");

    // ReadPluginDesc("MtlRenderStats");

    ProcessObjects(outputFile, NULL, sce, main, activeLayers, altDInstances);
}


void ExportNodesAnimation(const char *outputFilepath, Scene *sce, Main *main, int activeLayers, int altDInstances, int checkAnimated)
{
    PRINT_INFO("ExportNodesAnimation()");

    FILE              *outputFile = NULL;
    NodesCache         nodesCache;
    EvaluationContext  eval_ctx = {0};

    double timeMeasure = 0.0;
    char   timeMeasureBuf[32];

    // Store selected frame
    int frameCurrent = sce->r.cfra;

    int frameStart   = sce->r.sfra;
    int frameEnd     = sce->r.efra;

    int fra = frameStart;

    PRINT_INFO_LB("Exporting nodes for the start frame %i...", fra);

    eval_ctx.for_render = true;

    timeMeasure = PIL_check_seconds_timer();

    // Setup frame
    sce->r.cfra = fra;
    CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

    // Update scene
    BKE_scene_update_for_newframe(&eval_ctx, main, sce, (1<<20) - 1);

    // Export stuff
    outputFile = fopen(outputFilepath, "w");
    ProcessObjects(outputFile, &nodesCache, sce, main, activeLayers, altDInstances, true);
    fclose(outputFile);

    BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
    printf(" done [%s]\n", timeMeasureBuf);

    outputFile = fopen(outputFilepath, "a");

    // Setup next frame
    fra += sce->r.frame_step;

    // Export meshes for the rest of frames
    while(fra <= frameEnd) {
        PRINT_INFO_LB("Exporting nodes for frame %i...", fra);

        timeMeasure = PIL_check_seconds_timer();

        // Set frame
        sce->r.cfra = fra;
        CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

        // Update scene
        BKE_scene_update_for_newframe(&eval_ctx, main, sce, (1<<20) - 1);

        // Export stuff
        ProcessObjects(outputFile, &nodesCache, sce, main, activeLayers, altDInstances, true, checkAnimated);

        BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
        printf(" done [%s]\n", timeMeasureBuf);

        // Setup next frame
        fra += sce->r.frame_step;
    }

    fclose(outputFile);

    // Restore selected frame
    sce->r.cfra = frameCurrent;
    CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);
    BKE_scene_update_for_newframe(&eval_ctx, main, sce, (1<<20) - 1);
}
