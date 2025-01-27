#include "GuiGamelistOptions.h"

#include "guis/GuiGamelistFilter.h"
#include "scrapers/Scraper.h"
#include "views/gamelist/IGameListView.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "GuiMetaDataEd.h"
#include "SystemData.h"
#include "components/SwitchComponent.h"
#include "animations/LambdaAnimation.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiMsgBox.h"
#include "scrapers/ThreadedScraper.h"

std::vector<std::string> GuiGamelistOptions::gridSizes {
	"automatic",
	
	"1x1",

	"2x1",
	"2x2",
	"2x3",	
	"2x4",
	"2x5",
	"2x6",
	"2x7",

	"3x1",
	"3x2",
	"3x3",
	"3x4",
	"3x5",
	"3x6",
	"3x7",
	
	"4x1",
	"4x2",
	"4x3",
	"4x4",
	"4x5",
	"4x6",
	"4x7",

	"5x1",
	"5x2",
	"5x3",
	"5x4",
	"5x5",
	"5x6",
	"5x7",

	"6x1",
	"6x2",
	"6x3",
	"6x4",
	"6x5",
	"6x6",
	"6x7",

	"7x1",
	"7x2",
	"7x3",
	"7x4",
	"7x5",
	"7x6",
	"7x7"
};

GuiGamelistOptions::GuiGamelistOptions(Window* window, SystemData* system, bool showGridFeatures) : GuiComponent(window),
	mSystem(system), mMenu(window, "OPTIONS"), fromPlaceholder(false), mFiltersChanged(false), mReloadAll(false)
{	
	auto theme = ThemeData::getMenuTheme();

	mGridSize = nullptr;
	addChild(&mMenu);

	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
		addTextFilterToMenu();

	// check it's not a placeholder folder - if it is, only show "Filter Options"
	FileData* file = getGamelist()->getCursor();
	fromPlaceholder = file->isPlaceHolder();
	ComponentListRow row;

	if (!fromPlaceholder)
	{
		// jump to letter
		row.elements.clear();

		std::vector<std::string> letters = getGamelist()->getEntriesLetters();
		if (!letters.empty())
		{
			mJumpToLetterList = std::make_shared<LetterList>(mWindow, _("JUMP TO..."), false); // batocera

			char curChar = (char)toupper(getGamelist()->getCursor()->getName()[0]);

			if (std::find(letters.begin(), letters.end(), std::string(1, curChar)) == letters.end())
				curChar = letters.at(0)[0];

			for (auto letter : letters)
				mJumpToLetterList->add(letter, letter[0], letter[0] == curChar);

			row.addElement(std::make_shared<TextComponent>(mWindow, _("JUMP TO..."), theme->Text.font, theme->Text.color), true); // batocera
			row.addElement(mJumpToLetterList, false);
			row.input_handler = [&](InputConfig* config, Input input)
			{
				if (config->isMappedTo("a", input) && input.value)
				{
					jumpToLetter();
					return true;
				}
				else if (mJumpToLetterList->input(config, input))
				{
					return true;
				}
				return false;
			};
			mMenu.addRow(row);
		}
	}

	// sort list by
	unsigned int currentSortId = mSystem->getSortId();
	if (currentSortId > FileSorts::SortTypes.size()) {
		currentSortId = 0;
	}

	mListSort = std::make_shared<SortList>(mWindow, _("SORT GAMES BY"), false);
	for(unsigned int i = 0; i < FileSorts::SortTypes.size(); i++)
	{
		const FolderData::SortType& sort = FileSorts::SortTypes.at(i);
		mListSort->add(_(Utils::String::toUpper(sort.description)), i, i == currentSortId);
	}

	mMenu.addWithLabel(_("SORT GAMES BY"), mListSort);

	// GameList view style
	mViewMode = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
	std::vector<std::string> styles;
	styles.push_back("automatic");

	auto mViews = system->getTheme()->getViewsOfTheme();
	for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
		styles.push_back(*it);

	std::string viewMode = system->getSystemViewMode();

	bool found = false;
	for (auto it = styles.cbegin(); it != styles.cend(); it++)
	{
		bool sel = (viewMode.empty() && *it == "automatic") || viewMode == *it;
		if (sel)
			found = true;

		mViewMode->add(_(*it), *it, sel);
	}

	if (!found)
		mViewMode->selectFirstItem();

	mMenu.addWithLabel(_("GAMELIST VIEW STYLE"), mViewMode);	
	
	

	// Grid size override
	if (showGridFeatures)
	{
		auto gridOverride = system->getGridSizeOverride();
		auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

		mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GRID SIZE"), false);

		found = false;
		for (auto it = gridSizes.cbegin(); it != gridSizes.cend(); it++)
		{
			bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
			if (sel)
				found = true;

			mGridSize->add(_(*it), *it, sel);
		}

		if (!found)
			mGridSize->selectFirstItem();

		mMenu.addWithLabel(_("GRID SIZE"), mGridSize);
	}

	// show filtered menu
	if(!Settings::getInstance()->getBool("ForceDisableFilters"))
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(mWindow, _("APPLY FILTER"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
		row.addElement(makeArrow(mWindow), false);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::openGamelistFilter, this));
		mMenu.addRow(row);		
	}

	// Show favorites first in gamelists
	auto favoritesFirstSwitch = std::make_shared<SwitchComponent>(mWindow);
	favoritesFirstSwitch->setState(Settings::getInstance()->getBool("FavoritesFirst"));
	mMenu.addWithLabel(_("SHOW FAVORITES ON TOP"), favoritesFirstSwitch);	
	addSaveFunc([favoritesFirstSwitch, this]
	{
		if (Settings::getInstance()->setBool("FavoritesFirst", favoritesFirstSwitch->getState()))
			mReloadAll = true;
	});

	// hidden files
	auto hidden_files = std::make_shared<SwitchComponent>(mWindow);
	hidden_files->setState(Settings::getInstance()->getBool("ShowHiddenFiles"));
	mMenu.addWithLabel(_("SHOW HIDDEN FILES"), hidden_files);
	addSaveFunc([hidden_files, this]
	{
		if (Settings::getInstance()->setBool("ShowHiddenFiles", hidden_files->getState()))
			mReloadAll = true;
	});

	// Flat folders
	auto flatFolders = std::make_shared<SwitchComponent>(mWindow);
	flatFolders->setState(!Settings::getInstance()->getBool("FlatFolders"));
	mMenu.addWithLabel(_("SHOW FOLDERS"), flatFolders); // batocera
	addSaveFunc([flatFolders, this]
	{
		if (Settings::getInstance()->setBool("FlatFolders", !flatFolders->getState()))
			mReloadAll = true;
	});


	std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();

	if(UIModeController::getInstance()->isUIModeFull() &&
		((customCollections.find(system->getName()) != customCollections.cend() && CollectionSystemManager::get()->getEditingCollection() != system->getName()) ||
		CollectionSystemManager::get()->getCustomCollectionsBundle()->getName() == system->getName()))
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(mWindow, _("ADD/REMOVE GAMES TO THIS GAME COLLECTION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::startEditMode, this));
		mMenu.addRow(row);
	}

	if(UIModeController::getInstance()->isUIModeFull() && CollectionSystemManager::get()->isEditing())
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(mWindow, _("FINISH EDITING")+" '" + Utils::String::toUpper(CollectionSystemManager::get()->getEditingCollection()) + "' "+_("COLLECTION"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::exitEditMode, this));
		mMenu.addRow(row);
	}

	if (UIModeController::getInstance()->isUIModeFull() && !fromPlaceholder && !(mSystem->isCollection() && file->getType() == FOLDER))
	{
		row.elements.clear();
		row.addElement(std::make_shared<TextComponent>(mWindow, _("EDIT THIS GAME'S METADATA"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color), true);
		row.addElement(makeArrow(mWindow), false);
		row.makeAcceptInputHandler(std::bind(&GuiGamelistOptions::openMetaDataEd, this));
		mMenu.addRow(row);
	}

	// center the menu
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());	
	mMenu.animateTo(Vector2f((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2));
}

GuiGamelistOptions::~GuiGamelistOptions()
{
	if (mSystem == nullptr)
		return;

	for (auto it = mSaveFuncs.cbegin(); it != mSaveFuncs.cend(); it++)
		(*it)();

	// apply sort
	if (!fromPlaceholder && mListSort->getSelected() != mSystem->getSortId())
	{
		mSystem->setSortId(mListSort->getSelected());

		FolderData* root = mSystem->getRootFolder();
		const FolderData::SortType& sort = FileSorts::SortTypes.at(mListSort->getSelected());
		root->sort(sort);

		// notify that the root folder was sorted
		getGamelist()->onFileChanged(root, FILE_SORTED);
	}

	Vector2f gridSizeOverride(0, 0);

	if (mGridSize != NULL)
	{
		auto str = mGridSize->getSelected();

		size_t divider = str.find('x');
		if (divider != std::string::npos)
		{
			std::string first = str.substr(0, divider);
			std::string second = str.substr(divider+1, std::string::npos);

			gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
		}
	}
	
	bool viewModeChanged = mSystem->setSystemViewMode(mViewMode->getSelected(), gridSizeOverride);

	Settings::getInstance()->saveFile();

	if (mReloadAll)
	{
		mWindow->renderLoadingScreen(_("Loading..."));
		ViewController::get()->reloadAll(mWindow);
		mWindow->endRenderLoadingScreen();
	}
	else if (mFiltersChanged || viewModeChanged)
	{
		// only reload full view if we came from a placeholder
		// as we need to re-display the remaining elements for whatever new
		// game is selected
		ViewController::get()->reloadGameListView(mSystem);
	}
}

void GuiGamelistOptions::addTextFilterToMenu()
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(mWindow, _("FILTER GAMES BY TEXT"), font, color);
	row.addElement(lbl, true); // label

	std::string searchText;

	auto idx = mSystem->getIndex(false);
	if (idx != nullptr)
		searchText = idx->getTextFilter();

	mTextFilter = std::make_shared<TextComponent>(mWindow, searchText, font, color, ALIGN_RIGHT);
	row.addElement(mTextFilter, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);

	auto searchIcon = theme->getMenuIcon("searchIcon");
	bracket->setImage(searchIcon.empty() ? ":/search.svg" : searchIcon);

	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [this](const std::string& newVal)
	{
		mTextFilter->setValue(Utils::String::toUpper(newVal));

		auto index = mSystem->getIndex(!newVal.empty());
		if (index != nullptr)
		{
			mFiltersChanged = true;

			index->setTextFilter(newVal);
			if (!index->isFiltered())
				mSystem->deleteIndex();

			delete this;
		}
	};

	row.makeAcceptInputHandler([this, updateVal]
	{
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("FILTER GAMES BY TEXT"), mTextFilter->getValue(), updateVal, false));
	});

	mMenu.addRow(row);
}

void GuiGamelistOptions::openGamelistFilter()
{
	mFiltersChanged = true;
	GuiGamelistFilter* ggf = new GuiGamelistFilter(mWindow, mSystem);
	mWindow->pushGui(ggf);
}

void GuiGamelistOptions::startEditMode()
{
	std::string editingSystem = mSystem->getName();
	// need to check if we're editing the collections bundle, as we will want to edit the selected collection within
	if(editingSystem == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
	{
		FileData* file = getGamelist()->getCursor();
		// do we have the cursor on a specific collection?
		if (file->getType() == FOLDER)
		{
			editingSystem = file->getName();
		}
		else
		{
			// we are inside a specific collection. We want to edit that one.
			editingSystem = file->getSystem()->getName();
		}
	}
	CollectionSystemManager::get()->setEditMode(editingSystem);
	delete this;
}

void GuiGamelistOptions::exitEditMode()
{
	CollectionSystemManager::get()->exitEditMode();
	delete this;
}

void GuiGamelistOptions::openMetaDataEd()
{
	if (ThreadedScraper::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
		return;
	}

	// open metadata editor
	// get the FileData that hosts the original metadata
	FileData* file = getGamelist()->getCursor()->getSourceFileData();
	ScraperSearchParams p;
	p.game = file;
	p.system = file->getSystem();

	std::function<void()> deleteBtnFunc;

	if (file->getType() == FOLDER)
	{
		deleteBtnFunc = NULL;
	}
	else
	{
		deleteBtnFunc = [this, file] {
			CollectionSystemManager::get()->deleteCollectionFiles(file);
			ViewController::get()->getGameListView(file->getSystem()).get()->remove(file, true);
		};
	}

	mWindow->pushGui(new GuiMetaDataEd(mWindow, &file->metadata, file->metadata.getMDD(), p, Utils::FileSystem::getFileName(file->getPath()),
		std::bind(&IGameListView::onFileChanged, ViewController::get()->getGameListView(file->getSystem()).get(), file, FILE_METADATA_CHANGED), deleteBtnFunc, file));
}

void GuiGamelistOptions::jumpToLetter()
{
	char letter = mJumpToLetterList->getSelected();
	IGameListView* gamelist = getGamelist();

	// this is a really shitty way to get a list of files
	const std::vector<FileData*>& files = gamelist->getCursor()->getParent()->getChildrenListToDisplay();

	long min = 0;
	long max = (long)files.size() - 1;
	long mid = 0;

	while(max >= min)
	{
		mid = ((max - min) / 2) + min;

		// game somehow has no first character to check
		if(files.at(mid)->getName().empty())
			continue;

		char checkLetter = (char)toupper(files.at(mid)->getName()[0]);

		if(checkLetter < letter)
			min = mid + 1;
		else if(checkLetter > letter || (mid > 0 && (letter == toupper(files.at(mid - 1)->getName()[0]))))
			max = mid - 1;
		else
			break; //exact match found
	}

	gamelist->setCursor(files.at(mid));

	delete this;
}

bool GuiGamelistOptions::input(InputConfig* config, Input input)
{
	if((config->isMappedTo("b", input) || config->isMappedTo("select", input)) && input.value)
	{
		delete this;
		return true;
	}

	return mMenu.input(config, input);
}

HelpStyle GuiGamelistOptions::getHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(mSystem->getTheme(), "system");
	return style;
}

std::vector<HelpPrompt> GuiGamelistOptions::getHelpPrompts()
{
	auto prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("b", _("CLOSE")));
	return prompts;
}

IGameListView* GuiGamelistOptions::getGamelist()
{
	return ViewController::get()->getGameListView(mSystem).get();
}
