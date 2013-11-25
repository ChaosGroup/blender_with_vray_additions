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

#include "utils/CGR_data.h"

#include "vrscene.h"


static void WritePythonAttribute(PyObject *outputFile, PyObject *propGroup, const char *attrName)
{
	static char buf[MAX_PLUGIN_NAME];

	PyObject *attr      = NULL;
	PyObject *attrValue = NULL;

	if(propGroup == Py_None)
		return;

	attr      = PyObject_GetAttrString(propGroup, attrName);
	attrValue = PyNumber_Long(attr);

	if(attrValue) {
		WRITE_PYOBJECT(outputFile, "\n\t%s=%li;", attrName, PyLong_AsLong(attrValue));
	}
}


static int write_edge_visibility(PyObject *outputFile, int k, unsigned long int *ev)
{
	static char buf[MAX_PLUGIN_NAME];

	if(k == 9) {
		WRITE_PYOBJECT_HEX_VALUE(outputFile, *ev);
		*ev= 0;
		return 0;
	}

	return k + 1;
}


void write_GeomStaticMesh(PyObject *outputFile, Scene *sce, Object *ob, Mesh *mesh, const char *pluginName, PyObject *propGroup)
{
	static char buf[MAX_PLUGIN_NAME];

	Mesh   *data = ob->data;
	MFace  *face;
	MTFace *mtface;
	MCol   *mcol;
	MVert  *vert;
	MEdge  *edge;

	CustomData *fdata;

	int    verts;
	int    fve[4];
	float *ve[4];
	float  no[3];
	float  col[3];

	float  fno[3];
	float  n0[3], n1[3], n2[3], n3[3];

	int    matid       = 0;
	int    uv_count    = 0;
	int    uv_layer_id = 1;

	PointerRNA   rna_me;
	PointerRNA   VRayMesh;
	PointerRNA   GeomStaticMesh;

	int          dynamic_geometry= 0;

	const int ft[6]= {0,1,2,2,3,0};

	unsigned long int ev= 0;

	int i, j, f, k, l;
	int u;

	if(!(mesh->totface)) {
		PRINT_ERROR( "No faces in mesh \"%s\"", data->id.name);
		return;
	}

	WRITE_PYOBJECT(outputFile, "\nGeomStaticMesh %s {", pluginName);

	WRITE_PYOBJECT(outputFile, "\n\tvertices=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
	vert = mesh->mvert;
	for(f = 0; f < mesh->totvert; ++vert, ++f) {
		WRITE_PYOBJECT_HEX_VECTOR(outputFile, vert->co);
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	// TODO: velocities (?)

	WRITE_PYOBJECT(outputFile, "\n\tfaces=interpolate((%d,ListIntHex(\"", sce->r.cfra);
	face = mesh->mface;
	for(f = 0; f < mesh->totface; ++face, ++f) {
		if(face->v4) {
			WRITE_PYOBJECT_HEX_QUADFACE(outputFile, face);
		}
		else {
			WRITE_PYOBJECT_HEX_TRIFACE(outputFile, face);
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\tnormals=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
	face = mesh->mface;
	for(f = 0; f < mesh->totface; ++face, ++f) {
		if(face->flag & ME_SMOOTH) {
			normal_short_to_float_v3(n0, mesh->mvert[face->v1].no);
			normal_short_to_float_v3(n1, mesh->mvert[face->v2].no);
			normal_short_to_float_v3(n2, mesh->mvert[face->v3].no);

			if(face->v4)
				normal_short_to_float_v3(n3, mesh->mvert[face->v4].no);
		}
		else {
			if(face->v4)
				normal_quad_v3(fno, mesh->mvert[face->v1].co, mesh->mvert[face->v2].co, mesh->mvert[face->v3].co, mesh->mvert[face->v4].co);
			else
				normal_tri_v3(fno,  mesh->mvert[face->v1].co, mesh->mvert[face->v2].co, mesh->mvert[face->v3].co);

			copy_v3_v3(n0, fno);
			copy_v3_v3(n1, fno);
			copy_v3_v3(n2, fno);

			if(face->v4)
				copy_v3_v3(n3, fno);
		}

		if(face->v4) {
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n0);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n1);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n2);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n2);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n3);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n0);
		} else {
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n0);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n1);
			WRITE_PYOBJECT_HEX_VECTOR(outputFile, n2);
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");


	WRITE_PYOBJECT(outputFile, "\n\tfaceNormals=interpolate((%d,ListIntHex(\"", sce->r.cfra);
	face= mesh->mface;
	k= 0;
	for(f= 0; f < mesh->totface; ++face, ++f) {
		if(mesh->mface[f].v4)
			verts= 6;
		else
			verts= 3;

		for(i= 0; i < verts; i++) {
			WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
			k++;
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");


	WRITE_PYOBJECT(outputFile, "\n\tface_mtlIDs=ListIntHex(\"");
	face= mesh->mface;
	for(f= 0; f < mesh->totface; ++face, ++f) {
		matid= face->mat_nr + 1;
		if(face->v4) {
			WRITE_PYOBJECT(outputFile, "%08X%08X", HEX(matid), HEX(matid));
		}
		else {
			WRITE_PYOBJECT(outputFile, "%08X", HEX(matid));
		}
	}
	WRITE_PYOBJECT(outputFile, "\");");

//	WRITE_PYOBJECT(outputFile, "\n\tedge_creases_vertices=ListIntHex(\"");
//	WRITE_PYOBJECT(outputFile, "\");");

//	WRITE_PYOBJECT(outputFile, "\n\tedge_creases_sharpness=ListFloatHex(\"");
//	WRITE_PYOBJECT(outputFile, "\");");

	WRITE_PYOBJECT(outputFile, "\n\tedge_visibility=ListIntHex(\"");
	ev= 0;
	if(mesh->totface <= 5) {
		face= mesh->mface;
		for(f= 0; f < mesh->totface; ++face, ++f) {
			if(face->v4) {
				ev= (ev << 6) | 27;
			} else {
				ev= (ev << 3) | 8;
			}
		}
		WRITE_PYOBJECT(outputFile, "%08X", HEX(ev));
	} else {
		k= 0;
		face= mesh->mface;
		for(f= 0; f < mesh->totface; ++face, ++f) {
			if(face->v4) {
				ev= (ev << 3) | 3;
				k= write_edge_visibility(outputFile, k, &ev);
				ev= (ev << 3) | 3;
				k= write_edge_visibility(outputFile, k, &ev);
			} else {
				ev= (ev << 3) | 8;
				k= write_edge_visibility(outputFile, k, &ev);
			}
		}

		if(k) {
			WRITE_PYOBJECT(outputFile, "%08X", HEX(ev));
		}
	}
	WRITE_PYOBJECT(outputFile, "\");");


	fdata = &mesh->fdata;

	uv_count  = CustomData_number_of_layers(fdata, CD_MTFACE);
	uv_count += CustomData_number_of_layers(fdata, CD_MCOL);

	if(uv_count) {
		WRITE_PYOBJECT(outputFile, "\n\tmap_channels_names=List(");
		for(l = 0; l < fdata->totlayer; ++l) {
			if(fdata->layers[l].type == CD_MTFACE || fdata->layers[l].type == CD_MCOL) {
				WRITE_PYOBJECT(outputFile, "\"%s\"", fdata->layers[l].name);

				if(l < uv_count) {
					WRITE_PYOBJECT(outputFile, ",");
				}
			}
		}
		WRITE_PYOBJECT(outputFile, ");");

		WRITE_PYOBJECT(outputFile, "\n\tmap_channels=interpolate((%d, List(", sce->r.cfra);
		uv_layer_id = 0;
		for(l = 0; l < fdata->totlayer; ++l) {
			if(fdata->layers[l].type == CD_MTFACE || fdata->layers[l].type == CD_MCOL) {
				WRITE_PYOBJECT(outputFile, "\n\t\t// Name: %s", fdata->layers[l].name);
				WRITE_PYOBJECT(outputFile, "\n\t\tList(%i,ListVectorHex(\"", uv_layer_id++);

				if(fdata->layers[l].type == CD_MTFACE) {
					face   = mesh->mface;
					mtface = (MTFace*)fdata->layers[l].data;
					for(f = 0; f < mesh->totface; ++face, ++f) {
						if(face->v4)
							verts = 4;
						else
							verts = 3;
						for(i = 0; i < verts; i++) {
							WRITE_PYOBJECT(outputFile, "%08X%08X00000000", HEX(mtface[f].uv[i][0]), HEX(mtface[f].uv[i][1]));
						}
					}
				}
				else {
					face = mesh->mface;
					mcol = (MCol*)fdata->layers[l].data;
					for(f = 0; f < mesh->totface; ++face, ++f) {
						if(face->v4)
							verts = 4;
						else
							verts = 3;
						for(i = 0; i < verts; i++) {
							col[0] = (float)mcol[f * 4 + i].b / 255.0;
							col[1] = (float)mcol[f * 4 + i].g / 255.0;
							col[2] = (float)mcol[f * 4 + i].r / 255.0;

							WRITE_PYOBJECT_HEX_VECTOR(outputFile, col);
						}
					}
				}

				WRITE_PYOBJECT(outputFile, "\"),");

				WRITE_PYOBJECT(outputFile, "ListIntHex(\"");
				u = 0;
				face = mesh->mface;
				for(f = 0; f < mesh->totface; ++face, ++f) {
					if(face->v4) {
						WRITE_PYOBJECT(outputFile, "%08X", HEX(u));
						k = u+1;
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						k = u+2;
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						k = u+3;
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						WRITE_PYOBJECT(outputFile, "%08X", HEX(u));
						u += 4;
					} else {
						WRITE_PYOBJECT(outputFile, "%08X", HEX(u));
						k = u+1;
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						k = u+2;
						WRITE_PYOBJECT(outputFile, "%08X", HEX(k));
						u += 3;
					}
				}
				WRITE_PYOBJECT(outputFile, "\"))");

				if(l < uv_count) {
					WRITE_PYOBJECT(outputFile, ",");
				}
			}
		}
		WRITE_PYOBJECT(outputFile, "\n\t)));");
	}

	if(propGroup) {
		WritePythonAttribute(outputFile, propGroup, "dynamic_geometry");
		WritePythonAttribute(outputFile, propGroup, "osd_subdiv_level");
	}

	WRITE_PYOBJECT(outputFile, "\n}\n");
}


void  write_Mesh(PyObject *outputFile, Scene *sce, Object *ob, Main *main, const char *pluginName, PyObject *propGroup)
{
	Mesh *mesh = GetRenderMesh(sce, main, ob);
	if(NOT(mesh)) {
		PRINT_ERROR("Can't get render mesh!");
		return;
	}

	write_GeomStaticMesh(outputFile, sce, ob, mesh, pluginName, propGroup);

	/* remove the temporary mesh */
	BKE_mesh_free(mesh, TRUE);
	BLI_remlink(&main->mesh, mesh);
	MEM_freeN(mesh);
}
