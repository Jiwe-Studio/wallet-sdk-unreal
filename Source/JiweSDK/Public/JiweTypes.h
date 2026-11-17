#pragma once

#include "CoreMinimal.h"
#include "JiweTypes.generated.h"

/** How the client secret is sent when the login code is swapped for tokens. Leave on Auto. */
UENUM(BlueprintType)
enum class EJiweClientAuth : uint8
{
	/** Try the Authorization header first; if Jiwe rejects it, retry once with the secret in the body. */
	Auto,
	/** Authorization: Basic header only. */
	BasicHeader,
	/** client_id and client_secret in the request body. */
	RequestBody
};

/** Which login token identifies the player on reward, purchase and user-data calls. Leave on Auto. */
UENUM(BlueprintType)
enum class EJiwePlayerToken : uint8
{
	/** Access token when a Wallet Resource is set in the settings, otherwise the ID token. */
	Auto,
	IdToken,
	AccessToken
};

/** The logged-in player's public profile. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweUserInfo
{
	GENERATED_BODY()

	/** The player's permanent Jiwe ID. Use this to tell players apart, not the name. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Sub;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Nickname;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Bio;

	/** URL of the player's avatar image. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Picture;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString WalletId;
};

/** The answer to most wallet calls. Always check bSuccess first. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bSuccess = false;

	/** What went wrong, in words. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Error;

	/** Exactly what Jiwe sent back. Parse this yourself for Game Data and User Data. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString RawResponse;

	/** HTTP status. 0 means no answer arrived, or the call was never sent (for example, not logged in). */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 HttpStatus = 0;

	/** True when trying again makes sense: no answer, rate limited (429) or a server error (5xx). */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bRetryable = false;

	/** Jiwe's ID for this transaction. Keep it if you want to check its status later. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString LedgerTransactionId;

	/** For status checks: PENDING, POSTED, ARCHIVED, FAILED or REVERSED. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Status;
};

/** Your app's reward pool (float) on Jiwe. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweBalance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Error;

	/** Whole units, rounded down, so an "is there enough?" check never rounds up. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Debits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Available = 0;

	/** Exact values. They can have a fractional part. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	double DebitsAmount = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	double CreditsAmount = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	double AvailableAmount = 0.0;
};

/** Lifetime totals the player has earned from your game. Not a spendable balance. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweRewardsEarned
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString RawResponse;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 HttpStatus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	float Points = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	float XP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	float AirtimeEarned = 0.f;

	/** Currency of AirtimeEarned. "KES" when Jiwe does not say. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString AirtimeCurrency;
};

/** The answer to a purchase or a purchase status check. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiwePurchaseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString RawResponse;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 HttpStatus = 0;

	/** True: retry with the SAME idempotency key. False after a failure: the answer is final, a new attempt needs a NEW key. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bRetryable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString PaymentOrderId;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString LedgerTransactionId;

	/** PENDING, POSTED, ARCHIVED, FAILED or REVERSED. */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bRequiresApproval = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Currency;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bCatalogPriceValidated = false;
};

/** One row of a leaderboard. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Username;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Rank = 0;

	/**
	 * The number to show. Jiwe sends the score under different names depending on the board
	 * ("XP" for a period board, "currentXP" or "xp" for all-time); this is whichever one is set.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	int32 Score = 0;
};

/** The answer to a leaderboard request. */
USTRUCT(BlueprintType)
struct JIWESDK_API FJiweLeaderboard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "Jiwe")
	TArray<FJiweLeaderboardEntry> Entries;
};

// C++ callback types. Every callback runs on the game thread.
typedef TFunction<void(bool bSuccess, const FString& Error)> FJiweLoginCallback;
typedef TFunction<void(const FJiweResult& Result)> FJiweResultCallback;
typedef TFunction<void(const FJiweBalance& Balance)> FJiweBalanceCallback;
typedef TFunction<void(const FJiweRewardsEarned& Earned)> FJiweRewardsEarnedCallback;
typedef TFunction<void(const FJiwePurchaseResult& Result)> FJiwePurchaseCallback;
typedef TFunction<void(const FJiweLeaderboard& Leaderboard)> FJiweLeaderboardCallback;
typedef TFunction<void(bool bValid)> FJiweBoolCallback;
