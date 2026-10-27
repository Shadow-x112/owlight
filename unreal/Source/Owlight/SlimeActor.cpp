#include "SlimeActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SlimeComponent.h"
#include "UObject/ConstructorHelpers.h"

ASlimeActor::ASlimeActor()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShellMesh(TEXT("/Game/Owlight/Meshes/SM_SlimeShell"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoreMesh(TEXT("/Game/Owlight/Meshes/SM_SlimeCore"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShellMat(TEXT("/Game/Owlight/Materials/M_SlimeShell"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMat(TEXT("/Game/Owlight/Materials/M_SlimeCore"));

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	SetRootComponent(Shell);
	if (ShellMesh.Succeeded()) Shell->SetStaticMesh(ShellMesh.Object);
	if (ShellMat.Succeeded()) Shell->SetMaterial(0, ShellMat.Object);
	Shell->SetRenderCustomDepth(true);
	Shell->SetCustomDepthStencilValue(1);
	Shell->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Shell);
	if (CoreMesh.Succeeded()) Core->SetStaticMesh(CoreMesh.Object);
	if (CoreMat.Succeeded()) Core->SetMaterial(0, CoreMat.Object);
	Core->SetRenderCustomDepth(true);
	Core->SetCustomDepthStencilValue(1);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetRelativeLocation(FVector(0.f, 0.f, 11.f));

	Slime = CreateDefaultSubobject<USlimeComponent>(TEXT("Slime"));

	// design.md §6 — one colour per type; tuning lives here until a data table replaces it.
	TypeColors = {
		{TEXT("plain"),   FLinearColor(0.06f, 0.50f, 0.16f)},
		{TEXT("soot"),    FLinearColor(0.03f, 0.03f, 0.035f)},
		{TEXT("brook"),   FLinearColor(0.10f, 0.45f, 0.70f)},
		{TEXT("lantern"), FLinearColor(0.55f, 0.85f, 1.00f)},
		{TEXT("forge"),   FLinearColor(0.60f, 0.16f, 0.04f)},
		{TEXT("frost"),   FLinearColor(0.70f, 0.85f, 0.95f)},
		{TEXT("loam"),    FLinearColor(0.30f, 0.18f, 0.08f)},
		{TEXT("moss"),    FLinearColor(0.10f, 0.25f, 0.06f)},
		{TEXT("scrub"),   FLinearColor(0.75f, 0.70f, 0.15f)},
		{TEXT("puff"),    FLinearColor(0.95f, 0.60f, 0.75f)},
		{TEXT("shade"),   FLinearColor(0.12f, 0.06f, 0.20f)},
		{TEXT("glutton"), FLinearColor(0.65f, 0.15f, 0.45f)},
		{TEXT("steady"),  FLinearColor(0.35f, 0.38f, 0.40f)},
		{TEXT("pearl"),   FLinearColor(0.95f, 0.93f, 0.90f)},
	};
}

FLinearColor ASlimeActor::ColorForType(FName TypeId) const
{
	if (const FLinearColor* C = TypeColors.Find(TypeId)) return *C;
	if (const FLinearColor* P = TypeColors.Find(TEXT("plain"))) return *P;
	return FLinearColor(0.06f, 0.5f, 0.16f);
}

void ASlimeActor::BeginPlay()
{
	Super::BeginPlay();
	ShellMID = Shell->CreateAndSetMaterialInstanceDynamic(0);
	CoreMID = Core->CreateAndSetMaterialInstanceDynamic(0);
	ApplyLook(true);
}

void ASlimeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Slime) return;
	const FName Type = Slime->GetTypeId();
	const FLinearColor Target = ColorForType(Type);
	const float Clarity = Slime->GetHiddenValue(ESlimeProperty::Clarity);
	const float Heat = FMath::Clamp((Slime->GetHiddenValue(ESlimeProperty::Heat) - 0.5f) * 2.f, 0.f, 1.f);
	const float Density = Slime->GetHiddenValue(ESlimeProperty::Density);
	const float Glow = (Type == TEXT("lantern")) ? 0.6f : (Type == TEXT("pearl") ? 0.2f : 0.f);

	const float A = FMath::Clamp(DeltaSeconds * EaseSpeed, 0.f, 1.f);
	CurColor = FMath::Lerp(CurColor, Target, A);
	CurClarity = FMath::Lerp(CurClarity, Clarity, A);
	CurHeat = FMath::Lerp(CurHeat, Heat, A);
	CurGlow = FMath::Lerp(CurGlow, Glow, A);
	CurDensity = FMath::Lerp(CurDensity, Density, A);
	ApplyLook(false);
}

void ASlimeActor::ApplyLook(bool bSnap)
{
	if (bSnap && Slime)
	{
		CurColor = ColorForType(Slime->GetTypeId());
		CurClarity = Slime->GetHiddenValue(ESlimeProperty::Clarity);
		CurHeat = FMath::Clamp((Slime->GetHiddenValue(ESlimeProperty::Heat) - 0.5f) * 2.f, 0.f, 1.f);
		CurDensity = Slime->GetHiddenValue(ESlimeProperty::Density);
		CurGlow = 0.f;
	}
	// dense: squat and wide; light: tall and narrow (same rule as Scripts/style_proof.py)
	const float Sq = 1.f - 0.35f * (CurDensity - 0.5f) * 2.f;
	const float Xy = 1.f / FMath::Sqrt(FMath::Max(Sq, 0.3f));
	Shell->SetRelativeScale3D(FVector(Xy, Xy, Sq));
	const float Cs = 0.75f + 0.5f * CurDensity;
	Core->SetRelativeScale3D(FVector(Cs * 1.1f / Xy, Cs * 1.1f / Xy, Cs * 0.85f / Sq));
	Core->SetRelativeLocation(FVector(0.f, 0.f, 11.f));

	const FLinearColor Rim(FMath::Min(1.f, CurColor.R * 2.5f + 0.2f), FMath::Min(1.f, CurColor.G * 1.8f + 0.3f), FMath::Min(1.f, CurColor.B * 2.5f + 0.2f));
	if (ShellMID)
	{
		ShellMID->SetVectorParameterValue(TEXT("Color"), CurColor);
		ShellMID->SetVectorParameterValue(TEXT("RimColor"), Rim);
		ShellMID->SetScalarParameterValue(TEXT("Clarity"), CurClarity);
		ShellMID->SetScalarParameterValue(TEXT("Heat"), CurHeat);
		ShellMID->SetScalarParameterValue(TEXT("Glow"), CurGlow);
		ShellMID->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);
	}
	if (CoreMID)
	{
		CoreMID->SetVectorParameterValue(TEXT("Color"), CurColor * FLinearColor(0.5f, 0.55f, 0.5f));
		CoreMID->SetScalarParameterValue(TEXT("Heat"), CurHeat);
		CoreMID->SetScalarParameterValue(TEXT("Glow"), CurGlow);
		CoreMID->SetScalarParameterValue(TEXT("EmissiveScale"), EmissiveScale);
	}
}
