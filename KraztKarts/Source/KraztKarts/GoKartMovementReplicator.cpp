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
	AActor* Owner = GetOwner();
	MovementComponent = Owner->FindComponentByClass<UGoKartMovementComponent>();
}

void UGoKartMovementReplicator::SetMeshOffsetRoot(USceneComponent* Root)
{
	MeshOffsetRoot = Root;
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
		if (MeshOffsetRoot)
		{
			ClientStartTransform.SetLocation(MeshOffsetRoot->GetComponentLocation());
			ClientStartTransform.SetRotation(MeshOffsetRoot->GetComponentQuat());
			ClientStartVelocity = MovementComponent->GetVelocity();

			AActor* Owner = GetOwner();
			Owner->SetActorTransform(ServerState.Transform);
		}
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
		FHermiteCubicSpline Spline = CreateSpline();
		float LerpRatio = ClientTimeSinceUpdate / CLientTimeBetweenLastUpdate;

		InterpolateLocation(Spline, LerpRatio);
		InterpolateVelocity(Spline, LerpRatio);
		InterpolateRotation(LerpRatio);
	}
}

void UGoKartMovementReplicator::InterpolateLocation(const FHermiteCubicSpline &Spline, float LerpRatio)
{
	if (MeshOffsetRoot)
	{
		AActor* Owner = GetOwner();
		FVector NewLocation = Spline.InterpSpline(LerpRatio);
		MeshOffsetRoot->SetWorldLocation(NewLocation);
	}
}

void UGoKartMovementReplicator::InterpolateVelocity(const FHermiteCubicSpline &Spline, float LerpRatio)
{
	FVector NewDerivative = Spline.InterpDerivative(LerpRatio);
	FVector NewVelocity = NewDerivative / GetVelocityToDerivative();
	MovementComponent->SetVelocity(NewVelocity);
}

void UGoKartMovementReplicator::InterpolateRotation(float LerpRatio)
{
	if (MeshOffsetRoot)
	{
		AActor* Owner = GetOwner();
		FQuat TargetRotation = ServerState.Transform.GetRotation();
		FQuat StartRotation = ClientStartTransform.GetRotation();
		FQuat NewRotation = FQuat::Slerp(StartRotation, TargetRotation, LerpRatio);
		MeshOffsetRoot->SetWorldRotation(NewRotation);
	}
}

FHermiteCubicSpline UGoKartMovementReplicator::CreateSpline()
{
	FHermiteCubicSpline Spline;
	Spline.TargetLocation = ServerState.Transform.GetLocation();
	Spline.StartLocation = ClientStartTransform.GetLocation();
	// Velocity is in m/s but location is in cm so we have to multiply by
	// 100 to convert between meters and centimeters
	float VelocityToDerivate = GetVelocityToDerivative();
	Spline.StartDerivative = ClientStartVelocity * VelocityToDerivate;
	Spline.TargetDerivative = ServerState.Velocity * VelocityToDerivate;
	return Spline;
}

float UGoKartMovementReplicator::GetVelocityToDerivative()
{
	return CLientTimeBetweenLastUpdate * 100;
}

void UGoKartMovementReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGoKartMovementReplicator, ServerState);
}

FVector FHermiteCubicSpline::InterpSpline(float LerpRatio) const
{
	return FMath::CubicInterp(
		StartLocation,
		StartDerivative,
		TargetLocation,
		TargetDerivative,
		LerpRatio
	);
}

FVector FHermiteCubicSpline::InterpDerivative(float LerpRatio) const
{
	return FMath::CubicInterpDerivative(
		TargetLocation,
		StartDerivative,
		TargetLocation,
		TargetDerivative,
		LerpRatio
	);
}
