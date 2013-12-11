/*

  V-Ray/Blender

  http://vray.cgdo.ru

  Author: Andrey M. Izrantsev (aka bdancer)
  E-Mail: izrantsev@cgdo.ru

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  All Rights Reserved. V-Ray(R) is a registered trademark of Chaos Software.

*/

/* Export objects in vrscene format    */
/* Used currently for particles export */

#include "exporter_nodes.h"


typedef struct ExportThreadData {
    Scene  *sce;
    Object *ob;
    FILE   *nfile;
    char    buf[MAX_IDPROP_NAME];
} ExportThreadData;


typedef struct NodeData {
    Object *ob;
    char    name[MAX_IDPROP_NAME];
    float   obmat[4][4];
} NodeData;


static void clear_string(char *buf, char *str)
{
    int i;

    strncpy(buf, str, MAX_IDPROP_NAME);

    for(i = 0; i < strlen(str); i++) {
        if(buf[i]) {
            if(buf[i] == '|' || buf[i] == '@')
                continue;
            else if(buf[i] == '+')
                buf[i] = 'p';
            else if(buf[i] == '-')
                buf[i] = 'm';
            else if(!((buf[i] >= 'A' && buf[i] <= 'Z') || (buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= '0' && buf[i] <= '9')))
                buf[i] = '_';
        }
    }
}


static int write_node(const ExportThreadData *edata, const NodeData *node)
{
    Object *base_ob = edata->ob;

    // Just for test
    PRINT_TRANSFORM(node->obmat);

    return 0;
}


static int write_dupli_node(const ExportThreadData *edata, const DupliObject *dob)
{
    Object   *ob = edata->ob;
    NodeData  node;

    // DEBUG_OUTPUT(TRUE, "Writing dupli %i for node %s", dob->index, ob->id.name+2);

    node.ob = dob->ob;
    copy_m4_m4(node.obmat, dob->omat);

    write_node(edata, &node);

    return 0;
}


static int write_dupli(ExportThreadData *edata)
{
    Object      *ob  = edata->ob;
    DupliObject *dob = NULL;

    if(!(ob->transflag & OB_DUPLI)) {
        return 1;
    }

    /* free duplilist if a user forgets to */
    if(ob->duplilist) {
        free_object_duplilist(ob->duplilist);
        ob->duplilist = NULL;
    }

    ob->duplilist = object_duplilist(edata->sce, ob, TRUE);

    // Process dupli objects
    for(dob = ob->duplilist->first; dob; dob = dob->next) {
        write_dupli_node(edata, dob);
    }

    free_object_duplilist(ob->duplilist);
    ob->duplilist = NULL;

    return 0;
}


static int export_nodes_exec(bContext *C, wmOperator *op)
{
    ExportThreadData edata;

    Scene  *sce  = NULL;
    Base   *base = NULL;
    Object *ob   = NULL;

    FILE   *nfile = NULL;
    char   *nfilepath;

    double  time = 0.0;
    char    time_str[32];

    if(RNA_struct_property_is_set(op->ptr, "scene")) {
        sce = RNA_int_get(op->ptr, "scene");
    }

    if(!(sce))
        return OPERATOR_CANCELLED;

    if(RNA_struct_property_is_set(op->ptr, "filepath")) {
        nfilepath = (char*)malloc(FILE_MAX * sizeof(char));
        RNA_string_get(op->ptr, "filepath", nfilepath);
        nfile = fopen(nfilepath, "w");
    }

    if(!(nfile))
        return OPERATOR_CANCELLED;

    // Start measuring time
    time = PIL_check_seconds_timer();
    DEBUG_OUTPUT(TRUE, "Exporting nodes...");

    // Setup some common function data
    edata.nfile = nfile;
    edata.sce   = sce;

    // Process objects
    base = (Base*)sce->base.first;
    while(base) {
        ob = base->object;

        DEBUG_OUTPUT(TRUE, "Processing object: %s", ob->id.name+2);

        edata.ob = ob;

        write_dupli(&edata);

        base = base->next;
    }

    // Measure time
    BLI_timestr(PIL_check_seconds_timer()-time, time_str, sizeof(time_str));
    DEBUG_OUTPUT(TRUE, "Exporting nodes done [%s]%-32s", time_str, " ");

    // Clean up
    fclose(nfile);
    free(nfilepath);

    return OPERATOR_FINISHED;
}


static int export_nodes_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
     return OPERATOR_RUNNING_MODAL;
}


static int export_nodes_modal(bContext *C, wmOperator *op, wmEvent *event)
{
    return OPERATOR_RUNNING_MODAL;
}


void VRAY_OT_export_nodes(wmOperatorType *ot)
{
    /* identifiers */
    ot->name        = "Export Nodes";
    ot->idname      = "VRAY_OT_export_nodes";
    ot->description = "Export objects in .vrscene format";

    /* api callbacks */
    ot->invoke = export_nodes_invoke;
    ot->modal  = export_nodes_modal;
    ot->exec   = export_nodes_exec;

    RNA_def_int(ot->srna,     "scene",     0, INT_MIN, INT_MAX, "Scene",         "Scene pointer", INT_MIN, INT_MAX);
    RNA_def_string(ot->srna,  "filepath", "", FILE_MAX,         "Node filepath", "Node filepath");
    RNA_def_boolean(ot->srna, "debug",     0,                   "Debug",         "Debug mode");
}
