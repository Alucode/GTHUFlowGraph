// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Asset/SFlowInvokeTab.h"
#include "Asset/FlowAssetEditor.h"
#include "Asset/FlowInvokePathFinder.h"

#include "FlowAsset.h"
#include "Nodes/FlowNode.h"
#include "Nodes/Route/FlowNode_SubGraph.h"

#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SFlowInvokeTab"

void SFlowInvokeTab::Construct(const FArguments& InArgs, FFlowAssetEditor* InEditor)
{
	Editor = InEditor;

	PathListBox = SNew(SVerticalBox);
	BranchChoicesBox = SNew(SVerticalBox);
	LogEntriesBox = SNew(SVerticalBox);

	LogScrollBox = SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			LogEntriesBox.ToSharedRef()
		];

	ChildSlot
	[
		SNew(SVerticalBox)

		// --- Top pane: controls (scrollable) ---
		+ SVerticalBox::Slot()
		.FillHeight(0.6f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// --- Target Node ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 6.f, 6.f, 2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InvokeTargetLabel", "Invoke Target"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 0.f, 6.f, 6.f)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(6.f, 4.f))
					[
						SNew(STextBlock)
						.Text(this, &SFlowInvokeTab::GetTargetNodeText)
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
						.AutoWrapText(true)
					]
				]

				// --- Resolved Path ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 4.f, 6.f, 2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ResolvedPathLabel", "Resolved Path"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 0.f, 6.f, 6.f)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(6.f, 4.f))
					[
						PathListBox.ToSharedRef()
					]
				]

				// --- Branch Choices ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 0.f, 6.f, 0.f)
				[
					BranchChoicesBox.ToSharedRef()
				]

				// --- Invoke Button ---
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6.f, 6.f, 6.f, 6.f)
				[
					SNew(SButton)
					.Text(this, &SFlowInvokeTab::GetInvokeButtonText)
					.IsEnabled(this, &SFlowInvokeTab::CanInvoke)
					.OnClicked(this, &SFlowInvokeTab::OnInvokeClicked)
					.HAlign(HAlign_Center)
				]
			]
		]

		// --- Visible divider ---
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f))
			.Padding(FMargin(0.f, 1.f))
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.f))
				.Padding(FMargin(0.f, 1.f))
			]
		]

		// --- Bottom pane: log (scrollable) ---
		+ SVerticalBox::Slot()
		.FillHeight(0.4f)
		[
			SNew(SVerticalBox)

			// --- Log Header ---
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.f, 4.f, 6.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InvokeLogLabel", "Invoke Log"))
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ClearLogButton", "Clear"))
					.OnClicked(this, &SFlowInvokeTab::OnClearLogClicked)
				]
			]

			// --- Log Entries ---
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(6.f, 0.f, 6.f, 6.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					LogScrollBox.ToSharedRef()
				]
			]
		]
	];

	// Show empty-state path list
	RebuildPathDisplay();
}

void SFlowInvokeTab::SetTargetNode(UFlowNode* Node)
{
	if (!Node)
	{
		TargetNodeGuid = FGuid();
		TargetNodeDisplayName = TEXT("");
		CurrentPath = FFlowInvokePath();
		RebuildPathDisplay();
		RebuildBranchChoices();
		return;
	}

	TargetNodeGuid = Node->GetGuid();
#if WITH_EDITOR
	TargetNodeDisplayName = Node->GetNodeTitle().ToString();
#else
	TargetNodeDisplayName = Node->GetClass()->GetName();
#endif

	UFlowAsset* RootTemplateAsset = Editor ? Editor->GetFlowAsset() : nullptr;
	CurrentPath = FFlowInvokePathFinder::FindPath(RootTemplateAsset, Node);

	if (CurrentPath.IsComplete())
	{
		int32 TotalNodes = 0;
		for (const FFlowInvokePathSegment& Seg : CurrentPath.Segments)
		{
			TotalNodes += Seg.OrderedNodes.Num();
		}
		AddLogEntry(FString::Printf(TEXT("Path found: %d nodes across %d segment(s) to %s"),
			TotalNodes, CurrentPath.Segments.Num(), *TargetNodeDisplayName));
	}

	RebuildPathDisplay();
	RebuildBranchChoices();
}

void SFlowInvokeTab::RefreshPIEState()
{
	// Forces the Invoke button's enabled state to re-evaluate
}

void SFlowInvokeTab::RebuildPathDisplay()
{
	PathListBox->ClearChildren();

	if (!CurrentPath.HasTarget())
	{
		PathListBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTargetSet", "No target set. Right-click a node and choose 'Set as Invoke Target'."))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			.AutoWrapText(true)
		];
		return;
	}

	if (!CurrentPath.IsComplete())
	{
		if (!CurrentPath.AllChoicesResolved())
		{
			PathListBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ChoicesNeeded", "Resolve branch choices below to see path."))
				.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.2f))
				.AutoWrapText(true)
			];
		}
		return;
	}

	int32 GlobalIdx = 0;
	for (int32 SegIdx = 0; SegIdx < CurrentPath.Segments.Num(); SegIdx++)
	{
		const FFlowInvokePathSegment& Segment = CurrentPath.Segments[SegIdx];

		// Show a segment header for subgraph levels
		if (SegIdx > 0)
		{
			const FString SubGraphName = FFlowInvokePathFinder::GetNodeDisplayName(
				CurrentPath.Segments[SegIdx - 1].TemplateAsset,
				Segment.ParentSubGraphNodeGuid);

			PathListBox->AddSlot()
			.AutoHeight()
			.Padding(2.f, 4.f, 2.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("  [ SubGraph: %s ]"), *SubGraphName)))
				.ColorAndOpacity(FLinearColor(0.5f, 0.7f, 1.f))
			];
		}

		for (int32 i = 0; i < Segment.OrderedNodes.Num(); i++)
		{
			const FGuid& NodeGuid = Segment.OrderedNodes[i];
			const FString NodeName = FFlowInvokePathFinder::GetNodeDisplayName(Segment.TemplateAsset, NodeGuid);
			const bool bIsTarget = (NodeGuid == CurrentPath.TargetNodeGuid);

			PathListBox->AddSlot()
			.AutoHeight()
			.Padding(2.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%d. %s%s"), ++GlobalIdx, *NodeName, bIsTarget ? TEXT(" [TARGET]") : TEXT(""))))
				.ColorAndOpacity(bIsTarget ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(0.8f, 0.8f, 0.8f))
			];
		}
	}
}

void SFlowInvokeTab::RebuildBranchChoices()
{
	BranchChoicesBox->ClearChildren();

	// Count total choices across all segments
	int32 TotalChoices = 0;
	for (const FFlowInvokePathSegment& Seg : CurrentPath.Segments)
	{
		TotalChoices += Seg.PendingChoices.Num();
	}
	if (TotalChoices == 0) return;

	BranchChoicesBox->AddSlot()
	.AutoHeight()
	.Padding(6.f, 4.f, 6.f, 2.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BranchChoicesLabel", "Branch Choices"))
		.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.2f))
	];

	for (int32 SegIdx = 0; SegIdx < CurrentPath.Segments.Num(); SegIdx++)
	{
		FFlowInvokePathSegment& Segment = CurrentPath.Segments[SegIdx];
		if (Segment.PendingChoices.Num() == 0) continue;

		for (int32 ChoiceIdx = 0; ChoiceIdx < Segment.PendingChoices.Num(); ChoiceIdx++)
		{
			FFlowBranchChoice& Choice = Segment.PendingChoices[ChoiceIdx];
			const FString ChoiceNodeName = FFlowInvokePathFinder::GetNodeDisplayName(Segment.TemplateAsset, Choice.NodeGuid);

			BranchChoicesBox->AddSlot()
			.AutoHeight()
			.Padding(6.f, 2.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Entering '%s' via:"), *ChoiceNodeName)))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
			];

			for (int32 PredIdx = 0; PredIdx < Choice.AvailablePredecessors.Num(); PredIdx++)
			{
				const FGuid PredGuid = Choice.AvailablePredecessors[PredIdx];
				const FString PredName = FFlowInvokePathFinder::GetNodeDisplayName(Segment.TemplateAsset, PredGuid);
				const bool bSelected = (Choice.SelectedIndex == PredIdx);

				const int32 CapturedSegIdx = SegIdx;
				const int32 CapturedChoiceIdx = ChoiceIdx;
				const int32 CapturedPredIdx = PredIdx;

				BranchChoicesBox->AddSlot()
				.AutoHeight()
				.Padding(16.f, 1.f, 6.f, 1.f)
				[
					SNew(SButton)
					.Text(FText::FromString(FString::Printf(TEXT("%s %s"), bSelected ? TEXT("[x]") : TEXT("[ ]"), *PredName)))
					.ButtonColorAndOpacity(bSelected ? FLinearColor(0.2f, 0.4f, 0.2f) : FLinearColor(0.15f, 0.15f, 0.15f))
					.OnClicked_Lambda([this, CapturedSegIdx, CapturedChoiceIdx, CapturedPredIdx]() -> FReply
					{
						if (CurrentPath.Segments.IsValidIndex(CapturedSegIdx) &&
							CurrentPath.Segments[CapturedSegIdx].PendingChoices.IsValidIndex(CapturedChoiceIdx))
						{
							CurrentPath.Segments[CapturedSegIdx].PendingChoices[CapturedChoiceIdx].SelectedIndex = CapturedPredIdx;

							if (CurrentPath.AllChoicesResolved())
							{
								FFlowInvokePathFinder::BuildOrderedPath(CurrentPath);
							}

							RebuildBranchChoices();
							RebuildPathDisplay();
						}
						return FReply::Handled();
					})
				];
			}
		}
	}
}

void SFlowInvokeTab::AddLogEntry(const FString& Entry)
{
	LogEntries.Add(Entry);

	LogEntriesBox->AddSlot()
	.AutoHeight()
	.Padding(4.f, 1.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("> %s"), *Entry)))
		.Font(FEditorStyle::GetFontStyle("SmallFont"))
		.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 0.7f))
		.AutoWrapText(true)
	];

	// Scroll to bottom
	LogScrollBox->ScrollToEnd();
}

void SFlowInvokeTab::ClearLog()
{
	LogEntries.Empty();
	LogEntriesBox->ClearChildren();
}

FText SFlowInvokeTab::GetTargetNodeText() const
{
	if (!TargetNodeGuid.IsValid())
	{
		return LOCTEXT("NoTarget", "None");
	}
	return FText::FromString(TargetNodeDisplayName);
}

FText SFlowInvokeTab::GetInvokeButtonText() const
{
	if (!TargetNodeGuid.IsValid())
	{
		return LOCTEXT("InvokeButton_NoTarget", "Invoke (no target set)");
	}
	if (!CurrentPath.AllChoicesResolved())
	{
		return LOCTEXT("InvokeButton_UnresolvedChoices", "Invoke (resolve choices first)");
	}
	if (!FFlowAssetEditor::IsPIE())
	{
		return LOCTEXT("InvokeButton_NotPIE", "Invoke (enter Play mode first)");
	}
	return LOCTEXT("InvokeButton_Ready", "Invoke to Target");
}

bool SFlowInvokeTab::CanInvoke() const
{
	return CurrentPath.IsComplete() && FFlowAssetEditor::IsPIE();
}

FReply SFlowInvokeTab::OnInvokeClicked()
{
	if (!CanInvoke() || !Editor) return FReply::Handled();

	UFlowAsset* TemplateAsset = Editor->GetFlowAsset();
	if (!TemplateAsset) return FReply::Handled();

	UFlowAsset* Instance = TemplateAsset->GetInspectedInstance();
	if (!Instance)
	{
		AddLogEntry(TEXT("ERROR: No active flow instance to invoke on. Make sure a flow is running."));
		return FReply::Handled();
	}

	ClearLog();

	int32 TotalNodes = 0;
	for (const FFlowInvokePathSegment& Seg : CurrentPath.Segments)
	{
		TotalNodes += Seg.OrderedNodes.Num();
	}
	AddLogEntry(FString::Printf(TEXT("Invoking to: %s (%d nodes, %d segment(s))"),
		*TargetNodeDisplayName, TotalNodes, CurrentPath.Segments.Num()));

	auto LogCallback = [this](const FString& LogMsg) { AddLogEntry(LogMsg); };

	if (!CurrentPath.IsMultiSegment())
	{
		// Single-asset path — invoke directly on the root instance
		Instance->InvokeToNode(CurrentPath.Segments[0].OrderedNodes, LogCallback);
	}
	else
	{
		// Multi-segment path — invoke per level, descending into each subgraph instance
		UFlowAsset* CurrentInstance = Instance;
		for (int32 i = 0; i < CurrentPath.Segments.Num(); i++)
		{
			const FFlowInvokePathSegment& Segment = CurrentPath.Segments[i];
			const bool bIsLast = (i == CurrentPath.Segments.Num() - 1);

			CurrentInstance->InvokeToNode(Segment.OrderedNodes, LogCallback);

			if (!bIsLast)
			{
				// The last node in this segment is the SubGraph boundary node, now activated.
				// Retrieve the child instance it created.
				const FGuid SubGraphGuid = Segment.OrderedNodes.Last();
				UFlowNode_SubGraph* SubGraphNode = CurrentInstance->GetNode<UFlowNode_SubGraph>(SubGraphGuid);
				if (!SubGraphNode)
				{
					AddLogEntry(TEXT("ERROR: SubGraph boundary node not found on instance."));
					break;
				}

				UFlowAsset* ChildInstance = CurrentInstance->GetFlowInstance(SubGraphNode).Get();
				if (!ChildInstance)
				{
					AddLogEntry(TEXT("ERROR: SubGraph child instance was not created. "
						"Ensure the subgraph has at least one async node after its Start."));
					break;
				}

				CurrentInstance = ChildInstance;
			}
		}
	}

	return FReply::Handled();
}

FReply SFlowInvokeTab::OnClearLogClicked()
{
	ClearLog();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE