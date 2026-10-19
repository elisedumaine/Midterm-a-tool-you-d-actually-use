#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Components/Slider.h"
#include "Widgets/Views/STableViewBase.h"
#include "InteractiveWorldBuilderWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class USlider;
class UTextBlock;

/**
 * Native content for the EUW_InteractiveWorldBuilder Editor Utility Widget.
 * The asset makes this a dockable UMG Editor Utility Widget while this class
 * keeps all regeneration, preset, and placement logic in one testable place.
 */
UCLASS()
class INTERACTIVEWORLDBUILDEREDITOR_API UInteractiveWorldBuilderWidget final : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

private:
    UFUNCTION() void GenerateWorld();
    UFUNCTION() void PreviewWorld();
    UFUNCTION() void SavePreset();
    UFUNCTION() void LoadPreset();
    UFUNCTION() void ResetControls();
    UFUNCTION() void DeleteWorld();
    UFUNCTION() void OnThemeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnTerrainChanged(float Value);
    UFUNCTION() void OnTreesChanged(float Value);
    UFUNCTION() void OnRocksChanged(float Value);
    UFUNCTION() void OnGrassChanged(float Value);
    UFUNCTION() void OnLightingChanged(float Value);
    UFUNCTION() void OnSkyTintChanged(float Value);
    UFUNCTION() void OnGroundTintChanged(float Value);
    UFUNCTION() void OnLightTintChanged(float Value);
    UFUNCTION() void OnPlayableCharacterChanged(bool bIsChecked);

    void BuildWorld(bool bPreviewOnly);
    void ClearGeneratedActors(bool bPreviewOnly, bool bClearAll);
    void SetStatus(const FString& Message);
    void UpdateReadouts();
    void ApplyControlsToWidgets();
    void AddSliderRow(class UVerticalBox* Container, const FText& Label, USlider*& Slider, UTextBlock*& ValueText, float Initial, float Min, float Max);
    class UHorizontalBox* AddButtonRow(class UVerticalBox* Container);

    FString CurrentTheme = TEXT("Forest");
    float TerrainSize = 4000.f;
    float TreeCount = 45.f;
    float RockCount = 24.f;
    float GrassDensity = 120.f;
    float LightingIntensity = 8.f;
    float SkyTint = 0.55f;
    float GroundTint = 0.32f;
    float LightTint = 0.12f;
    bool bAddPlayableCharacter = true;

    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> WorldNameBox;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> SeedBox;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> ThemeBox;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> PlayableCharacterBox;
    UPROPERTY(Transient) TObjectPtr<USlider> TerrainSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> TreesSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> RocksSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> GrassSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> LightingSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> SkyTintSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> GroundTintSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> LightTintSlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TerrainReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TreesReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RocksReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GrassReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LightingReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SkyTintReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GroundTintReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LightTintReadout;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
};
