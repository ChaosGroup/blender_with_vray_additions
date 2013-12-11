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

/* V-Ray socket communication */

#include "exporter_socket.h"


static int transfer_exec(bContext *C, wmOperator *op)
{
    return OPERATOR_FINISHED;
}


static int transfer_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
    return OPERATOR_RUNNING_MODAL;
}


static int transfer_modal(bContext *C, wmOperator *op, wmEvent *event)
{
    return OPERATOR_RUNNING_MODAL;
}


void VRAY_OT_transfer(wmOperatorType *ot)
{
    /* identifiers */
    ot->name        = "Transfer";
    ot->idname      = "VRAY_OT_transfer";
    ot->description = "Transfer scene changes using communication socket";

    /* api callbacks */
    ot->invoke = transfer_invoke;
    ot->modal  = transfer_modal;
    ot->exec   = transfer_exec;

    RNA_def_int(ot->srna,     "scene",   0,  INT_MIN, INT_MAX, "Scene",   "Scene pointer", INT_MIN, INT_MAX);
    RNA_def_int(ot->srna,     "port",    0,  0,       INT_MAX, "Port",    "Socket port",   0,       INT_MAX);
    RNA_def_string(ot->srna,  "address", "", FILE_MAX,         "Address", "Socket address");
}
