#include "JiweBlueprintNodes.h"
#include "JiweSubsystem.h"

namespace JiweNodeDetail
{
	const TCHAR* NoSubsystemMessage = TEXT("The Jiwe subsystem is not available here. Is the Jiwe SDK plugin enabled, and is the game running?");

	/** Creates a node object and ties its lifetime to the running game. */
	template <typename NodeType>
	NodeType* NewNode(UObject* WorldContextObject)
	{
		NodeType* Node = NewObject<NodeType>();
		Node->RegisterWithGameInstance(WorldContextObject);
		return Node;
	}
}

// ------------------------------------------------------------------------------- base

UJiweSubsystem* UJiweNodeBase::FindJiwe() const
{
	return UJiweSubsystem::Get(WorldContext);
}

// ------------------------------------------------------------------------------- login

UJiweLoginNode* UJiweLoginNode::JiweLogin(UObject* WorldContextObject)
{
	UJiweLoginNode* Node = JiweNodeDetail::NewNode<UJiweLoginNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	return Node;
}

void UJiweLoginNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr)
	{
		OnFailure.Broadcast(FJiweUserInfo(), JiweNodeDetail::NoSubsystemMessage);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiweLoginNode> WeakNode(this);
	const TWeakObjectPtr<UJiweSubsystem> WeakJiwe(Jiwe);
	Jiwe->LoginWithCallback([WeakNode, WeakJiwe](bool bSuccess, const FString& Error)
	{
		UJiweLoginNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		const FJiweUserInfo User = WeakJiwe.IsValid() ? WeakJiwe->GetUserInfo() : FJiweUserInfo();
		if (bSuccess)
		{
			Node->OnSuccess.Broadcast(User, FString());
		}
		else
		{
			Node->OnFailure.Broadcast(FJiweUserInfo(), Error);
		}
		Node->SetReadyToDestroy();
	});
}

// ------------------------------------------------------------------------------- calls that answer with FJiweResult

UJiweCallNode* UJiweCallNode::Make(UObject* WorldContextObject, FJiweStartCall InStartCall)
{
	UJiweCallNode* Node = JiweNodeDetail::NewNode<UJiweCallNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	Node->StartCall = MoveTemp(InStartCall);
	return Node;
}

UJiweCallNode* UJiweCallNode::JiweGiveXp(UObject* WorldContextObject, int32 Xp, const FString& Description)
{
	return Make(WorldContextObject, [Xp, Description](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.GiveXpReward(Xp, Description, MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweGiveAirtime(UObject* WorldContextObject, int32 Amount, const FString& RecipientNumber, const FString& Description)
{
	return Make(WorldContextObject, [Amount, RecipientNumber, Description](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.GiveAirtimeReward(Amount, RecipientNumber, Description, MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweGetTransactionStatus(UObject* WorldContextObject, const FString& TransactionId)
{
	return Make(WorldContextObject, [TransactionId](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.GetTransactionStatus(TransactionId, MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweGetUserData(UObject* WorldContextObject)
{
	return Make(WorldContextObject, [](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.GetUserData(MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweSetUserData(UObject* WorldContextObject, const FString& JsonPayload)
{
	return Make(WorldContextObject, [JsonPayload](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.SetUserData(JsonPayload, MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweGetGameData(UObject* WorldContextObject)
{
	return Make(WorldContextObject, [](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.GetGameData(MoveTemp(Done));
	});
}

UJiweCallNode* UJiweCallNode::JiweSetGameData(UObject* WorldContextObject, const FString& JsonPayload)
{
	return Make(WorldContextObject, [JsonPayload](UJiweSubsystem& Jiwe, FJiweResultCallback Done)
	{
		Jiwe.SetGameData(JsonPayload, MoveTemp(Done));
	});
}

void UJiweCallNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr || !StartCall)
	{
		FJiweResult Failure;
		Failure.Error = JiweNodeDetail::NoSubsystemMessage;
		OnFailure.Broadcast(Failure);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiweCallNode> WeakNode(this);
	StartCall(*Jiwe, [WeakNode](const FJiweResult& Result)
	{
		UJiweCallNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		if (Result.bSuccess)
		{
			Node->OnSuccess.Broadcast(Result);
		}
		else
		{
			Node->OnFailure.Broadcast(Result);
		}
		Node->SetReadyToDestroy();
	});
}

// ------------------------------------------------------------------------------- leaderboard

UJiweLeaderboardNode* UJiweLeaderboardNode::JiweGetLeaderboard(UObject* WorldContextObject, const FString& RewardType, int32 MaxEntries, const FString& Period)
{
	UJiweLeaderboardNode* Node = JiweNodeDetail::NewNode<UJiweLeaderboardNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	Node->SavedRewardType = RewardType;
	Node->SavedMaxEntries = MaxEntries;
	Node->SavedPeriod = Period;
	return Node;
}

void UJiweLeaderboardNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr)
	{
		OnFailure.Broadcast(TArray<FJiweLeaderboardEntry>(), JiweNodeDetail::NoSubsystemMessage);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiweLeaderboardNode> WeakNode(this);
	Jiwe->GetLeaderboard(SavedRewardType, SavedMaxEntries, SavedPeriod, [WeakNode](const FJiweLeaderboard& Board)
	{
		UJiweLeaderboardNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		if (Board.bSuccess)
		{
			Node->OnSuccess.Broadcast(Board.Entries, FString());
		}
		else
		{
			Node->OnFailure.Broadcast(TArray<FJiweLeaderboardEntry>(), Board.Error);
		}
		Node->SetReadyToDestroy();
	});
}

// ------------------------------------------------------------------------------- balance

UJiweBalanceNode* UJiweBalanceNode::JiweGetWalletBalance(UObject* WorldContextObject)
{
	UJiweBalanceNode* Node = JiweNodeDetail::NewNode<UJiweBalanceNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	return Node;
}

void UJiweBalanceNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr)
	{
		FJiweBalance Failure;
		Failure.Error = JiweNodeDetail::NoSubsystemMessage;
		OnFailure.Broadcast(Failure);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiweBalanceNode> WeakNode(this);
	Jiwe->GetWalletBalance([WeakNode](const FJiweBalance& Balance)
	{
		UJiweBalanceNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		if (Balance.bSuccess)
		{
			Node->OnSuccess.Broadcast(Balance);
		}
		else
		{
			Node->OnFailure.Broadcast(Balance);
		}
		Node->SetReadyToDestroy();
	});
}

// ------------------------------------------------------------------------------- rewards earned

UJiweRewardsEarnedNode* UJiweRewardsEarnedNode::JiweGetRewardsEarned(UObject* WorldContextObject)
{
	UJiweRewardsEarnedNode* Node = JiweNodeDetail::NewNode<UJiweRewardsEarnedNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	return Node;
}

void UJiweRewardsEarnedNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr)
	{
		FJiweRewardsEarned Failure;
		Failure.Error = JiweNodeDetail::NoSubsystemMessage;
		OnFailure.Broadcast(Failure);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiweRewardsEarnedNode> WeakNode(this);
	Jiwe->GetRewardsEarned([WeakNode](const FJiweRewardsEarned& Earned)
	{
		UJiweRewardsEarnedNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		if (Earned.bSuccess)
		{
			Node->OnSuccess.Broadcast(Earned);
		}
		else
		{
			Node->OnFailure.Broadcast(Earned);
		}
		Node->SetReadyToDestroy();
	});
}

// ------------------------------------------------------------------------------- purchases

UJiwePurchaseNode* UJiwePurchaseNode::JiwePurchaseWithWallet(UObject* WorldContextObject, int32 Amount, const FString& ItemId, const FString& Description, const FString& IdempotencyKey)
{
	UJiwePurchaseNode* Node = JiweNodeDetail::NewNode<UJiwePurchaseNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	Node->bStatusCheckOnly = false;
	Node->SavedAmount = Amount;
	Node->SavedItemId = ItemId;
	Node->SavedDescription = Description;
	Node->SavedIdempotencyKey = IdempotencyKey;
	return Node;
}

UJiwePurchaseNode* UJiwePurchaseNode::JiweGetPurchaseStatus(UObject* WorldContextObject, const FString& PaymentOrderId)
{
	UJiwePurchaseNode* Node = JiweNodeDetail::NewNode<UJiwePurchaseNode>(WorldContextObject);
	Node->WorldContext = WorldContextObject;
	Node->bStatusCheckOnly = true;
	Node->SavedPaymentOrderId = PaymentOrderId;
	return Node;
}

void UJiwePurchaseNode::Activate()
{
	UJiweSubsystem* Jiwe = FindJiwe();
	if (Jiwe == nullptr)
	{
		FJiwePurchaseResult Failure;
		Failure.Error = JiweNodeDetail::NoSubsystemMessage;
		OnFailure.Broadcast(Failure);
		SetReadyToDestroy();
		return;
	}

	const TWeakObjectPtr<UJiwePurchaseNode> WeakNode(this);
	FJiwePurchaseCallback Done = [WeakNode](const FJiwePurchaseResult& Result)
	{
		UJiwePurchaseNode* Node = WeakNode.Get();
		if (Node == nullptr)
		{
			return;
		}
		if (Result.bSuccess)
		{
			Node->OnSuccess.Broadcast(Result);
		}
		else
		{
			Node->OnFailure.Broadcast(Result);
		}
		Node->SetReadyToDestroy();
	};

	if (bStatusCheckOnly)
	{
		Jiwe->GetPurchaseStatus(SavedPaymentOrderId, MoveTemp(Done));
	}
	else
	{
		Jiwe->PurchaseWithWallet(SavedAmount, SavedItemId, SavedDescription, SavedIdempotencyKey, MoveTemp(Done));
	}
}
