// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"

class UFlowAsset;
class UFlowNode;

/**
 * Represents a branch point where multiple predecessor nodes exist for a given node.
 * The user must choose which predecessor path to follow when invoking.
 */
struct FLOWEDITOR_API FFlowBranchChoice
{
	/** The node that has multiple predecessors — we need to know which one to approach from */
	FGuid NodeGuid;

	/** All available predecessor node GUIDs that can lead to NodeGuid */
	TArray<FGuid> AvailablePredecessors;

	/** Index into AvailablePredecessors of the user's selection. -1 = unresolved. */
	int32 SelectedIndex = -1;

	bool IsResolved() const { return SelectedIndex >= 0 && SelectedIndex < AvailablePredecessors.Num(); }
	FGuid GetSelectedPredecessor() const { return IsResolved() ? AvailablePredecessors[SelectedIndex] : FGuid(); }
};

/**
 * The result of path finding from the Start node to a target node.
 * May contain PendingChoices if branching is encountered.
 */
struct FLOWEDITOR_API FFlowInvokePath
{
	/** Ordered node GUIDs from Start to Target (inclusive). Populated after all choices are resolved. */
	TArray<FGuid> OrderedNodes;

	/** Branch choices that require user resolution before the path can be built. */
	TArray<FFlowBranchChoice> PendingChoices;

	/** The target node this path leads to. */
	FGuid TargetNodeGuid;

	bool HasTarget() const { return TargetNodeGuid.IsValid(); }

	bool AllChoicesResolved() const
	{
		for (const FFlowBranchChoice& Choice : PendingChoices)
		{
			if (!Choice.IsResolved()) return false;
		}
		return true;
	}

	bool IsComplete() const { return HasTarget() && AllChoicesResolved() && OrderedNodes.Num() > 0; }
};

/**
 * Finds execution paths through a FlowAsset graph from the Start node to a target node.
 * Used by the Invoke Tool to determine which nodes to force-complete during invocation.
 */
class FLOWEDITOR_API FFlowInvokePathFinder
{
public:
	/**
	 * Find the path from the Start node to TargetNodeGuid.
	 * Returns a path that may have PendingChoices if branching is encountered.
	 * After resolving all choices, call BuildOrderedPath() to populate OrderedNodes.
	 */
	static FFlowInvokePath FindPath(UFlowAsset* TemplateAsset, const FGuid& TargetNodeGuid);

	/**
	 * After all PendingChoices have been resolved, build the final ordered node sequence.
	 * Populates InOutPath.OrderedNodes. No-op if choices are unresolved.
	 */
	static void BuildOrderedPath(FFlowInvokePath& InOutPath, UFlowAsset* TemplateAsset);

	/** Get a display-friendly name for a node by GUID (for use in branch choice UI). */
	static FString GetNodeDisplayName(UFlowAsset* TemplateAsset, const FGuid& NodeGuid);

private:
	/** Build a map from node GUID to array of predecessor node GUIDs (reverse of Connections). */
	static TMap<FGuid, TArray<FGuid>> BuildReverseMap(UFlowAsset* TemplateAsset);
};