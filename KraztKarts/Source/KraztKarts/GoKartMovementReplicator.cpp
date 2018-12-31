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
	if (Owner)
	{
		if (Owner->Role == ROLE_AutonomousProxy && MovementComponent)
		{
			FGoKartMove Move = MovementComponent->CreateMove(DeltaTime);
			MovementComponent->SimulateMove(Move);

			UnacknowledgedMoves.Add(Move);
			Server_SendMove(Move);
		}

		// We are the server and in control of the pawn
		if (Owner->Role == ROLE_Authority && Owner->GetRemoteRole() == ROLE_SimulatedProxy && MovementComponent)
		{
			FGoKartMove Move = MovementComponent->CreateMove(DeltaTime);
			Server_SendMove(Move);
		}

		if (Owner->Role == ROLE_SimulatedProxy && MovementComponent)
		{
			MovementComponent->SimulateMove(ServerState.LastMove);
		}
	}
}

void UGoKartMovementReplicator::Server_SendMove_Implementation(const FGoKartMove& Move)
{
	AActor* Owner = GetOwner();
	if (Owner && MovementComponent)
	{
		MovementComponent->SimulateMove(Move);

		ServerState.LastMove = Move;
		ServerState.Transform = Owner->GetActorTransform();
		ServerState.Velocity = MovementComponent->GetVelocity();
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