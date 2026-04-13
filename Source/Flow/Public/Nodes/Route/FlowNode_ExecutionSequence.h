// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_ExecutionSequence.generated.h"

/**
 * Executes all outputs sequentially.
 * When bSavePinExecutionState is enabled, tracks which outputs have already fired so that
 * a checkpoint save mid-sequence can resume from the correct output on restore.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Sequence"))
class FLOW_API UFlowNode_ExecutionSequence final : public UFlowNode
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual bool CanUserAddOutput() const override { return true; }
#endif

protected:
	// If true, tracks which outputs have been fired. Allows correct restore when a checkpoint
	// is saved while this node is mid-sequence (i.e. some outputs fired, some have not).
	UPROPERTY(EditAnywhere, Category = "Sequence")
	bool bSavePinExecutionState;

	// GUIDs of connected nodes whose output pin has already been triggered this activation.
	UPROPERTY(SaveGame)
	TSet<FGuid> ExecutedConnections;

	virtual void ExecuteInput(const FName& PinName) override;
	virtual void OnLoad_Implementation() override;
	virtual void Cleanup() override;

	// Fires only the outputs whose connected node has not yet been recorded in ExecutedConnections.
	void ExecuteNewConnections();
};
