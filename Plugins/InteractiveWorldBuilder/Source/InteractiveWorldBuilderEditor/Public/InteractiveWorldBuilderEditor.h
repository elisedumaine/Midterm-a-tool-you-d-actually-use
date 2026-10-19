#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FInteractiveWorldBuilderEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void CreateWidgetAsset();
};
