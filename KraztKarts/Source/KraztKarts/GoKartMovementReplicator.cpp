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
			MovementComponent->SetLastMove(ServerState.LastMove);
		}
	}
}

void UGoKartMovementReplicator::UpdateServerState(const FGoKartMove& Move)
{
	AActor* Owner = GetOwner();
	ServerState.LastMove = Move;
	ServerState.Transform = Owner->GetActorTransform();
	ServerState.Velocity = MovementComponent->GetVelocity();
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

void UGoKartMovementReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGoKartMovementReplicator, ServerState);
}