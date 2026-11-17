#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JiweTypes.h"
#include "JiweSettings.generated.h"

/**
 * Project Settings > Plugins > Jiwe SDK.
 *
 * Jiwe gives every game FOUR values, in two pairs. Mixing the pairs up is the most common
 * setup mistake, so they are kept in two separate groups here:
 *
 *   Login keys   (Client ID + API Secret)    -> used once, to log a player in
 *   Wallet keys  (API Username + API Key)    -> sent with every reward/leaderboard call
 *
 * All four come from jiwe.io > Profile > My Apps > your application.
 * Values are saved to Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Jiwe SDK"))
class JIWESDK_API UJiweSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UJiweSettings();

	virtual FName GetCategoryName() const override;

	static const UJiweSettings* Get();

	// ---------------------------------------------------------------- 1. Login keys

	/** From jiwe.io > Profile > My Apps. Identifies your game when a player logs in. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "1 - Login Keys", meta = (DisplayName = "Client ID"))
	FString ClientId;

	/** From the same page. Leave empty only if Jiwe set your app up as a public client. This value ships inside your game. */
	UPROPERTY(Config, EditAnywhere, Category = "1 - Login Keys", meta = (DisplayName = "API Secret"))
	FString ApiSecret;

	// ---------------------------------------------------------------- 2. Wallet keys

	/** From jiwe.io > Profile > My Apps. Sent as X-API-USERNAME on every wallet call. */
	UPROPERTY(Config, EditAnywhere, Category = "2 - Wallet Keys", meta = (DisplayName = "API Username"))
	FString ApiUsername;

	/** From the same page. Sent as X-API-KEY on every wallet call. This value ships inside your game. */
	UPROPERTY(Config, EditAnywhere, Category = "2 - Wallet Keys", meta = (DisplayName = "API Key"))
	FString ApiKey;

	// ---------------------------------------------------------------- 3. Redirects

	/**
	 * Windows, Mac, Linux and the editor. After login the browser returns to
	 * http://127.0.0.1:<this port>/ and the plugin listens there.
	 * Register exactly that address (with the trailing slash) as a Redirect URI on your Jiwe app.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "3 - Redirects", meta = (DisplayName = "Desktop Loopback Port", ClampMin = "1024", ClampMax = "65535"))
	int32 LoopbackPort;

	/**
	 * Android. After login the browser returns to <this scheme>://oauth-callback, which reopens the game.
	 * Use lowercase letters only, unique to your game (for example "kentepunk").
	 * Register <scheme>://oauth-callback as a Redirect URI on your Jiwe app. Package again after changing it.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "3 - Redirects", meta = (DisplayName = "Android Redirect Scheme"))
	FString MobileRedirectScheme;

	// ---------------------------------------------------------------- 4. Behaviour

	/** Open the Jiwe login page as soon as the game starts. Off by default: most games log in from a menu button. */
	UPROPERTY(Config, EditAnywhere, Category = "4 - Behaviour")
	bool bLoginOnStart;

	/** How long the player has to finish logging in in the browser before login fails with a timeout. */
	UPROPERTY(Config, EditAnywhere, Category = "4 - Behaviour", meta = (ClampMin = "20", ClampMax = "900", Units = "s"))
	float LoginTimeoutSeconds;

	/** How long to wait for Jiwe to answer any single request. */
	UPROPERTY(Config, EditAnywhere, Category = "4 - Behaviour", meta = (ClampMin = "5", ClampMax = "120", Units = "s"))
	float NetworkTimeoutSeconds;

	// ---------------------------------------------------------------- 5. Advanced (leave alone unless Jiwe tells you otherwise)

	/** How the API Secret is presented when logging in. Auto works for every known Jiwe setup. */
	UPROPERTY(Config, EditAnywhere, Category = "5 - Advanced")
	EJiweClientAuth ClientAuth;

	/** Which login token identifies the player on wallet calls. */
	UPROPERTY(Config, EditAnywhere, Category = "5 - Advanced")
	EJiwePlayerToken PlayerToken;

	/** Extra login scopes, separated by spaces (for example "rewards:read"). Each must also be enabled on your Jiwe app. */
	UPROPERTY(Config, EditAnywhere, Category = "5 - Advanced")
	FString ExtraScopes;

	/** Wallet API resource identifier, if Jiwe issued you one. */
	UPROPERTY(Config, EditAnywhere, Category = "5 - Advanced")
	FString WalletResource;

	// ---------------------------------------------------------------- 6. Test environment (leave all four empty for live Jiwe)

	/** Empty = https://id.jiwe.io/auth */
	UPROPERTY(Config, EditAnywhere, Category = "6 - Test Environment")
	FString AuthorizationEndpoint;

	/** Empty = https://id.jiwe.io/token */
	UPROPERTY(Config, EditAnywhere, Category = "6 - Test Environment")
	FString TokenEndpoint;

	/** Empty = https://id.jiwe.io/me */
	UPROPERTY(Config, EditAnywhere, Category = "6 - Test Environment")
	FString UserInfoEndpoint;

	/** Empty = https://abacus.jiwe.io/rest  (do not add /api/v1) */
	UPROPERTY(Config, EditAnywhere, Category = "6 - Test Environment")
	FString WalletBaseUrl;

	// ---------------------------------------------------------------- Resolved values

	FString GetAuthEndpoint() const;
	FString GetTokenEndpoint() const;
	FString GetUserInfoEndpoint() const;
	/** Wallet base including /api/v1, no trailing slash. */
	FString GetWalletApiBase() const;
	/** True when any endpoint points somewhere other than live Jiwe. */
	bool IsUsingTestEnvironment() const;
};
