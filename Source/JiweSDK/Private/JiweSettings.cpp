#include "JiweSettings.h"

namespace
{
	const TCHAR* DefaultAuthEndpoint = TEXT("https://id.jiwe.io/auth");
	const TCHAR* DefaultTokenEndpoint = TEXT("https://id.jiwe.io/token");
	const TCHAR* DefaultUserInfoEndpoint = TEXT("https://id.jiwe.io/me");
	const TCHAR* DefaultWalletBaseUrl = TEXT("https://abacus.jiwe.io/rest");

	FString ValueOrDefault(const FString& Value, const TCHAR* Default)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		return Trimmed.IsEmpty() ? FString(Default) : Trimmed;
	}
}

UJiweSettings::UJiweSettings()
	: LoopbackPort(53682)
	, MobileRedirectScheme(TEXT("jiwewallet"))
	, bLoginOnStart(false)
	, LoginTimeoutSeconds(180.f)
	, NetworkTimeoutSeconds(20.f)
	, ClientAuth(EJiweClientAuth::Auto)
	, PlayerToken(EJiwePlayerToken::Auto)
{
}

FName UJiweSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

const UJiweSettings* UJiweSettings::Get()
{
	return GetDefault<UJiweSettings>();
}

FString UJiweSettings::GetAuthEndpoint() const
{
	return ValueOrDefault(AuthorizationEndpoint, DefaultAuthEndpoint);
}

FString UJiweSettings::GetTokenEndpoint() const
{
	return ValueOrDefault(TokenEndpoint, DefaultTokenEndpoint);
}

FString UJiweSettings::GetUserInfoEndpoint() const
{
	return ValueOrDefault(UserInfoEndpoint, DefaultUserInfoEndpoint);
}

FString UJiweSettings::GetWalletApiBase() const
{
	FString Base = ValueOrDefault(WalletBaseUrl, DefaultWalletBaseUrl);
	while (Base.EndsWith(TEXT("/")))
	{
		Base.LeftChopInline(1);
	}
	return Base + TEXT("/api/v1");
}

bool UJiweSettings::IsUsingTestEnvironment() const
{
	return !AuthorizationEndpoint.TrimStartAndEnd().IsEmpty()
		|| !TokenEndpoint.TrimStartAndEnd().IsEmpty()
		|| !UserInfoEndpoint.TrimStartAndEnd().IsEmpty()
		|| !WalletBaseUrl.TrimStartAndEnd().IsEmpty();
}
