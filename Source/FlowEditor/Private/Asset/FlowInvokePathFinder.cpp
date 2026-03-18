// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Asset/FlowInvokePathFinder.h"

#include "FlowAsset.h"
#include "Nodes/FlowNode.h"
#include "Nodes/Route/FlowNode_Start.h"
#include "Nodes/Route/FlowNode_SubGraph.h"

FFlowInvokePath FFlowInvokePathFinder::FindPath(UFlowAsset* RootTemplateAsset, UFlowNode* TargetNode)
{
	FFlowInvokePath Result;

	if (!RootTemplateAsset || !TargetNode || !TargetNode->GetGuid().IsValid())
	{
		return Result;
	}

	Result.TargetNodeGuid = TargetNode->GetGuid();

	UFlowAsset* TargetAsset = TargetNode->GetFlowAsset();

	if (TargetAsset == nullptr || TargetAsset == RootTemplateAsset)
	{
		// Single-segment path — target lives in the root asset
		FFlowInvokePathSegment Segment = BuildSegment(RootTemplateAsset, TargetNode->GetGuid(), FGuid());
		if (!Segment.SegmentTargetGuid.IsValid())
		{
			Result.TargetNodeGuid = FGuid();
			return Result;
		}
		Result.Segments.Add(Segment);
	}
	else
	{
		// Multi-segment path — target lives inside a nested subgraph
		TArray<UFlowNode_SubGraph*> Chain;
		if (!FindSubGraphChain(RootTemplateAsset, TargetAsset, Chain))
		{
			Result.TargetNodeGuid = FGuid();
			return Result;
		}

		// Root segment: root asset → Chain[0] (the first SubGraph boundary node)
		{
			const FGuid BoundaryGuid = Chain[0]->GetGuid();
			FFlowInvokePathSegment Seg = BuildSegment(RootTemplateAsset, BoundaryGuid, FGuid());
			if (!Seg.SegmentTargetGuid.IsValid())
			{
				Result.TargetNodeGuid = FGuid();
				return Result;
			}
			Result.Segments.Add(Seg);
		}

		// Intermediate segments (when chain depth > 1)
		for (int32 i = 0; i < Chain.Num() - 1; i++)
		{
			UFlowAsset* SegAsset = Cast<UFlowAsset>(Chain[i]->GetAssetToEdit());
			if (!SegAsset)
			{
				Result.TargetNodeGuid = FGuid();
				return Result;
			}

			const FGuid BoundaryGuid = Chain[i + 1]->GetGuid();
			const FGuid ParentSubGraphNodeGuid = Chain[i]->GetGuid();

			FFlowInvokePathSegment Seg = BuildSegment(SegAsset, BoundaryGuid, ParentSubGraphNodeGuid);
			if (!Seg.SegmentTargetGuid.IsValid())
			{
				Result.TargetNodeGuid = FGuid();
				return Result;
			}
			Result.Segments.Add(Seg);
		}

		// Final segment: TargetAsset → TargetNode
		{
			const FGuid ParentSubGraphNodeGuid = Chain.Last()->GetGuid();
			FFlowInvokePathSegment Seg = BuildSegment(TargetAsset, TargetNode->GetGuid(), ParentSubGraphNodeGuid);
			if (!Seg.SegmentTargetGuid.IsValid())
			{
				Result.TargetNodeGuid = FGuid();
				return Result;
			}
			Result.Segments.Add(Seg);
		}
	}

	if (Result.AllChoicesResolved())
	{
		BuildOrderedPath(Result);
	}

	return Result;
}

void FFlowInvokePathFinder::BuildOrderedPath(FFlowInvokePath& InOutPath)
{
	if (!InOutPath.HasTarget() || !InOutPath.AllChoicesResolved())
	{
		return;
	}

	for (FFlowInvokePathSegment& Segment : InOutPath.Segments)
	{
		if (Segment.OrderedNodes.Num() == 0)
		{
			BuildOrderedSegment(Segment);
		}
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

bool FFlowInvokePathFinder::FindSubGraphChain(UFlowAsset* CurrentAsset, UFlowAsset* TargetAsset, TArray<UFlowNode_SubGraph*>& OutChain)
{
	if (!CurrentAsset || !TargetAsset) return false;

	for (const TPair<FGuid, UFlowNode*>& NodePair : CurrentAsset->GetNodes())
	{
		UFlowNode_SubGraph* SubGraphNode = Cast<UFlowNode_SubGraph>(NodePair.Value);
		if (!SubGraphNode) continue;

		UFlowAsset* ChildAsset = Cast<UFlowAsset>(SubGraphNode->GetAssetToEdit());
		if (!ChildAsset) continue;

		if (ChildAsset == TargetAsset)
		{
			OutChain.Add(SubGraphNode);
			return true;
		}

		TArray<UFlowNode_SubGraph*> ChildChain;
		if (FindSubGraphChain(ChildAsset, TargetAsset, ChildChain))
		{
			OutChain.Add(SubGraphNode);
			OutChain.Append(ChildChain);
			return true;
		}
	}

	return false;
}

FFlowInvokePathSegment FFlowInvokePathFinder::BuildSegment(UFlowAsset* TemplateAsset, const FGuid& TargetGuid, const FGuid& ParentSubGraphNodeGuid)
{
	FFlowInvokePathSegment Segment;
	Segment.TemplateAsset = TemplateAsset;
	Segment.ParentSubGraphNodeGuid = ParentSubGraphNodeGuid;

	if (!TemplateAsset || !TargetGuid.IsValid())
	{
		// Leaving SegmentTargetGuid invalid signals failure to the caller
		return Segment;
	}

	const FGuid StartGuid = FindStartNodeGuid(TemplateAsset);
	if (!StartGuid.IsValid())
	{
		return Segment;
	}

	Segment.SegmentTargetGuid = TargetGuid;

	// Trivial case: target IS the start node
	if (TargetGuid == StartGuid)
	{
		Segment.OrderedNodes.Add(StartGuid);
		return Segment;
	}

	const TMap<FGuid, TArray<FGuid>> ReverseMap = BuildReverseMap(TemplateAsset);

	// BFS backwards from Target, collecting all ancestors and branch points
	TSet<FGuid> AncestorSet;
	TArray<FGuid> BFSQueue;

	BFSQueue.Add(TargetGuid);
	AncestorSet.Add(TargetGuid);

	while (BFSQueue.Num() > 0)
	{
		const FGuid Current = BFSQueue[0];
		BFSQueue.RemoveAt(0);

		const TArray<FGuid>* Predecessors = ReverseMap.Find(Current);
		if (!Predecessors) continue;

		if (Predecessors->Num() > 1)
		{
			FFlowBranchChoice Choice;
			Choice.NodeGuid = Current;
			Choice.AvailablePredecessors = *Predecessors;
			Segment.PendingChoices.Add(Choice);
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

	// If Start is unreachable from Target, there's no valid path
	if (!AncestorSet.Contains(StartGuid))
	{
		Segment.SegmentTargetGuid = FGuid();
		return Segment;
	}

	// Build ordered nodes immediately if no choices are needed
	if (Segment.AllChoicesResolved())
	{
		BuildOrderedSegment(Segment);
	}

	return Segment;
}

void FFlowInvokePathFinder::BuildOrderedSegment(FFlowInvokePathSegment& Segment)
{
	if (!Segment.TemplateAsset || !Segment.SegmentTargetGuid.IsValid() || !Segment.AllChoicesResolved())
	{
		return;
	}

	Segment.OrderedNodes.Empty();

	const TMap<FGuid, TArray<FGuid>> ReverseMap = BuildReverseMap(Segment.TemplateAsset);
	const FGuid StartGuid = FindStartNodeGuid(Segment.TemplateAsset);
	if (!StartGuid.IsValid()) return;

	// Build a map of chosen predecessors: node → which predecessor to use
	TMap<FGuid, FGuid> ChosenPredecessorMap;
	for (const TPair<FGuid, TArray<FGuid>>& Entry : ReverseMap)
	{
		if (Entry.Value.Num() == 1)
		{
			ChosenPredecessorMap.Add(Entry.Key, Entry.Value[0]);
		}
	}
	for (const FFlowBranchChoice& Choice : Segment.PendingChoices)
	{
		if (Choice.IsResolved())
		{
			ChosenPredecessorMap.Add(Choice.NodeGuid, Choice.GetSelectedPredecessor());
		}
	}

	// Walk backwards from SegmentTargetGuid to Start following chosen predecessors
	TArray<FGuid> ReversePath;
	FGuid Current = Segment.SegmentTargetGuid;

	constexpr int32 MaxIterations = 1024;
	int32 Iterations = 0;

	while (Current.IsValid() && Iterations++ < MaxIterations)
	{
		ReversePath.Add(Current);
		if (Current == StartGuid) break;

		const FGuid* Chosen = ChosenPredecessorMap.Find(Current);
		Current = Chosen ? *Chosen : FGuid();
	}

	// Reverse to get Start → Target order
	for (int32 i = ReversePath.Num() - 1; i >= 0; i--)
	{
		Segment.OrderedNodes.Add(ReversePath[i]);
	}
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

FGuid FFlowInvokePathFinder::FindStartNodeGuid(UFlowAsset* TemplateAsset)
{
	if (!TemplateAsset) return FGuid();

	for (const TPair<FGuid, UFlowNode*>& NodePair : TemplateAsset->GetNodes())
	{
		if (NodePair.Value && NodePair.Value->IsA<UFlowNode_Start>())
		{
			return NodePair.Key;
		}
	}

	return FGuid();
}