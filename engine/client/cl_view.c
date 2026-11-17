/*
cl_view.c - player rendering positioning
Copyright (C) 2009 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "const.h"
#include "entity_types.h"
#include "vgui_draw.h"
#include "sound.h"
#include "input.h" // touch
#include "platform/platform.h" // GL_UpdateSwapInterval

/*
===============
V_CalcViewRect

calc frame rectangle (Quake1 style)
===============
*/
static void V_CalcViewRect( void )
{
	qboolean	full = false;
	int	sb_lines;
	float	size;

	if( Host_IsQuakeCompatible( ))
	{
		// intermission is always full screen
		if( cl.intermission ) size = 120.0f;
		else size = scr_viewsize.value;

		if( size >= 120.0f )
			sb_lines = 0;		// no status bar at all
		else if( size >= 110.0f )
			sb_lines = 24;		// no inventory
		else sb_lines = 48;

		if( scr_viewsize.value >= 100.0f )
		{
			full = true;
			size = 100.0f;
		}
		else size = scr_viewsize.value;

		if( cl.intermission )
		{
			size = 100.0f;
			sb_lines = 0;
			full = true;
		}
		size /= 100.0f;
	}
	else
	{
		full = true;
		sb_lines = 0;
		size = 1.0f;
	}

	clgame.viewport[2] = refState.width * size;
	clgame.viewport[3] = refState.height * size;

	if( clgame.viewport[3] > refState.height - sb_lines )
		clgame.viewport[3] = refState.height - sb_lines;
	if( clgame.viewport[3] > refState.height )
		clgame.viewport[3] = refState.height;

	clgame.viewport[0] = ( refState.width - clgame.viewport[2] ) / 2;
	if( full ) clgame.viewport[1] = 0;
	else clgame.viewport[1] = ( refState.height - sb_lines - clgame.viewport[3] ) / 2;

}

/*
===============
V_SetupViewModel
===============
*/
static void V_SetupViewModel( void )
{
	cl_entity_t	*view = &clgame.viewent;
	player_info_t	*info = &cl.players[cl.playernum];

	if( !cl.local.weaponstarttime )
		cl.local.weaponstarttime = cl.time;

	// setup the viewent variables
	view->curstate.colormap = (info->topcolor & 0xFFFF)|((info->bottomcolor << 8) & 0xFFFF);
	view->curstate.number = cl.playernum + 1;
	view->index = cl.playernum + 1;
	view->model = CL_ModelHandle( cl.local.viewmodel );
	view->curstate.modelindex = cl.local.viewmodel;
	view->curstate.sequence = cl.local.weaponsequence;
	view->curstate.rendermode = kRenderNormal;

	// alias models has another animation methods
	if( view->model && view->model->type == mod_studio )
	{
		view->curstate.animtime = cl.local.weaponstarttime;
		view->curstate.frame = 0.0f;
	}
}

/*
===============
V_SetRefParams
===============
*/
static void V_SetRefParams( ref_params_t *fd )
{
	memset( fd, 0, sizeof( ref_params_t ));

	VectorCopy( refState.vieworg, fd->vieworg );
	VectorCopy( refState.viewangles, fd->viewangles );

	// Compute view vectors from angles each frame
	AngleVectors( fd->viewangles, fd->forward, fd->right, fd->up );

	fd->frametime = host.frametime;
	fd->time = cl.time;

	fd->intermission = cl.intermission;
	fd->paused = (cl.paused != 0);
	fd->spectator = (cls.spectator != 0);
	fd->onground = (cl.local.onground != -1);
	fd->waterlevel = cl.local.waterlevel;

	VectorCopy( cl.simvel, fd->simvel );
	VectorCopy( cl.simorg, fd->simorg );

	VectorCopy( cl.viewheight, fd->viewheight );
	fd->idealpitch = cl.local.idealpitch;

	VectorCopy( cl.viewangles, fd->cl_viewangles );
	fd->health = cl.local.health;
	VectorCopy( cl.crosshairangle, fd->crosshairangle );
	if( Host_IsQuakeCompatible( ))
		fd->viewsize = scr_viewsize.value;
	else fd->viewsize = 120.0f;

	VectorCopy( cl.punchangle, fd->punchangle );
	fd->maxclients = cl.maxclients;
	fd->viewentity = cl.viewentity;
	fd->playernum = cl.playernum;
	fd->max_entities = clgame.maxEntities;
	fd->demoplayback = cls.demoplayback;
	fd->hardware = 1; // OpenGL

	if( cl.first_frame || cl.skip_interp )
	{
		cl.first_frame = false;		// now can be unlocked
		fd->smoothing = true;		// NOTE: currently this used to prevent ugly un-duck effect while level is changed
	}
	else fd->smoothing = cl.local.pushmsec;		// enable smoothing in multiplayer by server request (AMX uses)

	// get pointers to movement vars and user cmd
	fd->movevars = &clgame.movevars;
	fd->cmd = &cl.cmd;

	// setup viewport
	fd->viewport[0] = clgame.viewport[0];
	fd->viewport[1] = clgame.viewport[1];
	fd->viewport[2] = clgame.viewport[2];
	fd->viewport[3] = clgame.viewport[3];

	fd->onlyClientDraw = 0;	// reset clientdraw
	fd->nextView = 0;		// reset nextview
}

/*
===============
V_MergeOverviewRefdef

merge refdef with overview settings
===============
*/
static void V_RefApplyOverview( ref_viewpass_t *rvp )
{
	ref_overview_t	*ov = &clgame.overView;

	if( !CL_IsDevOverviewMode( ))
		return;

	// NOTE: Xash3D may use 16:9 or 16:10 aspects
	float aspect = (float)refState.width / (float)refState.height;

	float size_x = fabs( 8192.0f / ov->flZoom );
	float size_y = fabs( 8192.0f / (ov->flZoom * aspect ));

	// compute rectangle
	ov->xLeft = -(size_x / 2);
	ov->xRight = (size_x / 2);
	ov->yTop = -(size_y / 2);
	ov->yBottom = (size_y / 2);

	if( CL_IsDevOverviewMode() == 1 )
	{
		Con_NPrintf( 0, " Overview: Zoom %.2f, Map Origin (%.2f, %.2f, %.2f), Z Min %.2f, Z Max %.2f, Rotated %i\n",
		ov->flZoom, ov->origin[0], ov->origin[1], ov->origin[2], ov->zNear, ov->zFar, ov->rotated );
	}

	VectorCopy( ov->origin, rvp->vieworigin );
	rvp->vieworigin[2] = ov->zFar + ov->zNear;
	vec2_t mins = Vec2( rvp->vieworigin );
	vec2_t maxs = Vec2( rvp->vieworigin );

	mins[!ov->rotated] += ov->xLeft;
	maxs[!ov->rotated] += ov->xRight;
	mins[ov->rotated] += ov->yTop;
	maxs[ov->rotated] += ov->yBottom;

	rvp->viewangles[0] = 90.0f;
	rvp->viewangles[1] = 90.0f;
	rvp->viewangles[2] = (ov->rotated) ? (ov->flZoom < 0.0f) ? 180.0f : 0.0f : (ov->flZoom < 0.0f) ? -90.0f : 90.0f;

	SetBits( rvp->flags, RF_DRAW_OVERVIEW );

	ref.dllFuncs.GL_OrthoBounds( mins, maxs );
}

/*
====================
V_CalcFov
====================
*/
static float V_CalcFov( float *fov_x, float width, float height )
{
	if( *fov_x < 1.0f || *fov_x > 179.0f )
		*fov_x = 90.0f; // default value

	float x = width / tan( DEG2RAD( *fov_x ) * 0.5f );
	float half_fov_y = atan( height / x );

	return RAD2DEG( half_fov_y ) * 2;
}

/*
====================
V_AdjustFov
====================
*/
static void V_AdjustFov( float *fov_x, float *fov_y, float width, float height, qboolean lock_x )
{
	float x, y;

	if( width * 3 == 4 * height || width * 4 == height * 5 )
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if( lock_x )
	{
		*fov_y = 2 * atan((width * 3) / (height * 4) * tan( *fov_y * M_PI_F / 360.0f * 0.5f )) * 360 / M_PI_F;
		return;
	}

	y = V_CalcFov( fov_x, 640, 480 );
	x = *fov_x;

	*fov_x = V_CalcFov( &y, height, width );
	if( *fov_x < x ) *fov_x = x;
	else *fov_y = y;
}

/*
=============
V_GetRefParams
=============
*/
static void V_GetRefParams( ref_params_t *fd, ref_viewpass_t *rvp )
{
	// part1: deniable updates
	VectorCopy( fd->simvel, cl.simvel );
	VectorCopy( fd->simorg, cl.simorg );
	VectorCopy( fd->punchangle, cl.punchangle );
	VectorCopy( fd->viewheight, cl.viewheight );

	// part2: really used updates
	VectorCopy( fd->crosshairangle, cl.crosshairangle );
	VectorCopy( fd->cl_viewangles, cl.viewangles );

	// setup ref_viewpass
	rvp->viewport[0] = fd->viewport[0];
	rvp->viewport[1] = fd->viewport[1];
	rvp->viewport[2] = fd->viewport[2];
	rvp->viewport[3] = fd->viewport[3];

	VectorCopy( fd->vieworg, rvp->vieworigin );
	VectorCopy( fd->viewangles, rvp->viewangles );

	rvp->viewentity = fd->viewentity;

	// calc FOV
	rvp->fov_x = bound( 10.0f, cl.local.scr_fov, 150.0f ); // this is a final fov value

	// first we need to compute FOV and other things that needs for frustum properly work
	rvp->fov_y = V_CalcFov( &rvp->fov_x, clgame.viewport[2], clgame.viewport[3] );

	// adjust FOV for widescreen
	if( refState.wideScreen && r_adjust_fov.value )
		V_AdjustFov( &rvp->fov_x, &rvp->fov_y, clgame.viewport[2], clgame.viewport[3], false );

	rvp->flags = 0;

	if( fd->onlyClientDraw )
		SetBits( rvp->flags, RF_ONLY_CLIENTDRAW );
	SetBits( rvp->flags, RF_DRAW_WORLD );
}

/*
==================
V_PreRender

==================
*/
qboolean V_PreRender( void )
{
	// too early
	if( !ref.initialized )
		return false;

	if( host.status == HOST_SLEEP )
		return false;

	// if the screen is disabled (loading plaque is up)
	if( cls.disable_screen )
	{
		if(( host.realtime - cls.disable_screen ) > cl_timeout.value )
		{
			Con_Reportf( "%s: loading plaque timed out\n", __func__ );
			cls.disable_screen = 0.0f;
		}
		return false;
	}

	V_CheckGamma();

	ref.dllFuncs.R_BeginFrame( !cl.paused && ( cls.state == ca_active ));

	GL_UpdateSwapInterval( );

	return true;
}

//============================================================================

/*
====================
V_ApplyRefUnderwater

apply underwater fov effect
====================
*/
static void V_ApplyRefUnderwater( ref_viewpass_t *rvp )
{
	if( !FBitSet( rvp->flags, RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW ))
		return;

	if( !FBitSet( host.features, ENGINE_QUAKE_COMPATIBLE ))
		return;

	if( cl.local.waterlevel < 3 )
		return;

	rvp->fov_x = atan( tan( DEG2RAD( rvp->fov_x ) / 2 ) * ( 0.97f + sin( cl.time * 1.5f ) * 0.03f )) * 2 / (M_PI_F / 180.0f);
	rvp->fov_y = atan( tan( DEG2RAD( rvp->fov_y ) / 2 ) * ( 1.03f - sin( cl.time * 1.5f ) * 0.03f )) * 2 / (M_PI_F / 180.0f);

}

/*
==================
V_RenderView

==================
*/
#include "studio.h"

int g_ps4_active_viewnum = 0;
extern int SV_GetSplitScreenCount( void );
extern int SV_GetSplitPlayerTeamStage( int viewnum, int *out_team_id );
extern void CL_ForceCS16ShowMenu( int menu_stage, int team_id );
extern qboolean SV_GetSplitScreenViewEx( int viewnum, vec3_t out_org, vec3_t out_ang, vec3_t out_punch, int *out_entnum, int *out_health, const char **out_viewmodel, int *out_vm_seq, int *out_vm_ev, int *out_shoot_ev );
extern qboolean SV_GetSplitPlayerEntityState( int viewnum, int *out_entnum, vec3_t out_body_org, vec3_t out_body_ang, int *out_modelindex, int *out_seq, int *out_gaitseq, float *out_frame, int *out_weaponmodel, byte *out_blend0, int *out_flags );

static int CL_FindWeaponSequence( model_t *vmdl, const char *keyword, qboolean unsilenced, float *out_duration )
{
	if( out_duration ) *out_duration = 0.5f;
	if( !vmdl || !vmdl->cache.data ) return -1;
	studiohdr_t *phdr = (studiohdr_t *)vmdl->cache.data;
	if( phdr->numseq <= 0 ) return -1;
	mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)( (byte *)phdr + phdr->seqindex );

	for( int pass = 0; pass < 2; pass++ )
	{
		for( int i = 0; i < phdr->numseq; i++ )
		{
			const char *lbl = pseq[i].label;
			if( !Q_stristr( lbl, keyword ) )
				continue;
			if( Q_stristr( lbl, "empty" ) && !Q_stristr( keyword, "empty" ) )
				continue;
			qboolean has_unsil = ( Q_stristr( lbl, "unsil" ) != NULL );
			if( pass == 0 && has_unsil != unsilenced )
				continue;
			if( out_duration )
				*out_duration = ( pseq[i].fps > 0.0f ) ? ( (float)pseq[i].numframes / pseq[i].fps ) : 0.4f;
			return i;
		}
	}
	return -1;
}

static int CL_FindWeaponShootSequence( model_t *vmdl, qboolean unsilenced, int shot_counter, float *out_duration )
{
	int matches[8];
	int match_count = 0;
	float durations[8];

	if( out_duration ) *out_duration = 0.25f;
	if( !vmdl || !vmdl->cache.data ) return 1;
	studiohdr_t *phdr = (studiohdr_t *)vmdl->cache.data;
	if( phdr->numseq <= 0 ) return 0;
	mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)( (byte *)phdr + phdr->seqindex );
	qboolean is_glock = ( Q_stristr( vmdl->name, "glock18" ) != NULL );
	qboolean is_knife = ( Q_stristr( vmdl->name, "knife" ) != NULL );

	for( int pass = 0; pass < 2 && match_count == 0; pass++ )
	{
		for( int i = 0; i < phdr->numseq && match_count < 8; i++ )
		{
			const char *lbl = pseq[i].label;
			if( Q_stristr( lbl, "empty" ) || Q_stristr( lbl, "last" ) || Q_stristr( lbl, "idle" ) || Q_stristr( lbl, "reload" ) || Q_stristr( lbl, "draw" ) )
				continue;
			// En v_glock18.mdl, "shoot" y "shoot2" son las animaciones de rafaga de 3 tiros;
			// el disparo normal semiautomatico es exclusivamente "shoot3"
			if( is_glock && ( !Q_stricmp( lbl, "shoot" ) || !Q_stricmp( lbl, "shoot2" ) ) )
				continue;
			// En v_knife.mdl, usar slash para ataque primario
			if( is_knife && Q_stristr( lbl, "stab" ) )
				continue;

			qboolean has_unsil = ( Q_stristr( lbl, "unsil" ) != NULL );
			if( pass == 0 && has_unsil != unsilenced )
				continue;
			if( Q_stristr( lbl, "shoot" ) || Q_stristr( lbl, "fire" ) || Q_stristr( lbl, "slash" ) )
			{
				matches[match_count] = i;
				float nframes = ( pseq[i].numframes > 1 ) ? ( (float)( pseq[i].numframes - 1 ) * 0.88f ) : 1.0f;
				durations[match_count] = ( pseq[i].fps > 0.0f ) ? ( nframes / pseq[i].fps ) : 0.25f;
				match_count++;
			}
		}
	}

	if( match_count > 0 )
	{
		int pick = ( shot_counter >= 0 ) ? ( shot_counter % match_count ) : 0;
		if( out_duration ) *out_duration = durations[pick];
		return matches[pick];
	}
	return ( phdr->numseq > 1 ) ? 1 : 0;
}

static float CL_GetWeaponSeqInfo( model_t *vmdl, int seq, qboolean *out_is_unsil, qboolean *out_is_idle )
{
	if( out_is_idle ) *out_is_idle = false;
	if( !vmdl || !vmdl->cache.data ) return 0.5f;
	studiohdr_t *phdr = (studiohdr_t *)vmdl->cache.data;
	if( seq < 0 || seq >= phdr->numseq ) return 0.5f;
	mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)( (byte *)phdr + phdr->seqindex );
	const char *lbl = pseq[seq].label;
	if( out_is_unsil )
	{
		if( Q_stristr( lbl, "unsil" ) || Q_stristr( lbl, "detach" ) )
			*out_is_unsil = true;
		else if( Q_stristr( lbl, "add_sil" ) )
			*out_is_unsil = false;
	}
	qboolean idle = ( Q_stristr( lbl, "idle" ) != NULL );
	if( out_is_idle ) *out_is_idle = idle;
	float nframes = ( pseq[seq].numframes > 1 ) ? ( (float)( pseq[seq].numframes - 1 ) * ( idle ? 1.0f : 0.90f ) ) : 1.0f;
	return ( pseq[seq].fps > 0.0f ) ? ( nframes / pseq[seq].fps ) : 0.5f;
}


static void CL_DrawCS16ClassicCrosshair( int split_count, const vec3_t punch )
{
	float cx = (float)refState.width * 0.5f;
	float cy = (float)refState.height * 0.5f;
	float y_scale = ( split_count == 2 ) ? 2.0f : 1.0f;
	float recoil = sqrtf( punch[0] * punch[0] + punch[1] * punch[1] );
	float gap_x = 6.0f + recoil * 3.5f;
	if( gap_x > 28.0f ) gap_x = 28.0f;
	float gap_y = gap_x * y_scale;
	float len_x = 10.0f;
	float len_y = len_x * y_scale;
	float thick_x = 2.0f;
	float thick_y = 2.0f * y_scale;

	// Verde clasico de Counter-Strike 1.6 (50, 250, 50) con los 9 argumentos de FillRGBA
	ref.dllFuncs.FillRGBA( kRenderTransAlpha, cx - len_x - gap_x, cy - ( thick_y * 0.5f ), len_x, thick_y, 50, 250, 50, 230 ); // Izquierda
	ref.dllFuncs.FillRGBA( kRenderTransAlpha, cx + gap_x,         cy - ( thick_y * 0.5f ), len_x, thick_y, 50, 250, 50, 230 ); // Derecha
	ref.dllFuncs.FillRGBA( kRenderTransAlpha, cx - ( thick_x * 0.5f ), cy - len_y - gap_y, thick_x, len_y, 50, 250, 50, 230 ); // Arriba
	ref.dllFuncs.FillRGBA( kRenderTransAlpha, cx - ( thick_x * 0.5f ), cy + gap_y,         thick_x, len_y, 50, 250, 50, 230 ); // Abajo
}

void V_RenderView( void )
{
	// HACKHACK: make ref params static
	// not really critical but allows client.dll to take address of refdef and don't trigger ASan
	static ref_params_t	rp;
	ref_viewpass_t	rvp;
	int		viewnum = 0;

	if( !cl.video_prepped || ( !ui_renderworld.value && UI_IsVisible() && !cl.background ))
		return; // still loading

	V_CalcViewRect ();	// compute viewport rectangle
	V_SetRefParams( &rp );
	V_SetupViewModel ();
	ref.dllFuncs.R_Set2DMode( false );
	SCR_DirtyScreen();
	ref.dllFuncs.GL_BackendStartFrame ();

	do
	{
		int split_count = SV_GetSplitScreenCount();
		int sp_ent = 0, sp_health = 100, sp_vm_seq = 0, sp_vm_ev = 0, sp_shoot_ev = 0;
		const char *sp_viewmodel = NULL;
		vec3_t sp_org, sp_ang, sp_punch;
		qboolean sp_valid = false;

		// En viewnum == 0 (Jugador 1), ejecutar pfnCalcRefdef y V_GetRefParams de forma 100% nativa
		// En viewnum > 0 (Jugadores 2..4), NO llamar a pfnCalcRefdef ni V_GetRefParams para no
		// sobrescribir cl.punchangle, ev_punchangle ni cl.viewangles del Jugador 1
		if( viewnum == 0 )
		{
			clgame.dllFuncs.pfnCalcRefdef( &rp );

			if( clgame.viewent.model && clgame.viewent.model->type == mod_studio )
				clgame.viewent.curstate.animtime = cl.local.weaponstarttime;

			V_GetRefParams( &rp, &rvp );
			V_RefApplyOverview( &rvp );
			V_ApplyRefUnderwater( &rvp );
		}
		else
		{
			sp_valid = SV_GetSplitScreenViewEx( viewnum, sp_org, sp_ang, sp_punch, &sp_ent, &sp_health, &sp_viewmodel, &sp_vm_seq, &sp_vm_ev, &sp_shoot_ev );
			if( sp_valid )
			{
				VectorCopy( sp_org, rvp.vieworigin );
				VectorCopy( sp_ang, rvp.viewangles );
				rvp.viewentity = sp_ent;
			}
			else
			{
				VectorClear( rvp.viewangles );
			}
		}

		static int s_prev_body_seq[4] = { -1, -1, -1, -1 };
		static float s_body_animtime[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		static model_t *s_vm_mdl[4] = { NULL, NULL, NULL, NULL };
		static int s_vm_cur_seq[4] = { 0, 0, 0, 0 };
		static float s_vm_animtime[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		static float s_vm_seq_end[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		static int s_last_vm_ev[4] = { 0, 0, 0, 0 };
		static int s_last_shoot_ev[4] = { 0, 0, 0, 0 };
		static qboolean s_vm_unsil[4] = { true, true, true, true };
		static qboolean s_vm_playing_action[4] = { false, false, false, false };

		if( split_count > 1 )
		{
			int full_w = refState.width;
			int full_h = refState.height;
			if( split_count == 2 )
			{
				rvp.viewport[0] = 0;
				rvp.viewport[1] = ( viewnum == 0 ) ? 0 : ( full_h / 2 );
				rvp.viewport[2] = full_w;
				rvp.viewport[3] = full_h / 2;
			}
			else
			{
				rvp.viewport[0] = ( viewnum & 1 ) ? ( full_w / 2 ) : 0;
				rvp.viewport[1] = ( viewnum >= 2 ) ? ( full_h / 2 ) : 0;
				rvp.viewport[2] = full_w / 2;
				rvp.viewport[3] = full_h / 2;
			}

			// Mantener fov_x estandar (90-100 grados) sin llamar a V_AdjustFov con 1920x540
			// para evitar que V_AdjustFov infle fov_x a 139 grados (ojo de pez que altera la
			// punteria y deja visibles los antebrazos cortados del modelo del arma)
			extern float SV_GetSplitPlayerFov( int viewnum );
			float sp_fov = SV_GetSplitPlayerFov( viewnum );
			if( viewnum == 0 ) sp_fov = cl.local.scr_fov; if( sp_fov < 10.0f ) sp_fov = 90.0f;
			rvp.fov_x = bound( 10.0f, sp_fov, 150.0f );
			rvp.fov_y = V_CalcFov( &rvp.fov_x, (float)rvp.viewport[2], (float)rvp.viewport[3] );
			if( r_adjust_fov.value )
				V_AdjustFov( &rvp.fov_x, &rvp.fov_y, (float)rvp.viewport[2], (float)rvp.viewport[3], false );

			if( viewnum == 0 && clgame.entities )
			{
				for( int pv = 0; pv < split_count; pv++ )
				{
					int o_ent = 0, o_modelindex = 0, o_seq = 0, o_gaitseq = 0, o_weaponmodel = 0, o_flags = 0;
					float o_frame = 0.0f;
					byte o_blend0 = 128;
					vec3_t o_org, o_ang;
					if( SV_GetSplitPlayerEntityState( pv, &o_ent, o_org, o_ang, &o_modelindex, &o_seq, &o_gaitseq, &o_frame, &o_weaponmodel, &o_blend0, &o_flags ) )
					{
						if( o_ent > 0 && o_ent <= cl.maxclients && o_modelindex > 0 )
						{
							cl_entity_t *pent = &clgame.entities[o_ent];
							pent->index = o_ent;
							pent->player = true;
							pent->model = CL_ModelHandle( o_modelindex );

							VectorCopy( pent->curstate.origin, pent->prevstate.origin );
							VectorCopy( pent->curstate.angles, pent->prevstate.angles );
							VectorCopy( o_org, pent->origin );
							VectorCopy( o_ang, pent->angles );
							VectorCopy( o_org, pent->curstate.origin );
							VectorCopy( o_ang, pent->curstate.angles );

							if( s_prev_body_seq[pv] != o_seq || s_body_animtime[pv] <= 0.0f || cl.time < s_body_animtime[pv] )
							{
								s_prev_body_seq[pv] = o_seq;
								s_body_animtime[pv] = cl.time;
							}

							pent->curstate.number = o_ent;
							pent->curstate.modelindex = o_modelindex;
							pent->curstate.sequence = o_seq;
							pent->curstate.gaitsequence = o_gaitseq;
							pent->curstate.frame = 0.0f;
							pent->curstate.animtime = s_body_animtime[pv];
							pent->curstate.framerate = 1.0f;
							pent->curstate.weaponmodel = o_weaponmodel;
							pent->curstate.blending[0] = o_blend0;
							pent->curstate.blending[1] = 0;
							pent->latched.prevblending[0] = o_blend0;
							pent->latched.sequencetime = 0.0f;
							pent->curstate.usehull = ( o_flags & FL_DUCKING ) ? 1 : 0;
							pent->curstate.entityType = ENTITY_NORMAL;
							if( pv == 0 && pent->model )
								ref.dllFuncs.R_AddEntity( pent, ET_PLAYER );
						}
					}
				}
			}

			rvp.flags = RF_DRAW_WORLD;
			rp.nextView = ( viewnum + 1 < split_count ) ? 1 : 0;
		}

		if( split_count <= 1 && viewnum == 0 && FBitSet( rvp.flags, RF_ONLY_CLIENTDRAW ))
		{
			ref.dllFuncs.R_ClearScreen();
		}

		cl_entity_t saved_viewent = clgame.viewent;
		int saved_rend_playernum = cl.playernum;
		int saved_rend_viewent = cl.viewentity;

		if( split_count > 1 && viewnum > 0 )
		{
			cl.viewentity = rvp.viewentity;
			if( rvp.viewentity > 0 && rvp.viewentity <= cl.maxclients )
				cl.playernum = rvp.viewentity - 1;

			vec3_t v_fwd, v_right, v_up;
			AngleVectors( rvp.viewangles, v_fwd, v_right, v_up );
			VectorCopy( rvp.vieworigin, clgame.viewent.origin );
			clgame.viewent.origin[2] -= 1.0f;
			VectorMA( clgame.viewent.origin, 1.5f, v_fwd, clgame.viewent.origin );
			VectorCopy( clgame.viewent.origin, clgame.viewent.curstate.origin );
			VectorCopy( clgame.viewent.origin, clgame.viewent.latched.prevorigin );

			clgame.viewent.angles[PITCH] = -rvp.viewangles[PITCH];
			clgame.viewent.angles[YAW]   = rvp.viewangles[YAW];
			clgame.viewent.angles[ROLL]  = rvp.viewangles[ROLL];
			VectorCopy( clgame.viewent.angles, clgame.viewent.curstate.angles );
			VectorCopy( clgame.viewent.angles, clgame.viewent.latched.prevangles );

			if( sp_viewmodel && sp_viewmodel[0] && sp_health > 0 )
			{
				model_t *vmdl = CL_LoadModel( sp_viewmodel, NULL );
				clgame.viewent.model = vmdl;

				if( s_vm_mdl[viewnum] != vmdl || cl.time < s_vm_animtime[viewnum] )
				{
					s_vm_mdl[viewnum] = vmdl;
					s_vm_unsil[viewnum] = true;
					float dur = 0.8f;
					int draw_seq = CL_FindWeaponSequence( vmdl, "draw", s_vm_unsil[viewnum], &dur );
					if( draw_seq < 0 ) draw_seq = 0;
					s_vm_cur_seq[viewnum] = draw_seq;
					s_vm_animtime[viewnum] = cl.time;
					s_vm_seq_end[viewnum] = cl.time + dur;
					s_vm_playing_action[viewnum] = true;
					s_last_vm_ev[viewnum] = sp_vm_ev;
					s_last_shoot_ev[viewnum] = sp_shoot_ev;
				}
				else if( sp_shoot_ev != s_last_shoot_ev[viewnum] )
				{
					s_last_shoot_ev[viewnum] = sp_shoot_ev;
					s_last_vm_ev[viewnum] = sp_vm_ev;
					float dur = 0.35f;
					int shoot_seq = CL_FindWeaponShootSequence( vmdl, s_vm_unsil[viewnum], sp_shoot_ev, &dur );
					s_vm_cur_seq[viewnum] = shoot_seq;
					s_vm_animtime[viewnum] = cl.time;
					s_vm_seq_end[viewnum] = cl.time + dur;
					s_vm_playing_action[viewnum] = true;
				}
				else if( sp_vm_ev != s_last_vm_ev[viewnum] )
				{
					s_last_vm_ev[viewnum] = sp_vm_ev;
					qboolean is_unsil = s_vm_unsil[viewnum];
					qboolean is_idle = false;
					float dur = CL_GetWeaponSeqInfo( vmdl, sp_vm_seq, &is_unsil, &is_idle );
					s_vm_unsil[viewnum] = is_unsil;
					if( !is_idle || !s_vm_playing_action[viewnum] || cl.time >= s_vm_seq_end[viewnum] )
					{
						s_vm_cur_seq[viewnum] = sp_vm_seq;
						s_vm_animtime[viewnum] = cl.time;
						s_vm_seq_end[viewnum] = cl.time + dur;
						s_vm_playing_action[viewnum] = !is_idle;
					}
				}
				else if( cl.time >= s_vm_seq_end[viewnum] )
				{
					s_vm_playing_action[viewnum] = false;
					float dur = 2.0f;
					int idle_seq = CL_FindWeaponSequence( vmdl, "idle", s_vm_unsil[viewnum], &dur );
					if( idle_seq < 0 ) idle_seq = 0;
					if( s_vm_cur_seq[viewnum] != idle_seq )
					{
						s_vm_cur_seq[viewnum] = idle_seq;
						s_vm_animtime[viewnum] = cl.time;
					}
					s_vm_seq_end[viewnum] = cl.time + dur;
				}

				clgame.viewent.curstate.sequence = s_vm_cur_seq[viewnum];
				clgame.viewent.curstate.animtime = s_vm_animtime[viewnum];
				clgame.viewent.curstate.framerate = 1.0f;
				clgame.viewent.curstate.frame = 0.0f;
				clgame.viewent.latched.prevsequence = s_vm_cur_seq[viewnum];
				clgame.viewent.latched.prevanimtime = s_vm_animtime[viewnum];
				clgame.viewent.latched.sequencetime = 0.0f;
			}
			else
			{
				clgame.viewent.model = NULL;
			}
		}

		if( split_count > 1 )
		{
			extern void CL_SyncSplitHudState( int v );
			g_ps4_active_viewnum = viewnum;
			CL_SyncSplitHudState( viewnum );
			qboolean p1_knife = ( saved_viewent.model && Q_stristr( saved_viewent.model->name, "knife" ) != NULL );
			qboolean cur_knife = ( clgame.viewent.model && Q_stristr( clgame.viewent.model->name, "knife" ) != NULL );
			convar_t *cv_rh = Cvar_FindVar( "cl_righthand" );
			if( cv_rh )
				Cvar_DirectSet( cv_rh, ( p1_knife != cur_knife ) ? "0" : "1" );
			if( SV_GetSplitPlayerFov( viewnum ) < 50.0f )
				clgame.viewent.model = NULL;
		}
		GL_RenderFrame( &rvp );
		if( split_count > 1 )
		{
			convar_t *cv_rh = Cvar_FindVar( "cl_righthand" );
			if( cv_rh )
				Cvar_DirectSet( cv_rh, "1" );
		}

		cl.playernum = saved_rend_playernum;
		cl.viewentity = saved_rend_viewent;
		if( viewnum > 0 )
		{
			clgame.viewent = saved_viewent;
		}

		if( split_count > 1 && cls.state == ca_active && cls.signon == SIGNONS && cls.scrshot_action != scrshot_mapshot )
		{
			vec3_t saved_cl_ang, saved_cl_simorg, saved_cl_punch;
			int saved_playernum = cl.playernum;
			int saved_health = cl.local.health;
			VectorCopy( cl.viewangles, saved_cl_ang );
			VectorCopy( cl.simorg, saved_cl_simorg );
			VectorCopy( cl.punchangle, saved_cl_punch );

			int p_team_id = 1;
			int p_stage = SV_GetSplitPlayerTeamStage( viewnum, &p_team_id );

			if( viewnum > 0 )
			{
				VectorCopy( rvp.viewangles, cl.viewangles );
				VectorCopy( rvp.vieworigin, cl.simorg );
				VectorCopy( sp_punch, cl.punchangle );
				cl.local.health = sp_health;
				if( sp_ent > 0 && sp_ent <= cl.maxclients )
					cl.playernum = sp_ent - 1;
			}

			g_ps4_active_viewnum = viewnum;
			CL_ForceCS16ShowMenu( p_stage, p_team_id );

			ref.dllFuncs.R_AllowFog( false );
			ref.dllFuncs.R_Set2DMode( false );
			ref.dllFuncs.R_Set2DMode( true );

			CL_DrawHUD( CL_ACTIVE );
			if( UI_IsVisible() )
				UI_UpdateMenu( host.realtime );
			Platform_DrawCursor();
			g_ps4_active_viewnum = 0;
			if( viewnum > 0 ) { extern void CL_SyncSplitHudState( int v ); CL_SyncSplitHudState( 0 ); int p0_tid = 1; int p0_st = SV_GetSplitPlayerTeamStage( 0, &p0_tid ); CL_ForceCS16ShowMenu( p0_st, p0_tid ); }

			ref.dllFuncs.R_Set2DMode( false );
			ref.dllFuncs.R_AllowFog( true );

			VectorCopy( saved_cl_ang, cl.viewangles );
			VectorCopy( saved_cl_simorg, cl.simorg );
			VectorCopy( saved_cl_punch, cl.punchangle );
			cl.playernum = saved_playernum;
			cl.local.health = saved_health;
		}

		if( viewnum == 0 )
			S_UpdateFrame( &rvp );
		viewnum++;
		if( split_count > 1 )
		{
			extern void CL_SyncSplitHudState( int v );
			int next_v = rp.nextView ? viewnum : 0;
			g_ps4_active_viewnum = next_v;
			CL_SyncSplitHudState( next_v );
			g_ps4_active_viewnum = 0;
		}

	} while( rp.nextView );

	// draw debug triangles on a server
	SV_DrawDebugTriangles ();
	ref.dllFuncs.GL_BackendEndFrame ();
}

#define POINT_SIZE		16.0f
#define NODE_INTERVAL_X(x)	(x * 16.0f)
#define NODE_INTERVAL_Y(x)	(x * 16.0f)

static void R_DrawLeafNode( float x, float y, float scale )
{
	float downScale = scale * 0.25f;// * POINT_SIZE;

	ref.dllFuncs.R_DrawStretchPic( x - downScale * 0.5f, y - downScale * 0.5f, downScale, downScale, 0, 0, 1, 1, R_GetBuiltinTexture( REF_PARTICLE_TEXTURE ) );
}

static void R_DrawNodeConnection( float x, float y, float x2, float y2 )
{
	ref.dllFuncs.Begin( TRI_LINES );
		ref.dllFuncs.Vertex3f( x, y, 0 );
		ref.dllFuncs.Vertex3f( x2, y2, 0 );
	ref.dllFuncs.End( );
}

static void R_ShowTree_r( mnode_t *node, float x, float y, float scale, int shownodes, mleaf_t *viewleaf )
{
	float	downScale = scale * 0.8f;

	downScale = Q_max( downScale, 1.0f );

	if( !node ) return;

	world.recursion_level++;

	if( node->contents < 0 )
	{
		mleaf_t	*leaf = (mleaf_t *)node;

		if( world.recursion_level > world.max_recursion )
			world.max_recursion = world.recursion_level;

		if( shownodes == 1 )
		{
			if( cl.worldmodel->leafs == leaf )
				ref.dllFuncs.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
			else if( viewleaf && viewleaf == leaf )
				ref.dllFuncs.Color4f( 1.0f, 0.0f, 0.0f, 1.0f );
			else ref.dllFuncs.Color4f( 0.0f, 1.0f, 0.0f, 1.0f );
			R_DrawLeafNode( x, y, scale );
		}
		world.recursion_level--;
		return;
	}

	if( shownodes == 1 )
	{
		ref.dllFuncs.Color4f( 0.0f, 0.0f, 1.0f, 1.0f );
		R_DrawLeafNode( x, y, scale );
	}
	else if( shownodes == 2 )
	{
		R_DrawNodeConnection( x, y, x - scale, y + scale );
		R_DrawNodeConnection( x, y, x + scale, y + scale );
	}

	R_ShowTree_r( node_child( node, 1, cl.worldmodel ), x - scale, y + scale, downScale, shownodes, viewleaf );
	R_ShowTree_r( node_child( node, 0, cl.worldmodel ), x + scale, y + scale, downScale, shownodes, viewleaf );

	world.recursion_level--;
}

static void R_ShowTree( void )
{
	float	x = (float)((refState.width - (int)POINT_SIZE) >> 1);
	float	y = NODE_INTERVAL_Y(1.0f);

	if( !cl.worldmodel || !r_showtree.value )
		return;

	world.recursion_level = 0;
	mleaf_t *viewleaf = Mod_PointInLeaf( refState.vieworg, cl.worldmodel->nodes, cl.worldmodel );

	ref.dllFuncs.TriRenderMode( kRenderTransTexture );

	//pglLineWidth( 2.0f );
	ref.dllFuncs.Color4f( 1, 0.7f, 0, 1.0f );
	//pglDisable( GL_TEXTURE_2D );
	R_ShowTree_r( cl.worldmodel->nodes, x, y, world.max_recursion * 3.5f, 2, viewleaf );
	//pglEnable( GL_TEXTURE_2D );
	//pglLineWidth( 1.0f );

	R_ShowTree_r( cl.worldmodel->nodes, x, y, world.max_recursion * 3.5f, 1, viewleaf );

	Con_NPrintf( 0, "max recursion %d\n", world.max_recursion );
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	qboolean		draw_2d = false;

	ref.dllFuncs.R_AllowFog( false );
	ref.dllFuncs.R_Set2DMode( true );

	if( cls.state == ca_active && cls.signon == SIGNONS && cls.scrshot_action != scrshot_mapshot )
	{
		if( SV_GetSplitScreenCount() <= 1 )
		{
			SCR_TileClear();
			CL_DrawHUD( CL_ACTIVE );
			VGui_Paint();
		}
	}

	switch( cls.scrshot_action )
	{
	case scrshot_inactive:
	case scrshot_normal:
	case scrshot_snapshot:
		draw_2d = true;
		break;
	}

	if( draw_2d )
	{
		SCR_RSpeeds();
		SCR_NetSpeeds();
		SCR_DrawPos();
		SCR_DrawEnts();
		SCR_DrawNetGraph();
		SCR_DrawUserCmd();
		Joy_DrawDebug();
		IN_GyroDrawDebug();
		SV_DrawOrthoTriangles();
		CL_DrawDemoRecording();
		CL_DrawHUD( CL_CHANGELEVEL );
		ref.dllFuncs.R_ShowTextures();
		R_ShowTree();
		UI_UpdateMenu( host.realtime );
		Con_DrawVersion();
		Con_DrawConsole();
		Con_DrawDebug(); // must be last
		Touch_Draw();
		OSK_Draw();
		if( UI_IsVisible() || !( cls.state == ca_active && cls.signon == SIGNONS && SV_GetSplitScreenCount() > 1 ) )
			Platform_DrawCursor();

		S_ExtraUpdate();
	}

	SCR_MakeScreenShot();
	ref.dllFuncs.R_AllowFog( true );
	Platform_SetTimer( 0.0f );
	ref.dllFuncs.R_EndFrame();

	V_CheckGammaEnd();
}
