#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Everything the plugin logs goes to this category: filter the Output Log by "LogJiwe". */
JIWESDK_API DECLARE_LOG_CATEGORY_EXTERN(LogJiwe, Log, All);

class FJiweSDKModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
