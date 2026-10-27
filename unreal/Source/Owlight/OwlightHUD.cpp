#include "OwlightHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "OwlightCharacter.h"
#include "SlimeActor.h"
#include "SlimeComponent.h"

void AOwlightHUD::Line(float X, float Y, const FString& S, const FLinearColor& C, float Scale)
{
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(S), GEngine->GetMediumFont(), C);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0, 0, 0, 0.8f));
	Canvas->DrawItem(Item);
}

void AOwlightHUD::DrawHUD()
{
	Super::DrawHUD();
	const AOwlightCharacter* Me = Cast<AOwlightCharacter>(GetOwningPawn());
	if (!Me || !Canvas) return;

	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const float S = FMath::Max(1.f, H / 720.f);
	const FLinearColor Ink(0.96f, 0.92f, 0.82f), Dim(0.75f, 0.70f, 0.60f), Mark(1.0f, 0.78f, 0.35f);

	// pockets, top left
	float y = 18.f * S;
	Line(18.f * S, y, TEXT("pockets"), Dim, S); y += 18.f * S;
	TArray<FName> Keys; Me->Pockets.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
	if (Keys.Num() == 0) { Line(18.f * S, y, TEXT("  (empty)"), Dim, S); y += 18.f * S; }
	for (const FName& K : Keys)
	{
		const bool Sel = (K == Me->SelectedFood);
		Line(18.f * S, y, FString::Printf(TEXT("%s%s x%d"), Sel ? TEXT("> ") : TEXT("  "), *K.ToString().Replace(TEXT("_"), TEXT(" ")), Me->Pockets[K]), Sel ? Mark : Ink, S);
		y += 18.f * S;
	}
	if (Keys.Num() > 1) { Line(18.f * S, y, TEXT("  Q  next"), Dim, S); }

	// prompt, bottom centre
	const FString Prompt = Me->PromptText().ToString();
	if (!Prompt.IsEmpty())
	{
		float tw = 0, th = 0; Canvas->TextSize(GEngine->GetMediumFont(), Prompt, tw, th, S * 1.2f, S * 1.2f);
		Line((W - tw) * 0.5f, H - 70.f * S, Prompt, Mark, S * 1.2f);
	}

	// the slime you are looking at (or holding): name, what you can see, what you wrote down
	const ASlimeActor* Look = Me->Carried ? Me->Carried.Get() : Me->FocusSlime.Get();
	if (Look && Look->Slime)
	{
		const FSlimeLook L = Look->Slime->GetLook();
		float yy = H - 70.f * S - 22.f * S * (3 + L.Signs.Num()) - 60.f * S;
		yy = FMath::Max(yy, H * 0.45f);
		Line(18.f * S, yy, Look->Slime->SlimeName, Ink, S * 1.3f); yy += 24.f * S;
		for (const FText& Sign : L.Signs) { Line(18.f * S, yy, TEXT("  ") + Sign.ToString(), Ink, S); yy += 18.f * S; }
		const TArray<FString> J = Look->Slime->GetJournal();
		if (J.Num())
		{
			yy += 6.f * S; Line(18.f * S, yy, TEXT("journal"), Dim, S); yy += 18.f * S;
			for (int32 i = FMath::Max(0, J.Num() - 3); i < J.Num(); ++i) { Line(18.f * S, yy, TEXT("  ") + J[i], Dim, S); yy += 18.f * S; }
		}
	}

	// dev clock, top right
	const FString Dev = Me->DevTimeScale > 1.f ? FString::Printf(TEXT("time x%.0f   T"), Me->DevTimeScale) : TEXT("real time   T");
	float tw = 0, th = 0; Canvas->TextSize(GEngine->GetMediumFont(), Dev, tw, th, S, S);
	Line(W - tw - 18.f * S, 18.f * S, Dev, Dim, S);
}
