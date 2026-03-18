// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "Nodes/FlowNode.h"
#include "FlowNode_InvokePoint.generated.h"

/**
 * A named waypoint for the Invoke Tool. Place these in the graph to create fast-travel
 * destinations that appear by name in the Invoke Tool tab.
 * At runtime this node is a pure pass-through with no side effects.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Invoke Point"))
class FLOW_API UFlowNode_InvokePoint : public UFlowNode
{
	GENERATED_UCLASS_BODY()

public:
	/** Name shown in the Invoke Tool tab. Should be human-readable, e.g. "After bedroom intro". */
	UPROPERTY(EditAnywhere, Category = "Invoke")
	FString InvokeName;

protected:
	virtual void ExecuteInput(const FName& PinName) override;

#if WITH_EDITOR
	virtual FString GetNodeDescription() const override;
#endif
};
