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
#include "cgr_export_geom.h"

#include "utils/CGR_string.h"

#include "vrscene_exporter/GeomStaticMesh.h"
#include "vrscene_exporter/anim.h"

#include "common/blender_includes.h"

#include <vector>


typedef AnimationCache<GeomStaticMesh> MeshesCache;


static int IsMeshAnimated(Object *ob)
{
    // TODO: Go through the adt and check if there is smth,
    // adt could be not NULL, but contain no actual data...
    // ob.animation_data_clear() will actually clear the adt pointer.

    switch(ob->type) {
        case OB_CURVE:
        case OB_SURF:
        case OB_FONT: {
            Curve *cu = (Curve*)ob->data;
            if(cu->adt)
                return 1;
        }
            break;
        case OB_MBALL: {
            MetaBall *mb = (MetaBall*)ob->data;
            if(mb->adt)
                return 1;
        }
            break;
        case OB_MESH: {
            Mesh *me = (Mesh*)ob->data;
            if(me->adt)
                return 1;
        }
            break;
        default:
            break;
    }

    ModifierData *mod = (ModifierData*)ob->modifiers.first;
    while(mod) {
        switch(mod->type) {
            case eModifierType_Armature:
            case eModifierType_Array:
            case eModifierType_Displace:
            case eModifierType_Softbody:
            case eModifierType_Explode:
            case eModifierType_MeshDeform:
            case eModifierType_SimpleDeform:
            case eModifierType_ShapeKey:
            case eModifierType_Screw:
            case eModifierType_Warp:
                return ob->adt != NULL;
            default:
                mod = mod->next;
        }
    }

    return 0;
}


static void WriteGeomStaticMesh(FILE *outputFile, Object *ob, const GeomStaticMesh *geomStaticMesh, const char *pluginName, int useAnimation=false, int frame=0)
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
        // Construct plugin name
        //
        BLI_strncpy(obName, ob->id.name+2, MAX_ID_NAME);
        StripString(obName);

        plugName.append("ME");
        plugName.append(obName);

        const Mesh *me = (Mesh*)ob->data;
        if(me->id.lib) {
            BLI_split_file_part(me->id.lib->name+2, libFilename, FILE_MAX);
            BLI_replace_extension(libFilename, FILE_MAX, "");

            StripString(libFilename);

            plugName.append("LI");
            plugName.append(libFilename);
        }
    }

    // Plugin name
    fprintf(outputFile, "\nGeomStaticMesh %s {", plugName.c_str());

    // Mesh components
    fprintf(outputFile, "\n\tvertices=%sListVectorHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getVertices());
    fprintf(outputFile, "\")%s;", interpEnd);

    fprintf(outputFile, "\n\tfaces=%sListIntHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getFaces());
    fprintf(outputFile, "\")%s;", interpEnd);

    fprintf(outputFile, "\n\tnormals=%sListVectorHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getNormals());
    fprintf(outputFile, "\")%s;", interpEnd);

    fprintf(outputFile, "\n\tfaceNormals=%sListIntHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getFaceNormals());
    fprintf(outputFile, "\")%s;", interpEnd);

    fprintf(outputFile, "\n\tface_mtlIDs=%sListIntHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getFace_mtlIDs());
    fprintf(outputFile, "\")%s;", interpEnd);

    fprintf(outputFile, "\n\tedge_visibility=%sListIntHex(\"", interpStart);
    fprintf(outputFile, geomStaticMesh->getEdge_visibility());
    fprintf(outputFile, "\")%s;", interpEnd);

    size_t mapChannelCount = geomStaticMesh->getMapChannelCount();
    if(mapChannelCount) {
        fprintf(outputFile, "\n\tmap_channels_names=List(");
        for(size_t i = 0; i < mapChannelCount; ++i) {
            const MChan *mapChannel = geomStaticMesh->getMapChannel(i);
            if(NOT(mapChannel))
                continue;

            fprintf(outputFile, "\"%s\"", mapChannel->name.c_str());
            if(i < mapChannelCount-1)
                fprintf(outputFile, ",");
        }
        fprintf(outputFile, ");");

        fprintf(outputFile, "\n\tmap_channels=%sList(", interpStart);
        for(size_t i = 0; i < mapChannelCount; ++i) {
            const MChan *mapChannel = geomStaticMesh->getMapChannel(i);
            if(NOT(mapChannel))
                continue;

            fprintf(outputFile, "List(%i,ListVectorHex(\"", mapChannel->index);
            fprintf(outputFile, mapChannel->uv_vertices);
            fprintf(outputFile, "\"),ListIntHex(\"");
            fprintf(outputFile, mapChannel->uv_faces);
            fprintf(outputFile, "\"))");

            if(i < mapChannelCount-1)
                fprintf(outputFile, ",");
        }
        fprintf(outputFile, ")%s;", interpEnd);
    }

    fprintf(outputFile, "\n}\n");
}


static void ExportMeshes(FILE *outputFile, MeshesCache *meshCache, Scene *sce, Main *main, int activeLayers, int altDInstances, int animation=false, int checkAnimated=ANIM_CHECK_NONE)
{
    Base *base = (Base*)sce->base.first;
    while(base) {
        Object *ob = base->object;
        base = base->next;

        if(activeLayers)
            if(NOT(ob->lay & sce->lay))
                continue;

        if(NOT(GEOM_TYPE(ob)))
            continue;

        if(NOT(animation)) {
            GeomStaticMesh geomStaticMesh;
            geomStaticMesh.init(sce, main, ob);
            if(geomStaticMesh.getHash())
                WriteGeomStaticMesh(outputFile, ob, &geomStaticMesh, NULL);
        } // not animated
        else {
            if(checkAnimated == ANIM_CHECK_NONE) {
                GeomStaticMesh geomStaticMesh;
                geomStaticMesh.init(sce, main, ob);
                if(geomStaticMesh.getHash())
                    WriteGeomStaticMesh(outputFile, ob, &geomStaticMesh, NULL, animation, sce->r.cfra);
            }
            else if(checkAnimated == ANIM_CHECK_HASH || checkAnimated == ANIM_CHECK_BOTH) {
                std::string obName(ob->id.name);

                if(checkAnimated == ANIM_CHECK_BOTH)
                    if(NOT(IsMeshAnimated(ob)))
                        continue;

                GeomStaticMesh *geomStaticMesh = new GeomStaticMesh();
                geomStaticMesh->init(sce, main, ob);

                MHash curHash  = geomStaticMesh->getHash();
                MHash prevHash = meshCache->getHash(obName);

                if(NOT(curHash == prevHash)) {
                    // Write previous frame if hash is more then 'frame_step' back
                    // If 'prevHash' is 0 than previous call was for the first frame
                    // and no need to export
                    if(prevHash) {
                        int cacheFrame = meshCache->getFrame(obName);
                        int prevFrame  = sce->r.cfra - sce->r.frame_step;

                        if(cacheFrame < prevFrame) {
                            WriteGeomStaticMesh(outputFile, ob, meshCache->getData(obName), NULL, animation, prevFrame);
                        }
                    }

                    // Write current frame data
                    WriteGeomStaticMesh(outputFile, ob, geomStaticMesh, NULL, animation, sce->r.cfra);

                    // This will free previous data and store new pointer
                    meshCache->update(obName, curHash, sce->r.cfra, geomStaticMesh);
                }
            } // ANIM_CHECK_HASH || ANIM_CHECK_BOTH
            else if(checkAnimated == ANIM_CHECK_SIMPLE) {
                if(IsMeshAnimated(ob)) {
                    GeomStaticMesh geomStaticMesh;
                    geomStaticMesh.init(sce, main, ob);
                    if(geomStaticMesh.getHash()) {
                        WriteGeomStaticMesh(outputFile, ob, &geomStaticMesh, NULL, animation, sce->r.cfra);
                    }
                }
            } // ANIM_CHECK_SIMPLE
        } // animated
    } // while(base)
}


void ExportGeometry(FILE *outputFile, Scene *sce, Main *main, int activeLayers, int altDInstances)
{
    PRINT_INFO("ExportGeometry()");

    ExportMeshes(outputFile, NULL, sce, main, activeLayers, altDInstances);
}


void ExportGeometryAnimation(const char *outputFilepath, Scene *sce, Main *main, int activeLayers, int altDInstances, int checkAnimated)
{
    PRINT_INFO("ExportGeometryAnimation()");

    FILE              *outputFile = NULL;
    MeshesCache        meshCache;
    EvaluationContext  eval_ctx = {0};

    double timeMeasure = 0.0;
    char   timeMeasureBuf[32];

    // Store selected frame
    int frameCurrent = sce->r.cfra;

    int frameStart   = sce->r.sfra;
    int frameEnd     = sce->r.efra;

    int fra = frameStart;

    PRINT_INFO_LB("Exporting meshes for the start frame %i...", fra);

    eval_ctx.for_render = true;

    timeMeasure = PIL_check_seconds_timer();

    // Setup frame
    sce->r.cfra = fra;
    CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

    // Update scene
    BKE_scene_update_for_newframe(&eval_ctx, main, sce, (1<<20) - 1);

    // Export stuff
    outputFile = fopen(outputFilepath, "w");
    ExportMeshes(outputFile, &meshCache, sce, main, activeLayers, altDInstances, true);
    fclose(outputFile);

    BLI_timestr(PIL_check_seconds_timer()-timeMeasure, timeMeasureBuf, sizeof(timeMeasureBuf));
    printf(" done [%s]\n", timeMeasureBuf);

    outputFile = fopen(outputFilepath, "a");

    // Setup next frame
    fra += sce->r.frame_step;

    // Export meshes for the rest of frames
    while(fra <= frameEnd) {
        PRINT_INFO_LB("Exporting meshes for frame %i...", fra);

        timeMeasure = PIL_check_seconds_timer();

        // Set frame
        sce->r.cfra = fra;
        CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);

        // Update scene
        BKE_scene_update_for_newframe(&eval_ctx, main, sce, (1<<20) - 1);

        // Export stuff
        ExportMeshes(outputFile, &meshCache, sce, main, activeLayers, altDInstances, true, checkAnimated);

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
