// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Asset/FlowInvokePathFinder.h"

#include "FlowAsset.h"
#include "Nodes/FlowNode.h"
#include "Nodes/Route/FlowNode_Start.h"

FFlowInvokePath FFlowInvokePathFinder::FindPath(UFlowAsset* TemplateAsset, const FGuid& TargetNodeGuid)
{
	FFlowInvokePath Result;

	if (!TemplateAsset || !TargetNodeGuid.IsValid())
	{
		return Result;
	}

	Result.TargetNodeGuid = TargetNodeGuid;

	// Find the Start node GUID
	FGuid StartNodeGuid;
	for (const TPair<FGuid, UFlowNode*>& NodePair : TemplateAsset->GetNodes())
	{
		if (NodePair.Value && NodePair.Value->IsA<UFlowNode_Start>())
		{
			StartNodeGuid = NodePair.Key;
			break;
		}
	}

	if (!StartNodeGuid.IsValid())
	{
		return Result;
	}

	// Trivial case: target IS the start node
	if (TargetNodeGuid == StartNodeGuid)
	{
		Result.OrderedNodes.Add(StartNodeGuid);
		return Result;
	}

	// Build reverse connection map: node GUID -> list of predecessor GUIDs
	const TMap<FGuid, TArray<FGuid>> ReverseMap = BuildReverseMap(TemplateAsset);

	// BFS backwards from Target, collecting all ancestors and branch points.
	// We walk ALL predecessors (not just one) to find the full ancestor set.
	TSet<FGuid> AncestorSet;
	TArray<FGuid> BFSQueue;

	BFSQueue.Add(TargetNodeGuid);
	AncestorSet.Add(TargetNodeGuid);

	while (BFSQueue.Num() > 0)
	{
		const FGuid Current = BFSQueue[0];
		BFSQueue.RemoveAt(0);

		const TArray<FGuid>* Predecessors = ReverseMap.Find(Current);
		if (!Predecessors) continue;

		if (Predecessors->Num() > 1)
		{
			// Multiple predecessors — record as a branch choice
			FFlowBranchChoice Choice;
			Choice.NodeGuid = Current;
			Choice.AvailablePredecessors = *Predecessors;
			Choice.SelectedIndex = -1;
			Result.PendingChoices.Add(Choice);
		}

		for (const FGuid& Pred : *Predecessors)
		{
			if (!AncestorSet.Contains(Pred))
			{
				AncestorSet.Add(Pred);
				BFSQueue.Add(Pred);
			}
		}
	}

	// If Start is not in the ancestor set, there's no path to target
	if (!AncestorSet.Contains(StartNodeGuid))
	{
		Result.TargetNodeGuid = FGuid(); // invalidate
		return Result;
	}

	// If no pending choices, build the ordered path immediately
	if (Result.PendingChoices.Num() == 0)
	{
		BuildOrderedPath(Result, TemplateAsset);
	}

	return Result;
}

void FFlowInvokePathFinder::BuildOrderedPath(FFlowInvokePath& InOutPath, UFlowAsset* TemplateAsset)
{
	if (!TemplateAsset || !InOutPath.HasTarget() || !InOutPath.AllChoicesResolved())
	{
		return;
	}

	InOutPath.OrderedNodes.Empty();

	// Build a map of chosen predecessors: node GUID -> which predecessor to use
	TMap<FGuid, FGuid> ChosenPredecessorMap;
	const TMap<FGuid, TArray<FGuid>> ReverseMap = BuildReverseMap(TemplateAsset);

	// Fill in single-predecessor entries and resolved choices
	for (const TPair<FGuid, TArray<FGuid>>& Entry : ReverseMap)
	{
		if (Entry.Value.Num() == 1)
		{
			ChosenPredecessorMap.Add(Entry.Key, Entry.Value[0]);
		}
	}
	for (const FFlowBranchChoice& Choice : InOutPath.PendingChoices)
	{
		if (Choice.IsResolved())
		{
			ChosenPredecessorMap.Add(Choice.NodeGuid, Choice.GetSelectedPredecessor());
		}
	}

	// Find Start node
	FGuid StartNodeGuid;
	for (const TPair<FGuid, UFlowNode*>& NodePair : TemplateAsset->GetNodes())
	{
		if (NodePair.Value && NodePair.Value->IsA<UFlowNode_Start>())
		{
			StartNodeGuid = NodePair.Key;
			break;
		}
	}

	if (!StartNodeGuid.IsValid()) return;

	// Walk backwards from Target to Start using the chosen predecessors
	TArray<FGuid> ReversePath;
	FGuid Current = InOutPath.TargetNodeGuid;

	constexpr int32 MaxIterations = 1024; // safety guard against cycles
	int32 Iterations = 0;

	while (Current.IsValid() && Iterations++ < MaxIterations)
	{
		ReversePath.Add(Current);

		if (Current == StartNodeGuid)
		{
			break;
		}

		const FGuid* Chosen = ChosenPredecessorMap.Find(Current);
		Current = Chosen ? *Chosen : FGuid();
	}

	// Reverse to get Start -> Target order
	for (int32 i = ReversePath.Num() - 1; i >= 0; i--)
	{
		InOutPath.OrderedNodes.Add(ReversePath[i]);
	}
}

FString FFlowInvokePathFinder::GetNodeDisplayName(UFlowAsset* TemplateAsset, const FGuid& NodeGuid)
{
	if (!TemplateAsset) return NodeGuid.ToString();

	if (const UFlowNode* Node = TemplateAsset->GetNode(NodeGuid))
	{
#if WITH_EDITOR
		return Node->GetNodeTitle().ToString();
#else
		return Node->GetClass()->GetName();
#endif
	}

	return NodeGuid.ToString();
}

TMap<FGuid, TArray<FGuid>> FFlowInvokePathFinder::BuildReverseMap(UFlowAsset* TemplateAsset)
{
	TMap<FGuid, TArray<FGuid>> ReverseMap;

	if (!TemplateAsset) return ReverseMap;

	for (const TPair<FGuid, UFlowNode*>& NodePair : TemplateAsset->GetNodes())
	{
		const UFlowNode* Node = NodePair.Value;
		if (!Node) continue;

		for (UFlowNode* ConnectedNode : Node->GetConnectedNodes())
		{
			if (ConnectedNode)
			{
				ReverseMap.FindOrAdd(ConnectedNode->GetGuid()).AddUnique(NodePair.Key);
			}
		}
	}

	return ReverseMap;
}