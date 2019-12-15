/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../client/client.h"
#include "sdl_local.h"

#ifdef MACOS_X
// Mouse acceleration needs to be disabled
#define MACOS_X_ACCELERATION_HACK
// Cursor needs hack to hide
#define MACOS_X_CURSOR_HACK
#endif

#ifdef MACOS_X_ACCELERATION_HACK
#include <IOKit/IOTypes.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
#endif

static cvar_t *in_keyboardDebug     = NULL;

static SDL_GameController *gamepad = NULL;
static SDL_Joystick *stick = NULL;

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive = qfalse;
static qboolean keyRepeatEnabled = qfalse;

static cvar_t *in_mouse             = NULL;
#ifdef MACOS_X_ACCELERATION_HACK
static cvar_t *in_disablemacosxmouseaccel = NULL;
static double originalMouseSpeed = -1.0;
#endif
static cvar_t *in_nograb;

static cvar_t *in_joystick          = NULL;
static cvar_t *in_joystickDebug     = NULL;
static cvar_t *in_joystickThreshold = NULL;
static cvar_t *in_joystickNo        = NULL;
static cvar_t *in_joystickUseAnalog = NULL;

static int vidRestartTime = 0;

static int in_eventTime = 0;

static SDL_Window *SDL_window = NULL;

#define CTRL(a) ((a)-'a'+1)

/*
===============
IN_PrintKey
===============
*/
static void IN_PrintKey( const SDL_Keysym *keysym, int key, qboolean down )
{
	if( down )
		Com_Printf( "+ " );
	else
		Com_Printf( "  " );

	Com_Printf( "0x%02x \"%s\"", keysym->scancode,
			SDL_GetKeyName( keysym->sym ) );

	if( keysym->mod & KMOD_LSHIFT )   Com_Printf( " KMOD_LSHIFT" );
	if( keysym->mod & KMOD_RSHIFT )   Com_Printf( " KMOD_RSHIFT" );
	if( keysym->mod & KMOD_LCTRL )    Com_Printf( " KMOD_LCTRL" );
	if( keysym->mod & KMOD_RCTRL )    Com_Printf( " KMOD_RCTRL" );
	if( keysym->mod & KMOD_LALT )     Com_Printf( " KMOD_LALT" );
	if( keysym->mod & KMOD_RALT )     Com_Printf( " KMOD_RALT" );
	if( keysym->mod & KMOD_LGUI )     Com_Printf( " KMOD_LGUI" );
	if( keysym->mod & KMOD_RGUI )     Com_Printf( " KMOD_RGUI" );
	if( keysym->mod & KMOD_NUM )      Com_Printf( " KMOD_NUM" );
	if( keysym->mod & KMOD_CAPS )     Com_Printf( " KMOD_CAPS" );
	if( keysym->mod & KMOD_MODE )     Com_Printf( " KMOD_MODE" );
	if( keysym->mod & KMOD_RESERVED ) Com_Printf( " KMOD_RESERVED" );

	Com_Printf( " Q:0x%02x(%s)", key, Key_KeynumToString( key ) );
/*
	if( keysym->unicode )
	{
		Com_Printf( " U:0x%02x", keysym->unicode );

		if( keysym->unicode > ' ' && keysym->unicode < '~' )
			Com_Printf( "(%c)", (char)keysym->unicode );
	}*/

	Com_Printf( "\n" );
}

#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey
===============
*/
static qboolean IN_IsConsoleKey( int key, const unsigned char character )
{
	typedef struct consoleKey_s
	{
		enum
		{
			KEY,
			CHARACTER
		} type;

		union
		{
			int key;
			unsigned char character;
		} u;
	} consoleKey_t;

	static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
	static int numConsoleKeys = 0;
	int i;

	// Only parse the variable when it changes
	if( cl_consoleKeys->modified )
	{
		const char *text_p, *token;

		cl_consoleKeys->modified = qfalse;
		text_p = cl_consoleKeys->string;
		numConsoleKeys = 0;

		while( numConsoleKeys < MAX_CONSOLE_KEYS )
		{
			consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
			int charCode = 0;

			token = COM_Parse( &text_p );
			if( !token[ 0 ] )
				break;

			if( strlen( token ) == 4 )
				charCode = Com_HexStrToInt( token );

			if( charCode > 0 )
			{
				c->type = consoleKey_t::CHARACTER;
				c->u.character = (unsigned char)charCode;
			}
			else
			{
				c->type = consoleKey_t::KEY;
				c->u.key = Key_StringToKeynum( token );

				// 0 isn't a key
				if( c->u.key <= 0 )
					continue;
			}

			numConsoleKeys++;
		}
	}

	// If the character is the same as the key, prefer the character
	if( key == character )
		key = 0;

	for( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch( c->type )
		{
			case consoleKey_t::KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case consoleKey_t::CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}

/*
===============
IN_TranslateSDLToQ3Key
===============
*/
static const char *IN_TranslateSDLToQ3Key( SDL_Keysym *keysym,
	int *key, qboolean down )
{
	static unsigned char buf[ 2 ] = { '\0', '\0' };

	*buf = '\0';
	*key = 0;

	if( keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0 )
	{
		// Always map the number keys as such even if they actually map
		// to other characters (eg, "1" is "&" on an AZERTY keyboard).
		// This is required for SDL before 2.0.6, except on Windows
		// which already had this behavior.
		if( keysym->scancode == SDL_SCANCODE_0 )
			*key = '0';
		else
			*key = '1' + keysym->scancode - SDL_SCANCODE_1;
	}
	else if( keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE )
	{
		// These happen to match the ASCII chars
		*key = (int)keysym->sym;
	}
	else
	{
		switch( keysym->sym )
		{
			case SDLK_PAGEUP:       *key = A_PAGE_UP;       break;
			case SDLK_KP_9:         *key = A_KP_9;          break;
			case SDLK_PAGEDOWN:     *key = A_PAGE_DOWN;     break;
			case SDLK_KP_3:         *key = A_KP_3;          break;
			case SDLK_KP_7:         *key = A_KP_7;          break;
			case SDLK_HOME:         *key = A_HOME;          break;
			case SDLK_KP_1:         *key = A_KP_1;          break;
			case SDLK_END:          *key = A_END;           break;
			case SDLK_KP_4:         *key = A_KP_4;          break;
			case SDLK_LEFT:         *key = A_CURSOR_LEFT;   break;
			case SDLK_KP_6:         *key = A_KP_6;          break;
			case SDLK_RIGHT:        *key = A_CURSOR_RIGHT;  break;
			case SDLK_KP_2:         *key = A_KP_2;          break;
			case SDLK_DOWN:         *key = A_CURSOR_DOWN;   break;
			case SDLK_KP_8:         *key = A_KP_8;          break;
			case SDLK_UP:           *key = A_CURSOR_UP;     break;
			case SDLK_ESCAPE:       *key = A_ESCAPE;        break;
			case SDLK_KP_ENTER:     *key = A_KP_ENTER;      break;
			case SDLK_RETURN:       *key = A_ENTER;         break;
			case SDLK_TAB:          *key = A_TAB;           break;
			case SDLK_F1:           *key = A_F1;            break;
			case SDLK_F2:           *key = A_F2;            break;
			case SDLK_F3:           *key = A_F3;            break;
			case SDLK_F4:           *key = A_F4;            break;
			case SDLK_F5:           *key = A_F5;            break;
			case SDLK_F6:           *key = A_F6;            break;
			case SDLK_F7:           *key = A_F7;            break;
			case SDLK_F8:           *key = A_F8;            break;
			case SDLK_F9:           *key = A_F9;            break;
			case SDLK_F10:          *key = A_F10;           break;
			case SDLK_F11:          *key = A_F11;           break;
			case SDLK_F12:          *key = A_F12;           break;
#if 0
			case SDLK_F13:          *key = A_F13;           break;
			case SDLK_F14:          *key = A_F14;           break;
			case SDLK_F15:          *key = A_F15;           break;
#endif

			case SDLK_BACKSPACE:    *key = A_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    *key = A_KP_PERIOD;     break;
			case SDLK_DELETE:       *key = A_DELETE;        break;
			case SDLK_PAUSE:        *key = A_PAUSE;         break;

			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       *key = A_SHIFT;         break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:        *key = A_CTRL;          break;

#if 0
			case SDLK_RMETA:
			case SDLK_LMETA:        *key = A_COMMAND;       break;
#endif

			case SDLK_RALT:
			case SDLK_LALT:         *key = A_ALT;           break;

#if 0
			case SDLK_LSUPER:
			case SDLK_RSUPER:       *key = A_SUPER;         break;
#endif

			case SDLK_KP_5:         *key = A_KP_5;          break;
			case SDLK_INSERT:       *key = A_INSERT;        break;
			case SDLK_KP_0:         *key = A_KP_0;          break;
			case SDLK_KP_MULTIPLY:  *key = A_STAR;          break;
			case SDLK_KP_PLUS:      *key = A_KP_PLUS;       break;
			case SDLK_KP_MINUS:     *key = A_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    *key = A_DIVIDE;        break;
#if 0
			case SDLK_MODE:         *key = A_MODE;          break;
			case SDLK_COMPOSE:      *key = A_COMPOSE;       break;
			case SDLK_HELP:         *key = A_HELP;          break;
#endif
			case SDLK_PRINTSCREEN:  *key = A_PRINTSCREEN;   break;
#if 0
			case SDLK_SYSREQ:       *key = A_SYSREQ;        break;
			case SDLK_BREAK:        *key = A_BREAK;         break;
			case SDLK_MENU:         *key = A_MENU;          break;
			case SDLK_POWER:        *key = A_POWER;         break;
#endif
//			case SDLK_EURO:         *key = A_EURO;          break;
//			case SDLK_UNDO:         *key = A_UNDO;          break;
			case SDLK_SCROLLLOCK:   *key = A_SCROLLLOCK;    break;
			case SDLK_NUMLOCKCLEAR: *key = A_NUMLOCK;       break;
			case SDLK_CAPSLOCK:     *key = A_CAPSLOCK;      break;

			default:
#if 0
				if( keysym->sym >= SDLK_WORLD_0 && keysym->sym <= SDLK_WORLD_95 )
					*key = ( keysym->sym - SDLK_WORLD_0 ) + K_WORLD_0;
#endif
				break;
		}
	}

	/*
	if( down && keysym->unicode && !( keysym->unicode & 0xFF00 ) )
	{
		unsigned char ch = (unsigned char)keysym->unicode & 0xFF;

		switch( ch )
		{
			case 127: // ASCII delete
				if( *key != A_DELETE )
				{
					// ctrl-h
					*buf = CTRL('h');
					break;
				}
				// fallthrough

			default: *buf = ch; break;
		}
	}*/

	if( in_keyboardDebug->integer )
		IN_PrintKey( keysym, *key, down );

	if( IN_IsConsoleKey( *key, *buf ) )
	{
		// Console keys can't be bound or generate characters
		*key = A_CONSOLE;
		*buf = '\0';
	}

	// Don't allow extended ASCII to generate characters
	if( *buf & 0x80 )
		*buf = '\0';

	return (char *)buf;
}

#ifdef MACOS_X_ACCELERATION_HACK
/*
===============
IN_GetIOHandle
===============
*/
static io_connect_t IN_GetIOHandle(void) // mac os x mouse accel hack
{
	io_connect_t iohandle = MACH_PORT_NULL;
	kern_return_t status;
	io_service_t iohidsystem = MACH_PORT_NULL;
	mach_port_t masterport;

	status = IOMasterPort(MACH_PORT_NULL, &masterport);
	if(status != KERN_SUCCESS)
		return 0;

	iohidsystem = IORegistryEntryFromPath(masterport, kIOServicePlane ":/IOResources/IOHIDSystem");
	if(!iohidsystem)
		return 0;

	status = IOServiceOpen(iohidsystem, mach_task_self(), kIOHIDParamConnectType, &iohandle);
	IOObjectRelease(iohidsystem);

	return iohandle;
}
#endif

/*
===============
IN_GobbleMotionEvents
===============
*/
static void IN_GobbleMotionEvents( void )
{
	SDL_Event dummy[ 1 ];
    int val = 0;
    
	// Gobble any mouse motion events
	SDL_PumpEvents( );
	while( ( val = SDL_PeepEvents( dummy, 1, SDL_GETEVENT,
		SDL_MOUSEMOTION, SDL_MOUSEMOTION ) ) > 0 ) { }

	if ( val < 0 )
		Com_Printf( "IN_GobbleMotionEvents failed: %s\n", SDL_GetError( ) );
}

/*
===============
IN_ActivateMouse
===============
*/
static void IN_ActivateMouse( qboolean isFullscreen )
{
	if (!mouseAvailable || !SDL_WasInit( SDL_INIT_VIDEO ) )
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	if (!mouseActive) // mac os x mouse accel hack
	{
		// Save the status of mouse acceleration
		originalMouseSpeed = -1.0; // in case of error
		if(in_disablemacosxmouseaccel->integer)
		{
			io_connect_t mouseDev = IN_GetIOHandle();
			if(mouseDev != 0)
			{
				if(IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &originalMouseSpeed) == kIOReturnSuccess)
				{
					Com_Printf("previous mouse acceleration: %f\n", originalMouseSpeed);
					if(IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
					{
						Com_Printf("Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
						Cvar_Set ("in_disablemacosxmouseaccel", 0);
					}
				}
				else
				{
					Com_Printf("Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
					Cvar_Set ("in_disablemacosxmouseaccel", 0);
				}
				IOServiceClose(mouseDev);
			}
			else
			{
				Com_Printf("Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
				Cvar_Set ("in_disablemacosxmouseaccel", 0);
			}
		}
	}
#endif

	if( !mouseActive )
	{
		SDL_SetRelativeMouseMode( SDL_TRUE );
		SDL_SetWindowGrab( SDL_window, SDL_TRUE );

		IN_GobbleMotionEvents( );
	}

	// in_nograb makes no sense in fullscreen mode
	if( !isFullscreen )
	{
		if( in_nograb->modified || !mouseActive )
		{
			if( in_nograb->integer ) {
				SDL_SetRelativeMouseMode( SDL_FALSE );
				SDL_SetWindowGrab( SDL_window, SDL_FALSE );
			} else {
				SDL_SetRelativeMouseMode( SDL_TRUE );
				SDL_SetWindowGrab( SDL_window, SDL_TRUE );
			}

			in_nograb->modified = qfalse;
		}
	}

	mouseActive = qtrue;
}

/*
===============
IN_DeactivateMouse
===============
*/
static void IN_DeactivateMouse( qboolean isFullscreen )
{
	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
		return;

	// Always show the cursor when the mouse is disabled,
	// but not when fullscreen
	if( !isFullscreen )
		SDL_ShowCursor( SDL_TRUE );

	if( !mouseAvailable )
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	if (mouseActive) // mac os x mouse accel hack
	{
		if(originalMouseSpeed != -1.0)
		{
			io_connect_t mouseDev = IN_GetIOHandle();
			if(mouseDev != 0)
			{
				Com_Printf("restoring mouse acceleration to: %f\n", originalMouseSpeed);
				if(IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), originalMouseSpeed) != kIOReturnSuccess)
					Com_Printf("Could not re-enable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
				IOServiceClose(mouseDev);
			}
			else
				Com_Printf("Could not re-enable mouse acceleration (failed at IO_GetIOHandle).\n");
		}
	}
#endif

	if( mouseActive )
	{
		IN_GobbleMotionEvents( );

		SDL_SetWindowGrab( SDL_window, SDL_FALSE );
		SDL_SetRelativeMouseMode( SDL_FALSE );

		// Don't warp the mouse unless the cursor is within the window
		if( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_MOUSE_FOCUS )
			SDL_WarpMouseInWindow( SDL_window, cls.glconfig.vidWidth / 2, cls.glconfig.vidHeight / 2 );

		mouseActive = qfalse;
	}
}

// We translate axes movement into keypresses
static int joy_keys[16] = {
	A_CURSOR_LEFT, A_CURSOR_RIGHT,
	A_CURSOR_UP, A_CURSOR_DOWN,
	A_JOY16, A_JOY17,
	A_JOY18, A_JOY19,
	A_JOY20, A_JOY21,
	A_JOY22, A_JOY23,
	A_JOY24, A_JOY25,
	A_JOY26, A_JOY27
};

// translate hat events into keypresses
// the 4 highest buttons are used for the first hat ...
static int hat_keys[16] = {
	A_JOY28, A_JOY29,
	A_JOY30, A_JOY31,
	A_JOY24, A_JOY25,
	A_JOY26, A_JOY27,
	A_JOY20, A_JOY21,
	A_JOY22, A_JOY23,
	A_JOY16, A_JOY17,
	A_JOY18, A_JOY19
};


struct
{
	qboolean buttons[SDL_CONTROLLER_BUTTON_MAX + 1]; // +1 because old max was 16, current SDL_CONTROLLER_BUTTON_MAX is 15
	unsigned int oldaxes;
	int oldaaxes[MAX_JOYSTICK_AXIS];
	unsigned int oldhats;
} stick_state;


/*
===============
IN_InitJoystick
===============
*/
static void IN_InitJoystick( void )
{
	int i = 0;
	int total = 0;
	char buf[16384] = "";

    if (gamepad)
		SDL_GameControllerClose(gamepad);
		
	if (stick != NULL)
		SDL_JoystickClose(stick);

	stick = NULL;
	gamepad = NULL;
	memset(&stick_state, '\0', sizeof (stick_state));

	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		if (SDL_Init(SDL_INIT_JOYSTICK) == -1)
		{
			Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMECONTROLLER)...\n");
		if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
		{
			Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) passed.\n");
	}
	
	total = SDL_NumJoysticks();
	Com_DPrintf("%d possible joysticks\n", total);

	// Print list and build cvar to allow ui to select joystick.
	for (i = 0; i < total; i++)
	{
		Q_strcat(buf, sizeof(buf), SDL_JoystickNameForIndex(i));
		Q_strcat(buf, sizeof(buf), "\n");
	}

	Cvar_Get( "in_availableJoysticks", buf, CVAR_ROM );

	if( !in_joystick->integer ) {
		Com_DPrintf( "Joystick is not active.\n" );
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
		return;
	}

	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	if( in_joystickNo->integer < 0 || in_joystickNo->integer >= total )
		Cvar_Set( "in_joystickNo", "0" );

	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );

	stick = SDL_JoystickOpen( in_joystickNo->integer );

	if (stick == NULL) {
		Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
		return;
	}
	
    if (SDL_IsGameController(in_joystickNo->integer))
		gamepad = SDL_GameControllerOpen(in_joystickNo->integer);

	Com_DPrintf( "Joystick %d opened\n", in_joystickNo->integer );
	Com_DPrintf( "Name:       %s\n", SDL_JoystickNameForIndex(in_joystickNo->integer) );
	Com_DPrintf( "Axes:       %d\n", SDL_JoystickNumAxes(stick) );
	Com_DPrintf( "Hats:       %d\n", SDL_JoystickNumHats(stick) );
	Com_DPrintf( "Buttons:    %d\n", SDL_JoystickNumButtons(stick) );
	Com_DPrintf( "Balls:      %d\n", SDL_JoystickNumBalls(stick) );
	Com_DPrintf( "Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
    Com_DPrintf( "Is gamepad: %s\n", gamepad ? "Yes" : "No" );
    
	SDL_JoystickEventState(SDL_QUERY);
	SDL_GameControllerEventState(SDL_QUERY);
}

/*
===============
IN_ShutdownJoystick
===============
*/
static void IN_ShutdownJoystick( void )
{
	if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) )
		return;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) )
		return;

	if (gamepad)
	{
		SDL_GameControllerClose(gamepad);
		gamepad = NULL;
	}

	if (stick)
	{
		SDL_JoystickClose(stick);
		stick = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
	char *bind;

	if (!keynum)
		return qfalse;

	bind = Key_GetBinding(keynum);

	if (!bind || *bind != '+')
		return qfalse;

	*outSign = 0;

	if (Q_stricmp(bind, "+forward") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+back") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveleft") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveright") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+lookup") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+lookdown") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+left") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+right") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveup") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+movedown") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? -1 : 1;
	}

	return *outSign != 0;
}

/*
===============
IN_GamepadMove
===============
*/
static void IN_GamepadMove( void )
{
	int i;
	int translatedAxes[MAX_JOYSTICK_AXIS];
	qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

	SDL_GameControllerUpdate();

	// check buttons
	for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
	{
		qboolean pressed = SDL_GameControllerGetButton(gamepad, (SDL_GameControllerButton) i);
		if (pressed != stick_state.buttons[i])
		{
			Sys_QueEvent(0, SE_KEY, A_PAD0_A + i, pressed, 0, NULL);
			stick_state.buttons[i] = pressed;
		}
	}

	// must defer translated axes until all real axes are processed
	// must be done this way to prevent a later mapped axis from zeroing out a previous one
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			translatedAxes[i] = 0;
			translatedAxesSet[i] = qfalse;
		}
	}

	// check axes
	for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
	{
		int axis = SDL_GameControllerGetAxis(gamepad, (SDL_GameControllerAxis) i);
		int oldAxis = stick_state.oldaaxes[i];

		// Smoothly ramp from dead zone to maximum value
		float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

		if (f < 0.0f)
			f = 0.0f;

		axis = (int)(32767 * ((axis < 0) ? -f : f));

		if (axis != oldAxis)
		{
			const int negMap[SDL_CONTROLLER_AXIS_MAX] = { A_PAD0_LEFTSTICK_LEFT,  A_PAD0_LEFTSTICK_UP,   A_PAD0_RIGHTSTICK_LEFT,  A_PAD0_RIGHTSTICK_UP, 0, 0 };
			const int posMap[SDL_CONTROLLER_AXIS_MAX] = { A_PAD0_LEFTSTICK_RIGHT, A_PAD0_LEFTSTICK_DOWN, A_PAD0_RIGHTSTICK_RIGHT, A_PAD0_RIGHTSTICK_DOWN, A_PAD0_LEFTTRIGGER, A_PAD0_RIGHTTRIGGER };

			qboolean posAnalog = qfalse, negAnalog = qfalse;
			int negKey = negMap[i];
			int posKey = posMap[i];

			if (in_joystickUseAnalog->integer)
			{
				int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

				// get axes and axes signs for keys if available
				posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
				negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

				// positive to negative/neutral -> keyup if axis hasn't yet been set
				if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
				{
					translatedAxes[posAxis] = 0;
					translatedAxesSet[posAxis] = qtrue;
				}

				// negative to positive/neutral -> keyup if axis hasn't yet been set
				if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
				{
					translatedAxes[negAxis] = 0;
					translatedAxesSet[negAxis] = qtrue;
				}

				// negative/neutral to positive -> keydown
				if (posAnalog && axis > 0)
				{
					translatedAxes[posAxis] = axis * posSign;
					translatedAxesSet[posAxis] = qtrue;
				}

				// positive/neutral to negative -> keydown
				if (negAnalog && axis < 0)
				{
					translatedAxes[negAxis] = -axis * negSign;
					translatedAxesSet[negAxis] = qtrue;
				}
			}

			// keyups first so they get overridden by keydowns later

			// positive to negative/neutral -> keyup
			if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
				Sys_QueEvent(0, SE_KEY, posKey, qfalse, 0, NULL);

			// negative to positive/neutral -> keyup
			if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
				Sys_QueEvent(0, SE_KEY, negKey, qfalse, 0, NULL);

			// negative/neutral to positive -> keydown
			if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
				Sys_QueEvent(0, SE_KEY, posKey, qtrue, 0, NULL);

			// positive/neutral to negative -> keydown
			if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
				Sys_QueEvent(0, SE_KEY, negKey, qtrue, 0, NULL);

			stick_state.oldaaxes[i] = axis;
		}
	}

	// set translated axes
	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			if (translatedAxesSet[i])
                Sys_QueEvent(0, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
		}
	}
}

/*
===============
IN_JoyMove
===============
*/
static void IN_JoyMove( void )
{
	qboolean joy_pressed[ARRAY_LEN(joy_keys)];
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	if (gamepad)
	{
		IN_GamepadMove();
		return;
	}
	
	if (!stick)
		return;

	SDL_JoystickUpdate();

	memset(joy_pressed, '\0', sizeof (joy_pressed));

	// update the ball state.
	total = SDL_JoystickNumBalls(stick);
	if (total > 0)
	{
		int balldx = 0;
		int balldy = 0;
		for (i = 0; i < total; i++)
		{
			int dx = 0;
			int dy = 0;
			SDL_JoystickGetBall(stick, i, &dx, &dy);
			balldx += dx;
			balldy += dy;
		}
		if (balldx || balldy)
		{
			// !!! FIXME: is this good for stick balls, or just mice?
			// Scale like the mouse input...
			if (abs(balldx) > 1)
				balldx *= 2;
			if (abs(balldy) > 1)
				balldy *= 2;
			Sys_QueEvent( 0, SE_MOUSE, balldx, balldy, 0, NULL );
		}
	}

	// now query the stick buttons...
	total = SDL_JoystickNumButtons(stick);
	if (total > 0)
	{
		if (total > ARRAY_LEN(stick_state.buttons))
			total = ARRAY_LEN(stick_state.buttons);
		for (i = 0; i < total; i++)
		{
			qboolean pressed = ((SDL_JoystickGetButton(stick, i) != 0)) ? qtrue : qfalse;
			if (pressed != stick_state.buttons[i])
			{
				Sys_QueEvent( 0, SE_KEY, A_JOY0 + i, pressed, 0, NULL );
				stick_state.buttons[i] = pressed;
			}
		}
	}

	// look at the hats...
	total = SDL_JoystickNumHats(stick);
	if (total > 0)
	{
		if (total > 4) total = 4;
		for (i = 0; i < total; i++)
		{
			((Uint8 *)&hats)[i] = SDL_JoystickGetHat(stick, i);
		}
	}

	// update hat state
	if (hats != stick_state.oldhats)
	{
		for( i = 0; i < 4; i++ ) {
			if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
				// release event
				switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
					case SDL_HAT_UP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					default:
						break;
				}
				// press event
				switch( ((Uint8 *)&hats)[i] ) {
					case SDL_HAT_UP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Sys_QueEvent( 0, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					default:
						break;
				}
			}
		}
	}

	// save hat state
	stick_state.oldhats = hats;

	// finally, look at the axes...
	total = SDL_JoystickNumAxes(stick);
	if (total > 0)
	{
		if (in_joystickUseAnalog->integer)
		{
			if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_JoystickGetAxis(stick, i);
				float f = ( (float) abs(axis) ) / 32767.0f;
				
				if( f < in_joystickThreshold->value ) axis = 0;

				if ( axis != stick_state.oldaaxes[i] )
				{
					Sys_QueEvent( 0, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
					stick_state.oldaaxes[i] = axis;
				}
			}
		}
		else
		{
			if (total > 16) total = 16;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_JoystickGetAxis(stick, i);
				float f = ( (float) axis ) / 32767.0f;
				if( f < -in_joystickThreshold->value ) {
					axes |= ( 1 << ( i * 2 ) );
				} else if( f > in_joystickThreshold->value ) {
					axes |= ( 1 << ( ( i * 2 ) + 1 ) );
				}
			}
		}
	}

	/* Time to update axes state based on old vs. new. */
	if (axes != stick_state.oldaxes)
	{
		for( i = 0; i < 16; i++ ) {
			if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
				Sys_QueEvent( 0, SE_KEY, joy_keys[i], qtrue, 0, NULL );
			}

			if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
				Sys_QueEvent( 0, SE_KEY, joy_keys[i], qfalse, 0, NULL );
			}
		}
	}

	/* Save for future generations. */
	stick_state.oldaxes = axes;
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
	SDL_Event e;
	const char *character = NULL;
	int key = 0;
	static int lastKeyDown = 0;

	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
			return;

	while( SDL_PollEvent( &e ) )
	{
		switch( e.type )
		{
			case SDL_KEYDOWN:
				if ( e.key.repeat && Key_GetCatcher( ) == 0 )
					break;
					
                character = IN_TranslateSDLToQ3Key( &e.key.keysym, &key, qtrue );
                if( key )
					Sys_QueEvent( 0, SE_KEY, key, qtrue, 0, NULL );
                if( character )
                    Sys_QueEvent( 0, SE_CHAR, *character, 0, 0, NULL );
				if( key == A_BACKSPACE )
					Sys_QueEvent( 0, SE_CHAR, CTRL('h'), 0, 0, NULL );
				else if( kg.keys[A_CTRL].down && key >= 'a' && key <= 'z' )
					Sys_QueEvent( 0, SE_CHAR, CTRL(key), 0, 0, NULL );

				lastKeyDown = key;
				break;

			case SDL_KEYUP:
                character = IN_TranslateSDLToQ3Key( &e.key.keysym, &key, qtrue );
                if( key )
					Sys_QueEvent( 0, SE_KEY, key, qfalse, 0, NULL );

				lastKeyDown = 0;
				break;

			case SDL_TEXTINPUT:
				if( lastKeyDown != A_CONSOLE )
				{
					char *c = e.text.text;

					// Quick and dirty UTF-8 to UTF-32 conversion
					while( *c )
					{
						int utf32 = 0;

						if( ( *c & 0x80 ) == 0 )
							utf32 = *c++;
						else if( ( *c & 0xE0 ) == 0xC0 ) // 110x xxxx
						{
							utf32 |= ( *c++ & 0x1F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF0 ) == 0xE0 ) // 1110 xxxx
						{
							utf32 |= ( *c++ & 0x0F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF8 ) == 0xF0 ) // 1111 0xxx
						{
							utf32 |= ( *c++ & 0x07 ) << 18;
							utf32 |= ( *c++ & 0x3F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else
						{
							Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
							c++;
						}

						if( utf32 != 0 )
						{
							if( IN_IsConsoleKey( 0, utf32 ) )
							{
								Sys_QueEvent( 0, SE_KEY, A_CONSOLE, qtrue, 0, NULL );
								Sys_QueEvent( 0, SE_KEY, A_CONSOLE, qfalse, 0, NULL );
							}
							else
								Sys_QueEvent( 0, SE_CHAR, utf32, 0, 0, NULL );
						}
                    }
                }
				break;

			case SDL_MOUSEMOTION:
				if( mouseActive )
				{
					if( !e.motion.xrel && !e.motion.yrel )
						break;
					Sys_QueEvent( 0, SE_MOUSE, e.motion.xrel, e.motion.yrel, 0, NULL );
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					int b;
					switch( e.button.button )
					{
						case SDL_BUTTON_LEFT:   b = A_MOUSE1;     break;
						case SDL_BUTTON_MIDDLE: b = A_MOUSE3;     break;
						case SDL_BUTTON_RIGHT:  b = A_MOUSE2;     break;
						case SDL_BUTTON_X1:     b = A_MOUSE4;     break;
						case SDL_BUTTON_X2:     b = A_MOUSE5;     break;
						default:                b = A_AUX1 + ( e.button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
					}
					Sys_QueEvent( 0, SE_KEY, b,
						( e.type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse ), 0, NULL );
				}
				break;

			case SDL_MOUSEWHEEL:
				if( e.wheel.y > 0 )
				{
					Sys_QueEvent( 0, SE_KEY, A_MWHEELUP, qtrue, 0, NULL );
					Sys_QueEvent( 0, SE_KEY, A_MWHEELUP, qfalse, 0, NULL );
				}
				else if( e.wheel.y < 0 )
				{
					Sys_QueEvent( 0, SE_KEY, A_MWHEELDOWN, qtrue, 0, NULL );
					Sys_QueEvent( 0, SE_KEY, A_MWHEELDOWN, qfalse, 0, NULL );
				}
				break;

			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
				if (in_joystick->integer)
					IN_InitJoystick();
				break;

			case SDL_QUIT:
				Cbuf_ExecuteText(EXEC_NOW, "quit Closed window\n");
				break;

			case SDL_WINDOWEVENT:
				switch( e.window.event )
				{
					case SDL_WINDOWEVENT_RESIZED:
						{
							int width, height;

							width = e.window.data1;
							height = e.window.data2;

							// ignore this event on fullscreen
							if( cls.glconfig.isFullscreen )
							{
								break;
							}

							// check if size actually changed
							if( cls.glconfig.vidWidth == width && cls.glconfig.vidHeight == height )
							{
								break;
							}

							Cvar_SetValue( "r_customwidth", width );
							Cvar_SetValue( "r_customheight", height );
							Cvar_Set( "r_mode", "-1" );

							// Wait until user stops dragging for 1 second, so
							// we aren't constantly recreating the GL context while
							// he tries to drag...
							vidRestartTime = Sys_Milliseconds( ) + 1000;
						}
						break;

					case SDL_WINDOWEVENT_MINIMIZED:    Cvar_SetValue( "com_minimized", 1 ); break;
					case SDL_WINDOWEVENT_RESTORED:
					case SDL_WINDOWEVENT_MAXIMIZED:    Cvar_SetValue( "com_minimized", 0 ); break;
					case SDL_WINDOWEVENT_FOCUS_LOST:   Cvar_SetValue( "com_unfocused", 1 ); break;
					case SDL_WINDOWEVENT_FOCUS_GAINED: Cvar_SetValue( "com_unfocused", 0 ); break;
				}
				break;

			default:
				break;
		}
	}
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
	qboolean loading;

	IN_JoyMove( );

	// If not DISCONNECTED (main menu) or ACTIVE (in game), we're loading
	loading = ( cls.state != CA_DISCONNECTED && cls.state != CA_ACTIVE );

	// update isFullscreen since it might of changed since the last vid_restart
	cls.glconfig.isFullscreen = Cvar_VariableIntegerValue( "r_fullscreen" ) != 0;

	if( !cls.glconfig.isFullscreen && ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) )
	{
		// Console is down in windowed mode
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else if( !cls.glconfig.isFullscreen && loading )
	{
		// Loading in windowed mode
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else if( !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_INPUT_FOCUS ) )
	{
		// Window not got focus
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else
		IN_ActivateMouse( cls.glconfig.isFullscreen );

	IN_ProcessEvents( );

	// Set event time for next frame to earliest possible time an event could happen
	in_eventTime = Sys_Milliseconds( );

	// In case we had to delay actual restart of video system
	if( ( vidRestartTime != 0 ) && ( vidRestartTime < Sys_Milliseconds( ) ) )
	{
		vidRestartTime = 0;
		Cbuf_AddText( "vid_restart\n" );
	}
}

/*
===============
IN_InitKeyLockStates
===============
*/
void IN_InitKeyLockStates( void )
{
#if 0
	unsigned char *keystate = SDL_GetKeyState(NULL);

	keys[A_SCROLLLOCK].down = keystate[SDLK_SCROLLOCK];
	keys[A_NUMLOCK].down = keystate[SDLK_NUMLOCK];
	keys[A_CAPSLOCK].down = keystate[SDLK_CAPSLOCK];
#endif
}

/*
===============
IN_Init
===============
*/
void IN_Init( void *windowData )
{
	int appState;

	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		Com_Error( ERR_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
		return;
	}

	SDL_window = (SDL_Window *)windowData;
	
	Com_DPrintf( "\n------- Input Initialization -------\n" );

	in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "0", CVAR_ARCHIVE );

	// mouse variables
	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	in_nograb = Cvar_Get( "in_nograb", "0", CVAR_ARCHIVE );

	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
	in_joystickDebug = Cvar_Get( "in_joystickDebug", "0", CVAR_TEMP );
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );

#ifdef MACOS_X_ACCELERATION_HACK
	in_disablemacosxmouseaccel = Cvar_Get( "in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE );
#endif
    
	//SDL_EnableUNICODE( 1 );
	//SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
	keyRepeatEnabled = qtrue;
	
	SDL_StartTextInput( );

	mouseAvailable = ( in_mouse->value != 0 ) ? qtrue : qfalse;
	IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );

	appState = SDL_GetWindowFlags( SDL_window );
	Cvar_SetValue( "com_unfocused",	!( appState & SDL_WINDOW_INPUT_FOCUS ) );
	Cvar_SetValue( "com_minimized", !( appState & SDL_WINDOW_MINIMIZED ) );

	IN_InitKeyLockStates( );

	IN_InitJoystick( );
	Com_DPrintf( "------------------------------------\n" );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
    SDL_StopTextInput( );
    
	IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );
	mouseAvailable = qfalse;

	IN_ShutdownJoystick( );
	
	SDL_window = NULL;
}

/*
===============
IN_Restart
===============
*/
void IN_Restart( void )
{
	IN_ShutdownJoystick( );
	IN_Init( SDL_window );
}

void Sys_SendKeyEvents (void)
{
}
