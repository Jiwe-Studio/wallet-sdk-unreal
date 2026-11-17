#include "JiweText.h"
#include "JiweSha256.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
#include <stdio.h>
#endif

TArray<uint8> JiweText::Sha256(const FString& Text)
{
	const FTCHARToUTF8 Utf8(*Text);
	TArray<uint8> Digest;
	Digest.SetNumZeroed(32);
	JiweSha256(reinterpret_cast<const uint8*>(Utf8.Get()), static_cast<uint64>(Utf8.Length()), Digest.GetData());
	return Digest;
}

FString JiweText::Base64Url(const TArray<uint8>& Bytes)
{
	FString Encoded = FBase64::Encode(Bytes);
	Encoded.ReplaceInline(TEXT("+"), TEXT("-"), ESearchCase::CaseSensitive);
	Encoded.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	while (Encoded.EndsWith(TEXT("=")))
	{
		Encoded.LeftChopInline(1);
	}
	return Encoded;
}

bool JiweText::Base64UrlDecodeToString(const FString& Encoded, FString& OutText)
{
	FString Standard = Encoded;
	Standard.ReplaceInline(TEXT("-"), TEXT("+"), ESearchCase::CaseSensitive);
	Standard.ReplaceInline(TEXT("_"), TEXT("/"), ESearchCase::CaseSensitive);
	while (Standard.Len() % 4 != 0)
	{
		Standard += TEXT("=");
	}

	TArray<uint8> Bytes;
	if (!FBase64::Decode(Standard, Bytes))
	{
		return false;
	}
	Bytes.Add(0); // null terminator for the conversion below
	OutText = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData())));
	return true;
}

FString JiweText::RandomUrlSafe(int32 ByteCount)
{
	// Gather unpredictable input from several places, then stretch it with SHA-256.
	// On Windows and Mac a GUID is already built from the OS random generator; on Linux and
	// Android the OS generator is read directly.
	TArray<uint8> Seed;
	auto Mix = [&Seed](const void* Data, int32 Size)
	{
		Seed.Append(static_cast<const uint8*>(Data), Size);
	};

	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FGuid Guid = FGuid::NewGuid();
		Mix(&Guid, sizeof(Guid));
	}

	const uint64 Cycles = FPlatformTime::Cycles64();
	Mix(&Cycles, sizeof(Cycles));

	const int64 Ticks = FDateTime::UtcNow().GetTicks();
	Mix(&Ticks, sizeof(Ticks));

	const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();
	Mix(&ProcessId, sizeof(ProcessId));

	static uint32 CallCounter = 0;
	++CallCounter;
	Mix(&CallCounter, sizeof(CallCounter));

	const void* StackAddress = &Seed;
	Mix(&StackAddress, sizeof(StackAddress));

#if PLATFORM_ANDROID || PLATFORM_LINUX || PLATFORM_MAC || PLATFORM_IOS
	if (FILE* Urandom = fopen("/dev/urandom", "rb"))
	{
		uint8 OsRandom[32];
		const size_t Got = fread(OsRandom, 1, sizeof(OsRandom), Urandom);
		fclose(Urandom);
		Mix(OsRandom, static_cast<int32>(Got));
	}
#endif

	TArray<uint8> Output;
	Output.Reserve(ByteCount);
	uint32 BlockIndex = 0;
	while (Output.Num() < ByteCount)
	{
		TArray<uint8> Input = Seed;
		Input.Append(reinterpret_cast<const uint8*>(&BlockIndex), sizeof(BlockIndex));
		++BlockIndex;

		uint8 Digest[32];
		JiweSha256(Input.GetData(), static_cast<uint64>(Input.Num()), Digest);
		for (int32 Index = 0; Index < 32 && Output.Num() < ByteCount; ++Index)
		{
			Output.Add(Digest[Index]);
		}
	}
	return Base64Url(Output);
}

FString JiweText::UrlEncode(const FString& Value)
{
	return FGenericPlatformHttp::UrlEncode(Value);
}

TMap<FString, FString> JiweText::ParseQuery(const FString& UrlOrQuery)
{
	TMap<FString, FString> Result;

	FString Query = UrlOrQuery;
	int32 QuestionIndex = INDEX_NONE;
	if (Query.FindChar(TEXT('?'), QuestionIndex))
	{
		Query = Query.Mid(QuestionIndex + 1);
	}
	int32 HashIndex = INDEX_NONE;
	if (Query.FindChar(TEXT('#'), HashIndex))
	{
		Query = Query.Left(HashIndex);
	}

	TArray<FString> Pairs;
	Query.ParseIntoArray(Pairs, TEXT("&"), true);
	for (const FString& Pair : Pairs)
	{
		FString Key;
		FString Value;
		if (Pair.Split(TEXT("="), &Key, &Value, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			Result.Add(FGenericPlatformHttp::UrlDecode(Key), FGenericPlatformHttp::UrlDecode(Value));
		}
	}
	return Result;
}

FString JiweText::JsonEscape(const FString& Value)
{
	FString Out;
	Out.Reserve(Value.Len() + 8);
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		const TCHAR Char = Value[Index];
		switch (Char)
		{
		case TEXT('"'): Out += TEXT("\\\""); break;
		case TEXT('\\'): Out += TEXT("\\\\"); break;
		case TEXT('\n'): Out += TEXT("\\n"); break;
		case TEXT('\r'): Out += TEXT("\\r"); break;
		case TEXT('\t'): Out += TEXT("\\t"); break;
		default:
			if (Char < 0x20)
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Char));
			}
			else
			{
				Out.AppendChar(Char);
			}
			break;
		}
	}
	return Out;
}

FString JiweText::JsonPair(const TCHAR* Key, const FString& Value)
{
	return FString::Printf(TEXT("\"%s\":\"%s\""), Key, *JsonEscape(Value));
}

FString JiweText::Truncate(const FString& Message, int32 MaxLength)
{
	return Message.Len() > MaxLength ? Message.Left(MaxLength) + TEXT("...") : Message;
}
