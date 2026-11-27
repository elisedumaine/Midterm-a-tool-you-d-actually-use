#include "ForestStartMenu.h"

#include "ForestGameInstance.h"
#include "HAL/PlatformTime.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(0.98f, 0.84f, 0.55f, 1.0f);
	const FLinearColor Cream(0.97f, 0.95f, 0.88f, 1.0f);
	const FLinearColor Night(0.01f, 0.015f, 0.02f, 1.0f);

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	/** A strip of shade; stacked with others it makes a soft gradient. */
	TSharedRef<SWidget> ShadeBand(float Alpha)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(WithAlpha(Night, Alpha))
			.Padding(0.0f);
	}
}

float UForestStartMenu::Appear(float Delay) const
{
	const float Age = float(FPlatformTime::Seconds() - ShownAt) - Delay;
	const float Alpha = FMath::Clamp(Age / 2.0f, 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}

TSharedRef<SWidget> UForestStartMenu::RebuildWidget()
{
	ShownAt = FPlatformTime::Seconds();
	// The lambdas below only read the menu while it is still alive (Slate can draw one more frame after it is gone).
	TWeakObjectPtr<UForestStartMenu> Self = this;

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Light", 76);
	TitleFont.LetterSpacing = 380;
	FSlateFontInfo TaglineFont = FCoreStyle::GetDefaultFontStyle("Regular", 15);
	TaglineFont.LetterSpacing = 160;
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Italic", 22);
	FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 26);
	ButtonFont.LetterSpacing = 300;

	// Soft shade, darkest at the very bottom, clear in the middle of the screen.
	TSharedRef<SVerticalBox> Shade = SNew(SVerticalBox);
	Shade->AddSlot().FillHeight(0.45f)[SNew(SSpacer)];
	for (int32 Band = 1; Band <= 12; ++Band)
	{
		Shade->AddSlot().FillHeight(0.55f / 12.0f)[ShadeBand(0.62f * FMath::Pow(Band / 12.0f, 1.6f))];
	}
	TSharedRef<SVerticalBox> TopShade = SNew(SVerticalBox);
	for (int32 Band = 6; Band >= 1; --Band)
	{
		TopShade->AddSlot().FillHeight(0.18f / 6.0f)[ShadeBand(0.28f * FMath::Pow(Band / 6.0f, 1.6f))];
	}
	TopShade->AddSlot().FillHeight(0.82f)[SNew(SSpacer)];

	TSharedRef<SHorizontalBox> Languages = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < (int32)EForestLanguage::Count; ++Index)
	{
		Languages->AddSlot().AutoWidth().Padding(FMargin(8.0f, 0.0f))[LanguageButton((EForestLanguage)Index)];
	}

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Padding(0.0f)
		.ColorAndOpacity_Lambda([Self]() { return FLinearColor(1.0f, 1.0f, 1.0f, (Self.IsValid() ? Self->Appear() : 0.0f)); })
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[TopShade]
			+ SOverlay::Slot()[Shade]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(0.5f)[SNew(SSpacer)]
				// The title breathes very slowly, like the forest.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("WHISPERING PINES")))
					.Font(TitleFont)
					.ColorAndOpacity_Lambda([Self]()
					{
						const float Breath = 0.88f + 0.12f * FMath::Sin(float(FPlatformTime::Seconds() - (Self.IsValid() ? Self->ShownAt : 0.0)) * 0.9f);
						return FSlateColor(WithAlpha(Gold, Breath * (Self.IsValid() ? Self->Appear() : 0.0f)));
					})
					.ShadowOffset(FVector2D(0.0f, 3.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
					.Justification(ETextJustify::Center)
				]
				// A thin golden line under the title.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 14.0f, 0.0f, 16.0f))
				[
					SNew(SBox)
					.WidthOverride(320.0f)
					.HeightOverride(1.5f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor_Lambda([Self]() { return FSlateColor(WithAlpha(Gold, 0.65f * (Self.IsValid() ? Self->Appear(0.6f) : 0.0f))); })
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("A sleeping forest is waiting for you to wake up."), TEXT("Une forêt endormie attend ton réveil."))); })
					.Font(SubtitleFont)
					.ColorAndOpacity_Lambda([Self]() { return FSlateColor(WithAlpha(Cream, 0.92f * (Self.IsValid() ? Self->Appear(0.8f) : 0.0f))); })
					.ShadowOffset(FVector2D(0.0f, 2.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("Gather  ·  Befriend  ·  Build  ·  Survive the night"), TEXT("Récolte  ·  Apprivoise  ·  Construis  ·  Survis à la nuit"))); })
					.Font(TaglineFont)
					.ColorAndOpacity_Lambda([Self]() { return FSlateColor(WithAlpha(Gold, 0.7f * (Self.IsValid() ? Self->Appear(1.2f) : 0.0f))); })
					.ShadowOffset(FVector2D(0.0f, 1.5f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().FillHeight(0.22f)[SNew(SSpacer)]
				// PLAY: a see-through button that warms up under the mouse.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ContentPadding(FMargin(84.0f, 16.0f))
					.ButtonColorAndOpacity_Lambda([Self]() { return FLinearColor(0.95f, 0.78f, 0.45f, 0.28f * (Self.IsValid() ? Self->Appear(1.5f) : 0.0f)); })
					.OnClicked_UObject(this, &UForestStartMenu::HandlePlayClicked)
					[
						SNew(STextBlock)
						.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("PLAY"), TEXT("JOUER"))); })
						.Font(ButtonFont)
						.ColorAndOpacity_Lambda([Self]() { return FSlateColor(WithAlpha(Cream, (Self.IsValid() ? Self->Appear(1.5f) : 0.0f))); })
						.ShadowOffset(FVector2D(0.0f, 2.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
						.Justification(ETextJustify::Center)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 30.0f, 0.0f, 0.0f))
				[
					Languages
				]
				+ SVerticalBox::Slot().FillHeight(0.16f)[SNew(SSpacer)]
			]
		];
}

TSharedRef<SWidget> UForestStartMenu::LanguageButton(EForestLanguage Language)
{
	// The chosen language glows, the others are quiet.
	auto Chosen = [Language]() { return ForestLanguage::Get() == Language; };
	TWeakObjectPtr<UForestStartMenu> Self = this;
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ContentPadding(FMargin(22.0f, 8.0f))
		.ButtonColorAndOpacity_Lambda([Self, Chosen]()
		{
			return Chosen() ? FLinearColor(0.95f, 0.78f, 0.45f, 0.45f * (Self.IsValid() ? Self->Appear(1.8f) : 0.0f)) : FLinearColor(0.2f, 0.2f, 0.2f, 0.18f * (Self.IsValid() ? Self->Appear(1.8f) : 0.0f));
		})
		.OnClicked_Lambda([Language]() { ForestLanguage::Set(Language); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text(FText::FromString(ForestLanguage::NativeName(Language)))
			.Font_Lambda([Chosen]() { return FCoreStyle::GetDefaultFontStyle(Chosen() ? "Bold" : "Regular", 16); })
			.ColorAndOpacity_Lambda([Self, Chosen]() { return FSlateColor(WithAlpha(Chosen() ? Cream : FLinearColor(0.85f, 0.85f, 0.82f, 0.75f), (Self.IsValid() ? Self->Appear(1.8f) : 0.0f))); })
			.ShadowOffset(FVector2D(0.0f, 1.5f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
		];
}

FReply UForestStartMenu::HandlePlayClicked()
{
	if (UForestGameInstance* GameInstance = GetGameInstance<UForestGameInstance>())
	{
		GameInstance->OpenCreator();
	}
	return FReply::Handled();
}
