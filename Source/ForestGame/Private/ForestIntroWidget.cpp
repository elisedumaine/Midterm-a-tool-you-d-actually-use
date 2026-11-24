#include "ForestIntroWidget.h"

#include "ForestLanguage.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UForestIntroWidget::RebuildWidget()
{
	const FSlateFontInfo NarrativeFont = FCoreStyle::GetDefaultFontStyle("Regular", 25);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(120.0f, 0.0f, 120.0f, 86.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.005f, 0.012f, 0.008f, 0.72f))
			.Padding(FMargin(34.0f, 20.0f))
			[
				SAssignNew(NarrativeText, STextBlock)
				.Text(FText::FromString(ForestLanguage::Pick(TEXT("The forest holds its breath..."), TEXT("La forêt retient son souffle..."))))
				.Font(NarrativeFont)
				.ColorAndOpacity(FLinearColor(0.95f, 0.94f, 0.82f, 1.0f))
				.Justification(ETextJustify::Center)
			]
		];
}

void UForestIntroWidget::SetNarrative(const FText& NewNarrative)
{
	if (NarrativeText.IsValid())
	{
		NarrativeText->SetText(NewNarrative);
	}
}
