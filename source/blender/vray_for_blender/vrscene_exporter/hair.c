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

#include "vrscene.h"


typedef struct ParticleStrandData {
	float *uvco;
	int    totuv;
} ParticleStrandData;


// TODO: Remove! V-Ray could do it itself!
//
// Spline Interpolation
//
//  Code from: http://www.mech.uq.edu.au/staff/jacobs/nm_lib/doc/spline.html
//

// c_spline_init()
//
//   Evaluate the coefficients b[i], c[i], d[i], i = 0, 1, .. n-1 for
//   a cubic interpolating spline
//
//   S(xx) = Y[i] + b[i] * w + c[i] * w**2 + d[i] * w**3
//   where w = xx - x[i]
//   and   x[i] <= xx <= x[i+1]
//
//   The n supplied data points are x[i], y[i], i = 0 ... n-1.
//
//   Input :
//   -------
//   n       : The number of data points or knots (n >= 2)
//   end1,
//   end2    : = 1 to specify the slopes at the end points
//             = 0 to obtain the default conditions
//   slope1,
//   slope2  : the slopes at the end points x[0] and x[n-1]
//             respectively
//   x[]     : the abscissas of the knots in strictly
//             increasing order
//   y[]     : the ordinates of the knots
//
//   Output :
//   --------
//   b, c, d : arrays of spline coefficients as defined above
//             (See note 2 for a definition.)
//   iflag   : status flag
//            = 0 normal return
//            = 1 less than two data points; cannot interpolate
//            = 2 x[] are not in ascending order
//
//   This C code written by ...  Peter & Nigel,
//   ----------------------      Design Software,
//                               42 Gubberley St,
//                               Kenmore, 4069,
//                               Australia.
//
//   Version ... 1.1, 30 September 1987
//   -------     2.0, 6 April 1989    (start with zero subscript)
//                                     remove ndim from parameter list
//               2.1, 28 April 1989   (check on x[])
//               2.2, 10 Oct   1989   change number order of matrix
//
//   Notes ...
//   -----
//   (1) The accompanying function seval() may be used to evaluate the
//       spline while deriv will provide the first derivative.
//   (2) Using p to denote differentiation
//       y[i] = S(X[i])
//       b[i] = Sp(X[i])
//       c[i] = Spp(X[i])/2
//       d[i] = Sppp(X[i])/6  ( Derivative from the right )
//   (3) Since the zero elements of the arrays ARE NOW used here,
//       all arrays to be passed from the main program should be
//       dimensioned at least [n].  These routines will use elements
//       [0 .. n-1].
//   (4) Adapted from the text
//       Forsythe, G.E., Malcolm, M.A. and Moler, C.B. (1977)
//       "Computer Methods for Mathematical Computations"
//       Prentice Hall
//   (5) Note that although there are only n-1 polynomial segments,
//       n elements are requird in b, c, d.  The elements b[n-1],
//       c[n-1] and d[n-1] are set to continue the last segment
//       past x[n-1].
//
static int c_spline_init(const int n, const int end1, const int end2, const float slope1, const float slope2,
						 const float x[], const float y[],
						 float b[], float c[], float d[], int *iflag)
{
	int     nm1, ib, i;
	float  t;
	int     ascend;

	nm1    = n - 1;
	*iflag = 0;

	/* no possible interpolation */
	if(n < 2) {
		*iflag = 1;
		goto LeaveSpline;
	}

	ascend = 1;
	for(i = 1; i < n; ++i)
		if (x[i] <= x[i-1]) ascend = 0;

	if(!ascend) {
		*iflag = 2;
		goto LeaveSpline;
	}

	if(n >= 3)
	{
		/* At least quadratic */

		/* Set up the symmetric tri-diagonal system
		   b = diagonal
		   d = offdiagonal
		   c = right-hand-side  */
		d[0] = x[1] - x[0];
		c[1] = (y[1] - y[0]) / d[0];
		for (i = 1; i < nm1; ++i)
		{
			d[i]   = x[i+1] - x[i];
			b[i]   = 2.0 * (d[i-1] + d[i]);
			c[i+1] = (y[i+1] - y[i]) / d[i];
			c[i]   = c[i+1] - c[i];
		}

		/* Default End conditions
		   Third derivatives at x[0] and x[n-1] obtained
		   from divided differences  */
		b[0]   = -d[0];
		b[nm1] = -d[n-2];
		c[0]   = 0.0;
		c[nm1] = 0.0;
		if(n != 3) {
			c[0]   = c[2] / (x[3] - x[1]) - c[1] / (x[2] - x[0]);
			c[nm1] = c[n-2] / (x[nm1] - x[n-3]) - c[n-3] / (x[n-2] - x[n-4]);
			c[0]   = c[0] * d[0] * d[0] / (x[3] - x[0]);
			c[nm1] = -c[nm1] * d[n-2] * d[n-2] / (x[nm1] - x[n-4]);
		}

		/* Alternative end conditions -- known slopes */
		if(end1 == 1) {
			b[0] = 2.0 * (x[1] - x[0]);
			c[0] = (y[1] - y[0]) / (x[1] - x[0]) - slope1;
		}
		if(end2 == 1) {
			b[nm1] = 2.0 * (x[nm1] - x[n-2]);
			c[nm1] = slope2 - (y[nm1] - y[n-2]) / (x[nm1] - x[n-2]);
		}

		/* Forward elimination */
		for(i = 1; i < n; ++i) {
			t    = d[i-1] / b[i-1];
			b[i] = b[i] - t * d[i-1];
			c[i] = c[i] - t * c[i-1];
		}

		/* Back substitution */
		c[nm1] = c[nm1] / b[nm1];
		for(ib = 0; ib < nm1; ++ib)
		{
			i    = n - ib - 2;
			c[i] = (c[i] - d[i] * c[i+1]) / b[i];
		}

		/* c[i] is now the sigma[i] of the text */

		/* Compute the polynomial coefficients */
		b[nm1] = (y[nm1] - y[n-2]) / d[n-2] + d[n-2] * (c[n-2] + 2.0 * c[nm1]);
		for(i = 0; i < nm1; ++i)
		{
			b[i] = (y[i+1] - y[i]) / d[i] - d[i] * (c[i+1] + 2.0 * c[i]);
			d[i] = (c[i+1] - c[i]) / d[i];
			c[i] = 3.0 * c[i];
		}
		c[nm1] = 3.0 * c[nm1];
		d[nm1] = d[n-2];

	}
	else
	{
		/* linear segment only  */
		b[0] = (y[1] - y[0]) / (x[1] - x[0]);
		c[0] = 0.0;
		d[0] = 0.0;
		b[1] = b[0];
		c[1] = 0.0;
		d[1] = 0.0;
	}

LeaveSpline:
	return 0;
}


// c_spline_eval()
//
//  Evaluate the cubic spline function
//
//  S(xx) = y[i] + b[i] * w + c[i] * w**2 + d[i] * w**3
//  where w = u - x[i]
//  and   x[i] <= u <= x[i+1]
//  Note that Horner's rule is used.
//  If u < x[0]   then i = 0 is used.
//  If u > x[n-1] then i = n-1 is used.
//
//  Input :
//  -------
//  n       : The number of data points or knots (n >= 2)
//  u       : the abscissa at which the spline is to be evaluated
//  Last    : the segment that was last used to evaluate U
//  x[]     : the abscissas of the knots in strictly increasing order
//  y[]     : the ordinates of the knots
//  b, c, d : arrays of spline coefficients computed by spline().
//
//  Output :
//  --------
//  seval   : the value of the spline function at u
//  Last    : the segment in which u lies
//
//  Notes ...
//  -----
//  (1) If u is not in the same interval as the previous call then a
//      binary search is performed to determine the proper interval.
//
static float c_spline_eval(int n, float u, float x[], float y[],
						   float b[], float c[], float d[], int *last)
{
	int    i, j, k;
	float w;

	i = *last;

	if(i >= n-1) i = 0;
	if(i < 0)  i = 0;

	/* perform a binary search */
	if((x[i] > u) || (x[i+1] < u))
	{
		i = 0;
		j = n;
		do
		{
			k = (i + j) / 2;         /* split the domain to search */
			if (u < x[k])  j = k;    /* move the upper bound */
			if (u >= x[k]) i = k;    /* move the lower bound */
		}                            /* there are no more segments to search */
		while (j > i+1);
	}
	*last = i;

	/* Evaluate the spline */
	w = u - x[i];
	w = y[i] + w * (b[i] + w * (c[i] + w * d[i]));

	return w;
}


// Taken from "source/blender/render/intern/source/convertblender.c"
// and slightly modified
//
static void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
{
	int i;

	/* get uvco */
	if (sd->uvco && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME)) {
		for (i=0; i<sd->totuv; i++) {
			if (num != DMCACHE_NOTFOUND) {
				MFace  *mface  = dm->getTessFaceData(dm, num, CD_MFACE);
				MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, i);
				mtface += num;

				psys_interpolate_uvs(mtface, mface->v4, fuv, sd->uvco + 2 * i);
			}
			else {
				sd->uvco[2*i] = 0.0f;
				sd->uvco[2*i + 1] = 0.0f;
			}
		}
	}
}


int write_GeomMayaHair(PyObject *outputFile, Scene *sce, Main *bmain, Object *ob, ParticleSystem *psys, const char *pluginName)
{
	static char buf[MAX_PLUGIN_NAME];

	int    i, c, p, s;
	float  f;
	float  t;

	EvaluationContext eval_ctx = {0};

	ParticleSettings           *pset = NULL;
	ParticleSystemModifierData *psmd = NULL;

	ParticleData       *pa   = NULL;
	HairKey            *hkey = NULL;
	ParticleStrandData  sd;

	ParticleCacheKey **child_cache = NULL;
	ParticleCacheKey  *child_key   = NULL;
	ChildParticle     *cpa         = NULL;
	int                child_total = 0;
	int                child_steps = 0;
	float              child_key_co[3];

	float     hairmat[4][4];
	float     segment[3];
	float     color[3] = {0.5f,0.5f,0.5f};
	float     width = 0.001f;
	float     cone_width = 0.001f;
	int       num = -1;

	int       spline_init_flag;
	int       interp_points_count;
	float     interp_points_step;
	int       data_points_count;
	float     data_points_step;
	float     data_points_ordinates[3][64];
	float     data_points_abscissas[64];

	float     s_b[3][16];
	float     s_c[3][16];
	float     s_d[3][16];

	int       spline_last[3];

	short     use_cone    = false;
	short     use_child   = 0;
	short     free_edit   = 0;
	short     need_recalc = 0;

	PointerRNA  rna_pset;
	PointerRNA  VRayParticleSettings;
	PointerRNA  VRayFur;

	int  display_percentage;
	int  display_percentage_child;

	int  debug = false;

	eval_ctx.for_render = true;

	need_recalc = 0;

	pset = psys->part;

	if(pset->type != PART_HAIR) {
		return 1;
	}

	if(psys->part->ren_as != PART_DRAW_PATH) {
		return 1;
	}

	psmd = psys_get_modifier(ob, psys);
	if(!psmd) {
		return 1;
	}
	if(!(psmd->modifier.mode & eModifierMode_Render)) {
		return 1;
	}

	RNA_id_pointer_create(&pset->id, &rna_pset);

	if(RNA_struct_find_property(&rna_pset, "vray")) {
		VRayParticleSettings= RNA_pointer_get(&rna_pset, "vray");

		if(RNA_struct_find_property(&VRayParticleSettings, "VRayFur")) {
			VRayFur = RNA_pointer_get(&VRayParticleSettings, "VRayFur");

			// Get hair width
			width = RNA_float_get(&VRayFur, "width");
		}
	}

	child_cache = psys->childcache;
	child_total = psys->totchildcache;
	use_child   = (pset->childtype && child_cache);

	// Store "Display percentage" setting
	display_percentage       = pset->disp;
	display_percentage_child = pset->child_nbr;

	// Check if particles are edited
	free_edit = psys_check_edited(psys);

	// Recalc parent hair only if they are not
	// manually edited
	if(!free_edit) {
		need_recalc = 1;
		pset->disp = 100;
		psys->recalc |= PSYS_RECALC;
	}

	if(use_child) {
		need_recalc = 1;
		pset->child_nbr = pset->ren_child_nbr;
		psys->recalc |= PSYS_RECALC_CHILD;
	}

	if(psys->flag & PSYS_HAIR_DYNAMICS)
		need_recalc = 0;

	// Recalc hair with render settings
	if(need_recalc) {
		ob->recalc |= OB_RECALC_ALL;
		BKE_scene_update_tagged(&eval_ctx, bmain, sce);
	}

	// Get new child data pointers
	if(use_child) {
		child_cache = psys->childcache;
		child_total = psys->totchildcache;

		DEBUG_PRINT(debug, "child_total = %i", child_total);
	}

	// Spline interpolation
	interp_points_count = (int)pow(2.0, pset->ren_step);
	interp_points_step = 1.0 / (interp_points_count - 1);

	DEBUG_PRINT(debug, "interp_points_count = %i", interp_points_count);

	WRITE_PYOBJECT(outputFile, "GeomMayaHair %s {", pluginName);
	WRITE_PYOBJECT(outputFile, "\n\tnum_hair_vertices=interpolate((%d,ListIntHex(\"", sce->r.cfra);
	if(use_child) {
		for(p = 0; p < child_total; ++p) {
			WRITE_PYOBJECT_HEX_VALUE(outputFile, interp_points_count);
		}
	}
	else {
		LOOP_PARTICLES {
			WRITE_PYOBJECT_HEX_VALUE(outputFile, interp_points_count);
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\thair_vertices=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
	if(use_child) {
		for(p = 0; p < child_total; ++p) {
			child_key   = child_cache[p];
			child_steps = child_key->steps;

			// Spline interpolation
			data_points_count = child_steps;
			data_points_step  = 1.0f / (child_steps - 1);

			// Store control points
			for(s = 0, f = 0.0f; s < child_steps; ++s, ++child_key, f += data_points_step) {
				data_points_abscissas[s] = f;

				// Child particles are stored in world space,
				// but we need them in object space
				copy_v3_v3(child_key_co, child_key->co);

				// Remove transform by applying inverse matrix
				mul_m4_v3(ob->imat, child_key_co);

				for(c = 0; c < 3; ++c) {
					data_points_ordinates[c][s] = child_key_co[c];
				}
			}

			// Init spline coefficients
			for(c = 0; c < 3; ++c) {
				c_spline_init(data_points_count, 0, 0, 0.0f, 0.0f,
							  data_points_abscissas, data_points_ordinates[c],
							  s_b[c], s_c[c], s_d[c], &spline_init_flag);
			}

			// Write interpolated child points
			for(c = 0; c < 3; ++c)
				spline_last[c] = 0;

			for(t = 0.0f; t <= 1.0; t += interp_points_step) {
				// Calculate interpolated coordinate
				for(c = 0; c < 3; ++c) {
					segment[c] = c_spline_eval(data_points_count, t, data_points_abscissas, data_points_ordinates[c],
											   s_b[c], s_c[c], s_d[c], &spline_last[c]);
				}

				WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
			}
		}
	}
	else {
		LOOP_PARTICLES {
			DEBUG_PRINT(debug, "\033[0;32mV-Ray/Blender:\033[0m Particle system: %s => Hair: %i\n", psys->name, p + 1);

			psys_mat_hair_to_object(NULL, psmd->dm, psmd->psys->part->from, pa, hairmat);

			// Spline interpolation
			data_points_count = pa->totkey;
			data_points_step  = 1.0f / (data_points_count - 1);

			DEBUG_PRINT(debug, "data_points_count = %i", data_points_count);
			DEBUG_PRINT(debug, "data_points_step = %.3f", data_points_step);

			for(i = 0, f = 0.0f; i < data_points_count; ++i, f += data_points_step) {
				data_points_abscissas[i] = f;
			}

			// Store control points
			for(s = 0, hkey = pa->hair; s < pa->totkey; ++s, ++hkey) {
				copy_v3_v3(segment, hkey->co);
				mul_m4_v3(hairmat, segment);

				for(c = 0; c < 3; ++c) {
					data_points_ordinates[c][s] = segment[c];
				}
			}

			// Init spline coefficients
			for(c = 0; c < 3; ++c) {
				c_spline_init(data_points_count, 0, 0, 0.0f, 0.0f,
							  data_points_abscissas, data_points_ordinates[c],
							  s_b[c], s_c[c], s_d[c], &spline_init_flag);
			}

			// Write interpolated points
			for(c = 0; c < 3; ++c)
				spline_last[c] = 0;
			for(t = 0.0f; t <= 1.0; t += interp_points_step) {
				// Calculate interpolated coordinates
				for(c = 0; c < 3; ++c) {
					segment[c] = c_spline_eval(data_points_count, t, data_points_abscissas, data_points_ordinates[c],
											   s_b[c], s_c[c], s_d[c], &spline_last[c]);
				}
				WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
			}
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	memset(&sd, 0, sizeof(ParticleStrandData));

	// DEBUG_PRINT(TRUE, "psmd->dm = 0x%X", psmd->dm);

	if(psmd->dm) {
		if(use_child) {
			sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);

			if(sd.totuv) {
				sd.uvco = MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");
			}
			else {
				sd.uvco = NULL;
			}

			if(sd.uvco) {
				WRITE_PYOBJECT(outputFile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);

				for(p = 0; p < child_total; ++p) {
					cpa = psys->child + p;

					/* get uvco & mcol */
					if(pset->childtype==PART_CHILD_FACES) {
						get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm, cpa->fuv, cpa->num, &sd);
					}
					else {
						ParticleData *parent = psys->particles + cpa->parent;
						num = parent->num_dmcache;

						if (num == DMCACHE_NOTFOUND)
							if (parent->num < psmd->dm->getNumTessFaces(psmd->dm))
								num = parent->num;

						get_particle_uvco_mcol(pset->from, psmd->dm, parent->fuv, num, &sd);
					}

					segment[0] = sd.uvco[0];
					segment[1] = sd.uvco[1];
					segment[2] = 0.0f;

					WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
				}
				WRITE_PYOBJECT(outputFile, "\")));");

				MEM_freeN(sd.uvco);
			}
		}
		else {
			sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);

			if(sd.totuv) {
				sd.uvco = MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");
			}
			else {
				sd.uvco = NULL;
			}

			if(sd.uvco) {
				WRITE_PYOBJECT(outputFile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
				LOOP_PARTICLES {
					/* get uvco & mcol */
					num = pa->num_dmcache;

					if(num == DMCACHE_NOTFOUND) {
						if(pa->num < psmd->dm->getNumTessFaces(psmd->dm)) {
							num = pa->num;
						}
					}

					get_particle_uvco_mcol(pset->from, psmd->dm, pa->fuv, num, &sd);

					// DEBUG_PRINT(TRUE, "Pa.uv = %.3f, %.3f", sd.uvco[0], sd.uvco[1]);

					segment[0] = sd.uvco[0];
					segment[1] = sd.uvco[1];
					segment[2] = 0.0f;

					WRITE_PYOBJECT_HEX_VECTOR(outputFile, segment);
				}
				WRITE_PYOBJECT(outputFile, "\")));");

				MEM_freeN(sd.uvco);
			}
		}
	}

	WRITE_PYOBJECT(outputFile, "\n\twidths=interpolate((%d,ListFloatHex(\"", sce->r.cfra);
	if(use_child) {
		for(p = 0; p < child_total; ++p) {
			cone_width = width;
			for(s = 0; s < interp_points_count; ++s) {
				if(use_cone && s > 0) {
					cone_width = width / s;
				}
				WRITE_PYOBJECT_HEX_VALUE(outputFile, cone_width);
			}
		}
	}
	else {
		for(p = 0; p < psys->totpart; ++p) {
			for(s = 0; s < interp_points_count; ++s) {
				cone_width = width;
				if(use_cone && s > 0) {
					cone_width = width / s;
				}
				WRITE_PYOBJECT_HEX_VALUE(outputFile, cone_width);
			}
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\tcolors=interpolate((%d,ListColorHex(\"", sce->r.cfra);
	if(use_child) {
		for(p = 0; p < child_total; ++p) {
			for(s = 0; s < interp_points_count; ++s) {
				WRITE_PYOBJECT_HEX_VECTOR(outputFile, color);
			}
		}
	}
	else {
		for(p = 0; p < psys->totpart; ++p) {
			for(s = 0; s < interp_points_count; ++s) {
				WRITE_PYOBJECT_HEX_VECTOR(outputFile, color);
			}
		}
	}
	WRITE_PYOBJECT(outputFile, "\")));");

	WRITE_PYOBJECT(outputFile, "\n\topacity=1.0;");
	if(psys->part->flag & PART_HAIR_BSPLINE) {
		WRITE_PYOBJECT(outputFile, "\n\tgeom_splines=1;");
		WRITE_PYOBJECT(outputFile, "\n\tgeom_tesselation_mult=1.0;");
	}
	WRITE_PYOBJECT(outputFile, "\n}\n\n");

	// Restore "Display percentage" setting
	pset->disp      = display_percentage;
	pset->child_nbr = display_percentage_child;

	if(!free_edit) {
		psys->recalc |= PSYS_RECALC;
	}
	if(use_child) {
		psys->recalc |= PSYS_RECALC_CHILD;
	}

	// Recalc hair back with viewport settings
	if(need_recalc) {
		ob->recalc |= OB_RECALC_ALL;
		BKE_scene_update_tagged(&eval_ctx, bmain, sce);
	}

	return 0;
}
