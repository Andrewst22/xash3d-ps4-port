extern void SV_NotifyP1ChooseTeam( void );
extern void SV_NotifyP1MenuSelect( int num );
/*
in_ps4.c - PlayStation 4 native gamepad input
Copyright (C) 2026 Xash3D FWGS contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

/*
DualShock 4 is read directly through libScePad: the PS4 has no mouse or window system,
so input is polled once per frame in Platform_RunEvents
*/

#include "platform/platform.h"
#include "input.h"
#include "keydefs.h"
#include "client.h"
#include "vgui_draw.h"
#include "utflib.h"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/CommonDialog.h>
#include <orbis/ImeDialog.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>

#ifndef ORBIS_PAD_PORT_TYPE_STANDARD
#define ORBIS_PAD_PORT_TYPE_STANDARD 0
#endif

static const struct
{
	uint32_t mask;
	int key;
} ps4_buttons[] =
{
{ ORBIS_PAD_BUTTON_CROSS,     K_A_BUTTON },
{ ORBIS_PAD_BUTTON_CIRCLE,    K_B_BUTTON },
{ ORBIS_PAD_BUTTON_SQUARE,    K_X_BUTTON },
{ ORBIS_PAD_BUTTON_TRIANGLE,  K_Y_BUTTON },
{ 0,                          K_L1_BUTTON },
{ 0,                          K_R1_BUTTON },
{ ORBIS_PAD_BUTTON_L3,        K_LSTICK },
{ ORBIS_PAD_BUTTON_R3,        K_RSTICK },
{ ORBIS_PAD_BUTTON_OPTIONS,   K_ESCAPE },
{ ORBIS_PAD_BUTTON_TOUCH_PAD, K_BACK_BUTTON },
{ ORBIS_PAD_BUTTON_UP,        K_DPAD_UP },
{ ORBIS_PAD_BUTTON_DOWN,      K_DPAD_DOWN },
{ ORBIS_PAD_BUTTON_LEFT,      K_DPAD_LEFT },
{ ORBIS_PAD_BUTTON_RIGHT,     K_DPAD_RIGHT },
};

extern qboolean PS4_TouchpadIsAiming( void );
extern void PS4_TouchpadCursorMotion( float dx, float dy );
extern void PS4_TouchpadAimMotion( float dx, float dy );
extern void PS4_TouchpadButton( int button, qboolean down );
extern void PS4_GyroMotion( float dx, float dy );
extern void PS4_GyroNotify( qboolean enabled );
extern void PS4_SetWheelState( qboolean open, int selected );

static CVAR_DEFINE_AUTO( ps4_binds_configured, "0", FCVAR_ARCHIVE, "PS4 controller default binds initialized" );

static void PS4_ApplyDefaultBinds( qboolean force )
{
	Cbuf_AddText( "name \"ps4-user\"\n" );
	Cbuf_AddText( "bind AUX31 \"+attack\"\n" );
	Cbuf_AddText( "bind AUX32 \"+attack2\"\n" );
	Cbuf_AddText( "bind A_BUTTON \"+jump\"\n" );
	Cbuf_AddText( "bind B_BUTTON \"+duck\"\n" );
	Cbuf_AddText( "bind X_BUTTON \"+reload\"\n" );
	Cbuf_AddText( "bind Y_BUTTON \"+use\"\n" );
	Cbuf_AddText( "bind L1_BUTTON \"invprev\"\n" );
	Cbuf_AddText( "bind R1_BUTTON \"invnext\"\n" );
	Cbuf_AddText( "bind LSTICK \"+speed\"\n" );
	Cbuf_AddText( "bind DPAD_DOWN \"_vgui_menus 0; chooseteam\"\n" );
	Cbuf_AddText( "bind DPAD_LEFT \"_vgui_menus 0; buy\"\n" );
	Cbuf_AddText( "bind DPAD_UP \"drop\"\n" );
	Cbuf_AddText( "bind DPAD_RIGHT \"impulse 201\"\n" );
	Cbuf_AddText( "bind BACK_BUTTON \"+showscores\"\n" );
	Cbuf_AddText( "bind START_BUTTON \"escape\"\n" );
	if( force || !ps4_binds_configured.value )
		Cvar_SetValue( "ps4_binds_configured", 1.0f );
}

static struct
{
	int32_t  handle;
	uint32_t buttons;
	short    axes[MAX_AXES];
	float    vibrate_until;
	qboolean vibrating;
	double   repeat_time;
	qboolean defaults_applied;
	int      wheel_release_key;
	double   wheel_release_time;
	qboolean l3r3_latch;
	qboolean wheel_open;
	int      wheel_sel;
	qboolean gyro_enabled;
	int      gyro_warmup;
	float    prev_yaw;
	float    prev_pitch;
	qboolean l2_down;
	qboolean r2_down;
	qboolean touch_down;
	uint8_t  touch_id;
	float    touch_fx;
	float    touch_fy;
	int      aim_still_frames;
	uint16_t cursor_anchor_x;
	uint16_t cursor_anchor_y;
	qboolean cursor_moving;
	double   last_click_time;
	int      active_click_btn;
	float    rstick_smooth_x;
	float    rstick_smooth_y;
} ps4pad = { -1 };

// the system doesn't repeat buttons like a keyboard, menus (sliders, lists) need it
#define PS4_REPEAT_DELAY    0.4
#define PS4_REPEAT_INTERVAL 0.08
#define PS4_REPEAT_BUTTONS  ( ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT )

static short PS4_StickToAxis( uint8_t value )
{
	// 0..255 to -32768..32767, 128 is center
	int v = ((int)value - 128 ) * 256;

	return (short)bound( -32768, v, 32767 );
}

static short PS4_TriggerToAxis( uint8_t value )
{
	return (short)( value * 32767 / 255 );
}

static void PS4_SendAxis( engineAxis_t axis, short value )
{
	if( ps4pad.axes[axis] == value )
		return;

	ps4pad.axes[axis] = value;
	Joy_AxisMotionEvent( axis, value );
}

static void PS4_ReleasePad( void )
{
	for( size_t i = 0; i < ARRAYSIZE( ps4_buttons ); i++ )
	{
		if( FBitSet( ps4pad.buttons, ps4_buttons[i].mask ))
			Key_Event( ps4_buttons[i].key, false );
	}
	PS4_SendAxis( JOY_AXIS_SIDE, 0 );
	PS4_SendAxis( JOY_AXIS_FWD, 0 );
	PS4_SendAxis( JOY_AXIS_YAW, 0 );
	PS4_SendAxis( JOY_AXIS_PITCH, 0 );
	PS4_SendAxis( JOY_AXIS_LT, 0 );
	PS4_SendAxis( JOY_AXIS_RT, 0 );
}

/*
==============================================================================

System keyboard

The chat line opens the system keyboard dialog together with itself, in the console
and after cancelling Cross opens it. Menu fields ask
for it with pfnSetTextInputBuffer and pfnEnableTextInput: text input is enabled
by the menu as soon as a field gets focus, the keyboard must open only when the
user activates the field.
The dialog starts with the text being edited and replaces it.
The dialog works with UTF-16, our wchar_t is 32 bits, so buffers are uint16_t.

==============================================================================
*/
#define IME_MAX_TEXT 255

static struct
{
	qboolean wanted;       // engine has text input enabled
	qboolean open;         // dialog is running
	qboolean initialized;  // module loaded, common dialog initialized
	qboolean seen_running; // dialog reported running at least once
	qboolean requested;    // the menu asked for the dialog, opened on next input update
	qboolean chat_opened;  // chat line was just opened, the dialog opens with it
	char request_text[IME_MAX_TEXT * 4 + 1];
	char edited_text[IME_MAX_TEXT * 4 + 1]; // what the dialog was opened with
	uint16_t text[IME_MAX_TEXT + 1];
} ps4ime;

static CVAR_DEFINE_AUTO( ime_use_console_language, "1", FCVAR_ARCHIVE, "system keyboard has the language of the console first and English after it, 0 - English only" );

// SceImeLanguage
#define IME_LANGUAGE_ENGLISH_US     0x00000004ULL

#define IME_OPTION_LANGUAGES_FORCED 0x00000008

static qboolean PS4_IME_Init( void )
{
	int ret;

	if( ps4ime.initialized )
		return true;

	if(( ret = sceSysmoduleLoadModule( ORBIS_SYSMODULE_IME_DIALOG )) < 0 )
	{
		Con_Printf( S_ERROR "IME dialog module: 0x%08x\n", ret );
		return false;
	}

	// fails if somebody initialized it already, that's fine
	sceCommonDialogInitialize();
	ps4ime.initialized = true;
	return true;
}

static void PS4_IME_FromUtf8( uint16_t *out, size_t size, const char *in )
{
	utfstate_t state = { 0 };
	size_t len = 0;

	for( const byte *p = (const byte *)in; p && *p; p++ )
	{
		uint32_t c = Q_DecodeUTF8( &state, *p );

		if( !c )
			continue;

		if( c >= 0x10000 )
		{
			if( len + 3 > size )
				break;

			c -= 0x10000;
			out[len++] = 0xd800 + ( c >> 10 );
			out[len++] = 0xdc00 + ( c & 0x3ff );
		}
		else
		{
			if( len + 2 > size )
				break;

			out[len++] = c;
		}
	}

	out[len] = 0;
}

static void PS4_IME_Open( const char *text )
{
	static const uint16_t title[] = { 'X', 'a', 's', 'h', '3', 'D', ' ', 'F', 'W', 'G', 'S', 0 };
	int32_t user = -1;
	int ret;

	if( !PS4_IME_Init( ))
		return;

	sceUserServiceGetInitialUser( &user );
	PS4_IME_FromUtf8( ps4ime.text, IME_MAX_TEXT + 1, text );
	Q_strncpy( ps4ime.edited_text, text, sizeof( ps4ime.edited_text ));

	OrbisImeDialogSetting param =
	{
		.userId = user,
		.type = ORBIS_TYPE_DEFAULT,
		.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT,
		.maxTextLength = IME_MAX_TEXT,
		.inputTextBuffer = (wchar_t *)ps4ime.text,
		.posx = 1920.0f / 2.0f,
		.posy = 1080.0f / 2.0f,
		.horizontalAlignment = ORBIS_H_CENTER,
		.verticalAlignment = ORBIS_V_CENTER,
		.title = (const wchar_t *)title,
	};

	// without a list the keyboard has the language of the console first and English
	// after it, with the list it has only the listed languages
	if( !ime_use_console_language.value )
	{
		param.supportedLanguages = IME_LANGUAGE_ENGLISH_US;
		param.option = IME_OPTION_LANGUAGES_FORCED;
	}

	if(( ret = sceImeDialogInit( &param, NULL )) < 0 )
	{
		Con_Printf( S_ERROR "sceImeDialogInit failed: 0x%08x\n", ret );
		return;
	}

	ps4ime.open = true;
	ps4ime.seen_running = false;

	// nothing reaches the game while the dialog takes the gamepad: held buttons
	// are released now, buttons still held when it closes don't press again
	PS4_ReleasePad();
}

static void PS4_IME_KeyPress( int key )
{
	Key_Event( key, true );
	Key_Event( key, false );
}

// typed text replaces the edited one: it's erased with the keys of the keyboard
// (End, then Backspace for every character), then the text goes in like text input
// events of a keyboard; console and chat send it right away
static void PS4_IME_Deliver( void )
{
	char text[IME_MAX_TEXT * 4 + 1];
	const qboolean menu = cls.key_dest == key_menu;

	Q_UTF16ToUTF8( text, sizeof( text ), ps4ime.text, IME_MAX_TEXT + 1 );

	PS4_IME_KeyPress( K_END );
	for( size_t i = Q_UTF8Length( ps4ime.edited_text ); i > 0; i-- )
		PS4_IME_KeyPress( K_BACKSPACE );

	VGui_ReportTextInput( text );

	for( const char *p = text; *p; p++ )
	{
		int ch = (byte)*p;

		if( !cls.accept_utf8 )
			ch = Con_UtfProcessCharForce( ch );

		if( ch )
			CL_CharEvent( ch );
	}

	if( !menu && text[0] )
		PS4_IME_KeyPress( K_ENTER );
}

static void PS4_IME_Update( void )
{
	OrbisDialogStatus status = sceImeDialogGetStatus();

	// dialog may report "none" for a while after it's created and a stale state of
	// the previous one, it's done only when it finishes after running
	if( status == ORBIS_DIALOG_STATUS_RUNNING )
		ps4ime.seen_running = true;
	if( status != ORBIS_DIALOG_STATUS_STOPPED || !ps4ime.seen_running )
		return;

	OrbisDialogResult result = { 0 };
	sceImeDialogGetResult( &result );
	sceImeDialogTerm();
	ps4ime.open = false;

	Con_Reportf( "IME dialog closed, result %d\n", result.endstatus );
	if( result.endstatus == ORBIS_DIALOG_OK )
		PS4_IME_Deliver();
}

void PS4_InitKeyboard( void )
{
	Cvar_RegisterVariable( &ime_use_console_language );
}

void Platform_EnableTextInput( qboolean enable, int x, int y, int w, int h )
{
	const char *text = Key_GetTextInputBuffer();
	const qboolean was_wanted = ps4ime.wanted;

	ps4ime.wanted = enable;

	// called from the key event of the menu, the dialog is opened outside of it
	if( !enable )
	{
		ps4ime.requested = false;
		ps4ime.chat_opened = false;
	}
	else if( PS4_KeyboardActive( ))
	{
		// the USB keyboard types, Cross still opens the system one
	}
	else if( !was_wanted && cls.key_dest == key_message )
	{
		// the chat line has nothing to look at without the keyboard, it opens with it,
		// the console doesn't: it's open during loading and the keyboard would cover it
		ps4ime.chat_opened = true;
	}
	else if( text && !ps4ime.open )
	{
		ps4ime.requested = true;
		Q_strncpy( ps4ime.request_text, text, sizeof( ps4ime.request_text ));
	}
}

static void PS4_StopVibration( void )
{
	OrbisPadVibeParam vibe = { 0 };

	if( ps4pad.handle >= 0 && ps4pad.vibrating )
		scePadSetVibration( ps4pad.handle, &vibe );

	ps4pad.vibrating = false;
}

static void PS4_PadUpdate( void )
{
	if( ps4pad.handle < 0 )
		return;

	if( ps4pad.vibrating && host.realtime > ps4pad.vibrate_until )
		PS4_StopVibration( );

	OrbisPadData data = { 0 };

	if( scePadReadState( ps4pad.handle, &data ) < 0 || !data.connected )
	{
		if( !ps4ime.open )
			PS4_ReleasePad();

		ps4pad.buttons = 0;
		ps4pad.repeat_time = 0.0;
		return;
	}

	if( ps4ime.requested )
	{
		ps4ime.requested = false;
		PS4_IME_Open( ps4ime.request_text );
	}

	if( ps4ime.chat_opened )
	{
		ps4ime.chat_opened = false;
		if( cls.key_dest == key_message && !ps4ime.open )
			PS4_IME_Open( Con_GetEditText( ));
	}

	// the system keyboard has the gamepad, state is only tracked
	if( ps4ime.open )
	{
		PS4_IME_Update();
		ps4pad.buttons = data.buttons;
		return;
	}

	const uint32_t changed = data.buttons ^ ps4pad.buttons;

	// Cross opens the system keyboard in the console and chat again, after it was cancelled
	if( FBitSet( changed & data.buttons, ORBIS_PAD_BUTTON_CROSS ) && ps4ime.wanted && host.textmode
		&& ( cls.key_dest == key_console || cls.key_dest == key_message ))
	{
		PS4_IME_Open( Con_GetEditText( ));
		ps4pad.buttons = data.buttons;
		return;
	}

	for( size_t i = 0; changed && i < ARRAYSIZE( ps4_buttons ); i++ )
	{
		if( ps4_buttons[i].mask == ORBIS_PAD_BUTTON_TOUCH_PAD && !PS4_TouchpadIsAiming() )
			continue;
		if( FBitSet( changed, ps4_buttons[i].mask ))
			{
				if( ps4_buttons[i].mask == ORBIS_PAD_BUTTON_DOWN && FBitSet( data.buttons, ORBIS_PAD_BUTTON_DOWN ) && cls.state == ca_active )
					SV_NotifyP1ChooseTeam();
				Key_Event( ps4_buttons[i].key, FBitSet( data.buttons, ps4_buttons[i].mask ) ? true : false );
			}
	}

	// repeat held D-pad buttons in menus and console, not in the game
	if( changed & PS4_REPEAT_BUTTONS )
		ps4pad.repeat_time = ( data.buttons & PS4_REPEAT_BUTTONS ) ? host.realtime + PS4_REPEAT_DELAY : 0.0;

	if( cls.key_dest != key_game && ps4pad.repeat_time > 0.0 && host.realtime >= ps4pad.repeat_time )
	{
		for( size_t i = 0; i < ARRAYSIZE( ps4_buttons ); i++ )
		{
			if( FBitSet( ps4_buttons[i].mask, PS4_REPEAT_BUTTONS ) && FBitSet( data.buttons, ps4_buttons[i].mask ))
				Key_Event( ps4_buttons[i].key, true );
		}

		ps4pad.repeat_time = host.realtime + PS4_REPEAT_INTERVAL;
	}

	if( !ps4pad.defaults_applied && cls.state >= ca_disconnected && host.realtime > 1.5 )
	{
		ps4pad.defaults_applied = true;
		PS4_ApplyDefaultBinds( false );
	}

	if( ps4pad.wheel_release_key && host.realtime >= ps4pad.wheel_release_time )
	{
		Key_Event( ps4pad.wheel_release_key, false );
		ps4pad.wheel_release_key = 0;
	}

	// 1. Alternar Giroscopio con L3 + R3, o abrir/cerrar Rueda Numerica (1..9, 0) con R3
	{
		const uint32_t l3r3 = ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3;
		if( ( data.buttons & l3r3 ) == l3r3 )
		{
			if( !ps4pad.l3r3_latch )
			{
				ps4pad.l3r3_latch = true;
				ps4pad.wheel_open = false;
				PS4_SetWheelState( false, -1 );
				ps4pad.gyro_enabled = !ps4pad.gyro_enabled;
				ps4pad.gyro_warmup = 3;
				PS4_GyroNotify( ps4pad.gyro_enabled );
				Con_Printf( "^2[PS4]^7 Giroscopio %s\n", ps4pad.gyro_enabled ? "ON" : "OFF" );
			}
		}
		else
		{
			ps4pad.l3r3_latch = false;
			extern void SV_NotifyP1WeaponSwitch( int dir );
			extern void SV_NotifyP1Buy( void );
			if( FBitSet( changed & data.buttons, ORBIS_PAD_BUTTON_L1 ) )
				SV_NotifyP1WeaponSwitch( -1 );
			if( FBitSet( changed & data.buttons, ORBIS_PAD_BUTTON_R1 ) )
				SV_NotifyP1WeaponSwitch( 1 );
			if( FBitSet( changed & data.buttons, ORBIS_PAD_BUTTON_LEFT ) )
				SV_NotifyP1Buy();
			if( FBitSet( changed & data.buttons, ORBIS_PAD_BUTTON_R3 ) && !FBitSet( data.buttons, ORBIS_PAD_BUTTON_L3 ) && cls.key_dest == key_game )
			{
				ps4pad.wheel_open = !ps4pad.wheel_open;
				ps4pad.wheel_sel = -1;
				PS4_SetWheelState( ps4pad.wheel_open, -1 );
			}
		}
	}

	// 2. Logica de la Rueda Numerica en R3
	if( ps4pad.wheel_open )
	{
		float rx = (float)((int)data.rightStick.x - 128) / 127.0f;
		float ry = (float)((int)data.rightStick.y - 128) / 127.0f;
		float mag = sqrtf( rx * rx + ry * ry );

		if( mag > 0.55f )
		{
			float angle = atan2f( ry, rx ) + ( 3.14159265f * 0.5f );
			if( angle < 0.0f ) angle += 2.0f * 3.14159265f;
			int sector = (int)floorf( ( angle + ( 3.14159265f / 10.0f ) ) / ( 2.0f * 3.14159265f / 10.0f ) ) % 10;
			ps4pad.wheel_sel = ( sector == 0 ) ? 9 : ( sector - 1 );
			PS4_SetWheelState( true, ps4pad.wheel_sel );
		}
		else if( ps4pad.wheel_sel >= 0 && mag < 0.28f )
		{
			extern int SV_GetSplitScreenCount( void );
			int num = ( ps4pad.wheel_sel == 9 ) ? 0 : ( ps4pad.wheel_sel + 1 );
			int slot_num = ( num == 0 ) ? 10 : num;
			if( SV_GetSplitScreenCount() > 1 )
			{
				SV_NotifyP1MenuSelect( slot_num );
			}
			else
			{
				Cbuf_AddText( va( "_vgui_menus 0; menuselect %d\n", slot_num ) );
			}
			ps4pad.wheel_open = false;
			ps4pad.wheel_sel = -1;
			PS4_SetWheelState( false, -1 );
		}
	}

	// 3. Gatillos L2 y R2
	{
		qboolean l2_now = ( data.analogButtons.l2 > 45 );
		qboolean r2_now = ( data.analogButtons.r2 > 45 );
		if( l2_now != ps4pad.l2_down )
		{
			ps4pad.l2_down = l2_now;
			Key_Event( K_AUX32, l2_now );
		}
		if( r2_now != ps4pad.r2_down )
		{
			ps4pad.r2_down = r2_now;
			Key_Event( K_AUX31, r2_now );
		}
	}

	// 4. Touchpad separado en Modo Mira 3D (alta precision) y Modo Cursor 2D (menus)
	{
		const qboolean is_aiming = PS4_TouchpadIsAiming();
		float accum_aim_x = 0.0f;
		float accum_aim_y = 0.0f;

		if( data.touch.fingers > 0 )
		{
			const uint8_t id = data.touch.touch[0].finger;
			const uint16_t tx = data.touch.touch[0].x;
			const uint16_t ty = data.touch.touch[0].y;

			if( ps4pad.touch_down && ps4pad.touch_id == id )
			{
				if( is_aiming )
				{
					float raw_dx = (float)tx - ps4pad.touch_fx;
					float raw_dy = (float)ty - ps4pad.touch_fy;
					float dist_sq = raw_dx * raw_dx + raw_dy * raw_dy;
					float min_dist_sq = ( ps4pad.aim_still_frames >= 3 ) ? 2.25f : 0.0f;

					if( dist_sq > min_dist_sq && dist_sq > 0.0f )
					{
						float speed = sqrtf( dist_sq );
						float sens = 0.90f + bound( 0.0f, ( speed - 1.0f ) * 0.12f, 1.20f );
						accum_aim_x += raw_dx * sens;
						accum_aim_y += raw_dy * sens * 1.12f;
						ps4pad.touch_fx = (float)tx;
						ps4pad.touch_fy = (float)ty;
						ps4pad.aim_still_frames = 0;
					}
					else if( dist_sq <= 1.0f )
					{
						if( ps4pad.aim_still_frames < 10 )
							ps4pad.aim_still_frames++;
					}
				}
				else
				{
					int raw_dx = (int)tx - (int)ps4pad.cursor_anchor_x;
					int raw_dy = (int)ty - (int)ps4pad.cursor_anchor_y;
					int abs_x = raw_dx < 0 ? -raw_dx : raw_dx;
					int abs_y = raw_dy < 0 ? -raw_dy : raw_dy;
					int threshold = ps4pad.cursor_moving ? 2 : 6;

					if( abs_x > threshold || abs_y > threshold )
					{
						if( !ps4pad.cursor_moving )
						{
							if( raw_dx > threshold ) raw_dx -= threshold;
							else if( raw_dx < -threshold ) raw_dx += threshold;
							else raw_dx = 0;

							if( raw_dy > threshold ) raw_dy -= threshold;
							else if( raw_dy < -threshold ) raw_dy += threshold;
							else raw_dy = 0;

							ps4pad.cursor_moving = true;
						}

						PS4_TouchpadCursorMotion( (float)raw_dx * 2.50f, (float)raw_dy * 2.80f );
						ps4pad.cursor_anchor_x = tx;
						ps4pad.cursor_anchor_y = ty;
					}
					else if( abs_x <= 1 && abs_y <= 1 )
					{
						ps4pad.cursor_moving = false;
					}
				}
			}
			else
			{
				ps4pad.touch_down = true;
				ps4pad.touch_id = id;
				ps4pad.touch_fx = (float)tx;
				ps4pad.touch_fy = (float)ty;
				ps4pad.cursor_anchor_x = tx;
				ps4pad.cursor_anchor_y = ty;
				ps4pad.cursor_moving = false;
				ps4pad.aim_still_frames = 10;
			}
		}
		else
		{
			ps4pad.touch_down = false;
			ps4pad.cursor_moving = false;
			ps4pad.aim_still_frames = 10;
		}

		if( accum_aim_x != 0.0f || accum_aim_y != 0.0f )
			PS4_TouchpadAimMotion( accum_aim_x, accum_aim_y );
	}

	// 5. Giroscopio en ejes locales del mando (delta cuaternionico q_prev^* * q_curr)
	if( ps4pad.gyro_enabled )
	{
		static float s_prev_q[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		static float s_smooth_gx = 0.0f, s_smooth_gy = 0.0f;
		float qx = data.quat.x;
		float qy = data.quat.y;
		float qz = data.quat.z;
		float qw = data.quat.w;
		const float norm_sq = qx * qx + qy * qy + qz * qz + qw * qw;

		if( norm_sq > 0.5f && norm_sq < 1.5f )
		{
			const float inv_norm = 1.0f / sqrtf( norm_sq );
			qx *= inv_norm;
			qy *= inv_norm;
			qz *= inv_norm;
			qw *= inv_norm;

			if( ps4pad.gyro_warmup > 0 )
			{
				ps4pad.gyro_warmup--;
				s_prev_q[0] = qx;
				s_prev_q[1] = qy;
				s_prev_q[2] = qz;
				s_prev_q[3] = qw;
				s_smooth_gx = 0.0f;
				s_smooth_gy = 0.0f;
			}
			else
			{
				float px = s_prev_q[0], py = s_prev_q[1], pz = s_prev_q[2], pw = s_prev_q[3];
				// Asegurar el mismo hemisferio cuaternionico (evitar saltos por doble cobertura q == -q)
				if( px * qx + py * qy + pz * qz + pw * qw < 0.0f )
				{
					px = -px; py = -py; pz = -pz; pw = -pw;
				}

				// Rotacion diferencial en los ejes fisicos locales del DualShock 4:
				// lx = Pitch local (inclinar morro arriba/abajo)
				// ly = Yaw local (girar morro izquierda/derecha)
				// lz = Roll local (inclinar mando izquierda/derecha)
				float lx = pw * qx - px * qw - py * qz + pz * qy;
				float ly = pw * qy + px * qz - py * qw - pz * qx;
				float lz = pw * qz - px * qy + py * qx - pz * qw;

				float d_horiz = ( ly - lz * 0.45f ) * 2.0f;
				float d_vert  = lx * 2.0f;

				if( d_horiz > -0.25f && d_horiz < 0.25f && d_vert > -0.25f && d_vert < 0.25f )
				{
					// Zona muerta contra deriva microscopica del sensor
					if( d_horiz > -0.0003f && d_horiz < 0.0003f ) d_horiz = 0.0f;
					if( d_vert  > -0.0003f && d_vert  < 0.0003f ) d_vert  = 0.0f;

					float raw_gx = -d_horiz * 1550.0f;
					float raw_gy = -d_vert  * 1550.0f;

					s_smooth_gx = s_smooth_gx * 0.25f + raw_gx * 0.75f;
					s_smooth_gy = s_smooth_gy * 0.25f + raw_gy * 0.75f;

					if( s_smooth_gx > -0.08f && s_smooth_gx < 0.08f && raw_gx == 0.0f ) s_smooth_gx = 0.0f;
					if( s_smooth_gy > -0.08f && s_smooth_gy < 0.08f && raw_gy == 0.0f ) s_smooth_gy = 0.0f;

					if( s_smooth_gx != 0.0f || s_smooth_gy != 0.0f )
						PS4_GyroMotion( s_smooth_gx, s_smooth_gy );
				}

				s_prev_q[0] = qx;
				s_prev_q[1] = qy;
				s_prev_q[2] = qz;
				s_prev_q[3] = qw;
			}
		}
	}

	if( FBitSet( changed, ORBIS_PAD_BUTTON_TOUCH_PAD ))
	{
		if( FBitSet( data.buttons, ORBIS_PAD_BUTTON_TOUCH_PAD ))
		{
			if( host.realtime - ps4pad.last_click_time <= 0.28 )
			{
				ps4pad.active_click_btn = 1;
				ps4pad.last_click_time = 0.0;
			}
			else
			{
				ps4pad.active_click_btn = 0;
				ps4pad.last_click_time = host.realtime;
			}
			PS4_TouchpadButton( ps4pad.active_click_btn, true );
		}
		else
		{
			PS4_TouchpadButton( ps4pad.active_click_btn, false );
		}
	}

	ps4pad.buttons = data.buttons;

	// 6. Stick Izquierdo directo y Stick Derecho con curva suave (35% lineal + 65% cubica) + filtro EMA
	PS4_SendAxis( JOY_AXIS_SIDE, PS4_StickToAxis( data.leftStick.x ));
	PS4_SendAxis( JOY_AXIS_FWD, PS4_StickToAxis( data.leftStick.y ));

	if( ps4pad.wheel_open )
	{
		ps4pad.rstick_smooth_x = 0.0f;
		ps4pad.rstick_smooth_y = 0.0f;
		PS4_SendAxis( JOY_AXIS_YAW, 0 );
		PS4_SendAxis( JOY_AXIS_PITCH, 0 );
	}
	else
	{
		float nx = ( (int)data.rightStick.x - 128 ) / 127.0f;
		float ny = ( (int)data.rightStick.y - 128 ) / 127.0f;
		nx = bound( -1.0f, nx, 1.0f );
		ny = bound( -1.0f, ny, 1.0f );

		float mag = sqrtf( nx * nx + ny * ny );
		const float deadzone = 0.08f;
		float target_x = 0.0f;
		float target_y = 0.0f;

		if( mag > deadzone )
		{
			float norm_mag = ( mag - deadzone ) / ( 1.0f - deadzone );
			norm_mag = bound( 0.0f, norm_mag, 1.0f );
			float curved_mag = ( 0.35f * norm_mag + 0.65f * norm_mag * norm_mag * norm_mag ) * 0.78f;
			float scale = ( curved_mag / mag ) * 32767.0f;
			target_x = nx * scale;
			target_y = ny * scale;
		}

		ps4pad.rstick_smooth_x = ps4pad.rstick_smooth_x * 0.45f + target_x * 0.55f;
		ps4pad.rstick_smooth_y = ps4pad.rstick_smooth_y * 0.45f + target_y * 0.55f;

		if( ps4pad.rstick_smooth_x > -32.0f && ps4pad.rstick_smooth_x < 32.0f && target_x == 0.0f )
			ps4pad.rstick_smooth_x = 0.0f;
		if( ps4pad.rstick_smooth_y > -32.0f && ps4pad.rstick_smooth_y < 32.0f && target_y == 0.0f )
			ps4pad.rstick_smooth_y = 0.0f;

		PS4_SendAxis( JOY_AXIS_YAW, (short)bound( -32768, (int)ps4pad.rstick_smooth_x, 32767 ) );
		PS4_SendAxis( JOY_AXIS_PITCH, (short)bound( -32768, (int)ps4pad.rstick_smooth_y, 32767 ) );
	}
	PS4_SendAxis( JOY_AXIS_LT, PS4_TriggerToAxis( data.analogButtons.l2 ));
	PS4_SendAxis( JOY_AXIS_RT, PS4_TriggerToAxis( data.analogButtons.r2 ));
}

// ========================================================================
// Soporte Multijugador Local en Pantalla Dividida (Mandos 2, 3 y 4)
// Usa la misma Rueda Numerica original (PS4_SetWheelState) y botones
// ========================================================================
typedef struct ps4_extra_pad_s
{
	int32_t  user_id;
	int32_t  handle;
	qboolean active;
	uint32_t prev_buttons;
	float    smooth_rx, smooth_ry;
	qboolean touch_down;
	uint8_t  touch_id;
	float    touch_fx, touch_fy;
	qboolean wheel_open;
	int      wheel_sel;
	int      menu_num;
	float    fmove, smove;
	float    yaw_delta, pitch_delta;
	uint16_t buttons;
	uint8_t  impulse;
	int      weapon_dir;
	int      action_cmd;
	qboolean l3r3_latch;
	qboolean gyro_enabled;
	int      gyro_warmup;
	float    prev_q[4];
	float    smooth_gx, smooth_gy;
} ps4_extra_pad_t;

static ps4_extra_pad_t ps4_extra_pads[3] =
{
	{ .user_id = -1, .handle = -1, .wheel_sel = -1 },
	{ .user_id = -1, .handle = -1, .wheel_sel = -1 },
	{ .user_id = -1, .handle = -1, .wheel_sel = -1 },
};
static double ps4_next_user_scan = 0.0;

static void PS4_ExtraPadsUpdate( void )
{
	if( ps4pad.handle < 0 || cls.state != ca_active )
		return;
	int32_t main_user = -1;
	sceUserServiceGetInitialUser( &main_user );

	if( host.realtime >= ps4_next_user_scan )
	{
		ps4_next_user_scan = host.realtime + 1.0;
		OrbisUserServiceLoginUserIdList list = { { -1, -1, -1, -1 } };
		if( sceUserServiceGetLoginUserIdList( &list ) >= 0 )
		{
			for( int u = 0; u < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; u++ )
			{
				int32_t uid = list.userId[u];
				if( uid == -1 || uid == main_user )
					continue;

				int assigned = -1;
				for( int s = 0; s < 3; s++ )
				{
					if( ps4_extra_pads[s].user_id == uid )
					{
						assigned = s;
						break;
					}
				}

				if( assigned < 0 )
				{
					for( int s = 0; s < 3; s++ )
					{
						if( ps4_extra_pads[s].handle < 0 )
						{
							int32_t h = scePadOpen( uid, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL );
							if( h < 0 )
								h = scePadGetHandle( uid, ORBIS_PAD_PORT_TYPE_STANDARD, 0 );
							if( h >= 0 )
							{
								scePadSetMotionSensorState( h, true );
								ps4_extra_pads[s].user_id = uid;
								ps4_extra_pads[s].handle = h;
								ps4_extra_pads[s].active = true;
								ps4_extra_pads[s].wheel_open = false;
								ps4_extra_pads[s].wheel_sel = -1;
								ps4_extra_pads[s].gyro_enabled = false;
								ps4_extra_pads[s].gyro_warmup = 3;
								Con_Printf( "^2[PS4 Split-Screen]^7 Mando del Jugador %d conectado (userId 0x%x)\n", s + 2, uid );
							}
							break;
						}
					}
				}
			}
		}
	}

	for( int s = 0; s < 3; s++ )
	{
		ps4_extra_pad_t *ep = &ps4_extra_pads[s];
		if( ep->handle < 0 )
			continue;

		OrbisPadData data = { 0 };
		if( scePadReadState( ep->handle, &data ) < 0 || !data.connected )
		{
			ep->fmove = 0.0f;
			ep->smove = 0.0f;
			ep->yaw_delta = 0.0f;
			ep->pitch_delta = 0.0f;
			ep->buttons = 0;
			continue;
		}

		ep->active = true;
		uint32_t changed = ep->prev_buttons ^ data.buttons;
		uint32_t pressed = changed & data.buttons;
		ep->prev_buttons = data.buttons;

		// Alternar Giroscopio del Jugador 2..4 con L3 + R3, o Rueda Numerica con R3
		{
			extern void PS4_GyroNotifySlot( int slot, qboolean enabled );
			const uint32_t l3r3_mask = ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3;
			if( ( data.buttons & l3r3_mask ) == l3r3_mask )
			{
				if( !ep->l3r3_latch )
				{
					ep->l3r3_latch = true;
					ep->wheel_open = false;
					ep->wheel_sel = -1;
					ep->gyro_enabled = !ep->gyro_enabled;
					ep->gyro_warmup = 3;
					PS4_GyroNotifySlot( s + 1, ep->gyro_enabled );
				}
			}
			else
			{
				ep->l3r3_latch = false;
				if( FBitSet( pressed, ORBIS_PAD_BUTTON_R3 ) && !FBitSet( data.buttons, ORBIS_PAD_BUTTON_L3 ) )
				{
					ep->wheel_open = !ep->wheel_open;
					ep->wheel_sel = -1;
					PS4_SetWheelState( ep->wheel_open, -1 );
				}
			}
		}

		// Giroscopio en ejes locales del mando para Jugador 2..4
		if( ep->gyro_enabled )
		{
			float qx = data.quat.x, qy = data.quat.y, qz = data.quat.z, qw = data.quat.w;
			float norm_sq = qx * qx + qy * qy + qz * qz + qw * qw;
			if( norm_sq > 0.5f && norm_sq < 1.5f )
			{
				float inv_n = 1.0f / sqrtf( norm_sq );
				qx *= inv_n; qy *= inv_n; qz *= inv_n; qw *= inv_n;
				if( ep->gyro_warmup > 0 )
				{
					ep->gyro_warmup--;
					ep->prev_q[0] = qx; ep->prev_q[1] = qy; ep->prev_q[2] = qz; ep->prev_q[3] = qw;
					ep->smooth_gx = 0.0f; ep->smooth_gy = 0.0f;
				}
				else
				{
					float px = ep->prev_q[0], py = ep->prev_q[1], pz = ep->prev_q[2], pw = ep->prev_q[3];
					if( px * qx + py * qy + pz * qz + pw * qw < 0.0f )
					{
						px = -px; py = -py; pz = -pz; pw = -pw;
					}
					float lx_q = pw * qx - px * qw - py * qz + pz * qy;
					float ly_q = pw * qy + px * qz - py * qw - pz * qx;
					float lz_q = pw * qz - px * qy + py * qx - pz * qw;
					float d_horiz = ( ly_q - lz_q * 0.45f ) * 2.0f;
					float d_vert  = lx_q * 2.0f;
					if( d_horiz > -0.25f && d_horiz < 0.25f && d_vert > -0.25f && d_vert < 0.25f )
					{
						if( d_horiz > -0.0003f && d_horiz < 0.0003f ) d_horiz = 0.0f;
						if( d_vert  > -0.0003f && d_vert  < 0.0003f ) d_vert  = 0.0f;
						float raw_dyaw   =  d_horiz * 36.0f;
						float raw_dpitch = -d_vert  * 36.0f;
						ep->smooth_gx = ep->smooth_gx * 0.25f + raw_dyaw   * 0.75f;
						ep->smooth_gy = ep->smooth_gy * 0.25f + raw_dpitch * 0.75f;
						if( ep->smooth_gx > -0.002f && ep->smooth_gx < 0.002f && raw_dyaw   == 0.0f ) ep->smooth_gx = 0.0f;
						if( ep->smooth_gy > -0.002f && ep->smooth_gy < 0.002f && raw_dpitch == 0.0f ) ep->smooth_gy = 0.0f;
						ep->yaw_delta   += ep->smooth_gx;
						ep->pitch_delta += ep->smooth_gy;
					}
					ep->prev_q[0] = qx; ep->prev_q[1] = qy; ep->prev_q[2] = qz; ep->prev_q[3] = qw;
				}
			}
		}

		float rx = ( (int)data.rightStick.x - 128 ) / 127.0f;
		float ry = ( (int)data.rightStick.y - 128 ) / 127.0f;
		float mag = sqrtf( rx * rx + ry * ry );

		if( ep->wheel_open )
		{
			ep->smooth_rx = 0.0f;
			ep->smooth_ry = 0.0f;
			if( mag > 0.55f )
			{
				float angle = atan2f( ry, rx ) + ( 3.14159265f * 0.5f );
				if( angle < 0.0f ) angle += 2.0f * 3.14159265f;
				int sector = (int)floorf( ( angle + ( 3.14159265f / 10.0f ) ) / ( 2.0f * 3.14159265f / 10.0f ) ) % 10;
				ep->wheel_sel = ( sector == 0 ) ? 9 : ( sector - 1 );
				PS4_SetWheelState( true, ep->wheel_sel );
			}
			else if( ep->wheel_sel >= 0 && mag < 0.28f )
			{
				int num = ( ep->wheel_sel == 9 ) ? 10 : ( ep->wheel_sel + 1 );
				ep->menu_num = num;
				ep->wheel_open = false;
				ep->wheel_sel = -1;
				PS4_SetWheelState( false, -1 );
			}
		}
		else
		{
			float tx = 0.0f, ty = 0.0f;
			if( mag > 0.08f )
			{
				float nm = bound( 0.0f, ( mag - 0.08f ) / 0.92f, 1.0f );
				float cm = ( 0.35f * nm + 0.65f * nm * nm * nm );
				tx = ( rx / mag ) * cm;
				ty = ( ry / mag ) * cm;
			}
			ep->smooth_rx = ep->smooth_rx * 0.40f + tx * 0.60f;
			ep->smooth_ry = ep->smooth_ry * 0.40f + ty * 0.60f;
			ep->yaw_delta += -ep->smooth_rx * 140.0f * (float)host.frametime;
			ep->pitch_delta += ep->smooth_ry * 110.0f * (float)host.frametime;
		}

		float lx = ( (int)data.leftStick.x - 128 ) / 127.0f;
		float ly = ( (int)data.leftStick.y - 128 ) / 127.0f;
		ep->smove = ( lx > 0.12f || lx < -0.12f ) ? ( lx * 250.0f ) : 0.0f;
		ep->fmove = ( ly > 0.12f || ly < -0.12f ) ? ( -ly * 250.0f ) : 0.0f;

		if( data.touch.fingers > 0 )
		{
			uint8_t id = data.touch.touch[0].finger;
			uint16_t ptx = data.touch.touch[0].x;
			uint16_t pty = data.touch.touch[0].y;
			if( ep->touch_down && ep->touch_id == id )
			{
				float tdx = (float)ptx - ep->touch_fx;
				float tdy = (float)pty - ep->touch_fy;
				if( tdx * tdx + tdy * tdy > 1.5f )
				{
					ep->yaw_delta -= tdx * 0.12f;
					ep->pitch_delta += tdy * 0.14f;
					ep->touch_fx = (float)ptx;
					ep->touch_fy = (float)pty;
				}
			}
			else
			{
				ep->touch_down = true;
				ep->touch_id = id;
				ep->touch_fx = (float)ptx;
				ep->touch_fy = (float)pty;
			}
		}
		else
		{
			ep->touch_down = false;
		}

		uint16_t btns = 0;
		if( data.analogButtons.r2 > 45 ) btns |= ( 1 << 0 );
		if( data.analogButtons.l2 > 45 ) btns |= ( 1 << 11 );
		if( data.buttons & ORBIS_PAD_BUTTON_CROSS )     btns |= ( 1 << 1 );
		if( data.buttons & ORBIS_PAD_BUTTON_CIRCLE )    btns |= ( 1 << 2 );
		if( data.buttons & ORBIS_PAD_BUTTON_SQUARE )    btns |= ( 1 << 13 );
		if( data.buttons & ORBIS_PAD_BUTTON_TRIANGLE )  btns |= ( 1 << 5 );
		if( data.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD ) btns |= ( 1 << 5 );
		if( ep->fmove > 10.0f )  btns |= ( 1 << 3 );
		if( ep->fmove < -10.0f ) btns |= ( 1 << 4 );
		if( ep->smove < -10.0f ) btns |= ( 1 << 9 );
		if( ep->smove > 10.0f )  btns |= ( 1 << 10 );
		if( data.buttons & ORBIS_PAD_BUTTON_L3 )
		{
			ep->fmove *= 0.52f;
			ep->smove *= 0.52f;
		}
		ep->buttons = btns;

		if( changed & ORBIS_PAD_BUTTON_OPTIONS )
			Key_Event( K_ESCAPE, FBitSet( data.buttons, ORBIS_PAD_BUTTON_OPTIONS ) ? true : false );

		if( cls.key_dest == key_menu )
		{
			if( changed & ORBIS_PAD_BUTTON_UP )    Key_Event( K_UPARROW,    FBitSet( data.buttons, ORBIS_PAD_BUTTON_UP ) ? true : false );
			if( changed & ORBIS_PAD_BUTTON_DOWN )  Key_Event( K_DOWNARROW,  FBitSet( data.buttons, ORBIS_PAD_BUTTON_DOWN ) ? true : false );
			if( changed & ORBIS_PAD_BUTTON_LEFT )  Key_Event( K_LEFTARROW,  FBitSet( data.buttons, ORBIS_PAD_BUTTON_LEFT ) ? true : false );
			if( changed & ORBIS_PAD_BUTTON_RIGHT ) Key_Event( K_RIGHTARROW, FBitSet( data.buttons, ORBIS_PAD_BUTTON_RIGHT ) ? true : false );
			if( changed & ORBIS_PAD_BUTTON_CROSS ) Key_Event( K_ENTER,      FBitSet( data.buttons, ORBIS_PAD_BUTTON_CROSS ) ? true : false );
			if( changed & ORBIS_PAD_BUTTON_CIRCLE )Key_Event( K_ESCAPE,     FBitSet( data.buttons, ORBIS_PAD_BUTTON_CIRCLE ) ? true : false );
			continue;
		}

		if( pressed & ORBIS_PAD_BUTTON_RIGHT )    ep->impulse = 201;
		if( pressed & ORBIS_PAD_BUTTON_L1 )       ep->weapon_dir = -1;
		if( pressed & ORBIS_PAD_BUTTON_R1 )       ep->weapon_dir = 1;
		if( pressed & ORBIS_PAD_BUTTON_DOWN )     ep->action_cmd = 1; // chooseteam
		if( pressed & ORBIS_PAD_BUTTON_LEFT )     ep->action_cmd = 2; // buy
		if( pressed & ORBIS_PAD_BUTTON_UP )       ep->action_cmd = 3; // drop
	}
}

qboolean PS4_GetSplitPlayerInput( int slot, float *fmove, float *smove, float *yaw_d, float *pitch_d, uint16_t *buttons, uint8_t *impulse, int *weapon_dir, int *action_cmd, int *menu_num );
qboolean PS4_GetSplitPlayerInput( int slot, float *fmove, float *smove, float *yaw_d, float *pitch_d, uint16_t *buttons, uint8_t *impulse, int *weapon_dir, int *action_cmd, int *menu_num )
{
	if( slot < 1 || slot > 3 )
		return false;
	ps4_extra_pad_t *ep = &ps4_extra_pads[slot - 1];
	if( !ep->active || ep->handle < 0 )
		return false;

	*fmove = ep->fmove;
	*smove = ep->smove;
	*yaw_d = ep->yaw_delta;
	*pitch_d = ep->pitch_delta;
	*buttons = ep->buttons;
	*impulse = ep->impulse;
	*weapon_dir = ep->weapon_dir;
	*action_cmd = ep->action_cmd;
	*menu_num = ep->menu_num;

	ep->yaw_delta = 0.0f;
	ep->pitch_delta = 0.0f;
	ep->impulse = 0;
	ep->weapon_dir = 0;
	ep->action_cmd = 0;
	ep->menu_num = 0;
	return true;
}

void Platform_RunEvents( void )
{
	// the system keyboard takes the keyboard and the mouse is of no use while it is open
	if( !ps4ime.open )
		PS4_KbmUpdate( );

	PS4_PadUpdate( );
	PS4_ExtraPadsUpdate( );
}

int Platform_JoyInit( void )
{
	int32_t user = -1;
	int ret;

	if( ps4pad.handle >= 0 )
		return 1;

	Con_Reportf( "Joystick: PS4 libScePad\n" );

	if(( ret = scePadInit( )) < 0 )
	{
		Con_Printf( S_ERROR "scePadInit failed: 0x%08x\n", ret );
		return 0;
	}

	if(( ret = sceUserServiceGetInitialUser( &user )) < 0 )
	{
		Con_Printf( S_ERROR "sceUserServiceGetInitialUser failed: 0x%08x\n", ret );
		return 0;
	}

	ps4pad.handle = scePadOpen( user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL );

	// might be already opened by somebody else
	if( ps4pad.handle < 0 )
		ps4pad.handle = scePadGetHandle( user, ORBIS_PAD_PORT_TYPE_STANDARD, 0 );

	if( ps4pad.handle < 0 )
	{
		Con_Printf( S_ERROR "scePadOpen failed: 0x%08x\n", ps4pad.handle );
		return 0;
	}

	ps4pad.buttons = 0;
	memset( ps4pad.axes, 0, sizeof( ps4pad.axes ));

	// gyroscope and touchpad of DualShock 4 aren't read yet
	Joy_SetCapabilities( false, 0, "playstation" );

	return 1;
}

void Platform_JoyShutdown( void )
{
	PS4_StopVibration( );

	if( ps4pad.handle >= 0 )
		scePadClose( ps4pad.handle );

	ps4pad.handle = -1;
}

void Platform_CalibrateGamepadGyro( void )
{
}

void Platform_Vibrate2( float time, int val1, int val2, uint flags )
{
	OrbisPadVibeParam vibe;

	if( ps4pad.handle < 0 )
		return;

	// strength of the common gamepad rumble
	if( val1 < 0 )
		val1 = COM_RandomLong( 0x7FFF, 0xFFFF );
	if( val2 < 0 )
		val2 = COM_RandomLong( 0x7FFF, 0xFFFF );

	vibe.lgMotor = (uint8_t)( bound( 0, val1, 0xFFFF ) >> 8 );
	vibe.smMotor = (uint8_t)( bound( 0, val2, 0xFFFF ) >> 8 );

	if( scePadSetVibration( ps4pad.handle, &vibe ) >= 0 )
	{
		ps4pad.vibrating = true;
		ps4pad.vibrate_until = host.realtime + time / 1000.0f; // time is in milliseconds
	}
}

void Platform_Vibrate( float time, char flags )
{
	Platform_Vibrate2( time, -1, -1, flags );
}

qboolean PS4_GetPlayerWheelState( int slot, int *out_selected );
qboolean PS4_GetPlayerWheelState( int slot, int *out_selected )
{
	if( slot <= 0 )
	{
		if( out_selected ) *out_selected = ps4pad.wheel_sel;
		return ps4pad.wheel_open;
	}
	if( slot >= 1 && slot <= 3 )
	{
		if( out_selected ) *out_selected = ps4_extra_pads[slot - 1].wheel_sel;
		return ps4_extra_pads[slot - 1].wheel_open;
	}
	if( out_selected ) *out_selected = -1;
	return false;
}
