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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_buttons.c
 *  \ingroup eduv
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"
#include "BKE_tessmesh.h"

#include "ED_image.h"
#include "ED_uvedit.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#define B_UVEDIT_VERTEX     3

/* UV Utilities */

static int uvedit_center(Scene *scene, BMEditMesh *em, Image *ima, float center[2])
{
	BMFace *f;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	int tot = 0.0;
	
	zero_v2(center);
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		tf = CustomData_bmesh_get(&em->bm->pdata, f->head.data, CD_MTEXPOLY);
		if (!uvedit_face_visible_test(scene, ima, f, tf))
			continue;

		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (uvedit_uv_select_test(em, scene, l)) {
				add_v2_v2(center, luv->uv);
				tot++;
			}
		}
	}

	if (tot > 0) {
		center[0] /= tot;
		center[1] /= tot;
	}

	return tot;
}

static void uvedit_translate(Scene *scene, BMEditMesh *em, Image *UNUSED(ima), float delta[2])
{
	BMFace *f;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	
	BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (uvedit_uv_select_test(em, scene, l)) {
				add_v2_v2(luv->uv, delta);
			}
		}
	}
}

/* Button Functions, using an evil static variable */

static float uvedit_old_center[2];

static void uvedit_vertex_buttons(const bContext *C, uiBlock *block)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = sima->image;
	BMEditMesh *em;
	float center[2];
	int imx, imy, step, digits;

	ED_space_image_get_size(sima, &imx, &imy);
	
	em = BMEdit_FromObject(obedit);

	if (uvedit_center(scene, em, ima, center)) {
		copy_v2_v2(uvedit_old_center, center);

		if (!(sima->flag & SI_COORDFLOATS)) {
			uvedit_old_center[0] *= imx;
			uvedit_old_center[1] *= imy;
		}

		if (sima->flag & SI_COORDFLOATS) {
			step = 1;
			digits = 3;
		}
		else {
			step = 100;
			digits = 2;
		}
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_UVEDIT_VERTEX, "X:",    10, 10, 145, 19, &uvedit_old_center[0], -10 * imx, 10.0 * imx, step, digits, "");
		uiDefButF(block, NUM, B_UVEDIT_VERTEX, "Y:",    165, 10, 145, 19, &uvedit_old_center[1], -10 * imy, 10.0 * imy, step, digits, "");
		uiBlockEndAlign(block);
	}
}

static void do_uvedit_vertex(bContext *C, void *UNUSED(arg), int event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Image *ima = sima->image;
	BMEditMesh *em;
	float center[2], delta[2];
	int imx, imy;

	if (event != B_UVEDIT_VERTEX)
		return;

	em = BMEdit_FromObject(obedit);

	ED_space_image_get_size(sima, &imx, &imy);
	uvedit_center(scene, em, ima, center);

	if (sima->flag & SI_COORDFLOATS) {
		delta[0] = uvedit_old_center[0] - center[0];
		delta[1] = uvedit_old_center[1] - center[1];
	}
	else {
		delta[0] = uvedit_old_center[0] / imx - center[0];
		delta[1] = uvedit_old_center[1] / imy - center[1];
	}

	uvedit_translate(scene, em, ima, delta);

	WM_event_add_notifier(C, NC_IMAGE, sima->image);
}

/* Panels */

static int image_panel_uv_poll(const bContext *C, PanelType *UNUSED(pt))
{
	Object *obedit = CTX_data_edit_object(C);
	return ED_uvedit_test(obedit);
}

static void image_panel_uv(const bContext *C, Panel *pa)
{
	uiBlock *block;
	
	block = uiLayoutAbsoluteBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_uvedit_vertex, NULL);

	uvedit_vertex_buttons(C, block);
}	

void ED_uvedit_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype image panel uv");
	strcpy(pt->idname, "IMAGE_PT_uv");
	strcpy(pt->label, "UV Vertex");
	pt->draw = image_panel_uv;
	pt->poll = image_panel_uv_poll;
	BLI_addtail(&art->paneltypes, pt);
}

