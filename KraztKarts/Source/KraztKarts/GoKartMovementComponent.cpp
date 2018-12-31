// Fill out your copyright notice in the Description page of Project Settings.

#include "GoKartMovementComponent.h"
#include "GoKart.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

// Sets default values for this component's properties
UGoKartMovementComponent::UGoKartMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UGoKartMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

// Called every frame
void UGoKartMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UGoKartMovementComponent::SimulateMove(const FGoKartMove& Move)
{
	FVector Force = GetDrivingForce(Move.Throttle) + GetAirResistance() + GetRollingResistance();

	FVector Acceleration = Force / Mass;

	Velocity = Velocity + Acceleration * Move.DeltaTime;

	AGoKart* Owner = Cast<AGoKart>(GetOwner());
	if (Owner)
	{
		ApplyRotation(Move.DeltaTime, Move.SteeringThrow);
		UpdateLocationFromVelocity(Move.DeltaTime);
	}
}

FGoKartMove UGoKartMovementComponent::CreateMove(float DeltaTime)
{
	FGoKartMove Move;
	Move.DeltaTime = DeltaTime;
	Move.SteeringThrow = SteeringThrow;
	Move.Throttle = Throttle;
	Move.Time = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

	return Move;
}

void UGoKartMovementComponent::ApplyRotation(float DeltaTime, float SteeringThrow)
{
	AActor* Owner = Cast<AActor>(GetOwner());
	if (Owner)
	{
		float DeltaLocation = FVector::DotProduct(Owner->GetActorForwardVector(), Velocity) * DeltaTime;
		float RotationAngle = DeltaLocation / MinTurningRadius * SteeringThrow;
		FQuat RotationDelta(Owner->GetActorUpVector(), RotationAngle);

		Velocity = RotationDelta.RotateVector(Velocity);

		Owner->AddActorWorldRotation(RotationDelta);
	}
}

void UGoKartMovementComponent::UpdateLocationFromVelocity(float DeltaTime)
{
	AActor* Owner = Cast<AActor>(GetOwner());
	if (Owner)
	{
		FVector Translation = Velocity * DeltaTime * 100;

		FHitResult HitResult;
		Owner->AddActorWorldOffset(Translation, true, &HitResult);
		if (HitResult.IsValidBlockingHit())
		{
			Velocity = FVector::ZeroVector;
		}
	}
}

FVector UGoKartMovementComponent::GetAirResistance()
{
	return  Velocity.GetSafeNormal() * -Velocity.SizeSquared() * DragCoefficient;;
}

FVector UGoKartMovementComponent::GetDrivingForce(float Throttle)
{
	return GetOwner()->GetActorForwardVector() * MaxDrivingForce * Throttle;
}

FVector UGoKartMovementComponent::GetRollingResistance()
{
	float NormalForce = GetWorld()->GetGravityZ() / -100;
	return Velocity.GetSafeNormal() * RollingResistanceCoefficient * NormalForce;
}

void UGoKartMovementComponent::SetThrottle(float Val)
{
	Throttle = Val;
}

void UGoKartMovementComponent::SetSteeringThrow(float Val)
{
	SteeringThrow = Val;
}

FVector UGoKartMovementComponent::GetVelocity()
{
	return Velocity;
}

float UGoKartMovementComponent::GetMinTurningRadius()
{
	return MinTurningRadius;
}

void UGoKartMovementComponent::SetVelocity(FVector Velocity)
{
	this->Velocity = Velocity;
}
