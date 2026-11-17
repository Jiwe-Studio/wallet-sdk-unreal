#include "JiweSDKModule.h"

DEFINE_LOG_CATEGORY(LogJiwe);

void FJiweSDKModule::StartupModule()
{
	UE_LOG(LogJiwe, Log, TEXT("Jiwe SDK loaded. Type Jiwe.Status in the console to check your setup."));
}

void FJiweSDKModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FJiweSDKModule, JiweSDK)
