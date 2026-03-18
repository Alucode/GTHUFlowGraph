// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Asset/SFlowInvokeTab.h"
#include "Asset/FlowAssetEditor.h"
#include "Asset/FlowInvokePathFinder.h"

#include "FlowAsset.h"
#include "Nodes/FlowNode.h"

#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
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
		SNew(SSplitter)
		.Orientation(Orient_Vertical)

		// --- Top pane: controls (scrollable) ---
		+ SSplitter::Slot()
		.Value(0.6f)
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

		// --- Bottom pane: log (scrollable) ---
		+ SSplitter::Slot()
		.Value(0.4f)
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

	// Find the template asset to run path finding on
	UFlowAsset* TemplateAsset = nullptr;
	if (Editor)
	{
		TemplateAsset = Editor->GetFlowAsset();
	}

	CurrentPath = FFlowInvokePathFinder::FindPath(TemplateAsset, TargetNodeGuid);

	// If no pending choices, the path is already built
	if (CurrentPath.IsComplete())
	{
		AddLogEntry(FString::Printf(TEXT("Path found: %d nodes to %s"), CurrentPath.OrderedNodes.Num(), *TargetNodeDisplayName));
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

	UFlowAsset* TemplateAsset = Editor ? Editor->GetFlowAsset() : nullptr;

	for (int32 i = 0; i < CurrentPath.OrderedNodes.Num(); i++)
	{
		const FGuid& NodeGuid = CurrentPath.OrderedNodes[i];
		const FString NodeName = FFlowInvokePathFinder::GetNodeDisplayName(TemplateAsset, NodeGuid);
		const bool bIsTarget = (NodeGuid == CurrentPath.TargetNodeGuid);

		PathListBox->AddSlot()
		.AutoHeight()
		.Padding(2.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d. %s%s"), i + 1, *NodeName, bIsTarget ? TEXT(" [TARGET]") : TEXT(""))))
			.ColorAndOpacity(bIsTarget ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(0.8f, 0.8f, 0.8f))
		];
	}
}

void SFlowInvokeTab::RebuildBranchChoices()
{
	BranchChoicesBox->ClearChildren();

	if (CurrentPath.PendingChoices.Num() == 0) return;

	UFlowAsset* TemplateAsset = Editor ? Editor->GetFlowAsset() : nullptr;

	BranchChoicesBox->AddSlot()
	.AutoHeight()
	.Padding(6.f, 4.f, 6.f, 2.f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BranchChoicesLabel", "Branch Choices"))
		.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		.ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.2f))
	];

	for (int32 ChoiceIdx = 0; ChoiceIdx < CurrentPath.PendingChoices.Num(); ChoiceIdx++)
	{
		FFlowBranchChoice& Choice = CurrentPath.PendingChoices[ChoiceIdx];
		const FString ChoiceNodeName = FFlowInvokePathFinder::GetNodeDisplayName(TemplateAsset, Choice.NodeGuid);

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
			const FString PredName = FFlowInvokePathFinder::GetNodeDisplayName(TemplateAsset, PredGuid);
			const bool bSelected = (Choice.SelectedIndex == PredIdx);

			// Capture by value for the lambda
			const int32 CapturedChoiceIdx = ChoiceIdx;
			const int32 CapturedPredIdx = PredIdx;

			BranchChoicesBox->AddSlot()
			.AutoHeight()
			.Padding(16.f, 1.f, 6.f, 1.f)
			[
				SNew(SButton)
				.Text(FText::FromString(FString::Printf(TEXT("%s %s"), bSelected ? TEXT("[x]") : TEXT("[ ]"), *PredName)))
				.ButtonColorAndOpacity(bSelected ? FLinearColor(0.2f, 0.4f, 0.2f) : FLinearColor(0.15f, 0.15f, 0.15f))
				.OnClicked_Lambda([this, CapturedChoiceIdx, CapturedPredIdx]() -> FReply
				{
					if (CurrentPath.PendingChoices.IsValidIndex(CapturedChoiceIdx))
					{
						CurrentPath.PendingChoices[CapturedChoiceIdx].SelectedIndex = CapturedPredIdx;

						// If all choices are now resolved, build the ordered path
						if (CurrentPath.AllChoicesResolved())
						{
							UFlowAsset* Asset = Editor ? Editor->GetFlowAsset() : nullptr;
							FFlowInvokePathFinder::BuildOrderedPath(CurrentPath, Asset);
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
	AddLogEntry(FString::Printf(TEXT("Invoking to: %s (%d nodes)"), *TargetNodeDisplayName, CurrentPath.OrderedNodes.Num()));

	// Execute the invoke on the active instance
	Instance->InvokeToNode(CurrentPath.OrderedNodes, [this](const FString& LogMsg)
	{
		AddLogEntry(LogMsg);
	});

	return FReply::Handled();
}

FReply SFlowInvokeTab::OnClearLogClicked()
{
	ClearLog();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE