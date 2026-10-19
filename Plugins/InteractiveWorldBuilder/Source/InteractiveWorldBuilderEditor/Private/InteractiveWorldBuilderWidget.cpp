#include "InteractiveWorldBuilderWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetTree.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "InteractiveWorldBuilderWidget"

namespace IWB
{
    static const FName GeneratedTag(TEXT("IWB_Generated"));
    static const FName PreviewTag(TEXT("IWB_Preview"));
    static const TCHAR PresetSection[] = TEXT("InteractiveWorldBuilderPreset");

    static float ToSliderValue(float Value, float Min, float Max)
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(Min, Max), FVector2D(0.f, 1.f), Value);
    }

    static float FromSliderValue(float Value, float Min, float Max)
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(Min, Max), Value);
    }

    static FLinearColor ForestGround(float Tint)
    {
        return FLinearColor::LerpUsingHSV(FLinearColor(0.035f, 0.12f, 0.025f), FLinearColor(0.20f, 0.48f, 0.075f), Tint);
    }

    static FLinearColor ForestSky(float Tint)
    {
        return FLinearColor::LerpUsingHSV(FLinearColor(0.18f, 0.32f, 0.55f), FLinearColor(0.38f, 0.72f, 1.f), Tint);
    }

    static FString NumberText(float Value)
    {
        return FString::FromInt(FMath::RoundToInt(Value));
    }
}

void UInteractiveWorldBuilderWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldBuilderScroll"));
    WidgetTree->RootWidget = Scroll;
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WorldBuilderContent"));
    Scroll->AddChild(Content);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
    Title->SetText(LOCTEXT("Title", "INTERACTIVE WORLD BUILDER"));
    Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold"), 18));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.82f, 0.50f)));
    Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(10.f, 12.f, 10.f, 2.f));

    UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
    Subtitle->SetText(LOCTEXT("Subtitle", "Seeded forest PCG-ready generator  •  third-person template compatible"));
    Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.76f, 0.72f)));
    Subtitle->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(10.f, 0.f, 10.f, 12.f));

    auto AddField = [this, Content](const FText& Label, UEditableTextBox*& Field, const FText& DefaultValue)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>();
        LabelText->SetText(Label);
        LabelText->SetMinDesiredWidth(105.f);
        Row->AddChildToHorizontalBox(LabelText)->SetPadding(FMargin(10.f, 3.f, 6.f, 3.f));
        Field = WidgetTree->ConstructWidget<UEditableTextBox>();
        Field->SetText(DefaultValue);
        Row->AddChildToHorizontalBox(Field)->SetPadding(FMargin(0.f, 3.f, 10.f, 3.f));
        Content->AddChildToVerticalBox(Row);
    };

    AddField(LOCTEXT("WorldName", "World name"), WorldNameBox, LOCTEXT("WorldNameDefault", "Whispering Pines"));
    AddField(LOCTEXT("Seed", "Seed"), SeedBox, LOCTEXT("SeedDefault", "20261018"));

    UHorizontalBox* ThemeRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    UTextBlock* ThemeLabel = WidgetTree->ConstructWidget<UTextBlock>();
    ThemeLabel->SetText(LOCTEXT("Theme", "World theme"));
    ThemeLabel->SetMinDesiredWidth(105.f);
    ThemeRow->AddChildToHorizontalBox(ThemeLabel)->SetPadding(FMargin(10.f, 3.f, 6.f, 3.f));
    ThemeBox = WidgetTree->ConstructWidget<UComboBoxString>();
    ThemeBox->AddOption(TEXT("Forest"));
    ThemeBox->AddOption(TEXT("Mountain (coming next)"));
    ThemeBox->AddOption(TEXT("Beach (coming next)"));
    ThemeBox->AddOption(TEXT("Meadow (coming next)"));
    ThemeBox->SetSelectedOption(CurrentTheme);
    ThemeBox->OnSelectionChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnThemeChanged);
    ThemeRow->AddChildToHorizontalBox(ThemeBox)->SetPadding(FMargin(0.f, 3.f, 10.f, 3.f));
    Content->AddChildToVerticalBox(ThemeRow);

    AddSliderRow(Content, LOCTEXT("TerrainSize", "Terrain size"), TerrainSlider, TerrainReadout, TerrainSize, 1600.f, 8000.f);
    TerrainSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnTerrainChanged);
    AddSliderRow(Content, LOCTEXT("TreeCount", "Trees"), TreesSlider, TreesReadout, TreeCount, 8.f, 120.f);
    TreesSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnTreesChanged);
    AddSliderRow(Content, LOCTEXT("RockCount", "Rocks"), RocksSlider, RocksReadout, RockCount, 0.f, 80.f);
    RocksSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnRocksChanged);
    AddSliderRow(Content, LOCTEXT("GrassDensity", "Grass density"), GrassSlider, GrassReadout, GrassDensity, 0.f, 320.f);
    GrassSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnGrassChanged);
    AddSliderRow(Content, LOCTEXT("Lighting", "Lighting"), LightingSlider, LightingReadout, LightingIntensity, 1.f, 15.f);
    LightingSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnLightingChanged);

    UTextBlock* ColorHeading = WidgetTree->ConstructWidget<UTextBlock>();
    ColorHeading->SetText(LOCTEXT("ColorHeading", "Color controls"));
    ColorHeading->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold"), 12));
    Content->AddChildToVerticalBox(ColorHeading)->SetPadding(FMargin(10.f, 10.f, 10.f, 2.f));
    AddSliderRow(Content, LOCTEXT("SkyTint", "Sky tint"), SkyTintSlider, SkyTintReadout, SkyTint, 0.f, 1.f);
    SkyTintSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnSkyTintChanged);
    AddSliderRow(Content, LOCTEXT("GroundTint", "Ground tint"), GroundTintSlider, GroundTintReadout, GroundTint, 0.f, 1.f);
    GroundTintSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnGroundTintChanged);
    AddSliderRow(Content, LOCTEXT("LightTint", "Light tint"), LightTintSlider, LightTintReadout, LightTint, 0.f, 1.f);
    LightTintSlider->OnValueChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnLightTintChanged);

    UHorizontalBox* CharacterRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayableCharacterBox = WidgetTree->ConstructWidget<UCheckBox>();
    PlayableCharacterBox->SetIsChecked(bAddPlayableCharacter);
    PlayableCharacterBox->OnCheckStateChanged.AddDynamic(this, &UInteractiveWorldBuilderWidget::OnPlayableCharacterChanged);
    CharacterRow->AddChildToHorizontalBox(PlayableCharacterBox)->SetPadding(FMargin(10.f, 6.f, 6.f, 6.f));
    UTextBlock* CharacterLabel = WidgetTree->ConstructWidget<UTextBlock>();
    CharacterLabel->SetText(LOCTEXT("CharacterLabel", "Use the Third Person template character when playing"));
    CharacterLabel->SetAutoWrapText(true);
    CharacterRow->AddChildToHorizontalBox(CharacterLabel)->SetPadding(FMargin(0.f, 6.f, 10.f, 6.f));
    Content->AddChildToVerticalBox(CharacterRow);

    UTextBlock* RuleNote = WidgetTree->ConstructWidget<UTextBlock>();
    RuleNote->SetText(LOCTEXT("RuleNote", "Placement rules: trees thin at high ground and steep virtual slopes; rocks form clusters; grass avoids the central path."));
    RuleNote->SetAutoWrapText(true);
    RuleNote->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.74f, 0.67f)));
    Content->AddChildToVerticalBox(RuleNote)->SetPadding(FMargin(10.f, 4.f, 10.f, 8.f));

    UHorizontalBox* PrimaryButtons = AddButtonRow(Content);
    auto AddButton = [this](UHorizontalBox* Row, const FText& Text, FLinearColor Color, void (UInteractiveWorldBuilderWidget::*Handler)())
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(Color);
        Button->OnClicked.AddDynamic(this, Handler);
        UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();
        ButtonText->SetText(Text);
        ButtonText->SetJustification(ETextJustify::Center);
        Button->AddChild(ButtonText);
        Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(3.f));
    };
    AddButton(PrimaryButtons, LOCTEXT("Generate", "Generate World"), FLinearColor(0.10f, 0.42f, 0.19f), &UInteractiveWorldBuilderWidget::GenerateWorld);
    AddButton(PrimaryButtons, LOCTEXT("Preview", "Preview"), FLinearColor(0.10f, 0.28f, 0.42f), &UInteractiveWorldBuilderWidget::PreviewWorld);

    UHorizontalBox* PresetButtons = AddButtonRow(Content);
    AddButton(PresetButtons, LOCTEXT("SavePreset", "Save Preset"), FLinearColor(0.18f, 0.22f, 0.30f), &UInteractiveWorldBuilderWidget::SavePreset);
    AddButton(PresetButtons, LOCTEXT("LoadPreset", "Load Preset"), FLinearColor(0.18f, 0.22f, 0.30f), &UInteractiveWorldBuilderWidget::LoadPreset);
    AddButton(PresetButtons, LOCTEXT("Reset", "Reset"), FLinearColor(0.25f, 0.20f, 0.08f), &UInteractiveWorldBuilderWidget::ResetControls);
    AddButton(PresetButtons, LOCTEXT("Delete", "Delete World"), FLinearColor(0.40f, 0.10f, 0.10f), &UInteractiveWorldBuilderWidget::DeleteWorld);

    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetAutoWrapText(true);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.88f, 0.78f)));
    Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(10.f, 10.f, 10.f, 14.f));
    SetStatus(TEXT("Ready. Forest is the focused theme; the other themes are intentionally scoped for a later pass."));
    UpdateReadouts();
}

void UInteractiveWorldBuilderWidget::AddSliderRow(UVerticalBox* Container, const FText& Label, USlider*& Slider, UTextBlock*& ValueText, float Initial, float Min, float Max)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>();
    LabelText->SetText(Label);
    LabelText->SetMinDesiredWidth(105.f);
    Row->AddChildToHorizontalBox(LabelText)->SetPadding(FMargin(10.f, 3.f, 6.f, 3.f));

    Slider = WidgetTree->ConstructWidget<USlider>();
    Slider->SetMinValue(Min);
    Slider->SetMaxValue(Max);
    Slider->SetValue(Initial);
    UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
    SliderSlot->SetPadding(FMargin(0.f, 3.f, 6.f, 3.f));
    SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    ValueText = WidgetTree->ConstructWidget<UTextBlock>();
    ValueText->SetMinDesiredWidth(48.f);
    ValueText->SetJustification(ETextJustify::Right);
    Row->AddChildToHorizontalBox(ValueText)->SetPadding(FMargin(0.f, 3.f, 10.f, 3.f));
    Container->AddChildToVerticalBox(Row);
}

UHorizontalBox* UInteractiveWorldBuilderWidget::AddButtonRow(UVerticalBox* Container)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Container->AddChildToVerticalBox(Row)->SetPadding(FMargin(7.f, 2.f));
    return Row;
}

void UInteractiveWorldBuilderWidget::OnThemeChanged(FString SelectedItem, ESelectInfo::Type)
{
    CurrentTheme = SelectedItem;
    if (CurrentTheme != TEXT("Forest"))
    {
        SetStatus(TEXT("Forest is the polished generation pass for this milestone. The selected theme is saved but currently uses the forest placement graph."));
    }
}

void UInteractiveWorldBuilderWidget::OnTerrainChanged(float Value) { TerrainSize = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnTreesChanged(float Value) { TreeCount = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnRocksChanged(float Value) { RockCount = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnGrassChanged(float Value) { GrassDensity = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnLightingChanged(float Value) { LightingIntensity = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnSkyTintChanged(float Value) { SkyTint = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnGroundTintChanged(float Value) { GroundTint = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnLightTintChanged(float Value) { LightTint = Value; UpdateReadouts(); }
void UInteractiveWorldBuilderWidget::OnPlayableCharacterChanged(bool bIsChecked) { bAddPlayableCharacter = bIsChecked; }

void UInteractiveWorldBuilderWidget::UpdateReadouts()
{
    if (TerrainReadout != nullptr) TerrainReadout->SetText(FText::FromString(IWB::NumberText(TerrainSize) + TEXT(" cm")));
    if (TreesReadout != nullptr) TreesReadout->SetText(FText::FromString(IWB::NumberText(TreeCount)));
    if (RocksReadout != nullptr) RocksReadout->SetText(FText::FromString(IWB::NumberText(RockCount)));
    if (GrassReadout != nullptr) GrassReadout->SetText(FText::FromString(IWB::NumberText(GrassDensity)));
    if (LightingReadout != nullptr) LightingReadout->SetText(FText::AsNumber(FMath::RoundToInt(LightingIntensity)));
    if (SkyTintReadout != nullptr) SkyTintReadout->SetText(FText::AsNumber(SkyTint));
    if (GroundTintReadout != nullptr) GroundTintReadout->SetText(FText::AsNumber(GroundTint));
    if (LightTintReadout != nullptr) LightTintReadout->SetText(FText::AsNumber(LightTint));
}

void UInteractiveWorldBuilderWidget::ApplyControlsToWidgets()
{
    if (TerrainSlider != nullptr) TerrainSlider->SetValue(TerrainSize);
    if (TreesSlider != nullptr) TreesSlider->SetValue(TreeCount);
    if (RocksSlider != nullptr) RocksSlider->SetValue(RockCount);
    if (GrassSlider != nullptr) GrassSlider->SetValue(GrassDensity);
    if (LightingSlider != nullptr) LightingSlider->SetValue(LightingIntensity);
    if (SkyTintSlider != nullptr) SkyTintSlider->SetValue(SkyTint);
    if (GroundTintSlider != nullptr) GroundTintSlider->SetValue(GroundTint);
    if (LightTintSlider != nullptr) LightTintSlider->SetValue(LightTint);
    if (ThemeBox != nullptr) ThemeBox->SetSelectedOption(CurrentTheme);
    if (PlayableCharacterBox != nullptr) PlayableCharacterBox->SetIsChecked(bAddPlayableCharacter);
    UpdateReadouts();
}

void UInteractiveWorldBuilderWidget::SetStatus(const FString& Message)
{
    if (StatusText != nullptr)
    {
        StatusText->SetText(FText::FromString(Message));
    }
}

void UInteractiveWorldBuilderWidget::GenerateWorld()
{
    ClearGeneratedActors(false, true);
    BuildWorld(false);
}

void UInteractiveWorldBuilderWidget::PreviewWorld()
{
    ClearGeneratedActors(true, false);
    BuildWorld(true);
}

void UInteractiveWorldBuilderWidget::DeleteWorld()
{
    ClearGeneratedActors(false, true);
    SetStatus(TEXT("The generated world and its preview were removed. Other level actors were left untouched."));
}

void UInteractiveWorldBuilderWidget::ClearGeneratedActors(bool bPreviewOnly, bool bClearAll)
{
    if (GEditor == nullptr)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (World == nullptr)
    {
        return;
    }

    TArray<TWeakObjectPtr<AActor>> ActorsToDelete;
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        AActor* Actor = *ActorIt;
        const bool bIsGenerated = Actor->Tags.Contains(IWB::GeneratedTag);
        const bool bIsPreview = Actor->Tags.Contains(IWB::PreviewTag);
        if ((bClearAll && (bIsGenerated || bIsPreview)) || (bPreviewOnly && bIsPreview))
        {
            ActorsToDelete.Add(Actor);
        }
    }

    if (ActorsToDelete.Num() > 0)
    {
        const FScopedTransaction Transaction(LOCTEXT("RemoveGeneratedWorld", "Remove Interactive World Builder actors"));
        for (const TWeakObjectPtr<AActor>& Actor : ActorsToDelete)
        {
            if (Actor.IsValid())
            {
                GEditor->EditorDestroyActor(Actor.Get(), true);
            }
        }
    }
}

void UInteractiveWorldBuilderWidget::BuildWorld(bool bPreviewOnly)
{
    if (GEditor == nullptr)
    {
        SetStatus(TEXT("No editor world is available."));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (World == nullptr)
    {
        SetStatus(TEXT("No editor world is available."));
        return;
    }

    const FScopedTransaction Transaction(LOCTEXT("BuildGeneratedWorld", "Generate Interactive World Builder forest"));
    const FName WorldTag = bPreviewOnly ? IWB::PreviewTag : IWB::GeneratedTag;
    const int32 Seed = SeedBox == nullptr ? 20261018 : FCString::Atoi(*SeedBox->GetText().ToString());
    FRandomStream Random(Seed);
    const FString WorldName = WorldNameBox == nullptr || WorldNameBox->GetText().IsEmpty() ? TEXT("Untitled Forest") : WorldNameBox->GetText().ToString();
    const FVector Origin = bPreviewOnly ? FVector(TerrainSize * 0.68f, 0.f, 0.f) : FVector::ZeroVector;
    const float HalfExtent = TerrainSize * 0.5f;
    const float PathHalfWidth = FMath::Clamp(TerrainSize * 0.045f, 160.f, 360.f);

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (CubeMesh == nullptr || ConeMesh == nullptr || CylinderMesh == nullptr || SphereMesh == nullptr)
    {
        SetStatus(TEXT("Engine basic-shape meshes could not be loaded."));
        return;
    }

    auto SpawnMesh = [World, WorldTag, &WorldName](UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FString& Label)
    {
        FActorSpawnParameters Parameters;
        Parameters.ObjectFlags |= RF_Transactional;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, Parameters);
        if (Actor != nullptr)
        {
            Actor->Tags.Add(WorldTag);
            Actor->SetActorLabel(FString::Printf(TEXT("IWB_%s_%s"), *WorldName, *Label));
            Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            Actor->SetActorScale3D(Scale);
        }
        return Actor;
    };

    auto TintMesh = [](AStaticMeshActor* Actor, const FLinearColor& Color)
    {
        if (Actor != nullptr)
        {
            if (UMaterialInstanceDynamic* Material = Actor->GetStaticMeshComponent()->CreateDynamicMaterialInstance(0))
            {
                Material->SetVectorParameterValue(TEXT("Color"), Color);
                Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            }
        }
    };

    const FLinearColor GroundColor = IWB::ForestGround(GroundTint);
    const FLinearColor SkyColor = IWB::ForestSky(SkyTint);
    const FLinearColor LightColor = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.86f, 0.64f), FLinearColor(0.78f, 0.90f, 1.f), LightTint);
    const FLinearColor TrunkColor(0.11f, 0.045f, 0.012f);
    const FLinearColor FoliageColor(0.025f, 0.26f, 0.05f);
    const FLinearColor RockColor(0.18f, 0.22f, 0.20f);

    AStaticMeshActor* Terrain = SpawnMesh(CubeMesh, Origin + FVector(0.f, 0.f, -55.f), FVector(TerrainSize / 100.f, TerrainSize / 100.f, 0.55f), FRotator::ZeroRotator, TEXT("Terrain"));
    TintMesh(Terrain, GroundColor);

    // Virtual elevation is used as a mask so placement remains deterministic and
    // ready to become height/slope attributes in a PCG graph later.
    auto VirtualElevation = [TerrainSize](float X, float Y)
    {
        const float Frequency = 6.25f / TerrainSize;
        return 0.5f + 0.5f * FMath::Sin(X * Frequency) * FMath::Cos(Y * Frequency * 1.18f);
    };
    auto VirtualSlope = [TerrainSize](float X, float Y)
    {
        const float Frequency = 6.25f / TerrainSize;
        return FMath::Abs(FMath::Cos(X * Frequency) * FMath::Sin(Y * Frequency * 1.18f));
    };

    const int32 DesiredTrees = FMath::RoundToInt(TreeCount);
    int32 PlacedTrees = 0;
    for (int32 Attempt = 0; Attempt < DesiredTrees * 4 && PlacedTrees < DesiredTrees; ++Attempt)
    {
        const float X = Random.FRandRange(-HalfExtent * 0.92f, HalfExtent * 0.92f);
        const float Y = Random.FRandRange(-HalfExtent * 0.92f, HalfExtent * 0.92f);
        const float Elevation = VirtualElevation(X, Y);
        const float Slope = VirtualSlope(X, Y);
        if (Elevation > 0.76f || Slope > 0.86f || FMath::Abs(Y) < PathHalfWidth)
        {
            continue;
        }

        const float Height = Random.FRandRange(480.f, 820.f);
        const FVector Base = Origin + FVector(X, Y, 0.f);
        AStaticMeshActor* Trunk = SpawnMesh(CylinderMesh, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(Random.FRandRange(0.12f, 0.18f), Random.FRandRange(0.12f, 0.18f), Height / 100.f), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), TEXT("TreeTrunk"));
        TintMesh(Trunk, TrunkColor);
        AStaticMeshActor* Crown = SpawnMesh(ConeMesh, Base + FVector(0.f, 0.f, Height + 110.f), FVector(Random.FRandRange(0.62f, 1.f), Random.FRandRange(0.62f, 1.f), Random.FRandRange(1.2f, 1.8f)), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), TEXT("TreeCrown"));
        TintMesh(Crown, FoliageColor);
        ++PlacedTrees;
    }

    const int32 DesiredRocks = FMath::RoundToInt(RockCount);
    int32 PlacedRocks = 0;
    while (PlacedRocks < DesiredRocks)
    {
        const FVector ClusterCenter = Origin + FVector(Random.FRandRange(-HalfExtent * 0.86f, HalfExtent * 0.86f), Random.FRandRange(-HalfExtent * 0.86f, HalfExtent * 0.86f), 0.f);
        const int32 ClusterSize = FMath::Min(Random.RandRange(2, 5), DesiredRocks - PlacedRocks);
        for (int32 RockIndex = 0; RockIndex < ClusterSize; ++RockIndex)
        {
            const FVector Location = ClusterCenter + FVector(Random.FRandRange(-260.f, 260.f), Random.FRandRange(-260.f, 260.f), Random.FRandRange(25.f, 70.f));
            const FVector RockScale(Random.FRandRange(0.35f, 1.1f), Random.FRandRange(0.30f, 0.90f), Random.FRandRange(0.26f, 0.70f));
            AStaticMeshActor* Rock = SpawnMesh(SphereMesh, Location, RockScale, FRotator(Random.FRandRange(0.f, 35.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 35.f)), TEXT("Rock"));
            TintMesh(Rock, RockColor);
            ++PlacedRocks;
        }
    }

    const int32 DesiredGrass = FMath::RoundToInt(GrassDensity);
    int32 PlacedGrass = 0;
    for (int32 Attempt = 0; Attempt < DesiredGrass * 3 && PlacedGrass < DesiredGrass; ++Attempt)
    {
        const float X = Random.FRandRange(-HalfExtent * 0.94f, HalfExtent * 0.94f);
        const float Y = Random.FRandRange(-HalfExtent * 0.94f, HalfExtent * 0.94f);
        if (FMath::Abs(Y) < PathHalfWidth || VirtualElevation(X, Y) > 0.88f)
        {
            continue;
        }
        AStaticMeshActor* Grass = SpawnMesh(ConeMesh, Origin + FVector(X, Y, 32.f), FVector(Random.FRandRange(0.035f, 0.065f), Random.FRandRange(0.035f, 0.065f), Random.FRandRange(0.32f, 0.7f)), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), TEXT("Grass"));
        TintMesh(Grass, FLinearColor(0.05f, 0.34f, 0.04f));
        ++PlacedGrass;
    }

    // A narrow, intentionally grass-free path gives the generated character a clear route.
    AStaticMeshActor* Path = SpawnMesh(CubeMesh, Origin + FVector(0.f, 0.f, 6.f), FVector(TerrainSize / 100.f, (PathHalfWidth * 1.4f) / 100.f, 0.06f), FRotator::ZeroRotator, TEXT("Path"));
    TintMesh(Path, FLinearColor(0.18f, 0.11f, 0.055f));

    if (!bPreviewOnly)
    {
        FActorSpawnParameters Parameters;
        Parameters.ObjectFlags |= RF_Transactional;
        ADirectionalLight* DirectionalLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Origin + FVector(0.f, 0.f, 1100.f), FRotator(-52.f, -28.f, 0.f), Parameters);
        if (DirectionalLight != nullptr)
        {
            DirectionalLight->Tags.Add(WorldTag);
            DirectionalLight->SetActorLabel(TEXT("IWB_ForestLight"));
            DirectionalLight->GetLightComponent()->SetIntensity(LightingIntensity * 10000.f);
            DirectionalLight->GetLightComponent()->SetLightColor(LightColor);
        }

        ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), Origin + FVector(0.f, 0.f, 1050.f), FRotator::ZeroRotator, Parameters);
        if (SkyLight != nullptr)
        {
            SkyLight->Tags.Add(WorldTag);
            SkyLight->SetActorLabel(TEXT("IWB_ForestSky"));
            SkyLight->GetLightComponent()->SetLightColor(SkyColor);
            SkyLight->GetLightComponent()->RecaptureSky();
        }
    }

    const FString Mode = bPreviewOnly ? TEXT("Preview") : TEXT("Generated");
    const FString CharacterNote = bAddPlayableCharacter ? TEXT(" Play with the Third Person template character.") : TEXT(" Character checkbox is off.");
    SetStatus(FString::Printf(TEXT("%s '%s' with seed %d — %d trees, %d clustered rocks, and %d grass instances.%s"), *Mode, *WorldName, Seed, PlacedTrees, PlacedRocks, PlacedGrass, *CharacterNote));
}

void UInteractiveWorldBuilderWidget::SavePreset()
{
    const FString PresetPath = FPaths::ProjectSavedDir() / TEXT("InteractiveWorldBuilderPreset.ini");
    GConfig->SetString(IWB::PresetSection, TEXT("WorldName"), *WorldNameBox->GetText().ToString(), PresetPath);
    GConfig->SetString(IWB::PresetSection, TEXT("Seed"), *SeedBox->GetText().ToString(), PresetPath);
    GConfig->SetString(IWB::PresetSection, TEXT("Theme"), *CurrentTheme, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("TerrainSize"), TerrainSize, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("TreeCount"), TreeCount, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("RockCount"), RockCount, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("GrassDensity"), GrassDensity, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("LightingIntensity"), LightingIntensity, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("SkyTint"), SkyTint, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("GroundTint"), GroundTint, PresetPath);
    GConfig->SetFloat(IWB::PresetSection, TEXT("LightTint"), LightTint, PresetPath);
    GConfig->SetBool(IWB::PresetSection, TEXT("PlayableCharacter"), bAddPlayableCharacter, PresetPath);
    GConfig->Flush(false, PresetPath);
    SetStatus(TEXT("Preset saved to Saved/InteractiveWorldBuilderPreset.ini."));
}

void UInteractiveWorldBuilderWidget::LoadPreset()
{
    const FString PresetPath = FPaths::ProjectSavedDir() / TEXT("InteractiveWorldBuilderPreset.ini");
    FString TextValue;
    if (!GConfig->GetString(IWB::PresetSection, TEXT("WorldName"), TextValue, PresetPath))
    {
        SetStatus(TEXT("No preset has been saved yet."));
        return;
    }
    WorldNameBox->SetText(FText::FromString(TextValue));
    if (GConfig->GetString(IWB::PresetSection, TEXT("Seed"), TextValue, PresetPath)) SeedBox->SetText(FText::FromString(TextValue));
    if (GConfig->GetString(IWB::PresetSection, TEXT("Theme"), TextValue, PresetPath)) CurrentTheme = TextValue;
    GConfig->GetFloat(IWB::PresetSection, TEXT("TerrainSize"), TerrainSize, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("TreeCount"), TreeCount, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("RockCount"), RockCount, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("GrassDensity"), GrassDensity, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("LightingIntensity"), LightingIntensity, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("SkyTint"), SkyTint, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("GroundTint"), GroundTint, PresetPath);
    GConfig->GetFloat(IWB::PresetSection, TEXT("LightTint"), LightTint, PresetPath);
    GConfig->GetBool(IWB::PresetSection, TEXT("PlayableCharacter"), bAddPlayableCharacter, PresetPath);
    ApplyControlsToWidgets();
    SetStatus(TEXT("Preset loaded. You can preview it without replacing the current generated world."));
}

void UInteractiveWorldBuilderWidget::ResetControls()
{
    CurrentTheme = TEXT("Forest");
    TerrainSize = 4000.f;
    TreeCount = 45.f;
    RockCount = 24.f;
    GrassDensity = 120.f;
    LightingIntensity = 8.f;
    SkyTint = 0.55f;
    GroundTint = 0.32f;
    LightTint = 0.12f;
    bAddPlayableCharacter = true;
    if (WorldNameBox != nullptr) WorldNameBox->SetText(LOCTEXT("WorldNameDefaultReset", "Whispering Pines"));
    if (SeedBox != nullptr) SeedBox->SetText(LOCTEXT("SeedDefaultReset", "20261018"));
    ApplyControlsToWidgets();
    SetStatus(TEXT("Controls reset. Existing generated actors were not changed."));
}

#undef LOCTEXT_NAMESPACE
