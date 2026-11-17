#pragma once

#include "CoreMinimal.h"

/**
 * The one place where the login redirect crosses threads.
 *
 * After the player logs in, the browser sends them back to the game with a URL that carries the
 * login code. That URL arrives on a thread that is not the game thread:
 *   - desktop: the loopback listener's own thread
 *   - Android: the activity (Java UI) thread, possibly before the engine has finished starting
 *
 * Those threads only drop the URL here. The Jiwe subsystem picks it up on the game thread.
 */
class FJiweRedirectInbox
{
public:
	/** Any thread. Safe to call before the engine is initialised (uses no engine services). */
	static void PushUtf8(const char* Utf8Url);

	/** Any thread, once the engine is running. */
	static void Push(const FString& Url);

	/** Game thread. Returns false when nothing is waiting. */
	static bool Pop(FString& OutUrl);

	/** Throws away anything waiting (called when a new login starts, so a stale link is never used). */
	static void Clear();
};
