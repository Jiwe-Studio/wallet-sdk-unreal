#include "JiweRedirectInbox.h"

#include <mutex>
#include <string>
#include <vector>

// Plain C++ containers on purpose: on Android a link can arrive from Java before Unreal's own
// memory and threading systems are up, so nothing here may depend on them.
namespace JiweInboxDetail
{
	std::mutex& Lock()
	{
		static std::mutex Instance;
		return Instance;
	}

	std::vector<std::string>& Items()
	{
		static std::vector<std::string> Instance;
		return Instance;
	}
}

void FJiweRedirectInbox::PushUtf8(const char* Utf8Url)
{
	if (Utf8Url == nullptr || *Utf8Url == 0)
	{
		return;
	}
	std::lock_guard<std::mutex> Guard(JiweInboxDetail::Lock());
	JiweInboxDetail::Items().emplace_back(Utf8Url);
}

void FJiweRedirectInbox::Push(const FString& Url)
{
	const FTCHARToUTF8 Utf8(*Url);
	PushUtf8(reinterpret_cast<const char*>(Utf8.Get()));
}

bool FJiweRedirectInbox::Pop(FString& OutUrl)
{
	std::string Next;
	{
		std::lock_guard<std::mutex> Guard(JiweInboxDetail::Lock());
		std::vector<std::string>& Items = JiweInboxDetail::Items();
		if (Items.empty())
		{
			return false;
		}
		Next = Items.front();
		Items.erase(Items.begin());
	}
	OutUrl = FString(UTF8_TO_TCHAR(Next.c_str()));
	return true;
}

void FJiweRedirectInbox::Clear()
{
	std::lock_guard<std::mutex> Guard(JiweInboxDetail::Lock());
	JiweInboxDetail::Items().clear();
}

#if PLATFORM_ANDROID
#include <jni.h>

// Called from GameActivity.onResume (see JiweSDK_UPL_Android.xml) with the link that opened the game.
extern "C" JNIEXPORT void JNICALL Java_com_epicgames_unreal_GameActivity_nativeJiweHandleDeepLink(JNIEnv* Env, jobject /*Activity*/, jstring Url)
{
	if (Env == nullptr || Url == nullptr)
	{
		return;
	}
	const char* Utf8Url = Env->GetStringUTFChars(Url, nullptr);
	if (Utf8Url != nullptr)
	{
		FJiweRedirectInbox::PushUtf8(Utf8Url);
		Env->ReleaseStringUTFChars(Url, Utf8Url);
	}
}
#endif
