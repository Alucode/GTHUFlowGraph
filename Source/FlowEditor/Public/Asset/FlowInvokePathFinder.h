// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"

class UFlowAsset;
class UFlowNode;
class UFlowNode_SubGraph;

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
 * One level of a potentially multi-level invoke path.
 * Represents execution through a single FlowAsset from its Start node to either
 * the final target node (last segment) or a SubGraph boundary node (intermediate segments).
 */
struct FLOWEDITOR_API FFlowInvokePathSegment
{
	/** Template asset for this segment. */
	UFlowAsset* TemplateAsset = nullptr;

	/**
	 * GUID of the SubGraph node in the parent asset that creates this asset's instance.
	 * Invalid for the root segment (index 0).
	 */
	FGuid ParentSubGraphNodeGuid;

	/**
	 * The target within this segment: for intermediate segments, the SubGraph boundary node GUID;
	 * for the last segment, the user's chosen target node GUID.
	 */
	FGuid SegmentTargetGuid;

	/** Ordered node GUIDs from Start to SegmentTargetGuid (inclusive). Populated after choices are resolved. */
	TArray<FGuid> OrderedNodes;

	/** Branch choices requiring user resolution within this segment's asset. */
	TArray<FFlowBranchChoice> PendingChoices;

	bool AllChoicesResolved() const
	{
		for (const FFlowBranchChoice& Choice : PendingChoices)
		{
			if (!Choice.IsResolved()) return false;
		}
		return true;
	}
};

/**
 * The result of path finding from a root asset's Start node to a target node,
 * potentially spanning multiple nested FlowAssets via SubGraph nodes.
 */
struct FLOWEDITOR_API FFlowInvokePath
{
	/**
	 * Path segments ordered from root (index 0) to the asset containing the target (last index).
	 * Single-asset paths have exactly one segment.
	 * Multi-asset paths have one segment per nesting level; intermediate segments end at
	 * the SubGraph boundary node that leads into the next segment.
	 */
	TArray<FFlowInvokePathSegment> Segments;

	/** The target node GUID, located within the last segment's asset. */
	FGuid TargetNodeGuid;

	bool HasTarget() const { return TargetNodeGuid.IsValid() && Segments.Num() > 0; }

	bool AllChoicesResolved() const
	{
		for (const FFlowInvokePathSegment& Segment : Segments)
		{
			if (!Segment.AllChoicesResolved()) return false;
		}
		return true;
	}

	bool IsComplete() const
	{
		if (!HasTarget() || !AllChoicesResolved()) return false;
		for (const FFlowInvokePathSegment& Segment : Segments)
		{
			if (Segment.OrderedNodes.Num() == 0) return false;
		}
		return true;
	}

	bool IsMultiSegment() const { return Segments.Num() > 1; }
};

/**
 * Finds execution paths through FlowAsset graphs from the Start node to a target node,
 * including descent into nested SubGraph assets.
 * Used by the Invoke Tool to determine which nodes to force-complete during invocation.
 */
class FLOWEDITOR_API FFlowInvokePathFinder
{
public:
	/**
	 * Find the path from RootTemplateAsset's Start node to TargetNode.
	 * If TargetNode lives inside a subgraph, produces a multi-segment path that descends
	 * through SubGraph boundary nodes to reach the target asset.
	 * Returns a path that may have PendingChoices if branching is encountered.
	 * After resolving all choices, call BuildOrderedPath() to populate each segment's OrderedNodes.
	 */
	static FFlowInvokePath FindPath(UFlowAsset* RootTemplateAsset, UFlowNode* TargetNode);

	/**
	 * After all PendingChoices in all segments have been resolved, build each segment's
	 * OrderedNodes. Skips segments that are already built. No-op if any choices remain unresolved.
	 */
	static void BuildOrderedPath(FFlowInvokePath& InOutPath);

	/** Get a display-friendly name for a node by GUID within a given asset. */
	static FString GetNodeDisplayName(UFlowAsset* TemplateAsset, const FGuid& NodeGuid);

private:
	/**
	 * Find the chain of SubGraph nodes leading from CurrentAsset down to TargetAsset.
	 * OutChain is ordered outermost-to-innermost. Returns false if no path is found.
	 */
	static bool FindSubGraphChain(UFlowAsset* CurrentAsset, UFlowAsset* TargetAsset, TArray<UFlowNode_SubGraph*>& OutChain);

	/**
	 * Build a single path segment via BFS backward from TargetGuid to Start within TemplateAsset.
	 * Fills PendingChoices. If no choices exist, also populates OrderedNodes immediately.
	 * An invalid SegmentTargetGuid on the returned segment indicates no path was found.
	 */
	static FFlowInvokePathSegment BuildSegment(UFlowAsset* TemplateAsset, const FGuid& TargetGuid, const FGuid& ParentSubGraphNodeGuid);

	/** Populate OrderedNodes for a single segment after its choices are resolved. */
	static void BuildOrderedSegment(FFlowInvokePathSegment& Segment);

	/** Build a reverse connection map within an asset: node GUID → predecessor GUIDs. */
	static TMap<FGuid, TArray<FGuid>> BuildReverseMap(UFlowAsset* TemplateAsset);

	/** Find the Start node GUID within an asset. Returns an invalid GUID if not found. */
	static FGuid FindStartNodeGuid(UFlowAsset* TemplateAsset);
};