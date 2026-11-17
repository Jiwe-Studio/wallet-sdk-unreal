#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "JiweTypes.h"
#include "JiweBlueprintNodes.generated.h"

class UJiweSubsystem;

// The Blueprint nodes. Each one has two output pins: On Success and On Failure.
// Find them by right-clicking in a Blueprint graph and typing "Jiwe".

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJiweLoginPin, const FJiweUserInfo&, User, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiweResultPin, const FJiweResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FJiweLeaderboardPin, const TArray<FJiweLeaderboardEntry>&, Entries, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiweBalancePin, const FJiweBalance&, Balance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiweRewardsEarnedPin, const FJiweRewardsEarned&, Earned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJiwePurchasePin, const FJiwePurchaseResult&, Result);

/** Internal: "start this call on the subsystem, and tell me when it is done". */
typedef TFunction<void(UJiweSubsystem&, FJiweResultCallback)> FJiweStartCall;

/** Shared plumbing for the nodes below. */
UCLASS(Abstract)
class JIWESDK_API UJiweNodeBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	/** The Jiwe subsystem for the game this node is running in, or null. */
	UJiweSubsystem* FindJiwe() const;

	UPROPERTY()
	TObjectPtr<UObject> WorldContext;
};

/** Jiwe Login */
UCLASS()
class JIWESDK_API UJiweLoginNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiweLoginPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiweLoginPin OnFailure;

	/** Opens the Jiwe login page in the player's browser and waits for them to finish. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Login"))
	static UJiweLoginNode* JiweLogin(UObject* WorldContextObject);

	virtual void Activate() override;
};

/**
 * Jiwe Give XP, Jiwe Give Airtime, Jiwe Get Transaction Status,
 * Jiwe Get User Data, Jiwe Set User Data, Jiwe Get Game Data, Jiwe Set Game Data
 */
UCLASS()
class JIWESDK_API UJiweCallNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiweResultPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiweResultPin OnFailure;

	/** Credits XP to the logged-in player. XP must be greater than 0. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Give XP"))
	static UJiweCallNode* JiweGiveXp(UObject* WorldContextObject, int32 Xp, const FString& Description);

	/** Sends airtime to a phone number, paid from your app's reward pool. Needs a logged-in player. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Give Airtime"))
	static UJiweCallNode* JiweGiveAirtime(UObject* WorldContextObject, int32 Amount, const FString& RecipientNumber, const FString& Description);

	/** Checks a transaction by the Ledger Transaction Id a reward returned. Status is in Result.Status. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Transaction Status"))
	static UJiweCallNode* JiweGetTransactionStatus(UObject* WorldContextObject, const FString& TransactionId);

	/** Reads the logged-in player's saved data. The JSON is in Result.RawResponse. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get User Data"))
	static UJiweCallNode* JiweGetUserData(UObject* WorldContextObject);

	/** Merges into the logged-in player's saved data. Json Payload looks like {"userData":{"levelUnlocked":15}} */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Set User Data"))
	static UJiweCallNode* JiweSetUserData(UObject* WorldContextObject, const FString& JsonPayload);

	/** Reads your app's shared Game Data (no login needed). The JSON is in Result.RawResponse. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Game Data"))
	static UJiweCallNode* JiweGetGameData(UObject* WorldContextObject);

	/** Merges into your app's shared Game Data. Json Payload looks like {"gameData":{"minVersion":"1.8"}} */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Set Game Data"))
	static UJiweCallNode* JiweSetGameData(UObject* WorldContextObject, const FString& JsonPayload);

	virtual void Activate() override;

private:
	static UJiweCallNode* Make(UObject* WorldContextObject, FJiweStartCall InStartCall);

	FJiweStartCall StartCall;
};

/** Jiwe Get Leaderboard */
UCLASS()
class JIWESDK_API UJiweLeaderboardNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiweLeaderboardPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiweLeaderboardPin OnFailure;

	/**
	 * Reads a leaderboard. No login needed.
	 * Reward Type: xp or cowrie.  Period: day, week, month, year, or leave empty for all-time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Leaderboard"))
	static UJiweLeaderboardNode* JiweGetLeaderboard(UObject* WorldContextObject, const FString& RewardType, int32 MaxEntries, const FString& Period);

	virtual void Activate() override;

private:
	// "Saved" prefix: these must not share a name with the node's input pins above.
	FString SavedRewardType;
	int32 SavedMaxEntries = 10;
	FString SavedPeriod;
};

/** Jiwe Get Wallet Balance */
UCLASS()
class JIWESDK_API UJiweBalanceNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiweBalancePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiweBalancePin OnFailure;

	/** Reads your app's reward pool. No login needed. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Wallet Balance"))
	static UJiweBalanceNode* JiweGetWalletBalance(UObject* WorldContextObject);

	virtual void Activate() override;
};

/** Jiwe Get Rewards Earned */
UCLASS()
class JIWESDK_API UJiweRewardsEarnedNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiweRewardsEarnedPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiweRewardsEarnedPin OnFailure;

	/** Lifetime totals the logged-in player has earned from your game. Not a spendable balance. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Rewards Earned"))
	static UJiweRewardsEarnedNode* JiweGetRewardsEarned(UObject* WorldContextObject);

	virtual void Activate() override;
};

/** Jiwe Purchase With Wallet, Jiwe Get Purchase Status */
UCLASS()
class JIWESDK_API UJiwePurchaseNode : public UJiweNodeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FJiwePurchasePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FJiwePurchasePin OnFailure;

	/**
	 * Charges the logged-in player's Jiwe wallet for an in-game item.
	 * Idempotency Key: make one per purchase attempt, save it, and send the SAME key if you retry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Purchase With Wallet"))
	static UJiwePurchaseNode* JiwePurchaseWithWallet(UObject* WorldContextObject, int32 Amount, const FString& ItemId, const FString& Description, const FString& IdempotencyKey);

	/** Checks a purchase by the Payment Order Id that Jiwe Purchase With Wallet returned. */
	UFUNCTION(BlueprintCallable, Category = "Jiwe", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Jiwe Get Purchase Status"))
	static UJiwePurchaseNode* JiweGetPurchaseStatus(UObject* WorldContextObject, const FString& PaymentOrderId);

	virtual void Activate() override;

private:
	bool bStatusCheckOnly = false;
	int32 SavedAmount = 0;
	FString SavedItemId;
	FString SavedDescription;
	FString SavedIdempotencyKey;
	FString SavedPaymentOrderId;
};
