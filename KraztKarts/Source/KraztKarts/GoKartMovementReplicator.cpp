// Fill out your copyright notice in the Description page of Project Settings.

#include "GoKartMovementReplicator.h"
#include "GameFramework/Actor.h"
#include "UnrealNetwork.h"

// Sets default values for this component's properties
UGoKartMovementReplicator::UGoKartMovementReplicator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
	// ...
}

// Called when the game starts
void UGoKartMovementReplicator::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent = GetOwner()->FindComponentByClass<UGoKartMovementComponent>();
}

// Called every frame
void UGoKartMovementReplicator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (Owner && MovementComponent)
	{
		FGoKartMove LastMove = MovementComponent->GetLastMove();
		if (Owner->Role == ROLE_AutonomousProxy)
		{
			UnacknowledgedMoves.Add(LastMove);
			Server_SendMove(LastMove);
		}

		// We are the server and in control of the pawn
		if (Owner->GetRemoteRole() == ROLE_SimulatedProxy)
		{
			UpdateServerState(LastMove);
		}

		if (Owner->Role == ROLE_SimulatedProxy)
		{
			ClientTick(DeltaTime);
		}
	}
}

void UGoKartMovementReplicator::UpdateServerState(const FGoKartMove& Move)
{

	AActor* Owner = GetOwner();
	if (MovementComponent)
	{
		ServerState.LastMove = Move;
		ServerState.Transform = Owner->GetActorTransform();
		ServerState.Velocity = MovementComponent->GetVelocity();
	}
}

void UGoKartMovementReplicator::Server_SendMove_Implementation(const FGoKartMove& Move)
{
	AActor* Owner = GetOwner();
	if (Owner && MovementComponent)
	{
		MovementComponent->SimulateMove(Move);
		UpdateServerState(Move);
	}
}

bool UGoKartMovementReplicator::Server_SendMove_Validate(const FGoKartMove& Move)
{
	return true;
}

void UGoKartMovementReplicator::ClearAcknowledgedMoves(const FGoKartMove& LastMove)
{
	TArray<FGoKartMove> NewMoves;
	for (const FGoKartMove& Move : UnacknowledgedMoves)
	{
		if (Move.Time > LastMove.Time)
		{
			NewMoves.Add(Move);
		}
	}

	UnacknowledgedMoves = NewMoves;
}

void UGoKartMovementReplicator::OnRep_ServerState()
{
	switch (GetOwnerRole())
	{
	case ROLE_AutonomousProxy:
		AutonomousProxy_OnRep_ServerState();
		break;
	case ROLE_SimulatedProxy:
		SimulatedProxy_OnRep_ServerState();
		break;
	default:
		break;
	}
}

void UGoKartMovementReplicator::SimulatedProxy_OnRep_ServerState()
{
	if (MovementComponent)
	{
		CLientTimeBetweenLastUpdate = ClientTimeSinceUpdate;
		ClientTimeSinceUpdate = 0;
		ClientStartTransform = GetOwner()->GetActorTransform();
		ClientStartVelocity = MovementComponent->GetVelocity();
	}
}

void UGoKartMovementReplicator::AutonomousProxy_OnRep_ServerState()
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->SetActorTransform(ServerState.Transform);
		ClearAcknowledgedMoves(ServerState.LastMove);

		if (MovementComponent)
		{
			MovementComponent->SetVelocity(ServerState.Velocity);
			for (const FGoKartMove& Move : UnacknowledgedMoves)
			{
				MovementComponent->SimulateMove(Move);
			}
		}
	}
}

void UGoKartMovementReplicator::ClientTick(float DeltaTime)
{
	ClientTimeSinceUpdate += DeltaTime;
	if ( (CLientTimeBetweenLastUpdate > KINDA_SMALL_NUMBER) && (MovementComponent))
	{
		AActor* Owner = GetOwner();

		FVector TargetLocation = ServerState.Transform.GetLocation();
		float LerpRatio = ClientTimeSinceUpdate / CLientTimeBetweenLastUpdate;
		FVector StartLocation = ClientStartTransform.GetLocation();

		// Velocity is in m/s but location is in cm so we have to multiply by
		// 100 to convert between meters and centimeters
		float VelocityToDerivate = CLientTimeBetweenLastUpdate * 100;
		FVector StartDerivative = ClientStartVelocity * VelocityToDerivate;
		FVector TargetDerivative = ServerState.Velocity * VelocityToDerivate;

		FVector NewLocation = FMath::CubicInterp(
			StartLocation,
			StartDerivative,
			TargetLocation,
			TargetDerivative,
			LerpRatio
		);
		Owner->SetActorLocation(NewLocation);

		FVector NewDerivative = FMath::CubicInterpDerivative(
			TargetLocation,
			StartDerivative,
			TargetLocation,
			TargetDerivative,
			LerpRatio
		);

		FVector NewVelocity = NewDerivative / VelocityToDerivate;
		MovementComponent->SetVelocity(NewVelocity);

		FQuat TargetRotation = ServerState.Transform.GetRotation();
		FQuat StartRotation = ClientStartTransform.GetRotation();
		FQuat NewRotation = FQuat::Slerp(StartRotation, TargetRotation, LerpRatio);
		Owner->SetActorRotation(NewRotation);
	}
}

void UGoKartMovementReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGoKartMovementReplicator, ServerState);
}