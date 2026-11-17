#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JiweTypes.h"
#include "JiweSubsystem.generated.h"

class FJiweLoopbackListener;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiweLoginSucceeded, const FJiweUserInfo&, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiweLoginFailed, const FString&, Error);

/** Where a login attempt has got to. Internal. */
enum class EJiweLoginState : uint8
{
	Idle,
	WaitingForBrowser,
	ExchangingCode
};

/** One HTTP answer from Jiwe. Internal. */
struct FJiweHttpReply
{
	/** False when no answer arrived at all (timeout, no connection). */
	bool bGotResponse = false;
	int32 Status = 0;
	FString Body;

	bool IsHttpOk() const { return bGotResponse && Status >= 200 && Status < 400; }
};

typedef TFunction<void(const FJiweHttpReply&)> FJiweHttpCallback;

/**
 * The one object you talk to.
 *
 *   Blueprint:  "Get Jiwe Subsystem", or the "Jiwe ..." nodes (Jiwe Login, Jiwe Give XP, ...)
 *   C++:        UJiweSubsystem::Get(this)->GiveXpReward(20, TEXT("Beat boss 1"), [](const FJiweResult& R) { ... });
 *   Console:    Jiwe.Status   Jiwe.Login   Jiwe.GiveXp 10   Jiwe.Leaderboard
 *
 * It exists for the whole game session. There is nothing to place in a level.
 *
 * Two kinds of calls:
 *   - Player calls (rewards, purchases, user data) need a logged-in player. Call Login first.
 *   - App calls (leaderboard, balance, game data) only need your Wallet keys and work without login.
 *
 * Keys are set in Project Settings > Plugins > Jiwe SDK.
 */
UCLASS()
class JIWESDK_API UJiweSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Returns the subsystem for the game that WorldContextObject belongs to (any actor, widget or component). */
	static UJiweSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// =====================================================================================
	// Login
	// =====================================================================================

	/**
	 * Opens the Jiwe login page in the player's browser. When they finish, OnLoginSucceeded or
	 * OnLoginFailed fires. Does nothing if a login is already in progress.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jiwe|Login")
	void Login();

	/** C++: same as Login(), with a one-off callback as well as the events. */
	void LoginWithCallback(FJiweLoginCallback OnComplete);

	/**
	 * Gives up on a login that is in progress. Always give players a Cancel button that stays
	 * clickable while they are in the browser. Safe to call at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jiwe|Login")
	void CancelLogin();

	/** Forgets the logged-in player on this device. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe|Login")
	void Logout();

	UFUNCTION(BlueprintPure, Category = "Jiwe|Login")
	bool IsLoggedIn() const;

	/** True from Login() until it succeeds, fails or is cancelled. */
	UFUNCTION(BlueprintPure, Category = "Jiwe|Login")
	bool IsLoggingIn() const;

	UFUNCTION(BlueprintPure, Category = "Jiwe|Login")
	FJiweUserInfo GetUserInfo() const;

	/** True once the access token's lifetime has passed. Send the player through Login() again. */
	UFUNCTION(BlueprintPure, Category = "Jiwe|Login")
	bool IsAccessTokenExpired() const;

	UPROPERTY(BlueprintAssignable, Category = "Jiwe|Login")
	FJiweLoginSucceeded OnLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Jiwe|Login")
	FJiweLoginFailed OnLoginFailed;

	// =====================================================================================
	// Setup help
	// =====================================================================================

	/**
	 * The exact Redirect URI this build sends when logging in on this platform.
	 * It must be registered, character for character, on your Jiwe app.
	 */
	UFUNCTION(BlueprintPure, Category = "Jiwe|Setup")
	FString GetRedirectUri() const;

	/** A plain-language checklist of what is set up and what is missing. Same text as the Jiwe.Status console command. */
	UFUNCTION(BlueprintPure, Category = "Jiwe|Setup")
	FString GetSetupReport() const;

	// =====================================================================================
	// Player calls: need a logged-in player
	// =====================================================================================

	/** Credits XP to the logged-in player. Xp must be greater than 0. TransactionId is optional. */
	void GiveXpReward(int32 Xp, const FString& Description, FJiweResultCallback OnComplete = nullptr, const FString& TransactionId = FString());

	/** Sends airtime to a phone number, paid from your app's reward pool. */
	void GiveAirtimeReward(int32 Amount, const FString& RecipientNumber, const FString& Description, FJiweResultCallback OnComplete = nullptr, const FString& TransactionId = FString());

	/**
	 * Charges the logged-in player's own Jiwe wallet for an in-game item.
	 * IdempotencyKey is required: create it once per purchase attempt, save it, and send the SAME key
	 * if you retry after a timeout. A new key on a retry can charge the player twice.
	 */
	void PurchaseWithWallet(int32 Amount, const FString& ItemId, const FString& Description, const FString& IdempotencyKey, FJiwePurchaseCallback OnComplete);

	void GetPurchaseStatus(const FString& PaymentOrderId, FJiwePurchaseCallback OnComplete);

	/** Lifetime totals this player has earned from your game. Not a spendable balance. */
	void GetRewardsEarned(FJiweRewardsEarnedCallback OnComplete);

	/** Same totals using only the player's access token (needs the rewards:read scope and a Wallet Resource). */
	void GetRewardsEarnedBearer(FJiweRewardsEarnedCallback OnComplete);

	/** Reads this player's saved data. The reply looks like {"type":"OK","userData":{...}} in RawResponse. */
	void GetUserData(FJiweResultCallback OnComplete);

	/** Merges into this player's saved data. JsonPayload looks like {"userData":{"levelUnlocked":15}}. */
	void SetUserData(const FString& JsonPayload, FJiweResultCallback OnComplete);

	// =====================================================================================
	// App calls: no login needed
	// =====================================================================================

	/**
	 * RewardType: "xp" or "cowrie".  Period: "day", "week", "month", "year", or empty for all-time.
	 * BestPointsRanking: "highest" (default) or "lowest".
	 */
	void GetLeaderboard(const FString& RewardType, int32 MaxEntries, const FString& Period, FJiweLeaderboardCallback OnComplete, const FString& BestPointsRanking = TEXT("highest"));

	/** Your app's reward pool. Check it before offering an airtime reward. */
	void GetWalletBalance(FJiweBalanceCallback OnComplete);

	void GetTransactionStatus(const FString& TransactionId, FJiweResultCallback OnComplete);

	/** False only when Jiwe definitely rejected this build's Wallet keys (401/403). Use it for a forced-update prompt. */
	void CheckCredentialsValid(FJiweBoolCallback OnComplete);

	/** Reads your app's shared Game Data. Treat everything stored there as public. */
	void GetGameData(FJiweResultCallback OnComplete);

	/** Merges into Game Data. JsonPayload looks like {"gameData":{"minVersion":"1.8"}}. */
	void SetGameData(const FString& JsonPayload, FJiweResultCallback OnComplete);

	/** Sent with reward calls so Jiwe can group them. Generated at start; change it if you have your own session IDs. */
	UPROPERTY(BlueprintReadWrite, Category = "Jiwe")
	FString GameSessionId;

private:
	// ---- login ----
	bool TickLogin(float DeltaTime);
	void HandleRedirect(const FString& Url);
	void SendTokenRequest(const FString& Code, bool bBasicHeader, bool bMayRetryOtherWay);
	void HandleTokenReply(const FJiweHttpReply& Reply, const FString& Code, bool bUsedBasicHeader, bool bMayRetryOtherWay);
	void FetchUserInfoThenFinish();
	void FinishLoginSuccess();
	void FailLogin(const FString& Message);
	void StopWaitingForBrowser();
	FString BuildAuthUrl(const FString& RedirectUri, const FString& CodeChallenge) const;
	static FJiweUserInfo DecodeIdTokenClaims(const FString& InIdToken);

	// ---- wallet ----
	FString GetPlayerToken() const;
	TMap<FString, FString> AppHeaders() const;
	TMap<FString, FString> PlayerHeaders() const;
	void SendRequest(const FString& Verb, const FString& Url, const TMap<FString, FString>& Headers, const FString& Body, const FString& ContentType, FJiweHttpCallback OnDone);
	void SendResultRequest(const FString& Verb, const FString& Path, const TMap<FString, FString>& Headers, const FString& JsonBody, FJiweResultCallback OnComplete);
	void SendRewardsEarned(bool bBearer, FJiweRewardsEarnedCallback OnComplete);

	static FJiweResult ToResult(const FJiweHttpReply& Reply);
	static FJiwePurchaseResult ToPurchaseResult(const FJiweHttpReply& Reply);
	static FJiweResult NotLoggedInResult();
	static bool IsRetryableStatus(int32 Status);
	static FString DescribeHttpFailure(const FJiweHttpReply& Reply);

	// ---- state ----
	EJiweLoginState LoginState = EJiweLoginState::Idle;
	/** Bumped whenever a login ends, so late replies from an old attempt are ignored. */
	int32 LoginAttempt = 0;
	FString CodeVerifier;
	FString StateParam;
	FString PendingRedirectUri;
	double LoginDeadlineSeconds = 0.0;
	TArray<FJiweLoginCallback> LoginCallbacks;
	FTSTicker::FDelegateHandle LoginTickerHandle;
	TSharedPtr<FJiweLoopbackListener> LoopbackListener;

	// Tokens from a login that is still finishing. Committed only when the whole login succeeds,
	// so a cancelled or failed attempt never leaves a half-logged-in player.
	FString PendingIdToken;
	FString PendingAccessToken;
	FDateTime PendingExpiresAtUtc;
	FJiweUserInfo PendingUserInfo;

	FString IdToken;
	FString AccessToken;
	FDateTime AccessTokenExpiresAtUtc;
	FJiweUserInfo UserInfo;

	bool bShuttingDown = false;
};
