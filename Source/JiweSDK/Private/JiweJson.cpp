#include "JiweJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonObject> JiweJson::ParseObject(const FString& Text)
{
	TSharedPtr<FJsonObject> Object;
	if (Text.IsEmpty())
	{
		return Object;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Object))
	{
		Object.Reset();
	}
	return Object;
}

FString JiweJson::GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	FString Value;
	if (Object.IsValid())
	{
		Object->TryGetStringField(Field, Value);
	}
	return Value;
}

double JiweJson::GetNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Default)
{
	double Value = Default;
	if (Object.IsValid() && !Object->TryGetNumberField(Field, Value))
	{
		Value = Default;
	}
	return Value;
}

bool JiweJson::GetBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool bDefault)
{
	bool bValue = bDefault;
	if (Object.IsValid() && !Object->TryGetBoolField(Field, bValue))
	{
		bValue = bDefault;
	}
	return bValue;
}

bool JiweJson::ParseLeaderboard(const FString& Text, TArray<FJiweLeaderboardEntry>& OutEntries)
{
	OutEntries.Reset();

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);

	// Nesting depth: the reply object is 1, its "leaderboard" array is 2, each entry object is 3.
	int32 Depth = 0;
	bool bInBoard = false;
	bool bInEntry = false;
	const int32 BoardDepth = 2;
	const int32 EntryDepth = 3;

	FJiweLeaderboardEntry Entry;
	int32 UpperXp = 0;   // "XP"        period boards
	int32 CurrentXp = 0; // "currentXP" all-time total on some entries
	int32 LowerXp = 0;   // "xp"

	EJsonNotation Notation;
	while (Reader->ReadNext(Notation))
	{
		const FString Key(Reader->GetIdentifier());

		switch (Notation)
		{
		case EJsonNotation::ObjectStart:
			++Depth;
			if (bInBoard && Depth == EntryDepth)
			{
				bInEntry = true;
				Entry = FJiweLeaderboardEntry();
				UpperXp = 0;
				CurrentXp = 0;
				LowerXp = 0;
			}
			break;

		case EJsonNotation::ObjectEnd:
			if (bInEntry && Depth == EntryDepth)
			{
				Entry.Score = UpperXp != 0 ? UpperXp : (CurrentXp != 0 ? CurrentXp : LowerXp);
				OutEntries.Add(Entry);
				bInEntry = false;
			}
			--Depth;
			break;

		case EJsonNotation::ArrayStart:
			++Depth;
			if (!bInBoard && Depth == BoardDepth && Key.Equals(TEXT("leaderboard"), ESearchCase::CaseSensitive))
			{
				bInBoard = true;
			}
			break;

		case EJsonNotation::ArrayEnd:
			if (bInBoard && Depth == BoardDepth)
			{
				bInBoard = false;
			}
			--Depth;
			break;

		case EJsonNotation::String:
			if (bInEntry && Depth == EntryDepth)
			{
				const FString Value(Reader->GetValueAsString());
				if (Key.Equals(TEXT("id"), ESearchCase::CaseSensitive))
				{
					Entry.Id = Value;
				}
				else if (Key.Equals(TEXT("name"), ESearchCase::CaseSensitive))
				{
					Entry.Name = Value;
				}
				else if (Key.Equals(TEXT("username"), ESearchCase::CaseSensitive))
				{
					Entry.Username = Value;
				}
			}
			break;

		case EJsonNotation::Number:
			if (bInEntry && Depth == EntryDepth)
			{
				const int32 Value = static_cast<int32>(Reader->GetValueAsNumber());
				if (Key.Equals(TEXT("rank"), ESearchCase::CaseSensitive))
				{
					Entry.Rank = Value;
				}
				else if (Key.Equals(TEXT("XP"), ESearchCase::CaseSensitive))
				{
					UpperXp = Value;
				}
				else if (Key.Equals(TEXT("currentXP"), ESearchCase::CaseSensitive))
				{
					CurrentXp = Value;
				}
				else if (Key.Equals(TEXT("xp"), ESearchCase::CaseSensitive))
				{
					LowerXp = Value;
				}
			}
			break;

		case EJsonNotation::Error:
			return false;

		default:
			break;
		}
	}

	if (!Reader->GetErrorMessage().IsEmpty())
	{
		return false;
	}

	// Jiwe does not promise the rows arrive in order. When every row has a rank, sort by it.
	bool bEveryRowRanked = OutEntries.Num() > 0;
	for (const FJiweLeaderboardEntry& Row : OutEntries)
	{
		if (Row.Rank <= 0)
		{
			bEveryRowRanked = false;
			break;
		}
	}
	if (bEveryRowRanked)
	{
		OutEntries.Sort([](const FJiweLeaderboardEntry& A, const FJiweLeaderboardEntry& B)
		{
			return A.Rank < B.Rank;
		});
	}
	return true;
}
