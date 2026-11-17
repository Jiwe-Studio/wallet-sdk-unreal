// Console commands for trying the plugin without writing any code.
// Press Play, open the console (the ` key, left of 1) and type one of:
//
//   Jiwe.Status                  check your setup and see the Redirect URI to register
//   Jiwe.Login                   open the Jiwe login page
//   Jiwe.Cancel                  give up on a login in progress
//   Jiwe.Logout                  forget the player
//   Jiwe.GiveXp 10               credit 10 XP to the logged-in player
//   Jiwe.Leaderboard             all-time XP board      (Jiwe.Leaderboard xp week  for a weekly one)
//   Jiwe.Balance                 your app's reward pool
//   Jiwe.CheckKeys               are the Wallet keys accepted?
//
// Results appear on screen and in the Output Log (filter by LogJiwe).

#include "JiweSubsystem.h"
#include "JiweSDKModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace JiweConsoleDetail
{
	void Say(const FString& Message, bool bGood = true)
	{
		UE_LOG(LogJiwe, Display, TEXT("%s"), *Message);
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, bGood ? FColor::Green : FColor::Orange, Message);
		}
	}

	UJiweSubsystem* FindJiwe(UWorld* World)
	{
		UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		UJiweSubsystem* Jiwe = GameInstance != nullptr ? GameInstance->GetSubsystem<UJiweSubsystem>() : nullptr;
		if (Jiwe == nullptr)
		{
			Say(TEXT("Jiwe: press Play first. These commands need a running game."), false);
		}
		return Jiwe;
	}

	void Status(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			// One log line per report line so it stays readable in the Output Log.
			TArray<FString> Lines;
			Jiwe->GetSetupReport().ParseIntoArrayLines(Lines, false);
			for (const FString& ReportLine : Lines)
			{
				UE_LOG(LogJiwe, Display, TEXT("%s"), *ReportLine);
			}
			if (GEngine != nullptr)
			{
				GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Cyan, Jiwe->GetSetupReport());
			}
		}
	}

	void Login(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			Say(FString::Printf(TEXT("Jiwe: opening the login page. Redirect URI: %s"), *Jiwe->GetRedirectUri()));
			const TWeakObjectPtr<UJiweSubsystem> WeakJiwe(Jiwe);
			Jiwe->LoginWithCallback([WeakJiwe](bool bSuccess, const FString& Error)
			{
				if (bSuccess && WeakJiwe.IsValid())
				{
					const FJiweUserInfo User = WeakJiwe->GetUserInfo();
					Say(FString::Printf(TEXT("Jiwe: logged in. name=%s nickname=%s id=%s"), *User.Name, *User.Nickname, *User.Sub));
				}
				else
				{
					Say(FString::Printf(TEXT("Jiwe: login failed. %s"), *Error), false);
				}
			});
		}
	}

	void Cancel(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			Jiwe->CancelLogin();
		}
	}

	void Logout(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			Jiwe->Logout();
			Say(TEXT("Jiwe: logged out."));
		}
	}

	void GiveXp(const TArray<FString>& Args, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			const int32 Xp = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10;
			Jiwe->GiveXpReward(Xp, TEXT("Jiwe.GiveXp console test"), [Xp](const FJiweResult& Result)
			{
				if (Result.bSuccess)
				{
					Say(FString::Printf(TEXT("Jiwe: %d XP awarded. transaction=%s"), Xp, *Result.LedgerTransactionId));
				}
				else
				{
					Say(FString::Printf(TEXT("Jiwe: XP not awarded. %s"), *Result.Error), false);
				}
			});
		}
	}

	void Leaderboard(const TArray<FString>& Args, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			const FString RewardType = Args.Num() > 0 ? Args[0] : FString(TEXT("xp"));
			const FString Period = Args.Num() > 1 ? Args[1] : FString();
			Jiwe->GetLeaderboard(RewardType, 10, Period, [](const FJiweLeaderboard& Board)
			{
				if (!Board.bSuccess)
				{
					Say(FString::Printf(TEXT("Jiwe: leaderboard failed. %s"), *Board.Error), false);
					return;
				}
				FString Text = FString::Printf(TEXT("Jiwe leaderboard (%d entries)"), Board.Entries.Num());
				for (const FJiweLeaderboardEntry& Entry : Board.Entries)
				{
					const FString Who = !Entry.Name.IsEmpty() ? Entry.Name : Entry.Username;
					Text += FString::Printf(TEXT("\n  #%d  %s  %d"), Entry.Rank, *Who, Entry.Score);
				}
				Say(Text);
			});
		}
	}

	void Balance(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			Jiwe->GetWalletBalance([](const FJiweBalance& Pool)
			{
				if (Pool.bSuccess)
				{
					Say(FString::Printf(TEXT("Jiwe: reward pool available=%.2f (credits=%.2f, debits=%.2f)"), Pool.AvailableAmount, Pool.CreditsAmount, Pool.DebitsAmount));
				}
				else
				{
					Say(FString::Printf(TEXT("Jiwe: balance failed. %s"), *Pool.Error), false);
				}
			});
		}
	}

	void CheckKeys(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (UJiweSubsystem* Jiwe = FindJiwe(World))
		{
			Jiwe->CheckCredentialsValid([](bool bValid)
			{
				if (bValid)
				{
					Say(TEXT("Jiwe: Wallet keys were not rejected."));
				}
				else
				{
					Say(TEXT("Jiwe: Wallet keys were REJECTED. Check API Username and API Key in Project Settings > Plugins > Jiwe SDK."), false);
				}
			});
		}
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("Jiwe.Status"),
		TEXT("Checks your Jiwe SDK setup and shows the Redirect URI to register."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));

	FAutoConsoleCommandWithWorldAndArgs LoginCommand(
		TEXT("Jiwe.Login"),
		TEXT("Opens the Jiwe login page in the browser."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Login));

	FAutoConsoleCommandWithWorldAndArgs CancelCommand(
		TEXT("Jiwe.Cancel"),
		TEXT("Gives up on a Jiwe login that is in progress."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Cancel));

	FAutoConsoleCommandWithWorldAndArgs LogoutCommand(
		TEXT("Jiwe.Logout"),
		TEXT("Forgets the logged-in Jiwe player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Logout));

	FAutoConsoleCommandWithWorldAndArgs GiveXpCommand(
		TEXT("Jiwe.GiveXp"),
		TEXT("Jiwe.GiveXp <amount>  Credits XP to the logged-in player (default 10)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GiveXp));

	FAutoConsoleCommandWithWorldAndArgs LeaderboardCommand(
		TEXT("Jiwe.Leaderboard"),
		TEXT("Jiwe.Leaderboard [xp|cowrie] [day|week|month|year]  Shows the top 10."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Leaderboard));

	FAutoConsoleCommandWithWorldAndArgs BalanceCommand(
		TEXT("Jiwe.Balance"),
		TEXT("Shows your app's reward pool on Jiwe."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Balance));

	FAutoConsoleCommandWithWorldAndArgs CheckKeysCommand(
		TEXT("Jiwe.CheckKeys"),
		TEXT("Checks that the Wallet keys are accepted by Jiwe."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CheckKeys));
}
