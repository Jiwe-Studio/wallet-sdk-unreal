#include "JiweLoopbackListener.h"

#if PLATFORM_DESKTOP

#include "JiweRedirectInbox.h"
#include "Async/Async.h"
#include "Common/TcpListener.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace JiweLoopbackDetail
{
	/**
	 * Reads one browser request, answers it, and closes the socket.
	 * Runs on a background worker (the listener's own thread has a very small stack).
	 * Touches nothing but the socket and the redirect inbox, so it is safe even if the
	 * listener that accepted the connection has since been stopped.
	 */
	void ServeConnection(FSocket* Socket)
	{
		// Read the whole request head. Closing a socket with unread data can make the browser drop
		// our reply, so keep reading until the blank line that ends the headers (a GET has no body).
		TArray<uint8> Received;
		bool bHeadComplete = false;
		const double GiveUpAt = FPlatformTime::Seconds() + 3.0;
		while (!bHeadComplete && FPlatformTime::Seconds() < GiveUpAt && Received.Num() < 16384)
		{
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(250)))
			{
				continue;
			}

			uint8 Chunk[2048];
			int32 BytesRead = 0;
			if (!Socket->Recv(Chunk, static_cast<int32>(sizeof(Chunk)), BytesRead) || BytesRead <= 0)
			{
				break;
			}
			Received.Append(Chunk, BytesRead);

			for (int32 Index = 3; Index < Received.Num(); ++Index)
			{
				if (Received[Index - 3] == '\r' && Received[Index - 2] == '\n' && Received[Index - 1] == '\r' && Received[Index] == '\n')
				{
					bHeadComplete = true;
					break;
				}
			}
		}

		Received.Add(0);
		const FString Request = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Received.GetData())));

		// First line looks like:  GET /?code=abc&state=xyz HTTP/1.1
		FString Target;
		int32 FirstSpace = INDEX_NONE;
		if (Request.FindChar(TEXT(' '), FirstSpace))
		{
			const int32 SecondSpace = Request.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstSpace + 1);
			if (SecondSpace > FirstSpace)
			{
				Target = Request.Mid(FirstSpace + 1, SecondSpace - FirstSpace - 1);
			}
		}

		// Browsers also ask for /favicon.ico and open spare connections. Only the real redirect counts.
		const bool bIsLoginRedirect = Target.Contains(TEXT("code=")) || Target.Contains(TEXT("error="));

		FString Body;
		if (bIsLoginRedirect)
		{
			Body = TEXT("<!doctype html><html><head><meta charset=\"utf-8\"><title>Jiwe</title></head>");
			Body += TEXT("<body style=\"font-family:sans-serif;text-align:center;padding-top:15vh\">");
			Body += TEXT("<h2>Login complete</h2><p>You can close this tab and return to the game.</p></body></html>");
		}
		else
		{
			Body = TEXT("Not found");
		}

		const FTCHARToUTF8 BodyUtf8(*Body);
		FString Head = bIsLoginRedirect ? TEXT("HTTP/1.1 200 OK\r\n") : TEXT("HTTP/1.1 404 Not Found\r\n");
		Head += TEXT("Content-Type: text/html; charset=utf-8\r\n");
		Head += FString::Printf(TEXT("Content-Length: %d\r\n"), BodyUtf8.Length());
		Head += TEXT("Cache-Control: no-store\r\nConnection: close\r\n\r\n");
		const FTCHARToUTF8 HeadUtf8(*Head);

		int32 BytesSent = 0;
		Socket->Send(reinterpret_cast<const uint8*>(HeadUtf8.Get()), HeadUtf8.Length(), BytesSent);
		Socket->Send(reinterpret_cast<const uint8*>(BodyUtf8.Get()), BodyUtf8.Length(), BytesSent);

		Socket->Close();
		if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			SocketSubsystem->DestroySocket(Socket);
		}

		if (bIsLoginRedirect)
		{
			FJiweRedirectInbox::Push(FString(TEXT("http://127.0.0.1")) + Target);
		}
	}
}

FJiweLoopbackListener::FJiweLoopbackListener()
	: Listener(nullptr)
{
}

FJiweLoopbackListener::~FJiweLoopbackListener()
{
	Stop();
}

bool FJiweLoopbackListener::Start(int32 Port)
{
	Stop();

	const FIPv4Endpoint Endpoint(FIPv4Address(127, 0, 0, 1), static_cast<uint16>(Port));

	// Windows: "reusable" would let two programs share the port, hiding a clash. Elsewhere it is
	// needed so a second login right after the first can bind again.
#if PLATFORM_WINDOWS
	const bool bReusable = false;
#else
	const bool bReusable = true;
#endif

	Listener = new FTcpListener(Endpoint, FTimespan::FromMilliseconds(100), bReusable);
	if (!Listener->IsActive())
	{
		delete Listener;
		Listener = nullptr;
		return false;
	}

	Listener->OnConnectionAccepted().BindRaw(this, &FJiweLoopbackListener::HandleConnection);
	return true;
}

void FJiweLoopbackListener::Stop()
{
	if (Listener != nullptr)
	{
		// The destructor stops and joins the listener thread, so HandleConnection can no longer run.
		delete Listener;
		Listener = nullptr;
	}
}

bool FJiweLoopbackListener::HandleConnection(FSocket* Socket, const FIPv4Endpoint& /*Endpoint*/)
{
	if (Socket == nullptr)
	{
		return false;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Socket]()
	{
		JiweLoopbackDetail::ServeConnection(Socket);
	});

	// True = "we took the socket". ServeConnection closes and destroys it.
	return true;
}

#endif // PLATFORM_DESKTOP
