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

#include "exporter_geometry.h"
#include "exporter_nodes.h"
#include "exporter_socket.h"

#include "ED_exporter.h"

void ED_operatortypes_exporter(void)
{
    WM_operatortype_append(VRAY_OT_export_nodes);
    WM_operatortype_append(VRAY_OT_export_meshes);
    WM_operatortype_append(VRAY_OT_transfer);
}
