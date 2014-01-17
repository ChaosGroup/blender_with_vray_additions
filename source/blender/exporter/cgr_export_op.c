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

#include "cgr_export_op.h"
#include "cgr_export_geom.h"
#include "cgr_export_nodes.h"

#include "common/blender_includes.h"


static EnumPropertyItem CheckAnimatedItems[] = {
    {ANIM_CHECK_NONE,   "NONE",   0, "None",   "Don't check meshes for animation"},
    {ANIM_CHECK_SIMPLE, "SIMPLE", 0, "Simple", "Simple check"},
    {ANIM_CHECK_HASH,   "HASH",   0, "Hash",   "Check mesh data hash"},
    {ANIM_CHECK_BOTH,   "BOTH",   0, "Both",   "Use both methods"},
    {0, NULL, 0, NULL, NULL},
};


static int export_op_nodes_exec(bContext *C, wmOperator *op)
{
    Main   *main    = CTX_data_main(C);

    Scene  *sce     = NULL;
    char   *sce_ptr = NULL;

    FILE   *outputFile = NULL;

    char   *filepath       = NULL;
    int     active_layers  = 0;
    int     animation      = 0;
    int     check_animated = 0;
    int     instances      = 0;
    int     debug          = 0;

    double  time;
    char    time_str[32];

    if(RNA_struct_property_is_set(op->ptr, "scene")) {
        sce_ptr = (char*)malloc(32 * sizeof(char));
        RNA_string_get(op->ptr, "scene", sce_ptr);
        sce = atol(sce_ptr);
        free(sce_ptr);
    }

    if(NOT(sce)) {
        sce = (Scene*)G.main->scene.first;
    }

    if(RNA_struct_property_is_set(op->ptr, "filepath")) {
        filepath = (char*)malloc(FILE_MAX * sizeof(char));
        RNA_string_get(op->ptr, "filepath", filepath);
    }

    if(RNA_struct_property_is_set(op->ptr, "use_active_layers")) {
        active_layers = RNA_boolean_get(op->ptr, "use_active_layers");
    }

    if(RNA_struct_property_is_set(op->ptr, "use_animation")) {
        animation = RNA_boolean_get(op->ptr, "use_animation");
    }

    if(RNA_struct_property_is_set(op->ptr, "use_instances")) {
        instances = RNA_boolean_get(op->ptr, "use_instances");
    }

    if(RNA_struct_property_is_set(op->ptr, "check_animated")) {
        check_animated = RNA_enum_get(op->ptr, "check_animated");
    }

    if(RNA_struct_property_is_set(op->ptr, "debug")) {
        debug = RNA_boolean_get(op->ptr, "debug");
    }

    if(filepath) {
        PRINT_INFO("Exporting nodes...");
        time = PIL_check_seconds_timer();

        if(animation) {
            ExportNodesAnimation(filepath, sce, main, active_layers, instances, check_animated);
        }
        else {
            outputFile = fopen(filepath, "w");
            ExportNodes(outputFile, sce, main, active_layers, instances);
            fclose(outputFile);
        }

        BLI_timestr(PIL_check_seconds_timer()-time, time_str, sizeof(time_str));
        PRINT_INFO("Exporting nodes done [%s]%-32s", time_str, " ");

        free(filepath);

        return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
}


static int export_op_meshes_exec(bContext *C, wmOperator *op)
{
    Main   *main    = CTX_data_main(C);

    Scene  *sce     = NULL;
    char   *sce_ptr = NULL;

    FILE   *outputFile = NULL;

    char   *filepath       = NULL;
    int     active_layers  = 0;
    int     animation      = 0;
    int     check_animated = 0;
    int     instances      = 0;
    int     debug          = 0;

    double  time;
    char    time_str[32];

    if(RNA_struct_property_is_set(op->ptr, "scene")) {
        sce_ptr = (char*)malloc(32 * sizeof(char));
        RNA_string_get(op->ptr, "scene", sce_ptr);
        sce = atol(sce_ptr);
        free(sce_ptr);
    }

    if(NOT(sce)) {
        sce = (Scene*)G.main->scene.first;
    }

    if(RNA_struct_property_is_set(op->ptr, "filepath")) {
        filepath = (char*)malloc(FILE_MAX * sizeof(char));
        RNA_string_get(op->ptr, "filepath", filepath);
    }

    if(RNA_struct_property_is_set(op->ptr, "use_active_layers")) {
        active_layers = RNA_boolean_get(op->ptr, "use_active_layers");
    }

    if(RNA_struct_property_is_set(op->ptr, "use_animation")) {
        animation = RNA_boolean_get(op->ptr, "use_animation");
    }

    if(RNA_struct_property_is_set(op->ptr, "use_instances")) {
        instances = RNA_boolean_get(op->ptr, "use_instances");
    }

    if(RNA_struct_property_is_set(op->ptr, "check_animated")) {
        check_animated = RNA_enum_get(op->ptr, "check_animated");
    }

    if(RNA_struct_property_is_set(op->ptr, "debug")) {
        debug = RNA_boolean_get(op->ptr, "debug");
    }

    if(filepath) {
        PRINT_INFO("Exporting meshes...");
        time = PIL_check_seconds_timer();

        if(animation) {
            ExportGeometryAnimation(filepath, sce, main, active_layers, instances, check_animated);
        }
        else {
            outputFile = fopen(filepath, "w");
            ExportGeometry(outputFile, sce, main, active_layers, instances);
            fclose(outputFile);
        }

        BLI_timestr(PIL_check_seconds_timer()-time, time_str, sizeof(time_str));
        PRINT_INFO("Exporting meshes done [%s]%-32s", time_str, " ");

        free(filepath);

        return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
}


static int export_op_dummy_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
     return OPERATOR_RUNNING_MODAL;
}


static int export_op_dummy_modal(bContext *C, wmOperator *op, wmEvent *event)
{
    return OPERATOR_RUNNING_MODAL;
}


void VRAY_OT_export_meshes(wmOperatorType *ot)
{
    /* identifiers */
    ot->name        = "Export Meshes";
    ot->idname      = "VRAY_OT_export_meshes";
    ot->description = "Export meshes in .vrscene format";

    /* api callbacks */
    ot->invoke = export_op_dummy_invoke;
    ot->modal  = export_op_dummy_modal;
    ot->exec   = export_op_meshes_exec;

    RNA_def_string(ot->srna,  "scene", "", 32, "Scene", "Scene pointer");
    RNA_def_string(ot->srna,  "filepath", "", FILE_MAX, "Geometry filepath", "Geometry filepath");

    RNA_def_boolean(ot->srna, "use_active_layers", 0,  "Active layer",      "Export only active layers");
    RNA_def_boolean(ot->srna, "use_animation",     0,  "Animation",         "Export animation");
    RNA_def_boolean(ot->srna, "use_instances",     0,  "Instances",         "Use instances");
    RNA_def_boolean(ot->srna, "debug",             0,  "Debug",             "Debug mode");

    RNA_def_enum(ot->srna, "check_animated", CheckAnimatedItems, 0, "Check Animated", "Detect if mesh is animated");
}


void VRAY_OT_export_nodes(wmOperatorType *ot)
{
    /* identifiers */
    ot->name        = "Export Nodes";
    ot->idname      = "VRAY_OT_export_nodes";
    ot->description = "Export nodes in .vrscene format";

    /* api callbacks */
    ot->invoke = export_op_dummy_invoke;
    ot->modal  = export_op_dummy_modal;
    ot->exec   = export_op_nodes_exec;

    RNA_def_string(ot->srna,  "scene", "", 32, "Scene", "Scene pointer");
    RNA_def_string(ot->srna,  "filepath", "", FILE_MAX, "Geometry filepath", "Geometry filepath");

    RNA_def_boolean(ot->srna, "use_active_layers", 0,  "Active layer",      "Export only active layers");
    RNA_def_boolean(ot->srna, "use_animation",     0,  "Animation",         "Export animation");
    RNA_def_boolean(ot->srna, "use_instances",     0,  "Instances",         "Use instances");
    RNA_def_boolean(ot->srna, "debug",             0,  "Debug",             "Debug mode");

    RNA_def_enum(ot->srna, "check_animated", CheckAnimatedItems, 0, "Check Animated", "Detect if mesh is animated");
}
