#pragma once

#include "CoreMinimal.h"

/** Small text and crypto helpers used by the login and wallet code. Private to the plugin. */
namespace JiweText
{
	/** SHA-256 of the text's UTF-8 bytes. */
	TArray<uint8> Sha256(const FString& Text);

	/** Base64 with the URL-safe alphabet and no padding (RFC 4648 section 5). */
	FString Base64Url(const TArray<uint8>& Bytes);

	/** Decodes URL-safe base64 holding UTF-8 text (a JWT segment). False if it is not valid. */
	bool Base64UrlDecodeToString(const FString& Encoded, FString& OutText);

	/** ByteCount unpredictable bytes, URL-safe base64 encoded. Used for the PKCE verifier and state. */
	FString RandomUrlSafe(int32 ByteCount);

	/** Percent-encodes a value for a URL query or a form body. */
	FString UrlEncode(const FString& Value);

	/** Splits "anything?a=1&b=2" (or "a=1&b=2") into decoded key/value pairs. Keys keep their case. */
	TMap<FString, FString> ParseQuery(const FString& UrlOrQuery);

	/** Escapes a string for use inside JSON double quotes. */
	FString JsonEscape(const FString& Value);

	/** "\"key\":\"escaped value\"" */
	FString JsonPair(const TCHAR* Key, const FString& Value);

	/** Shortens long server messages (an HTML error page, say) so they fit in a UI label. */
	FString Truncate(const FString& Message, int32 MaxLength = 300);
}
