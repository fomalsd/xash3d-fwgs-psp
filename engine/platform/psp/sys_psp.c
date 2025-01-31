/*
sys_psp.c - PSP System utils
Copyright (C) 2021 Sergey Galushko

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "platform/platform.h"
#include "platform/psp/ka/ka.h"
#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspprof.h>
#include <pspctrl.h>

#include <ctype.h>


PSP_MODULE_INFO( "Xash3D", PSP_MODULE_USER, 1, 0 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU );
PSP_MAIN_THREAD_STACK_SIZE_KB( 512 );
PSP_HEAP_SIZE_KB( -3 * 1024 ); /* 3 MB for prx modules */

/* Exit callback */
static int exit_callback( int arg1, int arg2, void *common )
{
	host.crashed = true;
	return 0;
}

/* Callback thread */
static int CallbackThread( SceSize args, void *argp )
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
static int SetupCallbacks( void )
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

#if XASH_TIMER == TIMER_PSP
double Platform_DoubleTime( void )
{
/*
	static u64 start_ticks;
	static qboolean timer_init = false;
	u64 current_ticks;

	if ( !timer_init )
	{
		sceRtcGetCurrentTick( &start_ticks );
		timer_init = true;
	}
	sceRtcGetCurrentTick( &current_ticks );

	return ( current_ticks - start_ticks ) * 0.000001;
*/
	u64 current_ticks;
	sceRtcGetCurrentTick( &current_ticks );

	return ( double )current_ticks * 0.000001;
}

void Platform_Sleep( int msec )
{
	sceKernelDelayThread( msec * 1000 );
}
#endif

void *Platform_GetNativeObject( const char *name )
{
	return NULL;
}

void Platform_Vibrate( float life, char flags )
{

}

void Platform_GetClipboardText( char *buffer, size_t size )
{

}

void Platform_SetClipboardText( const char *buffer, size_t size )
{

}

void Platform_ShellExecute( const char *path, const char *parms )
{

}

#if XASH_MESSAGEBOX == MSGBOX_PSP
void Platform_MessageBox( const char *title, const char *message, qboolean parentMainWindow )
{
	static qboolean g_dbgscreen = false;

	// Clear the sound buffer.
	S_StopAllSounds( true );

	// Print message to the debug screen.
	if ( !g_dbgscreen )
	{
		pspDebugScreenInit();
		g_dbgscreen = true;
	}
	pspDebugScreenSetTextColor( 0x0080ff );
	pspDebugScreenPrintf( "\n\n\n\n%s:\n", title );
	pspDebugScreenSetTextColor( 0x0000ff );
	pspDebugScreenPrintData( message, Q_strlen( message ) );
	pspDebugScreenSetTextColor( 0xffffff );
	pspDebugScreenPrintf( "\n\nPress X to continue.\n" );

	// Wait for a X button press.
	SceCtrlData pad;
	do
	{
		sceCtrlReadBufferPositive( &pad, 1 );
	}
	while( !( pad.Buttons & PSP_CTRL_CROSS ) );
	pspDebugScreenClear();
}
#endif // XASH_MESSAGEBOX == MSGBOX_PSP

void Platform_ReadCmd( const char *fname, int *argc, char **argv )
{
	int 		cmd_fd;
	size_t		cmd_fsize;
	byte		*cmd_buff;
	int			i, j = 0;

	cmd_fd = sceIoOpen( fname, PSP_O_RDONLY, 0777 );
	if( cmd_fd > 0 )
	{
		cmd_fsize = sceIoLseek( cmd_fd, 0, PSP_SEEK_END );
		sceIoLseek( cmd_fd, 0, PSP_SEEK_SET );

		printf( "CMD FILE(%s) Size: %i\n", fname, cmd_fsize );

		if( cmd_fsize == 0 ) return;

		cmd_buff = malloc( cmd_fsize );
		if( !cmd_buff )
		{
			printf( "CMD FILE(%s) Memory allocation error!\n", fname );
			sceIoClose(cmd_fd);
			return;
		}

		if( sceIoRead(cmd_fd, cmd_buff, cmd_fsize) >= 0 )
		{
			for( i = 0; i < cmd_fsize; i++ )
			{
				if( isspace( cmd_buff[i] ) != 0 )
				{
					if( j == 0 ) continue;

					argv[*argc] = malloc( j + 1 );
					if( !argv[*argc] )
					{
						printf( "CMD FILE(%s) Memory allocation error!\n", fname );
						free( cmd_buff );
						sceIoClose( cmd_fd );
						return;
					}
					memcpy( argv[*argc], &cmd_buff[i - j], j );
					argv[*argc][j] = 0x00;
					( int )( *argc )++;
					j = 0;
				}
				else j++;

			}
			if( j != 0 )
			{
				argv[*argc] = malloc( j + 1 );
				if( !argv[*argc] )
				{
					printf( "CMD FILE(%s) Memory allocation error!\n", fname );
					free( cmd_buff );
					sceIoClose( cmd_fd );
					return;
				}
				memcpy( argv[*argc], &cmd_buff[i - j], j );
				argv[*argc][j] = 0x00;
				( int )( *argc )++;
				j = 0;
			}
		}
		else printf( "CMD FILE(%s) Read error!\n", fname );

		free( cmd_buff );
		sceIoClose( cmd_fd );
	}
}

SceUID Platform_LoadModule( const char *filename, int mpid, SceSize argsize, void *argp )
{
	SceKernelLMOption option;
	SceUID modid = 0;
	int retVal = 0, mresult;

	memset( &option, 0, sizeof( option ) );
	option.size = sizeof( option );
	option.mpidtext = mpid;
	option.mpiddata = mpid;
	option.position = 0;
	option.access = 1;

	retVal = sceKernelLoadModule( filename, 0, &option );
	if(retVal < 0)
		return retVal;

	modid = retVal;

	retVal = sceKernelStartModule( modid, argsize, argp, &mresult, NULL );
	if( retVal < 0 )
		return retVal;

	return modid;
}

int Platform_UnloadModule( SceUID modid, int *sce_code )
{
	int status;
	*sce_code = sceKernelStopModule( modid, 0, NULL, &status, NULL);

	if( ( *sce_code ) < 0 )
		return -2;
	else if( status == SCE_KERNEL_ERROR_NOT_STOPPED )
		return -1;

	*sce_code = sceKernelUnloadModule( modid );
	return ( ( ( *sce_code ) < 0 ) ? -2 : 0 );
}

void Platform_Init( void )
{
	SceUID kamID;
	int result;

	// disable fpu exceptions (division by zero and etc...)
	pspSdkDisableFPUExceptions();

	// exit callback thread
	SetupCallbacks();

	// set max cpu/gpu frequency
	scePowerSetClockFrequency( 333, 333, 166 );

	// set max VRAM
	kamID = Platform_LoadModule( "ka.prx", 1, 0, NULL );
	if( kamID >= 0 )
	{
		result = kaGeEdramGetHwSize();
		if( result > 0 )
			result = kaGeEdramSetSize( result );
		Platform_UnloadModule( kamID, &result );
	}
}

void Platform_Shutdown( void )
{
#if XASH_PROFILING
	gprof_cleanup();
#endif
}
