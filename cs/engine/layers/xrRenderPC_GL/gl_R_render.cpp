#include "stdafx.h"
#include "../../xrEngine/igame_persistent.h"
#include "../xrRender/FBasicVisual.h"
#include "../../xrEngine/customhud.h"
#include "../../xrEngine/xr_object.h"

IC	bool	pred_sp_sort	(ISpatial*	_1, ISpatial* _2)
{
	float	d1		= _1->spatial.sphere.P.distance_to_sqr	(Device.vCameraPosition);
	float	d2		= _2->spatial.sphere.P.distance_to_sqr	(Device.vCameraPosition);
	return	d1<d2	;
}

void CRender::render_main	(Fmatrix&	m_ViewProjection, bool _fportals)
{
//	Msg						("---begin");
	marker					++;

	// Calculate sector(s) and their objects
	if (pLastSector)		{
		//!!!
		//!!! BECAUSE OF PARALLEL HOM RENDERING TRY TO DELAY ACCESS TO HOM AS MUCH AS POSSIBLE
		//!!!
		{
			// Traverse object database
			g_SpatialSpace->q_frustum
				(
				lstRenderables,
				ISpatial_DB::O_ORDERED,
				STYPE_RENDERABLE + STYPE_LIGHTSOURCE,
				ViewBase
				);

			// (almost) Exact sorting order (front-to-back)
			std::sort			(lstRenderables.begin(),lstRenderables.end(),pred_sp_sort);

			// Determine visibility for dynamic part of scene
			set_Object							(0);
			u32 uID_LTRACK						= 0xffffffff;
			if (phase==PHASE_NORMAL)			{
				uLastLTRACK	++;
				if (lstRenderables.size())		uID_LTRACK	= uLastLTRACK%lstRenderables.size();

				// update light-vis for current entity / actor
				CObject*	O					= g_pGameLevel->CurrentViewEntity();
				if (O)		{
					CROS_impl*	R					= (CROS_impl*) O->ROS();
					if (R)		R->update			(O);
				}

				// update light-vis for selected entity
				// track lighting environment
				if (lstRenderables.size())		{
					IRenderable*	renderable		= lstRenderables[uID_LTRACK]->dcast_Renderable	();
					if (renderable)	{
						CROS_impl*		T = (CROS_impl*)renderable->renderable_ROS	();
						if (T)			T->update	(renderable);
					}
				}
			}
		}

		// Traverse sector/portal structure
		PortalTraverser.traverse	
			(
			pLastSector,
			ViewBase,
			Device.vCameraPosition,
			m_ViewProjection,
			CPortalTraverser::VQ_HOM + CPortalTraverser::VQ_SSA + CPortalTraverser::VQ_FADE
			//. disabled scissoring (HW.Caps.bScissor?CPortalTraverser::VQ_SCISSOR:0)	// generate scissoring info
			);

		// Determine visibility for static geometry hierrarhy
		for (u32 s_it=0; s_it<PortalTraverser.r_sectors.size(); s_it++)
		{
			CSector*	sector		= (CSector*)PortalTraverser.r_sectors[s_it];
			dxRender_Visual*	root	= sector->root();
			for (u32 v_it=0; v_it<sector->r_frustums.size(); v_it++)	{
				set_Frustum			(&(sector->r_frustums[v_it]));
				add_Geometry		(root);
			}
		}

		// Traverse frustums
		for (u32 o_it=0; o_it<lstRenderables.size(); o_it++)
		{
			ISpatial*	spatial		= lstRenderables[o_it];		spatial->spatial_updatesector	();
			CSector*	sector		= (CSector*)spatial->spatial.sector;
			if	(0==sector)										continue;	// disassociated from S/P structure

			if (spatial->spatial.type & STYPE_LIGHTSOURCE)		{
				// lightsource
				light*			L				= (light*)	(spatial->dcast_Light());
				VERIFY							(L);
				float	lod		= L->get_LOD	();
				if (lod>EPS_L)	{
					vis_data&		vis		= L->get_homdata	( );
					if	(HOM.visible(vis))	Lights.add_light	(L);
				}
				continue					;
			}

			if	(PortalTraverser.i_marker != sector->r_marker)	continue;	// inactive (untouched) sector
			for (u32 v_it=0; v_it<sector->r_frustums.size(); v_it++)	{
				CFrustum&	view	= sector->r_frustums[v_it];
				if (!view.testSphere_dirty(spatial->spatial.sphere.P,spatial->spatial.sphere.R))	continue;

				if (spatial->spatial.type & STYPE_RENDERABLE)
				{
					// renderable
					IRenderable*	renderable		= spatial->dcast_Renderable	();
					VERIFY							(renderable);

					// Occlusion
					//	casting is faster then using getVis method
					vis_data&		v_orig			= ((dxRender_Visual*)renderable->renderable.visual)->vis;
					vis_data		v_copy			= v_orig;
					v_copy.box.xform				(renderable->renderable.xform);
					BOOL			bVisible		= HOM.visible(v_copy);
					v_orig.marker					= v_copy.marker;
					v_orig.accept_frame				= v_copy.accept_frame;
					v_orig.hom_frame				= v_copy.hom_frame;
					v_orig.hom_tested				= v_copy.hom_tested;
					if (!bVisible)					break;	// exit loop on frustums

					// Rendering
					set_Object						(renderable);
					renderable->renderable_Render	();
					set_Object						(0);
				}
				break;	// exit loop on frustums
			}
		}
		if (g_pGameLevel && (phase==PHASE_NORMAL))	g_pGameLevel->pHUD->Render_Last();		// HUD
	}
	else
	{
		set_Object									(0);
		if (g_pGameLevel && (phase==PHASE_NORMAL))	g_pGameLevel->pHUD->Render_Last();		// HUD
	}
}

void CRender::render_menu	()
{
	//	Globals
	RCache.set_CullMode				(CULL_CCW);
	RCache.set_Stencil				(FALSE);
	RCache.set_ColorWriteEnable		();

	// Main Render
	{
		Target->u_setrt						(Target->rt_Generic_0,0,0,RCache.pBaseZB);		// LDR RT
		g_pGamePersistent->OnRenderPPUI_main()	;	// PP-UI
	}
	// Distort
	{
		Target->u_setrt						(Target->rt_Generic_1,0,0,RCache.pBaseZB);		// Now RT is a distortion mask
		glClearColor						(127.0f/255.0f, 127.0f/255.0f, 0.0f, 127.0f/255.0f);
		CHK_GL								(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		g_pGamePersistent->OnRenderPPUI_PP	()	;	// PP-UI
	}

	// Actual Display
	RCache.set_FB(0);
	RCache.set_Shader				( Target->s_menu	);
	RCache.set_Geometry				( Target->g_menu	);

	Fvector2						p0,p1;
	u32								Offset;
	u32		C						= color_rgba	(255,255,255,255);
	float	_w						= float(Device.dwWidth);
	float	_h						= float(Device.dwHeight);
	float	d_Z						= EPS_S;
	float	d_W						= 1.f;
	p0.set							(.5f/_w, .5f/_h);
	p1.set							((_w+.5f)/_w, (_h+.5f)/_h );

	FVF::TL* pv						= (FVF::TL*) RCache.Vertex.Lock	(4,Target->g_menu->vb_stride,Offset);
	pv->set							(EPS,			float(_h+EPS),	d_Z,	d_W, C, p0.x, p0.y);	pv++;
	pv->set							(EPS,			EPS,			d_Z,	d_W, C, p0.x, p1.y);	pv++;
	pv->set							(float(_w+EPS),	float(_h+EPS),	d_Z,	d_W, C, p1.x, p0.y);	pv++;
	pv->set							(float(_w+EPS),	EPS,			d_Z,	d_W, C, p1.x, p1.y);	pv++;
	RCache.Vertex.Unlock			(4,Target->g_menu->vb_stride);
	RCache.Render					(D3DPT_TRIANGLELIST,Offset,0,4,0,2);
}

extern u32 g_r;
void CRender::Render		()
{
	g_r						= 1;
	VERIFY					(0==mapDistort.size());

	bool	_menu_pp		= g_pGamePersistent?g_pGamePersistent->OnRenderPPUI_query():false;
	if (_menu_pp)			{
		render_menu			()	;
		return					;
	};

	IMainMenu*	pMainMenu = g_pGamePersistent?g_pGamePersistent->m_pMainMenu:0;
	bool	bMenu = pMainMenu?pMainMenu->CanSkipSceneRendering():false;

	if( !(g_pGameLevel && g_pGameLevel->pHUD) || bMenu)	return;
//.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);

	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	// Begin
	//Target->Begin();
	//o.vis_intersect = FALSE;
	phase = PHASE_NORMAL;
	r_dsgraph_render_hud();				// hud
	r_dsgraph_render_graph(0);			// normal level
	if (Details)Details->Render();				// grass / details
	r_dsgraph_render_lods(true, false);	// lods - FB

	//g_pGamePersistent->Environment().RenderSky();				// sky / sun
	//g_pGamePersistent->Environment().RenderClouds();				// clouds

	r_pmask(true, false);	// disable priority "1"
	//o.vis_intersect = TRUE;
	HOM.Disable();
	//L_Dynamic->render();				// addititional light sources
	if (Wallmarks){
		g_r = 0;
		Wallmarks->Render();				// wallmarks has priority as normal geometry
	}
	HOM.Enable();
	//o.vis_intersect = FALSE;
	phase = PHASE_NORMAL;
	r_pmask(true, true);	// enable priority "0" and "1"
	//if (L_Shadows)L_Shadows->render();				// ... and shadows
	r_dsgraph_render_lods(false, true);	// lods - FB
	r_dsgraph_render_graph(1);			// normal level, secondary priority
	PortalTraverser.fade_render();				// faded-portals
	r_dsgraph_render_sorted();				// strict-sorted geoms
	//if (L_Glows)L_Glows->Render();				// glows
	//g_pGamePersistent->Environment().RenderFlares();				// lens-flares
	//g_pGamePersistent->Environment().RenderLast();				// rain/thunder-bolts

	// Postprocess, if necessary
	//Target->End();
	//if (L_Projector) L_Projector->finalize();
}

void CRender::render_forward				()
{
	VERIFY	(0==mapDistort.size());
	RImplementation.o.distortion				= RImplementation.o.distortion_enabled;	// enable distorion

	//******* Main render - second order geometry (the one, that doesn't support deffering)
	//.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
	{
		// level
		r_pmask									(false,true);			// enable priority "1"
		phase									= PHASE_NORMAL;
		render_main								(Device.mFullTransform,false);//
		//	Igor: we don't want to render old lods on next frame.
		mapLOD.clear							();
		r_dsgraph_render_graph					(1)	;					// normal level, secondary priority
		PortalTraverser.fade_render				()	;					// faded-portals
		r_dsgraph_render_sorted					()	;					// strict-sorted geoms
		g_pGamePersistent->Environment().RenderLast()	;					// rain/thunder-bolts
	}

	RImplementation.o.distortion				= FALSE;				// disable distorion
}
