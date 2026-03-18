// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Nodes/Route/FlowNode_InvokePoint.h"

UFlowNode_InvokePoint::UFlowNode_InvokePoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	Category = TEXT("Dev");
	NodeStyle = EFlowNodeStyle::Development;
#endif
}

void UFlowNode_InvokePoint::ExecuteInput(const FName& PinName)
{
	TriggerFirstOutput(true);
}

#if WITH_EDITOR
FString UFlowNode_InvokePoint::GetNodeDescription() const
{
	return InvokeName;
}
#endif
