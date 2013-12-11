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

#define WRITE_HEX_QUADFACE(f, face) fprintf(gfile, "%08X%08X%08X%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3), HEX(face->v3), HEX(face->v4), HEX(face->v1))
#define WRITE_HEX_TRIFACE(f, face)  fprintf(gfile, "%08X%08X%08X", HEX(face->v1), HEX(face->v2), HEX(face->v3))

#define MAX_MESH_THREADS    16
#define USE_STRING_POINTER  1

struct Material;
struct MTex;
struct Tex;

typedef struct UVLayer {
    char *name;
    int   id;
} UVLayer;

typedef struct ThreadData {
    Scene    *sce;
    Main     *bmain;
    LinkNode *objects;
    LinkNode *uvs;
    char     *filepath;
    int       id;
    int       animation;
    int       instances;
} ThreadData;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static ThreadData thread_data[MAX_MESH_THREADS];

static int debug = 0;

static char clean_string[MAX_IDPROP_NAME];


// http://rosettacode.org/wiki/Determine_if_a_string_is_numeric
// Used to check if UV layer name consist of digits
//
static int is_numeric(const char *s)
{
    int res = 0;
    char *p;
    if(s == NULL || *s == '\0' || isspace(*s))
        return 0;
    res = strtod(s, &p);
    return *p == '\0';
}


static int uvlayer_name_to_id(LinkNode *list, char *name)
{
    LinkNode *list_iter;
    UVLayer  *uv_layer;

    if(strcmp(name, "") == 0)
        return 1;

    list_iter= list;
    while(list_iter) {
        uv_layer= (UVLayer*)list_iter->link;
        if(strcmp(name, uv_layer->name) == 0)
            return uv_layer->id;
        list_iter= list_iter->next;
    }

    return 1;
}


static int uvlayer_in_list(LinkNode *list, char *name)
{
    LinkNode *list_iter;
    UVLayer  *uv_layer;

    list_iter= list;
    while(list_iter) {
        uv_layer= (UVLayer*)list_iter->link;
        if(strcmp(name, uv_layer->name) == 0)
            return 1;
        list_iter= list_iter->next;
    }
    return 0;
}


static int in_list(LinkNode *list, void *item)
{
    LinkNode *list_iter;

    if(!list)
        return 0;

    list_iter= list;
    while(list_iter) {
        if(list_iter->link == item)
            return 1;
        list_iter= list_iter->next;
    }
    return 0;
}


static void *uvlayer_ptr(char *name, int id)
{
    UVLayer *tmp;
    tmp= (UVLayer*)malloc(sizeof(UVLayer));
    tmp->name= name;
    tmp->id= id;
    return (void*)tmp;
}


static void clear_string(char *str)
{
    int i;

    strncpy(clean_string, str, MAX_IDPROP_NAME);

    for(i= 0; i < strlen(str); i++) {
        if(clean_string[i]) {
            if(clean_string[i] == '+')
                clean_string[i]= 'p';
            else if(clean_string[i] == '-')
                clean_string[i]= 'm';
            else if(!((clean_string[i] >= 'A' && clean_string[i] <= 'Z') || (clean_string[i] >= 'a' && clean_string[i] <= 'z') || (clean_string[i] >= '0' && clean_string[i] <= '9')))
                clean_string[i]= '_';
        }
    }
}


static int write_edge_visibility(FILE *gfile, int k, unsigned long int *ev)
{
    if(k == 9) {
        WRITE_HEX_VALUE(gfile, *ev);
        *ev= 0;
        return 0;
    }
    return k + 1;
}


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


typedef struct ParticleStrandData {
    float *uvco;
    int    totuv;
} ParticleStrandData;


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


static void write_GeomMayaHair(FILE *gfile, Scene *sce, Main *bmain, Object *ob)
{
    int    i, c, p, s;
    float  f;
    float  t;

    ParticleSystem             *psys = NULL;
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

    short     use_cone    = 1;
    short     use_child   = 0;
    short     free_edit   = 0;
    short     need_recalc = 0;

    PointerRNA  rna_pset;
    PointerRNA  VRayParticleSettings;
    PointerRNA  VRayFur;

    int  display_percentage;
    int  display_percentage_child;

    for(psys = ob->particlesystem.first; psys; psys = psys->next)
    {
        need_recalc = 0;

        pset = psys->part;

        if(pset->type != PART_HAIR) {
            continue;
        }

        if(psys->part->ren_as != PART_DRAW_PATH) {
            continue;
        }

        psmd = psys_get_modifier(ob, psys);
        if(!psmd) {
            continue;
        }
        if(!(psmd->modifier.mode & eModifierMode_Render)) {
            continue;
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
            BKE_scene_update_tagged(bmain, sce);
        }

        // Get new child data pointers
        if(use_child) {
            child_cache = psys->childcache;
            child_total = psys->totchildcache;

            DEBUG_OUTPUT(debug, "child_total = %i", child_total);
        }

        // Spline interpolation
        interp_points_count = (int)pow(2.0, pset->ren_step);
        interp_points_step = 1.0 / (interp_points_count - 1);

        DEBUG_OUTPUT(debug, "interp_points_count = %i", interp_points_count);

        clear_string(psys->name);
        fprintf(gfile, "GeomMayaHair HAIR%s", clean_string);
        clear_string(pset->id.name+2);
        fprintf(gfile, "%s {", clean_string);


        fprintf(gfile, "\n\tnum_hair_vertices=interpolate((%d,ListIntHex(\"", sce->r.cfra);
        if(use_child) {
            for(p = 0; p < child_total; ++p) {
                WRITE_HEX_VALUE(gfile, interp_points_count);
            }
        }
        else {
            LOOP_PARTICLES {
                WRITE_HEX_VALUE(gfile, interp_points_count);
            }
        }
        fprintf(gfile,"\")));");


        fprintf(gfile, "\n\thair_vertices=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
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

                    WRITE_HEX_VECTOR(gfile, segment);
                }
            }
        }
        else {
            LOOP_PARTICLES {
                DEBUG_OUTPUT(debug, "\033[0;32mV-Ray/Blender:\033[0m Particle system: %s => Hair: %i\n", psys->name, p + 1);

                psys_mat_hair_to_object(NULL, psmd->dm, psmd->psys->part->from, pa, hairmat);

                // Spline interpolation
                data_points_count = pa->totkey;
                data_points_step  = 1.0f / (data_points_count - 1);

                DEBUG_OUTPUT(debug, "data_points_count = %i", data_points_count);
                DEBUG_OUTPUT(debug, "data_points_step = %.3f", data_points_step);

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
                    WRITE_HEX_VECTOR(gfile, segment);
                }
            }
        }
        fprintf(gfile,"\")));");

        memset(&sd, 0, sizeof(ParticleStrandData));

        // DEBUG_OUTPUT(TRUE, "psmd->dm = 0x%X", psmd->dm);

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
					fprintf(gfile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);

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

						WRITE_HEX_VECTOR(gfile, segment);
					}
					fprintf(gfile,"\")));");

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
					fprintf(gfile, "\n\tstrand_uvw=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
					LOOP_PARTICLES {
						/* get uvco & mcol */
						num = pa->num_dmcache;

						if(num == DMCACHE_NOTFOUND) {
							if(pa->num < psmd->dm->getNumTessFaces(psmd->dm)) {
								num = pa->num;
							}
						}

						get_particle_uvco_mcol(pset->from, psmd->dm, pa->fuv, num, &sd);

						// DEBUG_OUTPUT(TRUE, "Pa.uv = %.3f, %.3f", sd.uvco[0], sd.uvco[1]);

						segment[0] = sd.uvco[0];
						segment[1] = sd.uvco[1];
						segment[2] = 0.0f;

						WRITE_HEX_VECTOR(gfile, segment);
					}
					fprintf(gfile,"\")));");

					MEM_freeN(sd.uvco);
				}
			}
        }

        fprintf(gfile, "\n\twidths=interpolate((%d,ListFloatHex(\"", sce->r.cfra);
        if(use_child) {
            for(p = 0; p < child_total; ++p) {
                cone_width = width;
                for(s = 0; s < interp_points_count; ++s) {
                    if(use_cone && s > 0) {
                        cone_width = width / s;
                    }
                    WRITE_HEX_VALUE(gfile, cone_width);
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
                    WRITE_HEX_VALUE(gfile, cone_width);
                }
            }
        }
        fprintf(gfile,"\")));");


        fprintf(gfile, "\n\tcolors=interpolate((%d,ListColorHex(\"", sce->r.cfra);
        if(use_child) {
            for(p = 0; p < child_total; ++p) {
                for(s = 0; s < interp_points_count; ++s) {
                    WRITE_HEX_VECTOR(gfile, color);
                }
            }
        }
        else {
            for(p = 0; p < psys->totpart; ++p) {
                for(s = 0; s < interp_points_count; ++s) {
                    WRITE_HEX_VECTOR(gfile, color);
                }
            }
        }
        fprintf(gfile,"\")));");


        fprintf(gfile, "\n\topacity=1.0;");
        if(psys->part->flag & PART_HAIR_BSPLINE) {
            fprintf(gfile, "\n\tgeom_splines=1;");
        }
        fprintf(gfile, "\n}\n\n");


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
            BKE_scene_update_tagged(bmain, sce);
        }
    }
}


// Taken from: source/blender/makesrna/intern/rna_object_api.c
// with a slight modifications
//
static Mesh *get_render_mesh(Scene *sce, Main *bmain, Object *ob)
{
    Mesh *tmpmesh;
    Curve *tmpcu = NULL, *copycu;
    Object *tmpobj = NULL;
    Object *basis_ob = NULL;
    ListBase disp = {NULL, NULL};

    /* Make a dummy mesh, saves copying */
    DerivedMesh *dm;

    CustomDataMask mask = CD_MASK_MESH;

    /* perform the mesh extraction based on type */
    switch (ob->type) {
    case OB_FONT:
    case OB_CURVE:
    case OB_SURF:
        /* copies object and modifiers (but not the data) */
        tmpobj = BKE_object_copy(ob);
        tmpcu = (Curve *)tmpobj->data;
        tmpcu->id.us--;

        /* copies the data */
        copycu = tmpobj->data = BKE_curve_copy( (Curve *) ob->data );

        /* temporarily set edit so we get updates from edit mode, but
         * also because for text datablocks copying it while in edit
         * mode gives invalid data structures */
        copycu->editfont = tmpcu->editfont;
        copycu->editnurb = tmpcu->editnurb;

        /* get updated display list, and convert to a mesh */
        BKE_displist_make_curveTypes( sce, tmpobj, 0 );

        copycu->editfont = NULL;
        copycu->editnurb = NULL;

        BKE_mesh_from_nurbs( tmpobj );

        /* nurbs_to_mesh changes the type to a mesh, check it worked */
        if (tmpobj->type != OB_MESH) {
            BKE_libblock_free_us( &(G.main->object), tmpobj );
            return NULL;
        }
        tmpmesh = tmpobj->data;
        BKE_libblock_free_us( &G.main->object, tmpobj );

        break;

    case OB_MBALL:
        /* metaballs don't have modifiers, so just convert to mesh */
        basis_ob = BKE_mball_basis_find(sce, ob);

        if (ob != basis_ob)
            return NULL; /* only do basis metaball */

        tmpmesh = BKE_mesh_add(bmain, "Mesh");

        BKE_displist_make_mball_forRender(sce, ob, &disp);
        BKE_mesh_from_metaball(&disp, tmpmesh);
        BKE_displist_free(&disp);

        break;

    case OB_MESH:
        /* Write the render mesh into the dummy mesh */
        dm = mesh_create_derived_render(sce, ob, mask);

        tmpmesh = BKE_mesh_add(bmain, "Mesh");
        DM_to_mesh(dm, tmpmesh, ob, mask);
        dm->release(dm);

        break;

    default:
        return NULL;
    }

    /* cycles and exporters rely on this still */
    BKE_mesh_tessface_ensure(tmpmesh);

    /* we don't assign it to anything */
    tmpmesh->id.us--;

    return tmpmesh;
}


static void write_GeomStaticMesh(FILE *gfile,
                                 Scene *sce, Object *ob, Mesh *mesh,
                                 LinkNode *uv_list, int instances)
{
    Mesh   *me = ob->data;
    MFace  *face;
    MTFace *mtface;
    MCol   *mcol;
    MVert  *vert;

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

    char  *lib_filename = NULL;

    PointerRNA   rna_me;
    PointerRNA   VRayMesh;
    PointerRNA   GeomStaticMesh;

    int          dynamic_geometry= 0;

    const int ft[6]= {0,1,2,2,3,0};

    unsigned long int ev= 0;

    int i, j, f, k, l;
    int u;

    DEBUG_OUTPUT(debug, "Processing object \"%s\": mesh \"%s\"", ob->id.name, me->id.name);

    if(!(mesh->totface)) {
        DEBUG_OUTPUT(debug, "No faces in mesh \"%s\"", me->id.name);
        return;
    }

    // Name format: ME<meshname>LI<libname>
    //
    if(instances)
        clear_string(me->id.name+2);
    else
        clear_string(ob->id.name+2);
    fprintf(gfile,"\nGeomStaticMesh ME%s", clean_string);

    if(me->id.lib) {
        lib_filename = (char*)malloc(FILE_MAX * sizeof(char));

        BLI_split_file_part(me->id.lib->name+2, lib_filename, FILE_MAX);
        BLI_replace_extension(lib_filename, FILE_MAX, "");

        clear_string(lib_filename);
        fprintf(gfile,"LI%s", clean_string);
        if(debug) {
            printf("V-Ray/Blender: Object: %s\n", ob->id.name+2);
            printf("  Mesh: %s\n", me->id.name+2);
            printf("    Lib filename: %s\n", lib_filename);
        }

        free(lib_filename);
    }
    fprintf(gfile," {\n");


	fprintf(gfile,"\tvertices=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
    vert= mesh->mvert;
    for(f= 0; f < mesh->totvert; ++vert, ++f) {
        WRITE_HEX_VECTOR(gfile, vert->co);
    }
    fprintf(gfile,"\")));\n");

    // TODO: velocities (?)

	fprintf(gfile,"\tfaces=interpolate((%d,ListIntHex(\"", sce->r.cfra);
    face= mesh->mface;
    for(f= 0; f < mesh->totface; ++face, ++f) {
        if(face->v4)
            WRITE_HEX_QUADFACE(gfile, face);
        else
            WRITE_HEX_TRIFACE(gfile, face);

    }
    fprintf(gfile,"\")));\n");


	fprintf(gfile,"\tnormals=interpolate((%d,ListVectorHex(\"", sce->r.cfra);
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
            WRITE_HEX_VECTOR(gfile, n0);
            WRITE_HEX_VECTOR(gfile, n1);
            WRITE_HEX_VECTOR(gfile, n2);
            WRITE_HEX_VECTOR(gfile, n2);
            WRITE_HEX_VECTOR(gfile, n3);
            WRITE_HEX_VECTOR(gfile, n0);
        } else {
            WRITE_HEX_VECTOR(gfile, n0);
            WRITE_HEX_VECTOR(gfile, n1);
            WRITE_HEX_VECTOR(gfile, n2);
        }
    }
    fprintf(gfile,"\")));\n");


	fprintf(gfile,"\tfaceNormals=interpolate((%d,ListIntHex(\"", sce->r.cfra);
    face= mesh->mface;
    k= 0;
    for(f= 0; f < mesh->totface; ++face, ++f) {
        if(mesh->mface[f].v4)
            verts= 6;
        else
            verts= 3;

        for(i= 0; i < verts; i++) {
            fprintf(gfile, "%08X", htonl(*(int*)&k));
            k++;
        }
    }
    fprintf(gfile,"\")));\n");


    fprintf(gfile,"\tface_mtlIDs=ListIntHex(\"");
    face= mesh->mface;
    for(f= 0; f < mesh->totface; ++face, ++f) {
        matid= face->mat_nr + 1;
        if(face->v4)
            fprintf(gfile, "%08X%08X", htonl(*(int*)&matid), htonl(*(int*)&matid));
        else
            fprintf(gfile, "%08X", htonl(*(int*)&matid));
    }
    fprintf(gfile,"\");\n");


    fprintf(gfile,"\tedge_visibility=ListIntHex(\"");
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
        fprintf(gfile, "%08X", htonl(*(int*)&ev));
    } else {
        k= 0;
        face= mesh->mface;
        for(f= 0; f < mesh->totface; ++face, ++f) {
            if(face->v4) {
                ev= (ev << 3) | 3;
                k= write_edge_visibility(gfile, k, &ev);
                ev= (ev << 3) | 3;
                k= write_edge_visibility(gfile, k, &ev);
            } else {
                ev= (ev << 3) | 8;
                k= write_edge_visibility(gfile, k, &ev);
            }
        }

        if(k) {
            fprintf(gfile, "%08X", htonl(*(int*)&ev));
        }
    }
    fprintf(gfile,"\");\n");


    fdata = &mesh->fdata;

    uv_count  = CustomData_number_of_layers(fdata, CD_MTFACE);
    uv_count += CustomData_number_of_layers(fdata, CD_MCOL);

    if(uv_count) {
        fprintf(gfile,"\tmap_channels_names=List(");
        for(l = 0; l < fdata->totlayer; ++l) {
            if(fdata->layers[l].type == CD_MTFACE || fdata->layers[l].type == CD_MCOL) {
                fprintf(gfile,"\"%s\"", fdata->layers[l].name);

                if(l < uv_count)
                    fprintf(gfile,",");
            }
        }
        fprintf(gfile,");\n");

        uv_layer_id = 0;
        fprintf(gfile,"\tmap_channels=interpolate((%d, List(", sce->r.cfra);
        for(l = 0; l < fdata->totlayer; ++l) {
            if(fdata->layers[l].type == CD_MTFACE || fdata->layers[l].type == CD_MCOL) {
                fprintf(gfile,"\n\t\t// Name: %s", fdata->layers[l].name);
                fprintf(gfile,"\n\t\tList(%i,ListVectorHex(\"", uv_layer_id++);

                if(fdata->layers[l].type == CD_MTFACE) {
                    face   = mesh->mface;
                    mtface = (MTFace*)fdata->layers[l].data;
                    for(f = 0; f < mesh->totface; ++face, ++f) {
                        if(face->v4)
                            verts = 4;
                        else
                            verts = 3;
                        for(i = 0; i < verts; i++) {
                            fprintf(gfile, "%08X%08X00000000",
                                    htonl(*(int*)&(mtface[f].uv[i][0])),
                                    htonl(*(int*)&(mtface[f].uv[i][1])));
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

                            WRITE_HEX_VECTOR(gfile, col);
                        }
                    }
                }

                fprintf(gfile,"\"),");

                fprintf(gfile,"ListIntHex(\"");
                u = 0;
                face = mesh->mface;
                for(f = 0; f < mesh->totface; ++face, ++f) {
                    if(face->v4) {
                        fprintf(gfile, "%08X", htonl(*(int*)&u));
                        k = u+1;
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        k = u+2;
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        k = u+3;
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        fprintf(gfile, "%08X", htonl(*(int*)&u));
                        u += 4;
                    } else {
                        fprintf(gfile, "%08X", htonl(*(int*)&u));
                        k = u+1;
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        k = u+2;
                        fprintf(gfile, "%08X", htonl(*(int*)&k));
                        u += 3;
                    }
                }
                fprintf(gfile,"\"))");

                if(l < uv_count)
                    fprintf(gfile,",");
            }
        }
        fprintf(gfile,")));\n");
    }

    RNA_id_pointer_create(&me->id, &rna_me);
    if(RNA_struct_find_property(&rna_me, "vray")) {
        VRayMesh = RNA_pointer_get(&rna_me, "vray");
        if(RNA_struct_find_property(&VRayMesh, "GeomStaticMesh")) {
            GeomStaticMesh = RNA_pointer_get(&VRayMesh, "GeomStaticMesh");
            if(RNA_struct_find_property(&GeomStaticMesh, "dynamic_geometry")) {
                dynamic_geometry = RNA_boolean_get(&GeomStaticMesh, "dynamic_geometry");
            }
        }
    }

    // When this flag is true V-Ray will use dynamic geometry for this mesh.
    // Instead of copying the mesh many times in the BSP tree, only the bounding
    // box will be present many times and ray intersections will occur
    // in a separate object space BSP tree.
	fprintf(gfile,"\tdynamic_geometry=%i;\n", dynamic_geometry);

    fprintf(gfile,"}\n\n");
}


static void write_TexVoxelData(FILE *gfile, const Scene *sce, Object *ob, const SmokeModifierData *smd, short use_smoke_hires)
{
    VoxelData *vd   = NULL;
    Material  *ma   = NULL;
    MTex      *mtex = NULL;
    Tex       *tex  = NULL;

    size_t i, m, t;
    size_t tot_res_high;
    size_t tot_res_low;

    float *density_data = NULL;
    float *heat_data    = NULL;
    float  density;
    float  heat;

    int    res_high[3];
    int    res_low[3];

    float  ob_imat[4][4];

    int    interp_type    = 0;
    float  int_multiplier = 1.0;

    char   uvwgen_name[255];
    char   tex_name[255];

    if(!(smd->domain && smd->domain->fluid)) {
        DEBUG_OUTPUT(TRUE, "Smoke modifier not found!");
        return;
    }

    // Take VoxelData pointer from texture
    //
    if(ob->mat) {
        for(m = 1; m <= ob->totcol; ++m) {
            ma = give_current_material(ob, m);
            if(!(ma))
                continue;

            for(t = 0; t < MAX_MTEX; ++t) {
                mtex = ma->mtex[t];
                if(!(mtex))
                    continue;

                tex = mtex->tex;
                if(!(tex))
                    continue;

                if(tex->type != TEX_VOXELDATA)
                    continue;

                vd = tex->vd;

                break;
            }

            if(vd)
                break;
        }
    }

    if(!(vd)) {
        DEBUG_OUTPUT(TRUE, "VoxelData texture not found!");
        return;
    }

    interp_type    = vd->interp_type;
    int_multiplier = vd->int_multiplier;

    clear_string(tex->id.name+2);
    sprintf(tex_name, "TE%s", clean_string);
    sprintf(uvwgen_name, "UVWGenWorldTE%s", clean_string);

    // Store object invert matrix
    invert_m4_m4(ob_imat, ob->obmat);

    if(use_smoke_hires && (smd->domain->flags & MOD_SMOKE_HIGHRES)) {
        COPY_VECTOR_3_3(res_low,  smd->domain->res);
        COPY_VECTOR_3_3(res_high, smd->domain->res_wt);

        density_data = smoke_turbulence_get_density(smd->domain->wt);
    }
    else {
        COPY_VECTOR_3_3(res_low,  smd->domain->res);
        COPY_VECTOR_3_3(res_high, smd->domain->res);

        density_data = smoke_get_density(smd->domain->fluid);
    }

    heat_data = smoke_get_heat(smd->domain->fluid);

    tot_res_high = (size_t)res_high[0] * (size_t)res_high[1] * (size_t)res_high[2];
    tot_res_low  = (size_t)res_low[0]  * (size_t)res_low[1]  * (size_t)res_low[2];

    fprintf(gfile, "\nUVWGenPlanarWorld %s {", uvwgen_name);

    // Blender smoke uses 2.0 x 2.0 x 2.0 mesh domain and then transform to form the final
    // smoke domain, so we need to transform UVWs
    fprintf(gfile, "\n\tuvw_transform = interpolate((%d,", sce->r.cfra);
    WRITE_TRANSFORM(gfile, ob_imat);
    fprintf(gfile, "));");

    fprintf(gfile, "\n}\n");

    fprintf(gfile, "\nTexVoxelData %s {", tex_name);
    fprintf(gfile, "\n\tuvwgen = %s;", uvwgen_name);
    fprintf(gfile, "\n\tdebug = %i;", debug);
    fprintf(gfile, "\n\tinterpolation = %i;", interp_type);
    fprintf(gfile, "\n\tresolution = Vector(%i,%i,%i);", res_high[0], res_high[1], res_high[2]);
    fprintf(gfile, "\n\tresolution_low = Vector(%i,%i,%i);", res_low[0], res_low[1], res_low[2]);

    // Density
    //
    fprintf(gfile, "\n\tdensity = interpolate((%d, ListFloatHex(\"", sce->r.cfra);
    for(i = 0; i < tot_res_high; ++i) {
        density = density_data[i];

        WRITE_HEX_VALUE(gfile, density);
    }
    fprintf(gfile, "\")));");

    // Heat
    //  Always low res
    //
    fprintf(gfile, "\n\theat = interpolate((%d, ListFloatHex(\"", sce->r.cfra);
    for(i = 0; i < tot_res_low; ++i) {
        heat  = (heat_data[i]+2.0f)/4.0f;
        heat *= int_multiplier;

        WRITE_HEX_VALUE(gfile, heat);
    }
    fprintf(gfile, "\")));");

    fprintf(gfile, "\n}\n");
}


static int mesh_animated(Object *ob)
{
    ModifierData *mod;

    switch(ob->type) {
    case OB_CURVE:
    case OB_SURF:
    case OB_FONT: {
        Curve *cu= (Curve*)ob->data;
        if(cu->adt)
            return 1;
    }
    break;
    case OB_MBALL: {
        MetaBall *mb= (MetaBall*)ob->data;
        if(mb->adt)
            return 1;
    }
    break;
    case OB_MESH: {
        Mesh *me= (Mesh*)ob->data;
        if(me->adt)
            return 1;
    }
    break;
    default:
        break;
    }

    mod= (ModifierData*)ob->modifiers.first;
    while(mod) {
        switch (mod->type) {
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
            return 1;
        default:
            mod= mod->next;
        }
    }

    return 0;
}


static void *export_meshes_thread(void *ptr)
{
    struct ThreadData *td;

    double    time = 0.0;
    char      time_str[32];

    FILE     *gfile= NULL;
    char      filepath[FILE_MAX];

    Scene    *sce;
    Main     *bmain;
    Object   *ob;
    Mesh     *mesh;

    LinkNode *tdl;

    short     use_hair  = 1;
    short     use_smoke = 1;
    short     use_smoke_hires = 1;

    PointerRNA rna_scene;
    PointerRNA VRayScene;
    PointerRNA VRayExporter;

    ModifierData      *md;
    SmokeModifierData *smd;

    td= (struct ThreadData*)ptr;

    sce=   td->sce;
    bmain= td->bmain;

    // Get export parameters from RNA
    RNA_id_pointer_create(&sce->id, &rna_scene);
    if(RNA_struct_find_property(&rna_scene, "vray")) {
        VRayScene= RNA_pointer_get(&rna_scene, "vray");

        if(RNA_struct_find_property(&VRayScene, "exporter")) {
            VRayExporter= RNA_pointer_get(&VRayScene, "exporter");

            use_hair = RNA_boolean_get(&VRayExporter, "use_hair");
            use_smoke       = RNA_boolean_get(&VRayExporter, "use_smoke");
            use_smoke_hires = RNA_boolean_get(&VRayExporter, "use_smoke_hires");
        }
    }

    if(debug) {
        time= PIL_check_seconds_timer();
        printf("V-Ray/Blender: Mesh export thread [%d]\n", td->id + 1);
    }
    sprintf(filepath, "%s_%.2d.vrscene", td->filepath, td->id);
    if(td->animation) {
        gfile = fopen(filepath, "a");
    } else {
        gfile = fopen(filepath, "w");
        fprintf(gfile,"// V-Ray/Blender\n");
        fprintf(gfile,"// Geometry file\n\n");
    }

    if(BLI_linklist_length(td->objects)) {
        tdl = td->objects;
        while(tdl) {
            ob = tdl->link;

            if(use_hair) {
                pthread_mutex_lock(&mtx);

                write_GeomMayaHair(gfile, sce, bmain, ob);

                pthread_mutex_unlock(&mtx);
            }

            if(use_smoke) {
                pthread_mutex_lock(&mtx);

                if((md = modifiers_findByType(ob, eModifierType_Smoke))) {
                    smd = (SmokeModifierData*)md;

                    write_TexVoxelData(gfile, sce, ob, smd, use_smoke_hires);
                }

                pthread_mutex_unlock(&mtx);
            }

            // Export mesh
            pthread_mutex_lock(&mtx);

            mesh = get_render_mesh(sce, bmain, ob);

            pthread_mutex_unlock(&mtx);

            if(mesh) {
                write_GeomStaticMesh(gfile, sce, ob, mesh, td->uvs, td->instances);

                pthread_mutex_lock(&mtx);

                /* remove the temporary mesh */
                BKE_mesh_free(mesh, TRUE);
                BLI_remlink(&bmain->mesh, mesh);
                MEM_freeN(mesh);

                pthread_mutex_unlock(&mtx);
            }

            tdl = tdl->next;
        }
    }

    fclose(gfile);

    if(debug) {
        BLI_timestr(PIL_check_seconds_timer() - time, time_str, sizeof(time_str));
        printf("V-Ray/Blender: Mesh export thread [%d] done [%s]\n", td->id + 1, time_str);
    }

    return NULL;
}


static void append_object(Scene *sce, LinkNode **objects, LinkNode **meshes, Object *ob,
                          int active_layers, int instances, int check_animated, int animation)
{
    GroupObject *gobject;
    Object      *gob;

    Mesh        *me;

    PointerRNA   rna_me;
    PointerRNA   VRayMesh;

    if(ob->dup_group) {
        gobject= (GroupObject*)ob->dup_group->gobject.first;
        while(gobject) {
            gob= gobject->ob;

            if(!in_list(*objects, (void*)gob)) {
                if(debug) {
                    printf("Group object: %s\n", gob->id.name);
                }
                append_object(sce, objects, meshes, gob,
                              active_layers, instances, check_animated, animation);
            }

            gobject= gobject->next;
        }
    }

    if(!ob->data)
        return;

    // TODO: geom_doHidden
    if(ob->restrictflag & OB_RESTRICT_RENDER)
        return;

    if(ob->type == OB_EMPTY   ||
       ob->type == OB_LAMP    ||
       ob->type == OB_CAMERA  ||
       ob->type == OB_LATTICE ||
       ob->type == OB_ARMATURE)
        return;

    if(active_layers)
        if(!(ob->lay & sce->lay))
            return;

    if(instances)
        if(in_list(*meshes, ob->data))
            return;

    if(ob->type == OB_MESH) {
        me= (Mesh*)ob->data;
        RNA_id_pointer_create(&me->id, &rna_me);
        if(RNA_struct_find_property(&rna_me, "vray")) {
            VRayMesh= RNA_pointer_get(&rna_me, "vray");
            if(RNA_struct_find_property(&VRayMesh, "override")) {
                if(RNA_boolean_get(&VRayMesh, "override"))
                    return;
            }
        }
    }

    if(animation)
        if(check_animated)
            if(!mesh_animated(ob))
                return;

    if(instances)
        BLI_linklist_prepend(meshes, ob->data);

    BLI_linklist_prepend(objects, ob);
}


static void export_meshes_threaded(char *filepath, Scene *sce, Main *bmain,
                                   int active_layers, int instances, int check_animated, int animation)
{
    Base     *base;
    Object   *ob;

    Material *ma;
    MTex     *mtex;
    Tex      *tex;

    pthread_t threads[MAX_MESH_THREADS];
    int       threads_count = 1;
    int       _threadsCount = 0;
    int       t;

    UVLayer  *uv_layer;
    LinkNode *uvs= NULL;
    int       uv_id= 0;

    LinkNode *list_iter= NULL;
    int       i;

    LinkNode *objects= NULL;
    LinkNode *objects_iter;
    LinkNode *meshes= NULL;

    PointerRNA rna_tex;
    PointerRNA rna_scene;

    PointerRNA VRayScene;
    PointerRNA VRayExporter;
    PointerRNA VRayTexture;

    if(sce->r.mode & R_FIXED_THREADS) {
        threads_count = sce->r.threads;
    }
    else {
        threads_count = BLI_system_thread_count();
    }

    // Get mesh exporter threads cout override from RNA
    RNA_id_pointer_create(&sce->id, &rna_scene);
    if(RNA_struct_find_property(&rna_scene, "vray")) {
        VRayScene = RNA_pointer_get(&rna_scene, "vray");

        if(RNA_struct_find_property(&VRayScene, "exporter")) {
            VRayExporter = RNA_pointer_get(&VRayScene, "exporter");

            _threadsCount = RNA_int_get(&VRayExporter, "meshExportThreads");
            if(_threadsCount) {
                threads_count = _threadsCount;
            }
        }
    }

    if(threads_count > MAX_MESH_THREADS)
        threads_count = MAX_MESH_THREADS;

    /*
      XXX: Remove this! Preprocess textures to find proper UV channel indexes
    */
    for(ma= bmain->mat.first; ma; ma= ma->id.next) {
        if(debug) {
            printf("Material: %s\n", ma->id.name);
        }
        for(i= 0; i < MAX_MTEX; ++i) {
            if(ma->mtex) {
                mtex= ma->mtex[i];
                if(mtex) {
                    tex= mtex->tex;
                    if(tex) {
                        RNA_id_pointer_create(&tex->id, &rna_tex);
                        if(RNA_struct_find_property(&rna_tex, "vray")) {
                            VRayTexture= RNA_pointer_get(&rna_tex, "vray");
                            if(RNA_enum_get(&VRayTexture, "texture_coords")) { // 0 - object; 1 - UV
                                if(debug) {
                                    printf("Texture:    %s [UV layer: %s]\n", mtex->tex->id.name, mtex->uvname);
                                }
                                if(!(strcmp(mtex->uvname, "") == 0)) {
                                    if(!uvs) {
                                        BLI_linklist_prepend(&uvs, uvlayer_ptr(mtex->uvname, ++uv_id));
                                    } else {
                                        if(!(uvlayer_in_list(uvs, mtex->uvname))) {
                                            BLI_linklist_append(&uvs, uvlayer_ptr(mtex->uvname, ++uv_id));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(debug) {
        list_iter= uvs;
        while(list_iter) {
            uv_layer= list_iter->link;
            if(uv_layer) {
                printf("UV.name= %s\n", uv_layer->name);
                printf("UV.id= %i\n", uv_layer->id);
            }
            list_iter= list_iter->next;
        }
    }

    /*
      Init thread data
    */
    for(t= 0; t < MAX_MESH_THREADS; ++t) {
        thread_data[t].sce= sce;
        thread_data[t].bmain= bmain;
        thread_data[t].id= t;
        thread_data[t].objects= NULL;
        thread_data[t].uvs= uvs;
        thread_data[t].filepath= filepath;
        thread_data[t].animation= animation;
        thread_data[t].instances= instances;
    }

    /*
      Collect objects
    */
    base= (Base*)sce->base.first;
    while(base) {
        ob= base->object;

        append_object(sce, &objects, &meshes, ob, active_layers, instances, check_animated, animation);

        base= base->next;
    }

    if(debug) {
        printf("Object list\n");
        objects_iter= objects;
        while(objects_iter) {
            ob= (Object*)objects_iter->link;

            printf("Object: %s\n", ob->id.name);

            objects_iter= objects_iter->next;
        }
    }

    /*
      Split object list to multiple lists
    */
    t = 0;
    objects_iter= objects;
    while(objects_iter) {
        ob = (Object*)objects_iter->link;

        BLI_linklist_prepend(&(thread_data[t].objects), ob);

        // TODO [LOW]: improve balancing using list sorting with ob->derivedFinal->numVertData
        if(t < threads_count - 1)
            t++;
        else
            t = 0;

        objects_iter= objects_iter->next;
    }

    if(debug) {
        for(t = 0; t < threads_count; ++t) {
            if(BLI_linklist_length(thread_data[t].objects)) {
                printf("Objects [%i]\n", t);
                list_iter = thread_data[t].objects;
                while(list_iter) {
                    ob = list_iter->link;
                    if(ob) {
                        printf("  %s\n", ob->id.name);
                    }
                    list_iter = list_iter->next;
                }
            }
        }
    }

    for(t = 0; t < threads_count; ++t) {
        pthread_create(&threads[t], NULL, export_meshes_thread, (void*) &thread_data[t]);
    }

    for(t = 0; t < threads_count; ++t) {
        pthread_join(threads[t], NULL);
    }

    for(t = 0; t < MAX_MESH_THREADS; ++t) {
        BLI_linklist_free(thread_data[t].objects, NULL);
    }

    BLI_linklist_free(uvs,     NULL);
    BLI_linklist_free(meshes,  NULL);
    BLI_linklist_free(objects, NULL);

    return;
}


static int export_scene_exec(bContext *C, wmOperator *op)
{
    Main   *bmain = CTX_data_main(C);

    Scene  *sce     = NULL;
    char   *sce_ptr = NULL;

    int     fra  = 0;
    int     cfra = 0;

    char   *filepath       = NULL;
    int     active_layers  = 0;
    int     animation      = 0;
    int     check_animated = 0;
    int     instances      = 0;

    double  time;
    double  frame_time;
    char    time_str[32];

    if(RNA_struct_property_is_set(op->ptr, "scene")) {
#if USE_STRING_POINTER
        sce_ptr = (char*)malloc(32 * sizeof(char));
        RNA_string_get(op->ptr, "scene", sce_ptr);
        sce = atol(sce_ptr);
        free(sce_ptr);
#else
        sce = RNA_int_get(op->ptr, "scene");
#endif
    }

    if(!sce) {
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
        check_animated = RNA_boolean_get(op->ptr, "check_animated");
    }

    if(RNA_struct_property_is_set(op->ptr, "debug")) {
        debug = RNA_boolean_get(op->ptr, "debug");
    }

    time = PIL_check_seconds_timer();

    if(filepath) {
        printf("V-Ray/Blender: Exporting meshes...\n");

        if(animation) {
            cfra = sce->r.cfra;
            fra  = sce->r.sfra;

            printf("V-Ray/Blender: Exporting meshes for the first frame %-32i\n", fra);

            /* Export meshes for the start frame */
            sce->r.cfra = fra;
            CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);
            BKE_scene_update_for_newframe(bmain, sce, (1<<20) - 1);
            export_meshes_threaded(filepath, sce, bmain, active_layers, instances, 0, 0);
            fra += sce->r.frame_step;

            /* Export meshes for the rest frames */
            while(fra <= sce->r.efra) {
                frame_time = PIL_check_seconds_timer();

                if(debug) {
                    printf("V-Ray/Blender: Exporting meshes for frame %-32i\n", fra);
                } else {
                    printf("V-Ray/Blender: Exporting meshes for frame %i...", fra);
                    fflush(stdout);
                }

                sce->r.cfra = fra;
                CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);
                BKE_scene_update_for_newframe(bmain, sce, (1<<20) - 1);
                export_meshes_threaded(filepath, sce, bmain, active_layers, instances, check_animated, 1);

                if(!debug) {
                    BLI_timestr(PIL_check_seconds_timer()-frame_time, time_str, sizeof(time_str));
                    printf(" done [%s]\n", time_str);
                }

                fra += sce->r.frame_step;
            }

            sce->r.cfra = cfra;
            CLAMP(sce->r.cfra, MINAFRAME, MAXFRAME);
            BKE_scene_update_for_newframe(bmain, sce, (1<<20) - 1);
        } else {
            export_meshes_threaded(filepath, sce, bmain, active_layers, instances, check_animated, 0);
        }

        BLI_timestr(PIL_check_seconds_timer()-time, time_str, sizeof(time_str));
        printf("V-Ray/Blender: Exporting meshes done [%s]%-32s\n", time_str, " ");

        free(filepath);

        return OPERATOR_FINISHED;
    }

    return OPERATOR_CANCELLED;
}


static int export_scene_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
     return OPERATOR_RUNNING_MODAL;
}


static int export_scene_modal(bContext *C, wmOperator *op, wmEvent *event)
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
    ot->invoke = export_scene_invoke;
    ot->modal  = export_scene_modal;
    ot->exec   = export_scene_exec;

#if USE_STRING_POINTER
    RNA_def_string(ot->srna, "scene", "", 32, "Scene", "Scene pointer");
#else
    RNA_def_int(ot->srna, "scene", 0, INT_MIN, INT_MAX, "Scene", "Scene pointer", INT_MIN, INT_MAX);
#endif

    RNA_def_string(ot->srna, "filepath", "", FILE_MAX, "Geometry filepath", "Geometry filepath");

    RNA_def_boolean(ot->srna, "use_active_layers", 0,  "Active layer",      "Export only active layers");
    RNA_def_boolean(ot->srna, "use_animation",     0,  "Animation",         "Export animation");
    RNA_def_boolean(ot->srna, "use_instances",     0,  "Instances",         "Use instances");
    RNA_def_boolean(ot->srna, "debug",             0,  "Debug",             "Debug mode");
    RNA_def_boolean(ot->srna, "check_animated",    0,  "Check animated",    "Try to detect if mesh is animated");
}
