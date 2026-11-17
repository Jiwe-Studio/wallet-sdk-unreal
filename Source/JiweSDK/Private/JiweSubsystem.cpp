// Jiwe subsystem: start-up, setup report, and the login flow.
// The wallet calls (rewards, leaderboard, purchases, data) are in JiweSubsystemWallet.cpp.
//
// Login in one paragraph (OAuth 2 Authorization Code + PKCE):
//   1. Make two random strings: a secret "verifier" kept in memory, and a "state" tag.
//   2. Open the Jiwe login page in the browser, sending a hash of the verifier.
//   3. The player logs in. Jiwe sends the browser back to our Redirect URI with a one-time code.
//        desktop: http://127.0.0.1:<port>/     caught by a tiny local listener
//        Android: <scheme>://oauth-callback    reopens the game
//   4. Swap the code (plus the verifier) for tokens at Jiwe's token endpoint.
//   5. The ID token says who the player is. It is sent with every reward call.

#include "JiweSubsystem.h"
#include "JiweJson.h"
#include "JiweLoopbackListener.h"
#include "JiweRedirectInbox.h"
#include "JiweSDKModule.h"
#include "JiweSettings.h"
#include "JiweText.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"

namespace JiweLoginDetail
{
	const TCHAR* BaseScope = TEXT("openid profile in-app-purchases rewards");
	const TCHAR* SettingsPath = TEXT("Project Settings > Plugins > Jiwe SDK");
	const TCHAR* JiweAppPath = TEXT("jiwe.io > Profile > My Apps");
}

// =========================================================================================
// Start-up
// =========================================================================================

UJiweSubsystem* UJiweSubsystem::Get(const UObject* WorldContextObject)
{
	if (GEngine == nullptr || WorldContextObject == nullptr)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UJiweSubsystem>() : nullptr;
}

void UJiweSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bShuttingDown = false;
	GameSessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens).ToLower();

	if (UJiweSettings::Get()->bLoginOnStart && !IsRunningDedicatedServer())
	{
		Login();
	}
}

void UJiweSubsystem::Deinitialize()
{
	bShuttingDown = true;
	StopWaitingForBrowser();
	FTSTicker::GetCoreTicker().RemoveTicker(LoginTickerHandle);
	LoginTickerHandle.Reset();
	LoginCallbacks.Reset();
	LoginState = EJiweLoginState::Idle;
	++LoginAttempt;

	Super::Deinitialize();
}

// =========================================================================================
// Setup help
// =========================================================================================

FString UJiweSubsystem::GetRedirectUri() const
{
#if PLATFORM_ANDROID
	return UJiweSettings::Get()->MobileRedirectScheme.TrimStartAndEnd() + TEXT("://oauth-callback");
#elif PLATFORM_DESKTOP
	return FString::Printf(TEXT("http://127.0.0.1:%d/"), UJiweSettings::Get()->LoopbackPort);
#else
	return FString();
#endif
}

FString UJiweSubsystem::GetSetupReport() const
{
	const UJiweSettings* Settings = UJiweSettings::Get();

	auto Line = [](bool bOk, const TCHAR* Label, const TCHAR* IfMissing) -> FString
	{
		return bOk
			? FString::Printf(TEXT("  [OK]      %s\n"), Label)
			: FString::Printf(TEXT("  [MISSING] %s  -> %s\n"), Label, IfMissing);
	};

	const bool bHasClientId = !Settings->ClientId.TrimStartAndEnd().IsEmpty();
	const bool bHasSecret = !Settings->ApiSecret.TrimStartAndEnd().IsEmpty();
	const bool bHasUsername = !Settings->ApiUsername.TrimStartAndEnd().IsEmpty();
	const bool bHasKey = !Settings->ApiKey.TrimStartAndEnd().IsEmpty();

	FString Report = TEXT("Jiwe SDK setup check\n");
	Report += FString::Printf(TEXT("Keys are set in %s. All four come from %s.\n\n"), JiweLoginDetail::SettingsPath, JiweLoginDetail::JiweAppPath);

	Report += TEXT("1. Login keys (used to log a player in)\n");
	Report += Line(bHasClientId, TEXT("Client ID"), TEXT("paste it from your Jiwe app"));
	Report += bHasSecret
		? TEXT("  [OK]      API Secret\n")
		: TEXT("  [EMPTY]   API Secret  -> fine only if Jiwe set your app up as a public client\n");

	Report += TEXT("2. Wallet keys (sent with every reward and leaderboard call)\n");
	Report += Line(bHasUsername, TEXT("API Username"), TEXT("paste it from your Jiwe app"));
	Report += Line(bHasKey, TEXT("API Key"), TEXT("paste it from your Jiwe app"));

	Report += TEXT("3. Redirect URIs to register on your Jiwe app, exactly as written\n");
	Report += FString::Printf(TEXT("  Windows / Mac / Linux / editor:  http://127.0.0.1:%d/\n"), Settings->LoopbackPort);
	const FString Scheme = Settings->MobileRedirectScheme.TrimStartAndEnd();
	Report += FString::Printf(TEXT("  Android:                         %s://oauth-callback\n"), *Scheme);
	if (Scheme.IsEmpty() || !Scheme.Equals(Scheme.ToLower(), ESearchCase::CaseSensitive) || Scheme.Contains(TEXT(" ")) || Scheme.Contains(TEXT("_")))
	{
		Report += TEXT("  [CHECK]   Android Redirect Scheme should be lowercase letters and digits only, with no spaces or underscores\n");
	}
	const FString ThisPlatform = GetRedirectUri();
	Report += ThisPlatform.IsEmpty()
		? FString(TEXT("  This platform: login is not supported here yet\n"))
		: FString::Printf(TEXT("  This build will send:            %s\n"), *ThisPlatform);

	Report += TEXT("4. Environment\n");
	if (Settings->IsUsingTestEnvironment())
	{
		Report += TEXT("  TEST environment (not live Jiwe):\n");
		Report += FString::Printf(TEXT("    login page  %s\n"), *Settings->GetAuthEndpoint());
		Report += FString::Printf(TEXT("    tokens      %s\n"), *Settings->GetTokenEndpoint());
		Report += FString::Printf(TEXT("    wallet      %s\n"), *Settings->GetWalletApiBase());
	}
	else
	{
		Report += TEXT("  Live Jiwe (id.jiwe.io, abacus.jiwe.io)\n");
	}

	Report += TEXT("5. Player\n");
	if (IsLoggedIn())
	{
		const FString Who = !UserInfo.Nickname.IsEmpty() ? UserInfo.Nickname : (!UserInfo.Name.IsEmpty() ? UserInfo.Name : UserInfo.Sub);
		Report += FString::Printf(TEXT("  Logged in as %s\n"), *Who);
	}
	else if (IsLoggingIn())
	{
		Report += TEXT("  Login in progress (waiting for the browser)\n");
	}
	else
	{
		Report += TEXT("  Not logged in. Type Jiwe.Login\n");
	}

	return Report;
}

// =========================================================================================
// Login
// =========================================================================================

bool UJiweSubsystem::IsLoggedIn() const
{
	return !IdToken.IsEmpty();
}

bool UJiweSubsystem::IsLoggingIn() const
{
	return LoginState != EJiweLoginState::Idle;
}

FJiweUserInfo UJiweSubsystem::GetUserInfo() const
{
	return UserInfo;
}

bool UJiweSubsystem::IsAccessTokenExpired() const
{
	return AccessTokenExpiresAtUtc.GetTicks() > 0 && FDateTime::UtcNow() >= AccessTokenExpiresAtUtc;
}

void UJiweSubsystem::Login()
{
	LoginWithCallback(nullptr);
}

void UJiweSubsystem::LoginWithCallback(FJiweLoginCallback OnComplete)
{
	if (OnComplete)
	{
		LoginCallbacks.Add(MoveTemp(OnComplete));
	}

	if (LoginState != EJiweLoginState::Idle)
	{
		UE_LOG(LogJiwe, Warning, TEXT("Login() was called while a login is already in progress. Ignored. Call CancelLogin() first to start again."));
		return;
	}

	const UJiweSettings* Settings = UJiweSettings::Get();
	if (Settings->ClientId.TrimStartAndEnd().IsEmpty())
	{
		FailLogin(FString::Printf(TEXT("Client ID is empty. Open %s and paste the Client ID from %s."), JiweLoginDetail::SettingsPath, JiweLoginDetail::JiweAppPath));
		return;
	}

	++LoginAttempt;
	CodeVerifier = JiweText::RandomUrlSafe(32);
	StateParam = JiweText::RandomUrlSafe(16);
	const FString CodeChallenge = JiweText::Base64Url(JiweText::Sha256(CodeVerifier));
	PendingRedirectUri = GetRedirectUri();
	FJiweRedirectInbox::Clear();

	// Each platform has its own way of getting the browser's answer back into the game.
	bool bCanReceiveRedirect = false;
	FString CannotReceiveReason;
#if PLATFORM_DESKTOP
	LoopbackListener = MakeShared<FJiweLoopbackListener>();
	if (LoopbackListener->Start(Settings->LoopbackPort))
	{
		bCanReceiveRedirect = true;
	}
	else
	{
		LoopbackListener.Reset();
		CannotReceiveReason = FString::Printf(
			TEXT("Port %d on this computer is already in use, so the login page cannot return to the game. Close the other program, or pick another Desktop Loopback Port in %s and register the new Redirect URI with Jiwe."),
			Settings->LoopbackPort, JiweLoginDetail::SettingsPath);
	}
#elif PLATFORM_ANDROID
	// The link comes back through GameActivity (see JiweSDK_UPL_Android.xml) into FJiweRedirectInbox.
	bCanReceiveRedirect = true;
#else
	CannotReceiveReason = TEXT("Jiwe login is not supported on this platform yet. Supported: Windows, Mac, Linux and Android.");
#endif

	if (!bCanReceiveRedirect)
	{
		FailLogin(CannotReceiveReason);
		return;
	}

	LoginState = EJiweLoginState::WaitingForBrowser;
	LoginDeadlineSeconds = FPlatformTime::Seconds() + static_cast<double>(Settings->LoginTimeoutSeconds);
	if (!LoginTickerHandle.IsValid())
	{
		LoginTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UJiweSubsystem::TickLogin), 0.1f);
	}

	const FString AuthUrl = BuildAuthUrl(PendingRedirectUri, CodeChallenge);
	UE_LOG(LogJiwe, Log, TEXT("Opening the Jiwe login page in the browser. Redirect URI sent: %s"), *PendingRedirectUri);

	FString LaunchError;
	FPlatformProcess::LaunchURL(*AuthUrl, nullptr, &LaunchError);
	if (!LaunchError.IsEmpty())
	{
		FailLogin(FString::Printf(TEXT("Could not open the browser: %s"), *LaunchError));
	}
}

void UJiweSubsystem::CancelLogin()
{
	if (LoginState != EJiweLoginState::Idle)
	{
		FailLogin(TEXT("Login cancelled."));
	}
}

void UJiweSubsystem::Logout()
{
	IdToken.Empty();
	AccessToken.Empty();
	AccessTokenExpiresAtUtc = FDateTime();
	UserInfo = FJiweUserInfo();
	UE_LOG(LogJiwe, Log, TEXT("Logged out."));
}

FString UJiweSubsystem::BuildAuthUrl(const FString& RedirectUri, const FString& CodeChallenge) const
{
	const UJiweSettings* Settings = UJiweSettings::Get();

	FString Scope = JiweLoginDetail::BaseScope;
	const FString Extra = Settings->ExtraScopes.TrimStartAndEnd();
	if (!Extra.IsEmpty())
	{
		Scope += TEXT(" ") + Extra;
	}

	FString Url = Settings->GetAuthEndpoint();
	Url += TEXT("?response_type=code");
	Url += TEXT("&scope=") + JiweText::UrlEncode(Scope);
	Url += TEXT("&redirect_uri=") + JiweText::UrlEncode(RedirectUri);
	Url += TEXT("&client_id=") + JiweText::UrlEncode(Settings->ClientId.TrimStartAndEnd());
	Url += TEXT("&state=") + StateParam;
	Url += TEXT("&code_challenge=") + CodeChallenge;
	Url += TEXT("&code_challenge_method=S256");

	const FString Resource = Settings->WalletResource.TrimStartAndEnd();
	if (!Resource.IsEmpty())
	{
		Url += TEXT("&resource=") + JiweText::UrlEncode(Resource);
	}
	return Url;
}

void UJiweSubsystem::StopWaitingForBrowser()
{
	// Dropping the listener joins its thread, so nothing can arrive after this returns.
	LoopbackListener.Reset();
	FJiweRedirectInbox::Clear();
}

bool UJiweSubsystem::TickLogin(float /*DeltaTime*/)
{
	if (LoginState == EJiweLoginState::WaitingForBrowser)
	{
		FString Url;
		while (LoginState == EJiweLoginState::WaitingForBrowser && FJiweRedirectInbox::Pop(Url))
		{
			HandleRedirect(Url);
		}
	}

	if (LoginState == EJiweLoginState::WaitingForBrowser && FPlatformTime::Seconds() > LoginDeadlineSeconds)
	{
		FailLogin(TEXT("Timed out waiting for the player to finish logging in in the browser. They can try again."));
	}

	// Keep ticking only while we are waiting for the browser.
	if (LoginState != EJiweLoginState::WaitingForBrowser)
	{
		LoginTickerHandle.Reset();
		return false;
	}
	return true;
}

void UJiweSubsystem::HandleRedirect(const FString& Url)
{
#if PLATFORM_ANDROID
	// Any link can open the game on Android. Only our own redirect counts.
	if (!Url.StartsWith(PendingRedirectUri, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogJiwe, Verbose, TEXT("Ignored a link that is not the Jiwe login redirect."));
		return;
	}
#endif

	const TMap<FString, FString> Query = JiweText::ParseQuery(Url);
	const FString Error = Query.FindRef(TEXT("error"));
	const FString ErrorDescription = Query.FindRef(TEXT("error_description"));
	const FString Code = Query.FindRef(TEXT("code"));
	const FString State = Query.FindRef(TEXT("state"));
	const FString RedirectUri = PendingRedirectUri;

	StopWaitingForBrowser();

	if (!Error.IsEmpty())
	{
		const FString Detail = ErrorDescription.IsEmpty() ? Error : FString::Printf(TEXT("%s (%s)"), *Error, *ErrorDescription);
		FailLogin(FString::Printf(
			TEXT("Jiwe refused the login: %s. On your Jiwe app, check that every scope box is ticked and that this Redirect URI is registered exactly: %s"),
			*Detail, *RedirectUri));
		return;
	}
	if (Code.IsEmpty())
	{
		FailLogin(TEXT("The browser came back without a login code. Please try again."));
		return;
	}
	if (!State.Equals(StateParam, ESearchCase::CaseSensitive))
	{
		FailLogin(TEXT("The reply did not belong to this login attempt (state mismatch). Please try again."));
		return;
	}

	LoginState = EJiweLoginState::ExchangingCode;

	const UJiweSettings* Settings = UJiweSettings::Get();
	const bool bHasSecret = !Settings->ApiSecret.TrimStartAndEnd().IsEmpty();
	const bool bBasicFirst = Settings->ClientAuth != EJiweClientAuth::RequestBody;
	const bool bMayRetryOtherWay = Settings->ClientAuth == EJiweClientAuth::Auto && bHasSecret;
	SendTokenRequest(Code, bBasicFirst, bMayRetryOtherWay);
}

void UJiweSubsystem::SendTokenRequest(const FString& Code, bool bBasicHeader, bool bMayRetryOtherWay)
{
	const UJiweSettings* Settings = UJiweSettings::Get();
	const FString ClientId = Settings->ClientId.TrimStartAndEnd();
	const FString Secret = Settings->ApiSecret.TrimStartAndEnd();
	const bool bHasSecret = !Secret.IsEmpty();

	FString Form = TEXT("grant_type=authorization_code");
	Form += TEXT("&code=") + JiweText::UrlEncode(Code);
	Form += TEXT("&redirect_uri=") + JiweText::UrlEncode(PendingRedirectUri);
	Form += TEXT("&code_verifier=") + JiweText::UrlEncode(CodeVerifier);

	TMap<FString, FString> Headers;
	if (bHasSecret && bBasicHeader)
	{
		// Secret in the Authorization header. Jiwe rejects a request that carries it in both places.
		const FString Pair = ClientId + TEXT(":") + Secret;
		const FTCHARToUTF8 PairUtf8(*Pair);
		TArray<uint8> PairBytes;
		PairBytes.Append(reinterpret_cast<const uint8*>(PairUtf8.Get()), PairUtf8.Length());
		Headers.Add(TEXT("Authorization"), TEXT("Basic ") + FBase64::Encode(PairBytes));
	}
	else
	{
		// No secret (public client), or secret in the body.
		Form += TEXT("&client_id=") + JiweText::UrlEncode(ClientId);
		if (bHasSecret)
		{
			Form += TEXT("&client_secret=") + JiweText::UrlEncode(Secret);
		}
	}

	const int32 Attempt = LoginAttempt;
	SendRequest(TEXT("POST"), Settings->GetTokenEndpoint(), Headers, Form, TEXT("application/x-www-form-urlencoded"),
		[this, Attempt, Code, bBasicHeader, bMayRetryOtherWay](const FJiweHttpReply& Reply)
		{
			if (Attempt != LoginAttempt || LoginState != EJiweLoginState::ExchangingCode)
			{
				return; // cancelled or superseded
			}
			HandleTokenReply(Reply, Code, bBasicHeader, bMayRetryOtherWay);
		});
}

void UJiweSubsystem::HandleTokenReply(const FJiweHttpReply& Reply, const FString& Code, bool bUsedBasicHeader, bool bMayRetryOtherWay)
{
	if (!Reply.bGotResponse)
	{
		FailLogin(TEXT("Jiwe did not answer while finishing the login (timeout or no internet). Please try again."));
		return;
	}

	if (!Reply.IsHttpOk())
	{
		// Jiwe has wanted the secret in the header for some apps and in the body for others.
		// These are the two replies seen when it was sent the wrong way.
		const bool bLooksLikeClientAuthRejection = Reply.Status == 401
			|| (Reply.Status == 400 && (Reply.Body.Contains(TEXT("client authentication")) || Reply.Body.Contains(TEXT("mechanism")) || Reply.Body.Contains(TEXT("invalid_client"))));
		if (bMayRetryOtherWay && bLooksLikeClientAuthRejection)
		{
			UE_LOG(LogJiwe, Log, TEXT("Jiwe rejected how the API Secret was sent. Retrying once the other way."));
			SendTokenRequest(Code, !bUsedBasicHeader, false);
			return;
		}

		const TSharedPtr<FJsonObject> ErrorObject = JiweJson::ParseObject(Reply.Body);
		const FString ErrorCode = JiweJson::GetString(ErrorObject, TEXT("error"));
		const FString ErrorDescription = JiweJson::GetString(ErrorObject, TEXT("error_description"));

		FString Message = FString::Printf(TEXT("Jiwe rejected the login at the last step (HTTP %d)"), Reply.Status);
		if (!ErrorCode.IsEmpty())
		{
			Message += TEXT(": ") + ErrorCode;
			if (!ErrorDescription.IsEmpty())
			{
				Message += TEXT(" - ") + ErrorDescription;
			}
		}
		else if (!Reply.Body.IsEmpty())
		{
			Message += TEXT(": ") + Reply.Body;
		}
		Message = JiweText::Truncate(Message, 220);

		// Say what to do about the two most common causes.
		if (ErrorCode.Equals(TEXT("invalid_client")) || bLooksLikeClientAuthRejection)
		{
			Message += FString::Printf(TEXT(" | Check Client ID and API Secret (the Login keys, not the Wallet keys) in %s."), JiweLoginDetail::SettingsPath);
		}
		else if (Reply.Body.Contains(TEXT("redirect_uri")))
		{
			Message += FString::Printf(TEXT(" | Register this Redirect URI on your Jiwe app, exactly: %s"), *PendingRedirectUri);
		}
		FailLogin(Message);
		return;
	}

	const TSharedPtr<FJsonObject> Token = JiweJson::ParseObject(Reply.Body);
	const FString NewIdToken = JiweJson::GetString(Token, TEXT("id_token"));
	if (NewIdToken.IsEmpty())
	{
		FailLogin(TEXT("Jiwe answered the login without an ID token. Check that the openid scope is ticked on your Jiwe app."));
		return;
	}

	PendingIdToken = NewIdToken;
	PendingAccessToken = JiweJson::GetString(Token, TEXT("access_token"));
	const double ExpiresInSeconds = JiweJson::GetNumber(Token, TEXT("expires_in"), 0.0);
	PendingExpiresAtUtc = ExpiresInSeconds > 0.0 ? FDateTime::UtcNow() + FTimespan::FromSeconds(ExpiresInSeconds) : FDateTime();

	// The player's profile travels inside the ID token itself.
	PendingUserInfo = DecodeIdTokenClaims(PendingIdToken);

	FetchUserInfoThenFinish();
}

FJiweUserInfo UJiweSubsystem::DecodeIdTokenClaims(const FString& InIdToken)
{
	// An ID token is three base64url parts joined by dots: header.payload.signature.
	// The payload is JSON. It came straight from Jiwe over HTTPS, and is only used for display.
	FJiweUserInfo Info;

	TArray<FString> Parts;
	InIdToken.ParseIntoArray(Parts, TEXT("."), false);
	if (Parts.Num() < 2)
	{
		return Info;
	}

	FString PayloadJson;
	if (!JiweText::Base64UrlDecodeToString(Parts[1], PayloadJson))
	{
		return Info;
	}

	const TSharedPtr<FJsonObject> Claims = JiweJson::ParseObject(PayloadJson);
	Info.Sub = JiweJson::GetString(Claims, TEXT("sub"));
	Info.Name = JiweJson::GetString(Claims, TEXT("name"));
	Info.Nickname = JiweJson::GetString(Claims, TEXT("nickname"));
	Info.Bio = JiweJson::GetString(Claims, TEXT("bio"));
	Info.Picture = JiweJson::GetString(Claims, TEXT("picture"));
	Info.WalletId = JiweJson::GetString(Claims, TEXT("walletId"));
	return Info;
}

void UJiweSubsystem::FetchUserInfoThenFinish()
{
	// Best effort: fills any profile fields the ID token did not carry. Login has already
	// succeeded at this point, so a failure here is ignored.
	if (PendingAccessToken.IsEmpty())
	{
		FinishLoginSuccess();
		return;
	}

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), TEXT("Bearer ") + PendingAccessToken);

	const int32 Attempt = LoginAttempt;
	SendRequest(TEXT("GET"), UJiweSettings::Get()->GetUserInfoEndpoint(), Headers, FString(), FString(),
		[this, Attempt](const FJiweHttpReply& Reply)
		{
			if (Attempt != LoginAttempt || LoginState != EJiweLoginState::ExchangingCode)
			{
				return;
			}
			if (Reply.IsHttpOk())
			{
				const TSharedPtr<FJsonObject> Fresh = JiweJson::ParseObject(Reply.Body);
				if (Fresh.IsValid())
				{
					if (PendingUserInfo.Sub.IsEmpty()) { PendingUserInfo.Sub = JiweJson::GetString(Fresh, TEXT("sub")); }
					if (PendingUserInfo.Name.IsEmpty()) { PendingUserInfo.Name = JiweJson::GetString(Fresh, TEXT("name")); }
					if (PendingUserInfo.Nickname.IsEmpty()) { PendingUserInfo.Nickname = JiweJson::GetString(Fresh, TEXT("nickname")); }
					if (PendingUserInfo.Bio.IsEmpty()) { PendingUserInfo.Bio = JiweJson::GetString(Fresh, TEXT("bio")); }
					if (PendingUserInfo.Picture.IsEmpty()) { PendingUserInfo.Picture = JiweJson::GetString(Fresh, TEXT("picture")); }
				}
			}
			FinishLoginSuccess();
		});
}

void UJiweSubsystem::FinishLoginSuccess()
{
	IdToken = PendingIdToken;
	AccessToken = PendingAccessToken;
	AccessTokenExpiresAtUtc = PendingExpiresAtUtc;
	UserInfo = PendingUserInfo;

	PendingIdToken.Empty();
	PendingAccessToken.Empty();
	PendingUserInfo = FJiweUserInfo();
	CodeVerifier.Empty();
	StateParam.Empty();
	LoginState = EJiweLoginState::Idle;
	++LoginAttempt;

	const FString Who = !UserInfo.Nickname.IsEmpty() ? UserInfo.Nickname : (!UserInfo.Name.IsEmpty() ? UserInfo.Name : UserInfo.Sub);
	UE_LOG(LogJiwe, Log, TEXT("Logged in to Jiwe as %s."), *Who);

	TArray<FJiweLoginCallback> Callbacks = MoveTemp(LoginCallbacks);
	LoginCallbacks.Reset();

	OnLoginSucceeded.Broadcast(UserInfo);
	for (const FJiweLoginCallback& Callback : Callbacks)
	{
		if (Callback)
		{
			Callback(true, FString());
		}
	}
}

void UJiweSubsystem::FailLogin(const FString& Message)
{
	StopWaitingForBrowser();

	PendingIdToken.Empty();
	PendingAccessToken.Empty();
	PendingUserInfo = FJiweUserInfo();
	CodeVerifier.Empty();
	StateParam.Empty();
	LoginState = EJiweLoginState::Idle;
	++LoginAttempt;

	// A server error can be a whole HTML page. Keep it short enough for a UI label.
	const FString ShortMessage = JiweText::Truncate(Message, 420);
	UE_LOG(LogJiwe, Warning, TEXT("Login failed: %s"), *ShortMessage);

	TArray<FJiweLoginCallback> Callbacks = MoveTemp(LoginCallbacks);
	LoginCallbacks.Reset();

	OnLoginFailed.Broadcast(ShortMessage);
	for (const FJiweLoginCallback& Callback : Callbacks)
	{
		if (Callback)
		{
			Callback(false, ShortMessage);
		}
	}
}

// =========================================================================================
// HTTP
// =========================================================================================

void UJiweSubsystem::SendRequest(const FString& Verb, const FString& Url, const TMap<FString, FString>& Headers, const FString& Body, const FString& ContentType, FJiweHttpCallback OnDone)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetTimeout(UJiweSettings::Get()->NetworkTimeoutSeconds);

	for (const TPair<FString, FString>& Header : Headers)
	{
		Request->SetHeader(Header.Key, Header.Value);
	}
	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), ContentType);
		Request->SetContentAsString(Body);
	}

	// The reply arrives on the game thread. Skip it if the game is shutting down.
	const TWeakObjectPtr<UJiweSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, OnDone](FHttpRequestPtr /*HttpRequest*/, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully)
		{
			const UJiweSubsystem* Self = WeakThis.Get();
			if (Self == nullptr || Self->bShuttingDown)
			{
				return;
			}

			FJiweHttpReply Reply;
			if (bConnectedSuccessfully && HttpResponse.IsValid())
			{
				Reply.bGotResponse = true;
				Reply.Status = HttpResponse->GetResponseCode();
				Reply.Body = HttpResponse->GetContentAsString();
			}
			if (OnDone)
			{
				OnDone(Reply);
			}
		});

	Request->ProcessRequest();
}
