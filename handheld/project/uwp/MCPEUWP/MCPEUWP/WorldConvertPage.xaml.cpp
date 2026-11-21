#include "pch.h"
#include "WorldConvertPage.xaml.h"
#include "XamlLocalization.h"

#include "../../../../src/platform/uwp/UwpStoragePaths.h"
#include "../../../../src/platform/uwp/UwpZip.h"
#include "../../../../src/world/level/storage/Legacy09WorldExporter.h"
#include "../../../../src/world/level/storage/Legacy17WorldExporter.h"

#include <windows.storage.h>
#include <windows.storage.pickers.h>
#include <ppltasks.h>
#include <collection.h>
#include <cstring>
#include <memory>
#include <string>

#include "../../../../src/world/level/LevelSettings.h"

using namespace MCPEUWP;
using namespace Platform;
using namespace concurrency;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::Xaml;

namespace
{
	const wchar_t* kTemporaryWorldId = L"_LastJoinedServer";

	struct ConversionResult
	{
		bool success = false;
		bool cancelled = false;
		std::wstring error;
		std::wstring outputPath;
	};
}

WorldConvertPage::WorldConvertPage()
{
	InitializeComponent();
	UwpXamlLocalization::install(this);
}

void WorldConvertPage::SetWorldContext(
	String^ sourceDirectory,
	String^ worldId,
	String^ worldName,
	int worldType,
	int indevType,
	bool alphaWinter,
	bool currentStorageFormat)
{
	m_sourceDirectory = sourceDirectory;
	m_worldId = worldId;
	m_worldName = worldName;
	m_worldType = worldType;
	m_indevType = indevType;
	m_alphaWinter = alphaWinter;
	m_currentStorageFormat = currentStorageFormat;

	const bool supportedWorldType =
		worldType == WorldType::Old ||
		worldType == WorldType::Infinite ||
		worldType == WorldType::Sky ||
		worldType == WorldType::Alpha ||
		worldType == WorldType::Indev ||
		worldType == WorldType::EarlyInfdev;
	const bool showLowerSeaLevel = worldType != WorldType::Indev &&
		(worldType == WorldType::Old ||
		 worldType == WorldType::Infinite || worldType == WorldType::Alpha ||
		 worldType == WorldType::EarlyInfdev);
	const bool showWinterFreeze =
		worldType == WorldType::Alpha && alphaWinter;
	const bool showSkyBedrock = worldType == WorldType::Sky;
	const bool showFloatingBedrock = worldType == WorldType::Indev &&
		indevType == IndevType::Floating;
	const bool showBedrock17Void = worldType == WorldType::Sky;
	const bool showTestLimit = supportedWorldType &&
		worldType != WorldType::Old;
	const auto visible = Windows::UI::Xaml::Visibility::Visible;
	const auto collapsed = Windows::UI::Xaml::Visibility::Collapsed;

	if (TestLimitCheckBox != nullptr)
	{
		TestLimitCheckBox->Visibility = showTestLimit ? visible : collapsed;
		TestLimitCheckBox->IsChecked = false;
	}
	if (Bedrock17VoidWorldCheckBox != nullptr)
	{
		Bedrock17VoidWorldCheckBox->Visibility =
			showBedrock17Void ? visible : collapsed;
		Bedrock17VoidWorldCheckBox->IsChecked = false;
	}
	if (LowerSeaLevelCheckBox != nullptr)
	{
		LowerSeaLevelCheckBox->Visibility = showLowerSeaLevel ? visible : collapsed;
		LowerSeaLevelCheckBox->IsChecked = false;
	}
	if (WinterFreezeWaterCheckBox != nullptr)
	{
		WinterFreezeWaterCheckBox->Visibility = showWinterFreeze ? visible : collapsed;
		WinterFreezeWaterCheckBox->IsChecked = false;
	}
	if (SkyBedrockFloorCheckBox != nullptr)
	{
		SkyBedrockFloorCheckBox->Visibility = showSkyBedrock ? visible : collapsed;
		SkyBedrockFloorCheckBox->IsChecked = false;
	}
	if (FloatingBedrockFloorCheckBox != nullptr)
	{
		FloatingBedrockFloorCheckBox->Visibility =
			showFloatingBedrock ? visible : collapsed;
		FloatingBedrockFloorCheckBox->IsChecked = false;
	}
	if (StorageFormatWarningText != nullptr)
		StorageFormatWarningText->Visibility =
			currentStorageFormat ? collapsed : visible;
	if (ConvertButton != nullptr)
		ConvertButton->IsEnabled = currentStorageFormat && supportedWorldType;

	if (SourcePathText != nullptr)
	{
		if (worldName != nullptr && worldName->Length() > 0)
			SourcePathText->Text = worldName;
		else if (worldId != nullptr && worldId->Length() > 0)
			SourcePathText->Text = worldId;
		else
			SourcePathText->Text = ref new String(L"Not configured");
	}

	if (DestinationPathText != nullptr)
	{
		DestinationPathText->Text = ref new String(
			L"Choose a destination ZIP file after clicking Convert");
	}
	UpdateConversionMode();

	if (!m_conversionRunning)
	{
		if (!currentStorageFormat)
			SetStatus(L"Conversion blocked. Open and save this world in the game first to upgrade its UWP storage format.");
		else if (m_convertTo17VoidWorld)
			SetStatus(L"Bedrock 1.17+ void conversion is selected. Existing Sky chunks will be converted and new terrain will remain void.");
		else if (supportedWorldType)
			SetStatus(L"Full conversion is enabled by default. The original world is not modified.");
		else
			SetStatus(L"This world type is not supported by the PE 0.9 converter.");
	}
}

void WorldConvertPage::UpdateConversionMode()
{
	const bool selected = m_worldType == WorldType::Sky &&
		Bedrock17VoidWorldCheckBox != nullptr &&
		Bedrock17VoidWorldCheckBox->IsChecked != nullptr &&
		Bedrock17VoidWorldCheckBox->IsChecked->Value;
	m_convertTo17VoidWorld = selected;

	if (ConversionTitleText != nullptr)
		ConversionTitleText->Text = ref new String(
			selected ? L"Convert to 1.17+ void world" : L"Convert to PE 0.9+");
	if (ConvertButton != nullptr)
		ConvertButton->Content = ref new String(
			selected ? L"Convert to 1.17+ void world" : L"Convert to PE 0.9+");

	// These options only modify the PE 0.9 LegacyTerrain output. A modern
	// void-world export copies the source Sky chunks as modern palettes and
	// leaves the new area empty, so do not leave misleading legacy switches
	// active while that mode is selected.
	const bool legacyOptionsEnabled = !selected;
	if (LowerSeaLevelCheckBox != nullptr)
		LowerSeaLevelCheckBox->IsEnabled = legacyOptionsEnabled;
	if (WinterFreezeWaterCheckBox != nullptr)
		WinterFreezeWaterCheckBox->IsEnabled = legacyOptionsEnabled;
	if (SkyBedrockFloorCheckBox != nullptr)
		SkyBedrockFloorCheckBox->IsEnabled = legacyOptionsEnabled;
	if (FloatingBedrockFloorCheckBox != nullptr)
		FloatingBedrockFloorCheckBox->IsEnabled = legacyOptionsEnabled;
}

void WorldConvertPage::On17VoidWorldToggleClick(
	Object^, RoutedEventArgs^)
{
	UpdateConversionMode();
	if (!m_conversionRunning)
	{
		if (m_convertTo17VoidWorld)
			SetStatus(L"Bedrock 1.17+ void conversion selected. New chunks will be pure void.");
		else
			SetStatus(L"PE 0.9 conversion selected.");
	}
}

void WorldConvertPage::OnConvertClick(
	Object^, RoutedEventArgs^)
{
	if (!m_currentStorageFormat)
	{
		SetStatus(L"Conversion blocked. Open and save this world in the game first to upgrade its UWP storage format.");
		return;
	}
	if (m_worldType != WorldType::Old &&
		m_worldType != WorldType::Infinite &&
		m_worldType != WorldType::Sky && m_worldType != WorldType::Alpha &&
		m_worldType != WorldType::Indev && m_worldType != WorldType::EarlyInfdev)
	{
		SetStatus(L"This world type is not supported by the PE 0.9 converter.");
		return;
	}
	if (m_conversionRunning || m_sourceDirectory == nullptr ||
		m_sourceDirectory->Length() == 0)
	{
		SetStatus(L"No source world is configured.");
		return;
	}

	m_conversionRunning = true;
	if (ConvertButton != nullptr)
		ConvertButton->IsEnabled = false;
	if (BackButton != nullptr)
		BackButton->IsEnabled = false;
	if (ConversionProgressBar != nullptr)
		ConversionProgressBar->Visibility =
			Windows::UI::Xaml::Visibility::Visible;
	SetStatus(L"Choose a destination ZIP file...");

	const std::wstring sourceDirectory(m_sourceDirectory->Data());
	const std::wstring worldName = m_worldName == nullptr
		? std::wstring()
		: std::wstring(m_worldName->Data());
	const bool testLimit = TestLimitCheckBox != nullptr &&
		TestLimitCheckBox->IsChecked != nullptr &&
		TestLimitCheckBox->IsChecked->Value;
	const bool lowerSeaLevel = LowerSeaLevelCheckBox != nullptr &&
		LowerSeaLevelCheckBox->IsChecked != nullptr &&
		LowerSeaLevelCheckBox->IsChecked->Value;
	const bool freezeWinterWater = WinterFreezeWaterCheckBox != nullptr &&
		WinterFreezeWaterCheckBox->IsChecked != nullptr &&
		WinterFreezeWaterCheckBox->IsChecked->Value;
	const bool skyBedrockFloor = SkyBedrockFloorCheckBox != nullptr &&
		SkyBedrockFloorCheckBox->IsChecked != nullptr &&
		SkyBedrockFloorCheckBox->IsChecked->Value;
	const bool floatingBedrockFloor = FloatingBedrockFloorCheckBox != nullptr &&
		FloatingBedrockFloorCheckBox->IsChecked != nullptr &&
		FloatingBedrockFloorCheckBox->IsChecked->Value;
	const bool convertTo17VoidWorld = m_convertTo17VoidWorld;
	auto result = std::make_shared<ConversionResult>();
	auto picker = ref new FileSavePicker();
	picker->SuggestedStartLocation = PickerLocationId::Downloads;
	picker->SuggestedFileName = ref new String(L"converted_world");
	auto zipExtensions = ref new Platform::Collections::Vector<String^>();
	zipExtensions->Append(ref new String(L".zip"));
	picker->FileTypeChoices->Insert(
		ref new String(L"ZIP archive"), zipExtensions);

	create_task(picker->PickSaveFileAsync())
		.then([this, sourceDirectory, worldName, testLimit, lowerSeaLevel,
			freezeWinterWater, skyBedrockFloor, floatingBedrockFloor, convertTo17VoidWorld,
			result](StorageFile^ outputFile)
			-> task<void>
		{
			if (outputFile == nullptr)
			{
				result->cancelled = true;
				return task_from_result();
			}

			result->outputPath = outputFile->Path->Data();
			if (DestinationPathText != nullptr)
				DestinationPathText->Text = outputFile->Path;
			SetStatus(L"Converting the world and preparing the ZIP file...");

			return create_task(UwpStoragePaths::ensureTemporaryWorldsFolderAsync())
			.then([](StorageFolder^ root) {
			// The core converter requires an empty destination.  The fixed
			// _LastJoinedServer folder is cache data, so replacing it here is
			// intentional and never touches the source world.
			return create_task(root->CreateFolderAsync(
				ref new String(kTemporaryWorldId),
				CreationCollisionOption::ReplaceExisting));
			})
			.then([sourceDirectory, worldName, testLimit, lowerSeaLevel,
				freezeWinterWater, skyBedrockFloor, floatingBedrockFloor, convertTo17VoidWorld,
				outputFile, result](StorageFolder^ destination)
				-> task<void>
		{
			const std::wstring destinationDirectory(destination->Path->Data());
			return create_task([sourceDirectory, worldName, destinationDirectory,
				testLimit, lowerSeaLevel, freezeWinterWater,
				skyBedrockFloor, floatingBedrockFloor, convertTo17VoidWorld, result]()
			{
				if (convertTo17VoidWorld)
				{
					legacy17::ExportOptions options;
					options.source = sourceDirectory;
					options.destination = destinationDirectory;
					options.name = worldName;
					options.maxChunks = testLimit ? 64 : 0;
					options.writeVoidAnchorChunk = true;
					result->success = legacy17::Export(
						options, legacy17::Log(), result->error);
				}
				else
				{
					legacy09::ExportOptions options;
					options.source = sourceDirectory;
					options.destination = destinationDirectory;
					options.name = worldName;
					options.maxChunks = testLimit ? 64 : 0;
					options.lowerSeaLevel = lowerSeaLevel;
					options.freezeWinterWater = freezeWinterWater;
					options.skyBedrockFloor = skyBedrockFloor;
					options.floatingBedrockFloor = floatingBedrockFloor;
					result->success = legacy09::Export(
						options, legacy09::Log(), result->error);
				}
			})
			.then([destination, outputFile, result](task<void> exportTask) -> task<void>
			{
				exportTask.get();
				if (!result->success)
					return task_from_result();

				return create_task(
					Windows::Storage::ApplicationData::Current->TemporaryFolder
						->CreateFileAsync(
							L"mcpe_conversion.zip",
							CreationCollisionOption::GenerateUniqueName))
					.then([destination, outputFile](StorageFile^ temporaryArchive) -> task<void>
					{
						return UwpZip::CreateFromFolder(
							destination, temporaryArchive->Path->Data())
							.then([temporaryArchive, outputFile](task<void> zipTask) -> task<void>
							{
								zipTask.get();
								return create_task(
									temporaryArchive->CopyAndReplaceAsync(outputFile))
									.then([temporaryArchive]()
									{
										return create_task(temporaryArchive->DeleteAsync());
									});
							});
					});
			})
			.then([](task<void> archiveTask)
			{
				archiveTask.get();
			});
		});
	}, task_continuation_context::use_current())
		.then([this, result](task<void> completed)
		{
			try
			{
				completed.get();
				if (result->cancelled)
					SetStatus(L"Conversion cancelled.");
				else if (result->success)
					SetStatus(std::wstring(L"Conversion complete. ZIP saved to:\n") +
						result->outputPath);
				else
					SetStatus(L"Conversion failed: " + result->error);
			}
			catch (Platform::Exception^ exception)
			{
				std::wstring message = L"Conversion failed: ";
				if (exception != nullptr && exception->Message != nullptr)
					message += exception->Message->Data();
				else
					message += L"UWP storage error";
				SetStatus(message);
			}
			catch (const std::exception& exception)
			{
				SetStatus(std::wstring(L"Conversion failed: ") +
					std::wstring(exception.what(), exception.what() +
						std::strlen(exception.what())));
			}

			m_conversionRunning = false;
			if (ConvertButton != nullptr)
				ConvertButton->IsEnabled = m_currentStorageFormat &&
					(m_worldType == WorldType::Old ||
						m_worldType == WorldType::Infinite ||
						m_worldType == WorldType::Sky ||
						m_worldType == WorldType::Alpha ||
						m_worldType == WorldType::Indev ||
						m_worldType == WorldType::EarlyInfdev);
			if (BackButton != nullptr)
				BackButton->IsEnabled = true;
			if (ConversionProgressBar != nullptr)
				ConversionProgressBar->Visibility =
					Windows::UI::Xaml::Visibility::Collapsed;
		}, task_continuation_context::use_current());
}

void WorldConvertPage::SetStatus(const std::wstring& text)
{
	if (ConversionStatusText != nullptr)
		ConversionStatusText->Text = ref new String(text.c_str());
}

void WorldConvertPage::OnBackClick(
	Object^, RoutedEventArgs^)
{
	if (m_conversionRunning)
		return;
	this->BackRequested(this, nullptr);
}
