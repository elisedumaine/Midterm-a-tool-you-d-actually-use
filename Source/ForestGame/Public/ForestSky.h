#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestSky.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class UAudioComponent;
class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class USoundBase;
class UStaticMeshComponent;

/** How strongly each ingredient of the weather is present, 0..1. */
USTRUCT(BlueprintType)
struct FForestWeatherMix
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Clouds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Rain = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Snow = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Mist = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Wind = 0.2f;
	/** Rain falling while the sun keeps shining (a sun shower): the light is not dimmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather") float Sunny = 0.0f;
};

/**
 * The living sky of the generated world.  In Play, time passes (a whole day
 * in DayMinutes real minutes): the sun crosses the sky, night falls with a
 * moon and stars, and the weather changes by itself (clear, cloudy, drizzle,
 * rain, storm, sun shower, mist, snow in winter), always blending softly.
 *
 * It drives what the World Builder already made: the sun, the height fog, the
 * rain and snow actors, and the weather parameter collection every material
 * reads (Snow, Wet, plus Night and Clouds for the stars, Wind for the swaying
 * plants).  It adds its own touches: leaves blown by the wind, a rainbow in
 * sun showers, lightning and thunder in storms, mist pooling in the valleys at
 * dawn, and the sound of rain and wind.  The World Builder spawns it and fills
 * these references (lowpoly_climate.py).
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestSky : public AActor
{
	GENERATED_BODY()

public:
	AForestSky();

	virtual void Tick(float DeltaSeconds) override;

	// --- What the World Builder chose -------------------------------------
	/** Hour of the day when Play starts (0..24). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky", meta = (ClampMin = "0", ClampMax = "24"))
	float StartHour = 17.0f;

	/** Real minutes for one whole day in Play.  0 stops time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky", meta = (ClampMin = "0"))
	float DayMinutes = 12.0f;

	/** Weather when Play starts: Clear, Cloudy, Drizzle, Rain, Storm, SunShower, Mist, Snow, LightSnow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	FName StartWeather = TEXT("Clear");

	/** The weather changes by itself in Play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	bool bLivingWeather = true;

	/** Winter: snow falls where rain would. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	bool bWinter = false;

	/** Compass direction the sun rises from, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	float SunriseYaw = -110.0f;

	/** How high the sun climbs at noon, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	float NoonElevation = 58.0f;

	// --- What it drives -----------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Scene")
	TObjectPtr<ADirectionalLight> Sun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Scene")
	TObjectPtr<AExponentialHeightFog> Fog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Scene")
	TObjectPtr<AActor> RainActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Scene")
	TObjectPtr<AActor> SnowActor;

	/** MPC_IWB_Weather: Snow, Wet, Night, Clouds, Wind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Scene")
	TObjectPtr<UMaterialParameterCollection> WeatherParameters;

	/** The moon's light; with the sky atmosphere it also draws the moon's disc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** A dome of small stars that follows the camera (mesh and material set by the World Builder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky")
	TObjectPtr<UStaticMeshComponent> Stars;

	// --- Wind ---------------------------------------------------------------
	/** Where the wind blows to.  The plants' material leans the same way (M_IWB_Tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Wind")
	FVector WindDirection = FVector(0.8f, 0.6f, 0.0f);

	/** Leaves carried by the wind around the player; more of them when it blows harder. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Wind")
	TObjectPtr<UInstancedStaticMeshComponent> Leaves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Wind")
	int32 LeafCount = 160;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Wind")
	float LeafRadius = 2600.0f;

	/** Actors with these tags sway in the wind (their materials get Sway = the value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Wind")
	TMap<FName, float> SwayingLayers;

	// --- Rainbow, storm, mist ----------------------------------------------
	/** A ring of colours around the point opposite the sun, seen in sun showers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Rainbow")
	TObjectPtr<UStaticMeshComponent> Rainbow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Rainbow")
	float RainbowDistance = 12000.0f;

	/** The lightning bolt, shown for a blink at each strike. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Storm")
	TObjectPtr<UStaticMeshComponent> Bolt;

	/** The flash that lights up the whole forest at each strike. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Storm")
	TObjectPtr<UDirectionalLightComponent> Flash;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Storm")
	TObjectPtr<USoundBase> ThunderSound;

	/** Height of the valley floors: dawn mist pools just above it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky|Mist")
	float ValleyHeight = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Sound")
	TObjectPtr<UAudioComponent> RainSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sky|Sound")
	TObjectPtr<UAudioComponent> WindSound;

	// --- Read by the HUD ------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Sky")
	float GetHour() const { return Hour; }

	UFUNCTION(BlueprintPure, Category = "Sky")
	int32 GetDay() const { return Day; }

	UFUNCTION(BlueprintPure, Category = "Sky")
	FName GetWeather() const { return Current; }

	/** "Day 2 · 06:40 · Mist" */
	UFUNCTION(BlueprintPure, Category = "Sky")
	FString Describe() const;

	static AForestSky* Get(const UObject* WorldContext);

	/** Jump straight to an hour and a weather (the wake-up at dawn).  bAfterRain leaves everything wet. */
	UFUNCTION(BlueprintCallable, Category = "Sky")
	void SetMoment(float AtHour, FName Weather, bool bAfterRain);

protected:
	virtual void BeginPlay() override;

private:
	static FForestWeatherMix MixFor(FName Weather);
	FName PickNext() const;
	float HoursToSeconds(float Hours) const;
	void ApplySun(float Elevation);
	void ApplyWeather(float DeltaSeconds, float Night);
	void SetParameter(FName Name, float Value);
	FVector Viewpoint() const;
	float GroundHeight(float X, float Y) const;
	void StartSway();
	void UpdateLeaves(float DeltaSeconds, const FVector& View);
	void UpdateRainbow(const FVector& View, float Elevation);
	void UpdateStorm(float DeltaSeconds, const FVector& View);
	void Strike(const FVector& View);
	void UpdateMist();
	void UpdateSounds();

	float Hour = 17.0f;
	int32 Day = 1;
	FName Current = TEXT("Clear");
	FForestWeatherMix Mix;
	FForestWeatherMix Target;
	float TimeToChange = 0.0f;
	float WetSurfaces = 0.0f;
	float SnowCover = 0.0f;

	// What the scene looked like before the sky took over.
	float SunIntensity = 8.0f;
	FLinearColor SunColor = FLinearColor::White;
	float FogDensity = 0.02f;
	FLinearColor FogColor = FLinearColor(0.4f, 0.4f, 0.45f);
	float RainOpacity = 0.2f;
	float SnowOpacity = 0.9f;
	float ValleyFogDensity = 0.0f;
	float Time = 0.0f;

	struct FLeaf
	{
		FVector Position;
		FVector Spin;
		float Phase;
		float Floor;
		float Size;
	};
	TArray<FLeaf> LeafState;
	FLeaf LeafSpawn(const FVector& Around, bool bAnywhere) const;

	// The storm: time to the next strike, the flash's age, thunder on its way.
	float NextStrike = 4.0f;
	float FlashAge = 10.0f;
	float ThunderDelay = -1.0f;
	float ThunderVolume = 1.0f;
	float FlashIntensity = 6.0f;
	float FlashGlow = 0.0f;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RainMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SnowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RainbowMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BoltMaterial;
};
