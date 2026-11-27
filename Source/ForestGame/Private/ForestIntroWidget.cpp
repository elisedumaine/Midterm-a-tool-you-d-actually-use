#include "ForestIntroWidget.h"

#include "ForestLanguage.h"
#include "HAL/PlatformTime.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float LineFadeSeconds = 1.6f;

	TSharedRef<SWidget> Black(float Alpha)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, Alpha))
			.Padding(0.0f);
	}

	/** One eyelid: solid black, with a soft edge toward the middle of the screen. */
	TSharedRef<SWidget> Eyelid(bool bTop)
	{
		TSharedRef<SVerticalBox> Lid = SNew(SVerticalBox);
		// Many thin strips fading out: the lid's edge looks soft, not striped.
		constexpr int32 Strips = 40;
		auto Strip = [](int32 Index) { const float T = 1.0f - (Index + 0.5f) / Strips; return T * T * (3.0f - 2.0f * T); };
		if (!bTop)
		{
			for (int32 Index = Strips - 1; Index >= 0; --Index)
			{
				Lid->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(3.0f)[Black(Strip(Index))]];
			}
		}
		Lid->AddSlot().FillHeight(1.0f)[Black(1.0f)];
		if (bTop)
		{
			for (int32 Index = 0; Index < Strips; ++Index)
			{
				Lid->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(3.0f)[Black(Strip(Index))]];
			}
		}
		return Lid;
	}
}

TSharedRef<SWidget> UForestIntroWidget::RebuildWidget()
{
	FSlateFontInfo LineFont = FCoreStyle::GetDefaultFontStyle("Italic", 30);
	LineFont.LetterSpacing = 60;

	// Slate may still draw a frame after the widget object is gone (end of the
	// intro, end of Play): the lambdas only read it while it is still alive.
	TWeakObjectPtr<UForestIntroWidget> Self = this;
	auto EyesOf = [Self]() { return Self.IsValid() ? Self->Eyes : 1.0f; };
	auto BlurOf = [Self]() { return Self.IsValid() ? Self->Blur : 0.0f; };
	auto AlphaOf = [Self]() { return Self.IsValid() ? Self->LineAlpha() : 0.0f; };

	return SNew(SOverlay)
		// The world through half-open eyes: blurry at first, then sharp.
		+ SOverlay::Slot()
		[
			SNew(SBackgroundBlur)
			.BlurStrength_Lambda(BlurOf)
			.Visibility_Lambda([BlurOf]() { return BlurOf() > 0.05f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]
		// The eyelids: they meet in the middle when the eyes are shut.
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([EyesOf]() { return 0.5f * (1.0f - EyesOf()) + 0.0001f; }))
			[
				Eyelid(true)
			]
			+ SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([EyesOf]() { return EyesOf() + 0.0001f; }))
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().FillHeight(TAttribute<float>::CreateLambda([EyesOf]() { return 0.5f * (1.0f - EyesOf()) + 0.0001f; }))
			[
				Eyelid(false)
			]
		]
		// Fully shut: the two soft lid edges would leave a grey line where they
		// meet, so a plain black sheet covers it until the eyes start to open.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([EyesOf]()
			{
				const float Alpha = 1.0f - FMath::SmoothStep(0.0f, 0.12f, EyesOf());
				return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
			})
			.Visibility_Lambda([EyesOf]() { return EyesOf() < 0.12f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]
		// The words, low in the screen, no box: just light text with a shadow.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(160.0f, 0.0f, 160.0f, 150.0f))
		[
			SNew(STextBlock)
			.Text_Lambda([Self]() { return FText::FromString(Self.IsValid() ? Self->Line : FString()); })
			.Font(LineFont)
			.ColorAndOpacity_Lambda([AlphaOf]() { return FSlateColor(FLinearColor(0.98f, 0.94f, 0.84f, AlphaOf())); })
			.ShadowOffset(FVector2D(0.0f, 2.0f))
			.ShadowColorAndOpacity_Lambda([AlphaOf]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.7f * AlphaOf()); })
			.Justification(ETextJustify::Center)
			.WrapTextAt(1300.0f)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 40.0f, 30.0f))
		[
			SNew(STextBlock)
			.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("[Space]  Skip"), TEXT("[Espace]  Passer"))); })
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.78f, 0.45f))
			.Visibility_Lambda([Self]() { return Self.IsValid() && Self->bSkipHint ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		];
}

void UForestIntroWidget::ShowLine(const FString& NewLine)
{
	Line = NewLine;
	bLineVisible = true;
	LineChangedAt = FPlatformTime::Seconds();
}

void UForestIntroWidget::HideLine()
{
	if (bLineVisible)
	{
		bLineVisible = false;
		LineChangedAt = FPlatformTime::Seconds();
	}
}

float UForestIntroWidget::LineAlpha() const
{
	const float Age = float(FPlatformTime::Seconds() - LineChangedAt);
	const float Alpha = FMath::Clamp(Age / LineFadeSeconds, 0.0f, 1.0f);
	const float Smooth = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	return bLineVisible ? Smooth : 1.0f - Smooth;
}
