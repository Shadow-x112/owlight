#include "SlimeComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Owlight.h"
#include "owlight/owlight_c.h"

namespace
{
	FString Utf8(const char* S) { return S ? UTF8_TO_TCHAR(S) : FString(); }
	FName NameOf(const char* S) { return S ? FName(UTF8_TO_TCHAR(S)) : NAME_None; }
	enum owl_property ToC(ESlimeProperty P) { return static_cast<enum owl_property>(static_cast<uint8>(P)); }
}

USlimeComponent::USlimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;   // the sim moves in hours; four times a second is plenty
}

void USlimeComponent::BeginPlay()
{
	Super::BeginPlay();
	Catalog = owl_catalog_builtin();
	if (!Slime)
	{
		Slime = owl_slime_new(Catalog, TCHAR_TO_UTF8(*SlimeName), static_cast<unsigned>(Seed));
	}
	LastType = GetTypeId();
	UE_LOG(LogOwlight, Log, TEXT("Slime '%s' (seed %d) alive, sim %s"), *SlimeName, Seed, UTF8_TO_TCHAR(owl_version()));
}

void USlimeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Slime) { owl_slime_free(Slime); Slime = nullptr; }
	Super::EndPlay(Reason);
}

void USlimeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Slime) return;
	PendingHours += static_cast<double>(DeltaTime) * TimeScale / 3600.0;
	const double StepLimit = FMath::Max(1e-4f, MaxStepHours);
	// Integrate in bounded steps so a long frame or a big TimeScale still passes through the
	// catalog the same way (the core sub-steps too; this keeps the context fresh).
	while (PendingHours >= StepLimit)
	{
		Step(StepLimit);
		PendingHours -= StepLimit;
	}
}

void USlimeComponent::Step(double Hours)
{
	if (!Slime || Hours <= 0.0) return;
	const FString Habitat = HabitatId.ToString();
	const FString Job = JobId.IsNone() ? FString() : JobId.ToString();
	const int Err = owl_slime_tick(Slime, Hours, TCHAR_TO_UTF8(*Habitat), Job.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*Job),
	                               IsNearPlayer() ? 1 : 0, IsDaytime() ? 1 : 0);
	if (Err != 0)
	{
		UE_LOG(LogOwlight, Warning, TEXT("Slime '%s': tick failed (%d) habitat=%s job=%s"), *SlimeName, Err, *Habitat, *Job);
		return;
	}
	const FName Now = GetTypeId();
	if (Now != LastType)
	{
		UE_LOG(LogOwlight, Log, TEXT("Slime '%s' became %s (was %s)"), *SlimeName, *Now.ToString(), *LastType.ToString());
		OnTypeChanged.Broadcast(LastType, Now);
		LastType = Now;
	}
}

void USlimeComponent::Advance(float Hours)
{
	if (Hours <= 0.f) return;
	Step(Hours);   // the core integrates safely over any span
}

bool USlimeComponent::Feed(FName FoodId)
{
	if (!Slime) return false;
	const int Err = owl_slime_feed(Slime, TCHAR_TO_UTF8(*FoodId.ToString()));
	if (Err != 0) UE_LOG(LogOwlight, Warning, TEXT("Slime '%s': unknown food %s"), *SlimeName, *FoodId.ToString());
	return Err == 0;
}

void USlimeComponent::PickUp()
{
	if (Slime) owl_slime_pick_up(Slime);
}

void USlimeComponent::GiveName(const FString& NewName)
{
	SlimeName = NewName;
	if (Slime) owl_slime_give_name(Slime, TCHAR_TO_UTF8(*NewName));
}

FName USlimeComponent::GetTypeId() const { return Slime ? NameOf(owl_slime_type(Slime)) : NAME_None; }
bool USlimeComponent::IsLocked() const { return Slime && owl_slime_locked(Slime) != 0; }
float USlimeComponent::GetAgeHours() const { return Slime ? static_cast<float>(owl_slime_age_hours(Slime)) : 0.f; }

FSlimeLook USlimeComponent::GetLook() const
{
	FSlimeLook Look;
	if (!Slime) return Look;
	Look.TypeId = GetTypeId();
	Look.bLocked = IsLocked();
	const int N = owl_slime_sign_count(Slime);
	for (int i = 0; i < N; ++i)
	{
		Look.Signs.Add(FText::FromString(Utf8(owl_slime_sign(Slime, i))));
	}
	return Look;
}

TArray<FString> USlimeComponent::GetJournal() const
{
	TArray<FString> Out;
	if (!Slime) return Out;
	const int N = owl_slime_journal_count(Slime);
	for (int i = 0; i < N; ++i)
	{
		double At = 0.0; const char* Kind = nullptr; const char* Text = nullptr;
		if (owl_slime_journal_entry(Slime, i, &At, &Kind, &Text) == 0)
		{
			Out.Add(FString::Printf(TEXT("%.1fh %s: %s"), At, *Utf8(Kind), *Utf8(Text)));
		}
	}
	return Out;
}

float USlimeComponent::GetHiddenValue(ESlimeProperty Property) const
{
	return Slime ? static_cast<float>(owl_slime_value(Slime, ToC(Property))) : 0.5f;
}

FString USlimeComponent::SaveToJson() const
{
	if (!Slime) return FString();
	const size_t Len = owl_slime_save(Slime, nullptr, 0);
	TArray<char> Buf; Buf.SetNumUninitialized(Len + 1);
	owl_slime_save(Slime, Buf.GetData(), Buf.Num());
	return UTF8_TO_TCHAR(Buf.GetData());
}

bool USlimeComponent::LoadFromJson(const FString& Json)
{
	if (!Catalog) Catalog = owl_catalog_builtin();
	char Err[256] = {0};
	owl_slime* Loaded = owl_slime_load(Catalog, TCHAR_TO_UTF8(*Json), Err, sizeof(Err));
	if (!Loaded)
	{
		UE_LOG(LogOwlight, Warning, TEXT("Slime load failed: %s"), UTF8_TO_TCHAR(Err));
		return false;
	}
	if (Slime) owl_slime_free(Slime);
	Slime = Loaded;
	SlimeName = Utf8(owl_slime_name(Slime));
	LastType = GetTypeId();
	return true;
}

bool USlimeComponent::IsDaytime_Implementation() const
{
	const FDateTime Now = FDateTime::Now();
	return Now.GetHour() >= 6 && Now.GetHour() < 20;
}

bool USlimeComponent::IsNearPlayer() const
{
	const AActor* Owner = GetOwner();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Owner || !Player) return false;
	return FVector::DistSquared(Owner->GetActorLocation(), Player->GetActorLocation()) < FMath::Square(NearPlayerDistance);
}
