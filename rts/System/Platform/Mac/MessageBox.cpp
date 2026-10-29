/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "System/Platform/MessageBox.h"
#include "System/Platform/errorhandler.h"

#include <cstdio>
#include <cstring>

#if !defined(DEDICATED) && !defined(HEADLESS)
	#if __has_include(<SDL2/SDL.h>)
		#include <SDL2/SDL.h>
		#define HAVE_SDL2 1
	#elif __has_include(<SDL.h>)
		#include <SDL.h>
		#define HAVE_SDL2 1
	#endif
	#include <CoreFoundation/CoreFoundation.h>
#endif

namespace Platform {

void MsgBox(const char* message, const char* caption, unsigned int flags)
{
#if !defined(DEDICATED) && !defined(HEADLESS)
	const char* safeCaption = (caption != nullptr) ? caption : "Recoil Engine";
	const char* safeMessage = (message != nullptr) ? message : "";

#if defined(HAVE_SDL2)
	Uint32 sdlFlags = SDL_MESSAGEBOX_ERROR;
	if (flags & MBF_INFO) {
		sdlFlags = SDL_MESSAGEBOX_INFORMATION;
	} else if (flags & MBF_EXCL) {
		sdlFlags = SDL_MESSAGEBOX_WARNING;
	}

	if (SDL_ShowSimpleMessageBox(sdlFlags, safeCaption, safeMessage, nullptr) == 0) {
		return;
	}
#endif

	// Fallback to CoreFoundation CFUserNotificationDisplayAlert
	CFStringRef cfCaption = CFStringCreateWithCString(kCFAllocatorDefault, safeCaption, kCFStringEncodingUTF8);
	CFStringRef cfMessage = CFStringCreateWithCString(kCFAllocatorDefault, safeMessage, kCFStringEncodingUTF8);

	CFOptionFlags cfAlertLevel = kCFUserNotificationStopAlertLevel;
	if (flags & MBF_INFO) {
		cfAlertLevel = kCFUserNotificationPlainAlertLevel;
	} else if (flags & MBF_EXCL) {
		cfAlertLevel = kCFUserNotificationCautionAlertLevel;
	}

	CFOptionFlags responseFlags = 0;
	CFUserNotificationDisplayAlert(
		0,                  // timeout
		cfAlertLevel,       // alert level
		nullptr,            // icon URL
		nullptr,            // sound URL
		nullptr,            // localization URL
		cfCaption,          // alert header
		cfMessage,          // alert message
		nullptr,            // default button title ("OK")
		nullptr,            // alternate button title
		nullptr,            // other button title
		&responseFlags      // response
	);

	if (cfCaption) CFRelease(cfCaption);
	if (cfMessage) CFRelease(cfMessage);
#else
	std::fprintf(stderr, "[%s] %s\n", (caption ? caption : "Recoil Engine"), (message ? message : ""));
#endif
}

} // namespace Platform
