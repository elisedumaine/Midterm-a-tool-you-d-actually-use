#include "InteractiveWorldBuilderEditor.h"

#include "InteractiveWorldBuilderWidget.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidgetBlueprintFactory.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "InteractiveWorldBuilderEditor"

void FInteractiveWorldBuilderEditorModule::StartupModule()
{
    // The widget asset is deliberately native-backed: it remains a normal,
    // editable Editor Utility Widget while the placement logic is versioned C++.
    if (!IsRunningCommandlet())
    {
        CreateWidgetAsset();
    }
}

void FInteractiveWorldBuilderEditorModule::ShutdownModule()
{
}

void FInteractiveWorldBuilderEditorModule::CreateWidgetAsset()
{
    constexpr TCHAR AssetPath[] = TEXT("/Game/WorldBuilder/EUW_InteractiveWorldBuilder");
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UEditorUtilityWidgetBlueprintFactory* Factory = NewObject<UEditorUtilityWidgetBlueprintFactory>();
    Factory->ParentClass = UInteractiveWorldBuilderWidget::StaticClass();

    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
        TEXT("EUW_InteractiveWorldBuilder"),
        TEXT("/Game/WorldBuilder"),
        UEditorUtilityWidgetBlueprint::StaticClass(),
        Factory);

    UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(NewAsset);
    if (WidgetBlueprint == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Interactive World Builder could not create its Editor Utility Widget asset."));
        return;
    }

    UEditorAssetLibrary::SaveLoadedAsset(WidgetBlueprint, false);

    if (GEditor != nullptr)
    {
        if (UEditorUtilitySubsystem* UtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
        {
            UtilitySubsystem->SpawnAndRegisterTab(WidgetBlueprint);
        }
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInteractiveWorldBuilderEditorModule, InteractiveWorldBuilderEditor)
