// Jiwe subsystem: wallet calls. Every request, header and payload here mirrors the Jiwe Unity SDK
// (JiweWallet.cs), which is what Jiwe's live API has been proven against in shipped games.
//
// Which headers go on which call:
//   App calls     X-API-USERNAME + X-API-KEY                      (leaderboard, balance, game data)
//   Player calls  X-API-USERNAME + X-API-KEY + X-API-TOKEN        (rewards, purchases, user data)
//   Bearer call   Authorization: Bearer <access token> only       (GetRewardsEarnedBearer)

#include "JiweSubsystem.h"
#include "JiweJson.h"
#include "JiweSDKModule.h"
#include "JiweSettings.h"
#include "JiweText.h"
#include "Misc/Guid.h"

namespace JiweWalletDetail
{
	const TCHAR* JsonContentType = TEXT("application/json");
	const TCHAR* NotLoggedInMessage = TEXT("No player is logged in. Call Login first (Blueprint: Jiwe Login, console: Jiwe.Login).");

	/** Adds a next step to the two failures a developer can fix from the settings page. */
	FString WithHint(int32 Status, const FString& Message)
	{
		if (Status == 403)
		{
			return Message + TEXT(" | Check the Wallet keys (API Username and API Key) in Project Settings > Plugins > Jiwe SDK.");
		}
		if (Status == 401)
		{
			return Message + TEXT(" | The player's login may have expired, or the keys are for a different Jiwe environment. Log in again.");
		}
		return Message;
	}
}

// =========================================================================================
// Shared pieces
// =========================================================================================

FString UJiweSubsystem::GetPlayerToken() const
{
	const UJiweSettings* Settings = UJiweSettings::Get();
	const bool bUseAccessToken = Settings->PlayerToken == EJiwePlayerToken::AccessToken
		|| (Settings->PlayerToken == EJiwePlayerToken::Auto && !Settings->WalletResource.TrimStartAndEnd().IsEmpty() && !AccessToken.IsEmpty());
	return bUseAccessToken ? AccessToken : IdToken;
}

TMap<FString, FString> UJiweSubsystem::AppHeaders() const
{
	const UJiweSettings* Settings = UJiweSettings::Get();
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("X-API-USERNAME"), Settings->ApiUsername.TrimStartAndEnd());
	Headers.Add(TEXT("X-API-KEY"), Settings->ApiKey.TrimStartAndEnd());
	return Headers;
}

TMap<FString, FString> UJiweSubsystem::PlayerHeaders() const
{
	TMap<FString, FString> Headers = AppHeaders();
	Headers.Add(TEXT("X-API-TOKEN"), GetPlayerToken());
	return Headers;
}

bool UJiweSubsystem::IsRetryableStatus(int32 Status)
{
	// No answer at all (you cannot tell whether it arrived), rate limited, or a server error.
	// Any other 4xx is a final answer that will not change on retry.
	return Status == 0 || Status == 429 || Status >= 500;
}

FString UJiweSubsystem::DescribeHttpFailure(const FJiweHttpReply& Reply)
{
	if (!Reply.bGotResponse)
	{
		return TEXT("No answer from Jiwe (timeout or no internet connection).");
	}
	return FString::Printf(TEXT("Jiwe answered with HTTP %d."), Reply.Status);
}

FJiweResult UJiweSubsystem::NotLoggedInResult()
{
	FJiweResult Result;
	Result.bSuccess = false;
	Result.Error = JiweWalletDetail::NotLoggedInMessage;
	return Result;
}

FJiweResult UJiweSubsystem::ToResult(const FJiweHttpReply& Reply)
{
	FJiweResult Result;
	Result.RawResponse = Reply.Body;
	Result.HttpStatus = Reply.Status;

	bool bSuccess = Reply.IsHttpOk();
	FString Message;

	const TSharedPtr<FJsonObject> Parsed = JiweJson::ParseObject(Reply.Body);
	if (Parsed.IsValid())
	{
		Message = JiweJson::GetString(Parsed, TEXT("message"));
		Result.LedgerTransactionId = JiweJson::GetString(Parsed, TEXT("ledger_transaction_id"));
		Result.Status = JiweJson::GetString(Parsed, TEXT("status"));
		if (JiweJson::GetString(Parsed, TEXT("type")).Equals(TEXT("ERROR"), ESearchCase::CaseSensitive))
		{
			bSuccess = false;
		}
	}

	Result.bSuccess = bSuccess;
	if (!bSuccess)
	{
		Result.Error = JiweWalletDetail::WithHint(Reply.Status, Message.IsEmpty() ? DescribeHttpFailure(Reply) : Message);
		Result.bRetryable = IsRetryableStatus(Reply.Status);
	}
	return Result;
}

FJiwePurchaseResult UJiweSubsystem::ToPurchaseResult(const FJiweHttpReply& Reply)
{
	FJiwePurchaseResult Result;
	Result.RawResponse = Reply.Body;
	Result.HttpStatus = Reply.Status;

	bool bSuccess = Reply.IsHttpOk();
	FString Message;

	const TSharedPtr<FJsonObject> Parsed = JiweJson::ParseObject(Reply.Body);
	if (Parsed.IsValid())
	{
		Message = JiweJson::GetString(Parsed, TEXT("message"));
		if (JiweJson::GetString(Parsed, TEXT("type")).Equals(TEXT("ERROR"), ESearchCase::CaseSensitive))
		{
			bSuccess = false;
		}
		Result.PaymentOrderId = JiweJson::GetString(Parsed, TEXT("payment_order_id"));
		Result.LedgerTransactionId = JiweJson::GetString(Parsed, TEXT("ledger_transaction_id"));
		Result.Status = JiweJson::GetString(Parsed, TEXT("status"));
		Result.bRequiresApproval = JiweJson::GetBool(Parsed, TEXT("requires_approval"));
		Result.Amount = static_cast<int32>(JiweJson::GetNumber(Parsed, TEXT("amount")));
		Result.Currency = JiweJson::GetString(Parsed, TEXT("currency"));
		Result.ItemId = JiweJson::GetString(Parsed, TEXT("itemId"));
		Result.Description = JiweJson::GetString(Parsed, TEXT("description"));
		Result.bCatalogPriceValidated = JiweJson::GetBool(Parsed, TEXT("catalogPriceValidated"));
	}

	Result.bSuccess = bSuccess;
	if (!bSuccess)
	{
		Result.Error = JiweWalletDetail::WithHint(Reply.Status, Message.IsEmpty() ? DescribeHttpFailure(Reply) : Message);
		Result.bRetryable = IsRetryableStatus(Reply.Status);
	}
	return Result;
}

void UJiweSubsystem::SendResultRequest(const FString& Verb, const FString& Path, const TMap<FString, FString>& Headers, const FString& JsonBody, FJiweResultCallback OnComplete)
{
	SendRequest(Verb, UJiweSettings::Get()->GetWalletApiBase() + Path, Headers, JsonBody, JiweWalletDetail::JsonContentType,
		[OnComplete](const FJiweHttpReply& Reply)
		{
			const FJiweResult Result = ToResult(Reply);
			if (!Result.bSuccess)
			{
				UE_LOG(LogJiwe, Warning, TEXT("Jiwe call failed: %s"), *Result.Error);
			}
			if (OnComplete)
			{
				OnComplete(Result);
			}
		});
}

// =========================================================================================
// Player calls
// =========================================================================================

void UJiweSubsystem::GiveXpReward(int32 Xp, const FString& Description, FJiweResultCallback OnComplete, const FString& TransactionId)
{
	if (!IsLoggedIn())
	{
		if (OnComplete) { OnComplete(NotLoggedInResult()); }
		return;
	}
	if (Xp <= 0)
	{
		// Caught here because it is a common mistake: the score is read after game-over code reset it to 0.
		FJiweResult Result;
		Result.Error = FString::Printf(TEXT("XP must be greater than 0 (got %d). Read the score before your game-over code resets it."), Xp);
		UE_LOG(LogJiwe, Warning, TEXT("%s"), *Result.Error);
		if (OnComplete) { OnComplete(Result); }
		return;
	}

	FString Json = FString::Printf(TEXT("{\"xpEarned\":%d"), Xp);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("description"), Description);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("gameSessionId"), GameSessionId);
	if (!TransactionId.IsEmpty())
	{
		Json += TEXT(",") + JiweText::JsonPair(TEXT("transactionId"), TransactionId);
	}
	Json += TEXT(",\"metadata\":{}}");

	SendResultRequest(TEXT("POST"), TEXT("/rewards/xp"), PlayerHeaders(), Json, MoveTemp(OnComplete));
}

void UJiweSubsystem::GiveAirtimeReward(int32 Amount, const FString& RecipientNumber, const FString& Description, FJiweResultCallback OnComplete, const FString& TransactionId)
{
	if (!IsLoggedIn())
	{
		if (OnComplete) { OnComplete(NotLoggedInResult()); }
		return;
	}

	// Jiwe wants a reference on every airtime reward. Make a short one if the game did not supply it.
	const FString Reference = TransactionId.IsEmpty()
		? FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToLower()
		: TransactionId;

	FString Json = FString::Printf(TEXT("{\"airtimeReward\":%d"), Amount);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("description"), Description);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("recipientNumber"), RecipientNumber);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("gameSessionId"), GameSessionId);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("transactionId"), Reference);
	Json += TEXT(",\"metadata\":{}}");

	SendResultRequest(TEXT("POST"), TEXT("/rewards/airtime"), PlayerHeaders(), Json, MoveTemp(OnComplete));
}

void UJiweSubsystem::PurchaseWithWallet(int32 Amount, const FString& ItemId, const FString& Description, const FString& IdempotencyKey, FJiwePurchaseCallback OnComplete)
{
	if (IdempotencyKey.IsEmpty() || IdempotencyKey.Len() > 200)
	{
		FJiwePurchaseResult Result;
		Result.Error = TEXT("An idempotency key is required (1 to 200 characters). Create one per purchase attempt, save it, and reuse the same key if you retry.");
		if (OnComplete) { OnComplete(Result); }
		return;
	}
	if (!IsLoggedIn())
	{
		FJiwePurchaseResult Result;
		Result.Error = JiweWalletDetail::NotLoggedInMessage;
		if (OnComplete) { OnComplete(Result); }
		return;
	}

	FString Json = FString::Printf(TEXT("{\"amount\":%d"), Amount);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("itemId"), ItemId);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("description"), Description);
	Json += TEXT(",\"metadata\":{}}");

	TMap<FString, FString> Headers = PlayerHeaders();
	Headers.Add(TEXT("Idempotency-Key"), IdempotencyKey);

	SendRequest(TEXT("POST"), UJiweSettings::Get()->GetWalletApiBase() + TEXT("/purchases/wallet"), Headers, Json, JiweWalletDetail::JsonContentType,
		[OnComplete](const FJiweHttpReply& Reply)
		{
			if (OnComplete) { OnComplete(ToPurchaseResult(Reply)); }
		});
}

void UJiweSubsystem::GetPurchaseStatus(const FString& PaymentOrderId, FJiwePurchaseCallback OnComplete)
{
	TMap<FString, FString> Headers = AppHeaders();
	if (IsLoggedIn())
	{
		Headers.Add(TEXT("X-API-TOKEN"), GetPlayerToken());
	}

	const FString Url = UJiweSettings::Get()->GetWalletApiBase() + TEXT("/purchases/wallet/") + JiweText::UrlEncode(PaymentOrderId);
	SendRequest(TEXT("GET"), Url, Headers, FString(), FString(),
		[OnComplete](const FJiweHttpReply& Reply)
		{
			if (OnComplete) { OnComplete(ToPurchaseResult(Reply)); }
		});
}

void UJiweSubsystem::GetRewardsEarned(FJiweRewardsEarnedCallback OnComplete)
{
	SendRewardsEarned(false, MoveTemp(OnComplete));
}

void UJiweSubsystem::GetRewardsEarnedBearer(FJiweRewardsEarnedCallback OnComplete)
{
	SendRewardsEarned(true, MoveTemp(OnComplete));
}

void UJiweSubsystem::SendRewardsEarned(bool bBearer, FJiweRewardsEarnedCallback OnComplete)
{
	if (!IsLoggedIn() || (bBearer && AccessToken.IsEmpty()))
	{
		FJiweRewardsEarned Earned;
		Earned.Error = JiweWalletDetail::NotLoggedInMessage;
		if (OnComplete) { OnComplete(Earned); }
		return;
	}

	TMap<FString, FString> Headers;
	FString Path;
	if (bBearer)
	{
		// This route refuses a request that also carries the API-key headers.
		Headers.Add(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
		Path = TEXT("/oidc/rewards/earned");
	}
	else
	{
		Headers = PlayerHeaders();
		Path = TEXT("/rewards/earned");
	}

	SendRequest(TEXT("GET"), UJiweSettings::Get()->GetWalletApiBase() + Path, Headers, FString(), FString(),
		[OnComplete](const FJiweHttpReply& Reply)
		{
			FJiweRewardsEarned Earned;
			Earned.RawResponse = Reply.Body;
			Earned.HttpStatus = Reply.Status;

			const TSharedPtr<FJsonObject> Parsed = JiweJson::ParseObject(Reply.Body);
			if (!Reply.IsHttpOk())
			{
				const FString Message = JiweJson::GetString(Parsed, TEXT("message"));
				Earned.Error = JiweWalletDetail::WithHint(Reply.Status, Message.IsEmpty() ? DescribeHttpFailure(Reply) : Message);
			}
			else if (!Parsed.IsValid())
			{
				Earned.Error = TEXT("Unexpected reply from Jiwe.");
			}
			else
			{
				Earned.bSuccess = true;
				Earned.Points = static_cast<float>(JiweJson::GetNumber(Parsed, TEXT("points")));
				Earned.XP = static_cast<float>(JiweJson::GetNumber(Parsed, TEXT("XP")));
				Earned.AirtimeEarned = static_cast<float>(JiweJson::GetNumber(Parsed, TEXT("airtimeEarned")));
				Earned.AirtimeCurrency = JiweJson::GetString(Parsed, TEXT("airtimeCurrency"));
				if (Earned.AirtimeCurrency.IsEmpty())
				{
					Earned.AirtimeCurrency = TEXT("KES");
				}
			}
			if (OnComplete) { OnComplete(Earned); }
		});
}

void UJiweSubsystem::GetUserData(FJiweResultCallback OnComplete)
{
	if (!IsLoggedIn())
	{
		if (OnComplete) { OnComplete(NotLoggedInResult()); }
		return;
	}
	SendResultRequest(TEXT("GET"), TEXT("/metadata/user-data"), PlayerHeaders(), FString(), MoveTemp(OnComplete));
}

void UJiweSubsystem::SetUserData(const FString& JsonPayload, FJiweResultCallback OnComplete)
{
	if (!IsLoggedIn())
	{
		if (OnComplete) { OnComplete(NotLoggedInResult()); }
		return;
	}
	SendResultRequest(TEXT("POST"), TEXT("/metadata/update/user-data"), PlayerHeaders(), JsonPayload, MoveTemp(OnComplete));
}

// =========================================================================================
// App calls
// =========================================================================================

void UJiweSubsystem::GetLeaderboard(const FString& RewardType, int32 MaxEntries, const FString& Period, FJiweLeaderboardCallback OnComplete, const FString& BestPointsRanking)
{
	FString Json = TEXT("{") + JiweText::JsonPair(TEXT("rewardType"), RewardType.IsEmpty() ? FString(TEXT("xp")) : RewardType);
	Json += FString::Printf(TEXT(",\"maxEntries\":%d"), MaxEntries);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("leaderboardPeriod"), Period);
	Json += TEXT(",") + JiweText::JsonPair(TEXT("bestPointsRanking"), BestPointsRanking.IsEmpty() ? FString(TEXT("highest")) : BestPointsRanking);
	Json += TEXT("}");

	SendRequest(TEXT("POST"), UJiweSettings::Get()->GetWalletApiBase() + TEXT("/leaderboard"), AppHeaders(), Json, JiweWalletDetail::JsonContentType,
		[OnComplete](const FJiweHttpReply& Reply)
		{
			FJiweLeaderboard Board;
			const TSharedPtr<FJsonObject> Parsed = JiweJson::ParseObject(Reply.Body);
			const bool bErrorType = JiweJson::GetString(Parsed, TEXT("type")).Equals(TEXT("ERROR"), ESearchCase::CaseSensitive);

			if (!Reply.IsHttpOk() || bErrorType)
			{
				const FString Message = JiweJson::GetString(Parsed, TEXT("message"));
				Board.Error = JiweWalletDetail::WithHint(Reply.Status, Message.IsEmpty() ? DescribeHttpFailure(Reply) : Message);
			}
			else if (!JiweJson::ParseLeaderboard(Reply.Body, Board.Entries))
			{
				Board.Error = TEXT("Unexpected reply from Jiwe.");
			}
			else
			{
				Board.bSuccess = true;
			}

			if (!Board.bSuccess)
			{
				UE_LOG(LogJiwe, Warning, TEXT("Leaderboard request failed: %s"), *Board.Error);
			}
			if (OnComplete) { OnComplete(Board); }
		});
}

void UJiweSubsystem::GetWalletBalance(FJiweBalanceCallback OnComplete)
{
	SendRequest(TEXT("GET"), UJiweSettings::Get()->GetWalletApiBase() + TEXT("/clients/account-balance"), AppHeaders(), FString(), FString(),
		[OnComplete](const FJiweHttpReply& Reply)
		{
			FJiweBalance Balance;
			const TSharedPtr<FJsonObject> Parsed = JiweJson::ParseObject(Reply.Body);
			if (!Reply.IsHttpOk() || !Parsed.IsValid())
			{
				const FString Message = JiweJson::GetString(Parsed, TEXT("message"));
				Balance.Error = JiweWalletDetail::WithHint(Reply.Status, Message.IsEmpty() ? DescribeHttpFailure(Reply) : Message);
			}
			else
			{
				Balance.bSuccess = true;
				Balance.DebitsAmount = JiweJson::GetNumber(Parsed, TEXT("debits"));
				Balance.CreditsAmount = JiweJson::GetNumber(Parsed, TEXT("credits"));
				Balance.AvailableAmount = JiweJson::GetNumber(Parsed, TEXT("available"));
				// Whole numbers are rounded down so an "is there enough?" check never rounds up.
				Balance.Debits = static_cast<int32>(FMath::FloorToDouble(Balance.DebitsAmount));
				Balance.Credits = static_cast<int32>(FMath::FloorToDouble(Balance.CreditsAmount));
				Balance.Available = static_cast<int32>(FMath::FloorToDouble(Balance.AvailableAmount));
			}
			if (OnComplete) { OnComplete(Balance); }
		});
}

void UJiweSubsystem::GetTransactionStatus(const FString& TransactionId, FJiweResultCallback OnComplete)
{
	const FString Json = TEXT("{") + JiweText::JsonPair(TEXT("transactionId"), TransactionId) + TEXT("}");
	SendResultRequest(TEXT("POST"), TEXT("/transactions/status"), AppHeaders(), Json, MoveTemp(OnComplete));
}

void UJiweSubsystem::CheckCredentialsValid(FJiweBoolCallback OnComplete)
{
	SendRequest(TEXT("GET"), UJiweSettings::Get()->GetWalletApiBase() + TEXT("/clients/account-balance"), AppHeaders(), FString(), FString(),
		[OnComplete](const FJiweHttpReply& Reply)
		{
			// Only a clear rejection counts as invalid. A timeout or a server error must never be
			// mistaken for "this build's keys are dead".
			const bool bValid = Reply.IsHttpOk() || (Reply.Status != 401 && Reply.Status != 403);
			if (OnComplete) { OnComplete(bValid); }
		});
}

void UJiweSubsystem::GetGameData(FJiweResultCallback OnComplete)
{
	SendResultRequest(TEXT("GET"), TEXT("/metadata/game-data"), AppHeaders(), FString(), MoveTemp(OnComplete));
}

void UJiweSubsystem::SetGameData(const FString& JsonPayload, FJiweResultCallback OnComplete)
{
	// The write path is /metadata/update/game-data. Posting to /metadata/game-data does not apply the change.
	SendResultRequest(TEXT("POST"), TEXT("/metadata/update/game-data"), AppHeaders(), JsonPayload, MoveTemp(OnComplete));
}
