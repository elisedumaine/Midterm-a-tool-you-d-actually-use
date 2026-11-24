#include "ForestStartMenu.h"

#include "ForestGameInstance.h"
#include "ForestLanguage.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

TSharedRef<SWidget> UForestStartMenu::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 58);
	const FSlateFontInfo SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 22);
	const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.012f, 0.025f, 0.018f, 0.94f))
			.Padding(FMargin(80.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SSpacer)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("WHISPERING PINES")))
					.Font(TitleFont)
					.ColorAndOpacity(FLinearColor(0.92f, 0.80f, 0.51f, 1.0f))
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 16.0f, 0.0f, 52.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("A sleeping forest is waiting for you to wake up."), TEXT("Une forêt endormie attend ton réveil."))); })
					.Font(SubtitleFont)
					.ColorAndOpacity(FLinearColor(0.76f, 0.86f, 0.77f, 1.0f))
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(FMargin(72.0f, 18.0f))
					.OnClicked_UObject(this, &UForestStartMenu::HandlePlayClicked)
					[
						SNew(STextBlock)
						.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("PLAY"), TEXT("JOUER"))); })
						.Font(ButtonFont)
						.ColorAndOpacity(FLinearColor(0.96f, 0.94f, 0.86f, 1.0f))
						.Justification(ETextJustify::Center)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 40.0f, 0.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f))
					[
						LanguageButton(false, TEXT("English"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 0.0f))
					[
						LanguageButton(true, TEXT("Français"))
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SSpacer)
				]
			]
		];
}

TSharedRef<SWidget> UForestStartMenu::LanguageButton(bool bFrench, const TCHAR* Label)
{
	// The chosen language is bright, the other one dim.
	auto Chosen = [bFrench]() { return ForestLanguage::IsFrench() == bFrench; };
	return SNew(SButton)
		.ContentPadding(FMargin(26.0f, 10.0f))
		.ButtonColorAndOpacity_Lambda([Chosen]() { return Chosen() ? FLinearColor(0.55f, 0.75f, 0.45f, 1.0f) : FLinearColor(0.25f, 0.25f, 0.25f, 0.6f); })
		.OnClicked_Lambda([bFrench]() { ForestLanguage::SetFrench(bFrench); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ColorAndOpacity_Lambda([Chosen]() { return FSlateColor(Chosen() ? FLinearColor(1.0f, 0.98f, 0.9f, 1.0f) : FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)); })
		];
}

FReply UForestStartMenu::HandlePlayClicked()
{
	if (UForestGameInstance* GameInstance = GetGameInstance<UForestGameInstance>())
	{
		GameInstance->StartForestIntro();
	}
	return FReply::Handled();
}
