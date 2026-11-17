#pragma once

#include "CoreMinimal.h"

#if PLATFORM_DESKTOP

class FSocket;
class FTcpListener;
struct FIPv4Endpoint;

/**
 * Desktop login only. A tiny local web server on http://127.0.0.1:<port>/ that waits for the
 * browser to come back from the Jiwe login page, shows "you can return to the game", and drops
 * the returned URL into FJiweRedirectInbox.
 *
 * It only ever listens on 127.0.0.1, so nothing outside this computer can reach it.
 */
class FJiweLoopbackListener
{
public:
	FJiweLoopbackListener();
	~FJiweLoopbackListener();

	/** False if the port is already taken by another program. */
	bool Start(int32 Port);

	/** Game thread only. Safe to call twice. */
	void Stop();

private:
	/** Runs on the listener's own thread. */
	bool HandleConnection(FSocket* Socket, const FIPv4Endpoint& Endpoint);

	FTcpListener* Listener;
};

#endif // PLATFORM_DESKTOP
