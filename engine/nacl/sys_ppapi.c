#include "quakedef.h"

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_graphics_3d.h>
#include <ppapi/c/ppb_instance.h>
#include <ppapi/c/ppp.h>
#include <ppapi/c/ppp_instance.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/c/ppp_input_event.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_url_loader.h>
#include <ppapi/c/ppb_url_response_info.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/ppb_audio.h>
#include <ppapi/c/ppb_audio_config.h>
#include <ppapi/c/ppb_mouse_lock.h>
#include <ppapi/c/ppp_mouse_lock.h>
#include <ppapi/c/ppb_fullscreen.h>
#include <ppapi/c/ppb_websocket.h>
#include <ppapi/c/ppb_view.h>
#include <ppapi/c/ppp_messaging.h>

#include <ppapi/c/pp_input_event.h>
#include <ppapi/gles2/gl2ext_ppapi.h>

PPB_Core *ppb_core = NULL;
PPB_Graphics3D *graphics3d_interface = NULL;
PPB_Instance *instance_interface = NULL;
PPB_Messaging *ppb_messaging_interface = NULL;
PPB_Var *ppb_var_interface = NULL;
PPB_InputEvent *ppb_inputevent_interface = NULL;
PPB_KeyboardInputEvent *ppb_keyboardinputevent_interface = NULL;
PPB_MouseInputEvent *ppb_mouseinputevent_interface = NULL;
PPB_WheelInputEvent *ppb_wheelinputevent_interface = NULL;
PPB_FileIO *ppb_fileio = NULL;
PPB_FileRef *ppb_fileref = NULL;
PPB_FileSystem *ppb_filesystem = NULL;
PPB_URLLoader  *urlloader = NULL;
PPB_URLRequestInfo *urlrequestinfo = NULL;
PPB_URLResponseInfo *urlresponseinfo = NULL;
PPB_Audio *audio_interface = NULL;
PPB_AudioConfig *audioconfig_interface = NULL;
PPB_MouseLock *ppb_mouselock_interface = NULL;
PPB_Fullscreen *ppb_fullscreen_interface = NULL;
PPB_WebSocket *ppb_websocket_interface = NULL;
PPB_View *ppb_view_instance = NULL;
PP_Instance pp_instance;
PPB_GetInterface sys_gbi;
static double lasttime;
static qboolean mouselocked;
static qboolean shuttingdown;

qboolean FSPPAPI_Init(int *filenocookie);

unsigned short htons(unsigned short a)
{
	union
	{
		unsigned char c[2];
		unsigned short s;
	} u;
	u.s = a;

	return u.c[0] | (unsigned short)(u.c[1]<<8);
}
unsigned short ntohs(unsigned short a)
{
	return htons(a);
}
unsigned int htonl(unsigned int a)
{
	union
	{
		unsigned char c[4];
		unsigned int s;
	} u;
	u.s = a;

	return u.c[0] | (unsigned int)(u.c[1]<<8) | (unsigned int)(u.c[2]<<16) | (unsigned int)(u.c[3]<<24);
}
unsigned long ntohl(unsigned long a)
{
	return htonl(a);
}

qboolean isDedicated = false;


dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	return NULL;
}
void Sys_CloseLibrary(dllhandle_t *lib)
{
}
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	return NULL;
}
char *Sys_GetNameForAddress(dllhandle_t *module, void *address)
{
	return NULL;
}

qboolean Sys_RandomBytes(qbyte *string, int len)
{
	return false;
}

//q2...
void Sys_UnloadGame (void)
{
}
//q2...
void *Sys_GetGameAPI (void *parms)
{
	return NULL;
}

qboolean Sys_InitTerminal (void)
{
	return false;
}
void Sys_CloseTerminal (void)
{
}

char *Sys_GetClipboard(void)
{
	return NULL;
}
void Sys_CloseClipboard(char *buf)
{
}
void Sys_SaveClipboard(char *text)
{
}
void Sys_ServerActivity(void)
{
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}
void Sys_SendKeyEvents(void)
{
}

void Sys_Init (void)
{
}
void Sys_Shutdown(void)
{
}

//nacl supposedly has no way to implement this (other than us writing a listfile in each directory)
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, int, void *), void *parm)
{
	return 0;
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	*width = 1024;
	*height = 768;
	*bpp = 32;
	*refreshrate = 60;
	return true;

	return false;
}

// an error will cause the entire program to exit
NORETURN void VARGS Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof(string)-1, error, argptr);
	va_end (argptr);

	Sys_Printf("Sys_Error: ");
	Sys_Printf("%s", string);
	exit(1);
}

static struct PP_Var CStrToVar(const char* str)
{
	if (ppb_var_interface != NULL)
	{
		return ppb_var_interface->VarFromUtf8(str, strlen(str));
	}
	return PP_MakeUndefined();
}
void VARGS Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string)-1, fmt, argptr);
	va_end (argptr);

	//this stuff generally doesn't even get shown
	printf("%s", string);
	if (pp_instance)
		ppb_messaging_interface->PostMessage(pp_instance, CStrToVar(string));
}
void ppp_handlemessage(PP_Instance instance, struct PP_Var message)
{
	char *clean;
	const char *msg;
	unsigned int len;
	msg = ppb_var_interface->VarToUtf8(message, &len);
	clean = malloc(len+2);
	clean[len+0] = '\n';
	clean[len+1] = 0;
	memcpy(clean, msg, len);
	Cbuf_AddText(clean, RESTRICT_INSECURE);
	free(clean);
}

void Sys_Quit (void)
{
	Sys_Printf("Sys_Quit\n");

	shuttingdown = true;
}
void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
}

void Sys_mkdir (char *path)
{
}
qboolean Sys_remove (char *path)
{
	return false;
}
qboolean Sys_Rename (char *oldfname, char *newfname)
{
	return false;
}

#include <sys/time.h>
static int secbase;
double Sys_DoubleTime(void)
{
    struct timeval tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    if (!secbase)
    {
            secbase = tp.tv_sec;
            return tp.tv_usec/1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}

unsigned int Sys_Milliseconds (void)
{
	return Sys_DoubleTime()*1000;
}

void FrameEvent(void* user_data, int32_t result)
{
	if (shuttingdown)
	{
		if (!mouselocked && ppb_mouselock_interface)
			ppb_mouselock_interface->UnlockMouse(pp_instance);
		if (ppb_fullscreen_interface)
			ppb_fullscreen_interface->SetFullscreen(pp_instance, PP_FALSE);
		Host_Shutdown ();
		ppb_inputevent_interface->RequestInputEvents(pp_instance, 0);
		shuttingdown = false;
		return;
	}
	if (pp_instance)
	{
		double newtime = Sys_DoubleTime();
		Host_Frame(newtime - lasttime);
		lasttime = newtime;

//		Sys_Printf("Frame %f\n", newtime);

		struct PP_CompletionCallback ccb = {FrameEvent, user_data, PP_COMPLETIONCALLBACK_FLAG_NONE};
		ppb_core->CallOnMainThread(0, ccb, 0);
	}
}
void startquake(void)
{
	const char *args [] =
	{
		"ftedroid",
		"",
		""
	};
	quakeparms_t parms;
	parms.basedir = "";	/*filled in later*/
	parms.argc = 1;
	parms.argv = args;
//FIXME: do something with the embed arguments
	parms.memsize = 16*1024*1024;
	parms.membase = malloc(parms.memsize);
	if (!parms.membase)
	{
		Sys_Printf("Unable to alloc heap\n");
		return;
	}

	Sys_Printf("Starting up\n");

	COM_InitArgv(parms.argc, parms.argv);
	TL_InitLanguages();
	#ifdef SERVERONLY
		SV_Init(&parms);
	#else
		Host_Init(&parms);
	#endif

	lasttime = Sys_DoubleTime();

	FrameEvent(NULL, 0);
}

void trystartquake(void* user_data, int32_t result)
{
	if (FSPPAPI_Init(&result))
		startquake();
	else
	{
		struct PP_CompletionCallback ccb = {trystartquake, user_data, PP_COMPLETIONCALLBACK_FLAG_NONE};
		ppb_core->CallOnMainThread(100, ccb, result);
	}
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[])
{
	pp_instance = instance;

//FIXME: do something with the embed arguments

	ppb_inputevent_interface->RequestInputEvents(pp_instance, PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_KEYBOARD | PP_INPUTEVENT_CLASS_WHEEL);

	trystartquake(NULL, 0);

	return PP_TRUE;
}

static void cb_mouselocked(void* user_data, int32_t result)
{
	if (result == PP_OK)
	{
		mouselocked = true;
	}
}
static void ppp_mouseunlocked(PP_Instance instance)
{
	mouselocked = false;
}

unsigned int domkeytoquake(unsigned int code)
{
#define K_PRINTSCREEN ' '
	unsigned int tab[256] =
	{
		/*  0*/ 0,0,0,0,0,0,0,0,		K_BACKSPACE,K_TAB,0,0,0,K_ENTER,0,0,
		/* 16*/ K_SHIFT,K_CTRL,K_ALT,K_PAUSE,K_CAPSLOCK,0,0,0,		0,0,0,K_ESCAPE,0,0,0,0,
		/* 32*/ ' ',K_PGUP,K_PGDN,K_END,K_HOME,K_LEFTARROW,K_UPARROW,K_RIGHTARROW,		K_DOWNARROW,0,0,0,K_PRINTSCREEN,K_INS,K_DEL,0,
		/* 48*/ '0','1','2','3','4','5','6','7',		'8','9',0,0,0,0,0,0,

		/* 64*/ 0,'a','b','c','d','e','f','g',		'h','i','j','k','l','m','n','o',
		/* 80*/ 'p','q','r','s','t','u','v','w',		'x','y','z',K_LWIN,K_RWIN,K_APP,0,0,
		/* 96*/ K_KP_INS,K_KP_END,K_KP_DOWNARROW,K_KP_PGDN,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_HOME,		K_KP_UPARROW,K_KP_PGDN,K_KP_STAR,K_KP_PLUS,0,K_KP_MINUS,K_KP_DEL,K_KP_SLASH,
		/*112*/ K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,		K_F9,K_F10,K_F11,K_F12,0,0,0,0,
		/*128*/ 0,0,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
		/*144*/ K_KP_NUMLOCK,K_SCRLCK,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
		/*160*/ 0,0,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
		/*176*/ 0,0,0,0,0,0,0,0,		0,0,';','=',',','-','.','/',
		/*192*/ '\'',0,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
		/*208*/ 0,0,0,0,0,0,0,0,		0,0,0,'[','\\',']','#','`',
		/*224*/ 0,0,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
		/*240*/ 0,0,0,0,0,0,0,0,		0,0,0,0,0,0,0,0,
	};
	if (code >= sizeof(tab)/sizeof(tab[0]))
	{
		Con_DPrintf("You just pressed key %u, but I don't know what its meant to be\n", code);
		return 0;
	}
	if (!tab[code])
		Con_DPrintf("You just pressed key %u, but I don't know what its meant to be\n", code);
	return tab[code];
}

void IN_QueueKey(int down, int keycode, int unicode);
void IN_QueueMouse(int act, int ptrid, float x, float y, int button);
void IN_AmmendUnicode(int unicode);
PP_Bool InputEvent_HandleEvent(PP_Instance pp_instance, PP_Resource resource)
{
	extern cvar_t vid_fullscreen;
	if (!pp_instance || !host_initialized)
		return PP_FALSE;

	switch(ppb_inputevent_interface->GetType(resource))
	{
	case PP_INPUTEVENT_TYPE_MOUSEDOWN:
		if (vid_fullscreen.ival)
		{
			if (ppb_fullscreen_interface)
				ppb_fullscreen_interface->SetFullscreen(pp_instance, PP_TRUE);

			if (!mouselocked && ppb_mouselock_interface)
			{
				struct PP_CompletionCallback ccb = {cb_mouselocked, NULL, PP_COMPLETIONCALLBACK_FLAG_NONE};
				int res = ppb_mouselock_interface->LockMouse(pp_instance, ccb);
				if (res != PP_OK_COMPLETIONPENDING)
					cb_mouselocked(NULL, res);
				else
					return PP_TRUE;
			}
		}
		IN_QueueMouse(1, 0, 0, 0, ppb_mouseinputevent_interface->GetButton(resource));
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_MOUSEUP:
		IN_QueueMouse(2, 0, 0, 0, ppb_mouseinputevent_interface->GetButton(resource));
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_MOUSEMOVE:
		{
			struct PP_Point p;
			if (mouselocked)
			{
				p = ppb_mouseinputevent_interface->GetMovement(resource);
				IN_QueueMouse(3, 0, p.x, p.y, 0);
			}
			else
			{
				p = ppb_mouseinputevent_interface->GetPosition(resource);
				IN_QueueMouse(0, 0, p.x, p.y, 0);
			}
		}
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_MOUSEENTER:
		//we don't really care too much if it leave the window
//		Con_Printf("mouseenter\n");
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_MOUSELEAVE:
		//we don't really care too much if it leave the window (should throttle framerates perhaps, but that's all)
//		Con_Printf("mouseleave\n");
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_WHEEL:
		{
			struct PP_FloatPoint p;
			p = ppb_wheelinputevent_interface->GetTicks(resource);
			//BUG: the value is fractional.
			while (p.x >= 1)
			{
				IN_QueueKey(1, K_MWHEELUP, 0);
				IN_QueueKey(0, K_MWHEELUP, 0);
				p.x--;
			}
			while (p.x <= -1)
			{
				IN_QueueKey(1, K_MWHEELDOWN, 0);
				IN_QueueKey(0, K_MWHEELDOWN, 0);
				p.x++;
			}
		}
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
//		Con_Printf("rawkeydown\n");
		return PP_FALSE;
	case PP_INPUTEVENT_TYPE_KEYDOWN:
		IN_QueueKey(1, domkeytoquake(ppb_keyboardinputevent_interface->GetKeyCode(resource)), 0);
		return PP_FALSE;
	case PP_INPUTEVENT_TYPE_KEYUP:
		IN_QueueKey(0, domkeytoquake(ppb_keyboardinputevent_interface->GetKeyCode(resource)), 0);
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_CHAR:
		{
			const unsigned char *s;
			unsigned int c;
			unsigned int len;
			len = 0;
			s = ppb_var_interface->VarToUtf8(ppb_keyboardinputevent_interface->GetCharacterText(resource), &len);
			while(len)
			{
				if (*s & 0x80)
				{
					if (!(*s & 0x40))
					{
						//illegal encoding
						c = '?';
						len -= 1;
					}
					else if (!(*s & 0x20) && (s[1] & 0xc0) == 0x80)
					{
						c = ((s[0] & 0x1f)<<6) | ((s[1] & 0x3f)<<0);
						if (c < (1<<7))
							c = '?';
						len -= 2;
					}
					else if (!(*s & 0x10) && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80)
					{
						c = ((s[0] & 0x0f)<<12) | ((s[1] & 0x3f)<<6) | ((s[2] & 0x3f)<<0);
						if (c < (1<<13))
							c = '?';
						len -= 3;
					}
					else if (!(*s & 0x08) && (s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 && (s[3] & 0xc0) == 0x80)
					{
						c = ((s[0] & 0x07)<<18) | ((s[1] & 0x3f)<<12) | ((s[2] & 0x3f)<<6) | ((s[3] & 0x3f)<<0);
						if (c < (1<<19))
							c = '?';
						len -= 4;
					}
					else
					{
						//too lazy to handle that encoding
						c = '?';
						len -= 1;
					}
				}
				else
				{
					c = *s;
					len--;
				}

				IN_AmmendUnicode(c);
			}
		}
		return PP_TRUE;
	case PP_INPUTEVENT_TYPE_CONTEXTMENU:
		//We don't care about the context menu, we just want to be able to right-click.
		return PP_TRUE;
	default:
		Con_Printf("Unknown input event type\n");
		break;
	}
	return PP_FALSE;
}

static void Instance_DidDestroy(PP_Instance instance)
{
}
void GL_Resized(int width, int height);
static void Instance_DidChangeView(PP_Instance instance, PP_Resource view_resource)
{
	int newwidth;
	int newheight;
	struct PP_Rect rect;
	ppb_view_instance->GetRect(view_resource, &rect);
	GL_Resized(rect.size.width, rect.size.height);
}
static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus)
{
}
static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader)
{
  return PP_FALSE;
}


PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id, PPB_GetInterface get_browser)
{
	ppb_core = (PPB_Core*)(get_browser(PPB_CORE_INTERFACE));
	sys_gbi = get_browser;
	graphics3d_interface = (PPB_Graphics3D*)(get_browser(PPB_GRAPHICS_3D_INTERFACE));
	ppb_messaging_interface = (PPB_Messaging*)(get_browser(PPB_MESSAGING_INTERFACE));
	ppb_var_interface = (PPB_Var*)(get_browser(PPB_VAR_INTERFACE));
	instance_interface = (PPB_Instance*)(get_browser(PPB_INSTANCE_INTERFACE));
	ppb_inputevent_interface = (PPB_InputEvent*)(get_browser(PPB_INPUT_EVENT_INTERFACE));
	ppb_keyboardinputevent_interface = (PPB_KeyboardInputEvent*)(get_browser(PPB_KEYBOARD_INPUT_EVENT_INTERFACE));
	ppb_mouseinputevent_interface = (PPB_MouseInputEvent*)(get_browser(PPB_MOUSE_INPUT_EVENT_INTERFACE));
	ppb_wheelinputevent_interface = (PPB_WheelInputEvent*)(get_browser(PPB_WHEEL_INPUT_EVENT_INTERFACE));
	ppb_fileio = (PPB_FileIO*)(get_browser(PPB_FILEIO_INTERFACE));
	ppb_fileref = (PPB_FileRef*)(get_browser(PPB_FILEREF_INTERFACE));
	ppb_filesystem = (PPB_FileSystem*)(get_browser(PPB_FILESYSTEM_INTERFACE));
	urlloader = (PPB_URLLoader*)(get_browser(PPB_URLLOADER_INTERFACE )); 
	urlrequestinfo = (PPB_URLRequestInfo*)(get_browser(PPB_URLREQUESTINFO_INTERFACE)); 
	urlresponseinfo = (PPB_URLResponseInfo*)(get_browser(PPB_URLRESPONSEINFO_INTERFACE)); 
	audio_interface = (PPB_Audio*)(get_browser(PPB_AUDIO_INTERFACE)); 
	audioconfig_interface = (PPB_AudioConfig*)(get_browser(PPB_AUDIO_CONFIG_INTERFACE)); 
	ppb_mouselock_interface = (PPB_MouseLock*)(get_browser(PPB_MOUSELOCK_INTERFACE)); 
	ppb_fullscreen_interface = (PPB_Fullscreen*)(get_browser(PPB_FULLSCREEN_INTERFACE));
	ppb_websocket_interface = (PPB_WebSocket*)(get_browser(PPB_WEBSOCKET_INTERFACE));
	ppb_view_instance = (PPB_View*)(get_browser(PPB_VIEW_INTERFACE));

	glInitializePPAPI(sys_gbi);

	return PP_OK;
}
PP_EXPORT const void* PPP_GetInterface(const char* interface_name)
{
	if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
	{
		static PPP_Instance instance_interface =
		{
			&Instance_DidCreate,
			&Instance_DidDestroy,
			&Instance_DidChangeView,
			&Instance_DidChangeFocus,
			&Instance_HandleDocumentLoad,
		};
		return &instance_interface;
	}
	if (strcmp(interface_name, PPP_INPUT_EVENT_INTERFACE) == 0)
	{
		static PPP_InputEvent input_event_interface =
		{
			&InputEvent_HandleEvent
		};
		return &input_event_interface;
	}
	if (strcmp(interface_name, PPP_MOUSELOCK_INTERFACE) == 0)
	{
		static PPP_MouseLock mouselock_interface =
		{
			&ppp_mouseunlocked
		};
		return &mouselock_interface;
	}
	if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0)
	{
		static PPP_Messaging messaging_interface =
		{
			ppp_handlemessage
		};
		return &messaging_interface;
	}
	return NULL;
}
PP_EXPORT void PPP_ShutdownModule()
{
}