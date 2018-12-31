// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GoKartMovementComponent.h"
#include "GoKartMovementReplicator.generated.h"

USTRUCT()
struct FGoKartState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FVector Velocity;

	UPROPERTY()
	FGoKartMove LastMove;
};

struct FHermiteCubicSpline
{
	FVector StartLocation;
	FVector StartDerivative;
	FVector TargetLocation;
	FVector TargetDerivative;

	FVector InterpSpline(float LerpRatio) const;
	FVector InterpDerivative(float LerpRatio) const;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class KRAZTKARTS_API UGoKartMovementReplicator : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY(ReplicatedUsing = OnRep_ServerState)
	FGoKartState ServerState;

	TArray<FGoKartMove> UnacknowledgedMoves;

	UPROPERTY()
	UGoKartMovementComponent* MovementComponent;

	UPROPERTY()
	USceneComponent* MeshOffsetRoot;

	float ClientSimulatedTime = 0.f;

	float ClientTimeSinceUpdate = 0.f;

	float CLientTimeBetweenLastUpdate = 0.f;

	FTransform ClientStartTransform;

	FVector ClientStartVelocity;

public:
	// Sets default values for this component's properties
	UGoKartMovementReplicator();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void SetMeshOffsetRoot(USceneComponent* Root);

private:
	void ClearAcknowledgedMoves(const FGoKartMove& LastMove);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendMove(const FGoKartMove& Move);

	UFUNCTION()
	void OnRep_ServerState();

	void SimulatedProxy_OnRep_ServerState();

	void AutonomousProxy_OnRep_ServerState();

	void ClientTick(float DeltaTime);

	void InterpolateLocation(const FHermiteCubicSpline &Spline, float LerpRatio);

	void InterpolateVelocity(const FHermiteCubicSpline &Spline, float LerpRatio);

	void InterpolateRotation(float LerpRatio);

	FHermiteCubicSpline CreateSpline();

	float GetVelocityToDerivative();

	void UpdateServerState(const FGoKartMove& Move);
};
