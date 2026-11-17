#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JiweTypes.h"

/** JSON reading helpers. Private to the plugin. */
namespace JiweJson
{
	/** Parses a JSON object. Returns an invalid pointer for empty text, non-JSON (an HTML error page) or a non-object. */
	TSharedPtr<FJsonObject> ParseObject(const FString& Text);

	/** Missing or wrong-typed fields come back as the default. A null Object is fine. */
	FString GetString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field);
	double GetNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Default = 0.0);
	bool GetBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool bDefault = false);

	/**
	 * Reads the "leaderboard" array out of a leaderboard reply.
	 *
	 * Done with the streaming reader, not FJsonObject, because Jiwe uses "XP", "currentXP" and "xp"
	 * as three different fields and FJsonObject treats keys that differ only by case as the same key.
	 * False if the text is not valid JSON.
	 */
	bool ParseLeaderboard(const FString& Text, TArray<FJiweLeaderboardEntry>& OutEntries);
}
