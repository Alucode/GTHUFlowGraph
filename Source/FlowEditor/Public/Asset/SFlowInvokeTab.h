// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Asset/FlowInvokePathFinder.h"

class FFlowAssetEditor;
class UFlowNode;
class SScrollBox;
class SVerticalBox;

/**
 * Slate widget for the Invoke Tool tab in the FlowAsset editor.
 *
 * Allows the user to:
 *   - Set a target node to invoke to
 *   - Resolve branch choices if multiple paths exist
 *   - Execute the invoke during PIE (force-completing all nodes up to the target)
 *   - View a log of what was completed during the invoke
 */
class FLOWEDITOR_API SFlowInvokeTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlowInvokeTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FFlowAssetEditor* InEditor);

	/** Called by the editor when the user selects "Set as Invoke Target" on a node. */
	void SetTargetNode(UFlowNode* Node);

	/** Called by the editor when PIE state changes, to refresh button availability. */
	void RefreshPIEState();

	/** Re-scan the asset for FlowNode_InvokePoint nodes and rebuild the waypoint list. */
	void ScanInvokePoints();

private:
	// --- Path state ---

	/** The node GUID this tab is targeting. */
	FGuid TargetNodeGuid;

	/** Display name of the target node (cached for display). */
	FString TargetNodeDisplayName;

	/** Current invoke path (may have pending branch choices). */
	FFlowInvokePath CurrentPath;

	// --- Log state ---

	TArray<FString> LogEntries;

	// --- Invoke points ---

	TArray<FFlowInvokePoint> InvokePoints;

	// --- Widget references ---

	TSharedPtr<SVerticalBox> InvokePointsBox;
	TSharedPtr<SVerticalBox> BranchChoicesBox;
	TSharedPtr<SScrollBox> LogScrollBox;
	TSharedPtr<SVerticalBox> LogEntriesBox;
	TSharedPtr<SVerticalBox> PathListBox;

	// --- Editor back-reference ---

	FFlowAssetEditor* Editor = nullptr;

	// --- Helpers ---

	void RebuildInvokePoints();
	void RebuildPathDisplay();
	void RebuildBranchChoices();
	void AddLogEntry(const FString& Entry);
	void ClearLog();

	// --- Slate callbacks ---

	FText GetTargetNodeText() const;
	FText GetInvokeButtonText() const;
	bool CanInvoke() const;
	FReply OnInvokeClicked();
	FReply OnClearLogClicked();
	FReply OnScanClicked();
};