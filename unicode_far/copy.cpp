﻿/*
copy.cpp

Копирование файлов
*/
/*
Copyright © 1996 Eugene Roshal
Copyright © 2000 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "headers.hpp"
#pragma hdrstop

#include "copy.hpp"
#include "keys.hpp"
#include "flink.hpp"
#include "dialog.hpp"
#include "ctrlobj.hpp"
#include "filepanels.hpp"
#include "panel.hpp"
#include "foldtree.hpp"
#include "treelist.hpp"
#include "chgprior.hpp"
#include "scantree.hpp"
#include "filefilter.hpp"
#include "fileview.hpp"
#include "syslog.hpp"
#include "cddrv.hpp"
#include "interf.hpp"
#include "keyboard.hpp"
#include "colormix.hpp"
#include "message.hpp"
#include "stddlg.hpp"
#include "fileattr.hpp"
#include "datetime.hpp"
#include "dirinfo.hpp"
#include "pathmix.hpp"
#include "drivemix.hpp"
#include "dirmix.hpp"
#include "strmix.hpp"
#include "panelmix.hpp"
#include "processname.hpp"
#include "DlgGuid.hpp"
#include "console.hpp"
#include "language.hpp"
#include "manager.hpp"
#include "copy_progress.hpp"

enum
{
	SDDATA_SIZE = 64*1024,
};

enum
{
	COPY_RULE_NUL    = 0x0001,
	COPY_RULE_FILES  = 0x0002,
};

enum COPY_CODES: int
{
	COPY_CANCEL,
	COPY_SKIPPED,
	COPY_NOFILTER,                              // не считать размеры, т.к. файл не прошел по фильтру
	COPY_FAILURE,
	COPY_SUCCESS,
	COPY_SUCCESS_MOVE,
	COPY_RETRY,
};

enum COPY_FLAGS
{
	FCOPY_NONE                    = 0,
	FCOPY_COPYTONUL               = bit(0), // Признак копирования в NUL
	FCOPY_CURRENTONLY             = bit(1), // Только текущий?
	FCOPY_ONLYNEWERFILES          = bit(2), // Copy only newer files
	FCOPY_OVERWRITENEXT           = bit(3), // Overwrite all
	FCOPY_LINK                    = bit(4), // создание линков
	FCOPY_MOVE                    = bit(5), // перенос/переименование
	FCOPY_DIZREAD                 = bit(6), //
	FCOPY_COPYSECURITY            = bit(7), // [x] Copy access rights
	FCOPY_VOLMOUNT                = bit(8), // операция монтирования тома
	FCOPY_STREAMSKIP              = bit(9), // потоки
	FCOPY_STREAMALL               = bit(10), // потоки
	FCOPY_SKIPSETATTRFLD          = bit(11), // больше не пытаться ставить атрибуты для каталогов - когда нажали Skip All
	FCOPY_COPYSYMLINKCONTENTS     = bit(12), // Копировать содержимое символических связей?
	FCOPY_COPYPARENTSECURITY      = bit(13), // Накладывать родительские права, в случае если мы не копируем права доступа
	FCOPY_LEAVESECURITY           = bit(14), // Move: [?] Ничего не делать с правами доступа
	FCOPY_DECRYPTED_DESTINATION   = bit(15), // для криптованных файлов - расшифровывать...
	FCOPY_USESYSTEMCOPY           = bit(16), // использовать системную функцию копирования
	FCOPY_COPYLASTTIME            = bit(17), // При копировании в несколько каталогов устанавливается для последнего.
	FCOPY_UPDATEPPANEL            = bit(18), // необходимо обновить пассивную панель
};

enum COPYSECURITYOPTIONS
{
	CSO_MOVE_SETCOPYSECURITY       = bit(0),          // Move: по умолчанию выставлять опцию "Copy access rights"?
	CSO_MOVE_SETINHERITSECURITY    = bit(0) | bit(1), // Move: по умолчанию выставлять опцию "Inherit access rights"?
	CSO_MOVE_SESSIONSECURITY       = bit(2),          // Move: сохранять состояние "access rights" внутри сессии?
	CSO_COPY_SETCOPYSECURITY       = bit(3),          // Copy: по умолчанию выставлять опцию "Copy access rights"?
	CSO_COPY_SETINHERITSECURITY    = bit(3) | bit(4), // Copy: по умолчанию выставлять опцию "Inherit access rights"?
	CSO_COPY_SESSIONSECURITY       = bit(5),          // Copy: сохранять состояние "access rights" внутри сессии?
};

static int CopySecurityCopy=-1;
static int CopySecurityMove=-1;

static BOOL ZoomedState,IconicState;

static constexpr size_t default_copy_buffer_size = 32 * 1024;

enum enumShellCopy
{
	ID_SC_TITLE,
	ID_SC_TARGETTITLE,
	ID_SC_TARGETEDIT,
	ID_SC_SEPARATOR1,
	ID_SC_ACTITLE,
	ID_SC_ACLEAVE,
	ID_SC_ACCOPY,
	ID_SC_ACINHERIT,
	ID_SC_SEPARATOR2,
	ID_SC_COMBOTEXT,
	ID_SC_COMBO,
	ID_SC_COPYSYMLINK,
	ID_SC_MULTITARGET,
	ID_SC_SEPARATOR3,
	ID_SC_USEFILTER,
	ID_SC_SEPARATOR4,
	ID_SC_BTNCOPY,
	ID_SC_BTNTREE,
	ID_SC_BTNFILTER,
	ID_SC_BTNCANCEL,
	ID_SC_SOURCEFILENAME,
};

enum CopyMode
{
	CM_ASK,
	CM_OVERWRITE,
	CM_SKIP,
	CM_RENAME,
	CM_APPEND,
	CM_ONLYNEWER,
	CM_SEPARATOR,
	CM_ASKRO,
};

/* $ 25.05.2002 IS
 + Всегда работаем с реальными _длинными_ именами, в результате чего
   отлавливается ситуация, когда
   Src="D:\Program Files\filename"
   Dest="D:\PROGRA~1\filename"
   ("D:\PROGRA~1" - короткое имя для "D:\Program Files")
   считается, что имена тоже одинаковые, а раньше считалось,
   что они разные (функция не знала, что и в первом, и во втором случае
   путь один и тот же)
 ! Оптимизация - "велосипед" заменен на DeleteEndSlash
 ! Убираем всю самодеятельность по проверке имен с разным
   регистром из функции прочь, потому что это нужно делать только при
   переименовании, а функция вызывается и при копировании тоже.
   Теперь функция вернет 1, для случая имен src=path\filename,
   dest=path\filename (раньше возвращала 2 - т.е. сигнал об ошибке).
*/

int CmpFullNames(const string& Src,const string& Dest)
{
	const auto& ToFull = [](const string& in)
	{
		// получим полные пути с учетом символических связей
		// (ConvertNameToReal eliminates short names too)
		auto out = ConvertNameToReal(in);
		DeleteEndSlash(out);
		return out;
	};

	return !StrCmpI(ToFull(Src), ToFull(Dest));
}

bool CheckNulOrCon(const wchar_t *Src)
{
	if (HasPathPrefix(Src))
		Src+=4;

	return (!StrCmpNI(Src,L"nul",3) || !StrCmpNI(Src,L"con",3)) && (IsSlash(Src[3])||!Src[3]);
}

string GetParentFolder(const string& Src)
{
	auto Result = ConvertNameToReal(Src);
	CutToSlash(Result, true);
	return Result;
}

int CmpFullPath(const string& Src, const string& Dest)
{
	const auto& ToFull = [](const string& in)
	{
		auto out = GetParentFolder(in);
		DeleteEndSlash(out);
		// избавимся от коротких имен
		return ConvertNameToReal(out);
	};

	return !StrCmpI(ToFull(Src), ToFull(Dest));
}

static void GenerateName(string &strName, const string& Path)
{
	if (!Path.empty())
	{
		string strTmp=Path;
		AddEndSlash(strTmp);
		strTmp+=PointToName(strName);
		strName=strTmp;
	}

	string strExt=PointToExt(strName);
	size_t NameLength=strName.size()-strExt.size();

	// file (2).ext, file (3).ext and so on
	for (int i = 2; os::fs::exists(strName); ++i)
	{
		strName.resize(NameLength);
		strName += L" (" + str(i) + L")";
		strName+=strExt;
	}
}

void CheckAndUpdateConsole()
{
	const auto hWnd = Console().GetWindow();
	if (ZoomedState != IsZoomed(hWnd) && IconicState == IsIconic(hWnd))
	{
		ZoomedState = !ZoomedState;
		ChangeVideoMode(ZoomedState != FALSE);
	}
}

enum
{
	DM_CALLTREE = DM_USER+1,
	DM_SWITCHRO = DM_USER+2,
};

intptr_t ShellCopy::CopyDlgProc(Dialog* Dlg,intptr_t Msg,intptr_t Param1,void* Param2)
{
	switch (Msg)
	{
		case DN_INITDIALOG:
			Dlg->SendMessage(DM_SETCOMBOBOXEVENT,ID_SC_COMBO,ToPtr(CBET_KEY|CBET_MOUSE));
			Dlg->SendMessage(DM_SETINPUTNOTIFY, TRUE, nullptr);
			break;
		case DM_SWITCHRO:
		{
			FarListGetItem LGI={sizeof(FarListGetItem),CM_ASKRO};
			Dlg->SendMessage(DM_LISTGETITEM,ID_SC_COMBO,&LGI);

			if (LGI.Item.Flags&LIF_CHECKED)
				LGI.Item.Flags&=~LIF_CHECKED;
			else
				LGI.Item.Flags|=LIF_CHECKED;

			Dlg->SendMessage(DM_LISTUPDATE,ID_SC_COMBO,&LGI);
			Dlg->SendMessage(DM_REDRAW, 0, nullptr);
			return TRUE;
		}
		case DN_BTNCLICK:
		{
			if (Param1==ID_SC_USEFILTER) // "Use filter"
			{
				m_UseFilter = static_cast<FARCHECKEDSTATE>(reinterpret_cast<intptr_t>(Param2)) == BSTATE_CHECKED;
				return TRUE;
			}

			if (Param1 == ID_SC_BTNTREE) // Tree
			{
				Dlg->SendMessage(DM_CALLTREE, 0, nullptr);
				return FALSE;
			}
			else if (Param1 == ID_SC_BTNCOPY)
			{
				Dlg->SendMessage(DM_CLOSE, ID_SC_BTNCOPY, nullptr);
			}
			else if (Param1==ID_SC_BTNFILTER) // Filter
			{
				m_Filter->FilterEdit();
				return TRUE;
			}

			break;
		}
		case DN_CONTROLINPUT: // по поводу дерева!
		{
			const auto& record = *static_cast<const INPUT_RECORD*>(Param2);
			if (record.EventType==KEY_EVENT)
			{
				int key = InputRecordToKey(&record);
				if (!Global->Opt->Tree.TurnOffCompletely)
				{
					if (key == KEY_ALTF10 || key == KEY_RALTF10 || key == KEY_F10 || key == KEY_SHIFTF10)
					{
						AltF10 = (key == KEY_ALTF10 || key == KEY_RALTF10) ? 1 : (key == KEY_SHIFTF10 ? 2 : 0);
						Dlg->SendMessage(DM_CALLTREE, AltF10, nullptr);
						return TRUE;
					}
				}

				if (Param1 == ID_SC_COMBO)
				{
					if (Dlg->SendMessage(DM_LISTGETCURPOS, ID_SC_COMBO, nullptr) == CM_ASKRO)
					{
						if (key==KEY_ENTER || key==KEY_NUMENTER || key==KEY_INS || key==KEY_NUMPAD0 || key==KEY_SPACE)
						{
							return Dlg->SendMessage(DM_SWITCHRO, 0, nullptr);
						}
						else if (key == KEY_TAB)
						{
							Dlg->SendMessage(DM_SETDROPDOWNOPENED, 0, nullptr);
							return TRUE;
						}
					}
				}
			}
		}
		break;

		case DN_LISTHOTKEY:
			if(Param1==ID_SC_COMBO)
			{
				auto Index = reinterpret_cast<intptr_t>(Param2);
				if (Index == CM_ASKRO)
				{
					Dlg->SendMessage(DM_SWITCHRO, 0, nullptr);
					FarListPos flp = { sizeof(flp), Index, -1 };
					Dlg->SendMessage(DM_LISTSETCURPOS, Param1, &flp);
					return TRUE;
				}
			}
			break;
		case DN_INPUT:
			{
				auto ir = reinterpret_cast<INPUT_RECORD*>(Param2);
				if (ir->EventType == MOUSE_EVENT && Dlg->SendMessage(DM_GETDROPDOWNOPENED, ID_SC_COMBO, nullptr))
				{
					if (Dlg->SendMessage(DM_LISTGETCURPOS, ID_SC_COMBO, nullptr) == CM_ASKRO && ir->Event.MouseEvent.dwButtonState && !(ir->Event.MouseEvent.dwEventFlags & MOUSE_MOVED))
					{
						Dlg->SendMessage(DM_SWITCHRO, 0, nullptr);
						return FALSE;
					}
				}
			}
			break;
		case DM_CALLTREE:
		{
			/* $ 13.10.2001 IS
			   + При мультикопировании добавляем выбранный в "дереве" каталог к уже
			     существующему списку через точку с запятой.
			   - Баг: при мультикопировании выбранный в "дереве" каталог не
			     заключался в кавычки, если он содержал в своем
			     имени символы-разделители.
			   - Баг: неправильно работало Shift-F10, если строка ввода содержала
			     слеш на конце.
			   - Баг: неправильно работало Shift-F10 при мультикопировании -
			     показывался корневой каталог, теперь показывается самый первый каталог
			     в списке.
			*/
			BOOL MultiCopy = Dlg->SendMessage(DM_GETCHECK, ID_SC_MULTITARGET, nullptr) == BSTATE_CHECKED;
			string strOldFolder = reinterpret_cast<const wchar_t*>(Dlg->SendMessage(DM_GETCONSTTEXTPTR, ID_SC_TARGETEDIT, nullptr));
			string strNewFolder;

			if (AltF10 == 2)
			{
				strNewFolder = strOldFolder;

				if (MultiCopy)
				{
					const auto DestList = split<std::vector<string>>(strOldFolder, STLF_UNIQUE);
					if (!DestList.empty())
						strNewFolder = DestList.front();
				}

				if (strNewFolder.empty())
					AltF10=-1;
				else // убираем лишний слеш
					DeleteEndSlash(strNewFolder);
			}

			if (AltF10 != -1)
			{
				{
					string strNewFolder2 = strNewFolder;
					FolderTree::create(strNewFolder2,
					                (AltF10==1?MODALTREE_PASSIVE:
					                 (AltF10==2?MODALTREE_FREE:
					                  MODALTREE_ACTIVE)),
					                FALSE, false);
					strNewFolder = strNewFolder2;
				}

				if (!strNewFolder.empty())
				{
					AddEndSlash(strNewFolder);

					if (MultiCopy) // мультикопирование
					{
						// Добавим кавычки, если имя каталога содержит символы-разделители
						if (strNewFolder.find_first_of(L";,") != string::npos)
							InsertQuote(strNewFolder);

						if (strOldFolder.size())
							strOldFolder += L";"; // добавим разделитель к непустому списку

						strOldFolder += strNewFolder;
						strNewFolder = strOldFolder;
					}

					Dlg->SendMessage(DM_SETTEXTPTR,ID_SC_TARGETEDIT, UNSAFE_CSTR(strNewFolder));
					Dlg->SendMessage(DM_SETFOCUS, ID_SC_TARGETEDIT, nullptr);
				}
			}

			AltF10=0;
			return TRUE;
		}
		case DN_CLOSE:
		{
			if (Param1==ID_SC_BTNCOPY)
			{
				FarListGetItem LGI={sizeof(FarListGetItem),CM_ASKRO};
				Dlg->SendMessage(DM_LISTGETITEM,ID_SC_COMBO,&LGI);

				if (LGI.Item.Flags&LIF_CHECKED)
					AskRO = true;
			}
		}
		break;

		default:
			break;
	}

	return Dlg->DefProc(Msg,Param1,Param2);
}

ShellCopy::ShellCopy(panel_ptr SrcPanel,     // исходная панель (активная)
                     int Move,               // =1 - операция Move
                     int Link,               // =1 - Sym/Hard Link
                     int CurrentOnly,        // =1 - только текущий файл, под курсором
                     int Ask,                // =1 - выводить диалог?
                     int &ToPlugin,          // =?
                     const wchar_t* PluginDestPath,
                     bool ToSubdir):
	m_Filter(std::make_unique<FileFilter>(SrcPanel.get(), FFT_COPY)),
	Flags((Move? FCOPY_MOVE : FCOPY_NONE) | (Link? FCOPY_LINK : FCOPY_NONE) | (CurrentOnly? FCOPY_CURRENTONLY : FCOPY_NONE)),
	SrcPanel(SrcPanel),
	DestPanel(Global->CtrlObject->Cp()->GetAnotherPanel(SrcPanel)),
	SrcPanelMode(SrcPanel->GetMode()),
	DestPanelMode(ToPlugin? DestPanel->GetMode() : panel_mode::NORMAL_PANEL),
	SrcDriveType(),
	DestDriveType(),
	CopyBufferSize(!Global->Opt->CMOpt.BufferSize.Get()? default_copy_buffer_size : Global->Opt->CMOpt.BufferSize.Get()),
	SelectedFolderNameLength(),
	RPT(RP_EXACTCOPY),
	AltF10(),
	m_CopySecurity(),
	SelCount(SrcPanel->GetSelCount()),
	m_FileAttr(),
	FolderPresent(),
	FilesPresent(),
	AskRO(),
	m_UseFilter(),
	m_FileHandleForStreamSizeFix(),
	m_NumberOfTargets()
{
	if (!SelCount)
		return;

	string strSelName;
	unsigned long long SingleSelectedFileSize = 0;

	if (SelCount==1)
	{
		SrcPanel->GetSelName(nullptr,m_FileAttr); //????
		os::FAR_FIND_DATA fd;
		SrcPanel->GetSelName(&strSelName,m_FileAttr, nullptr, &fd);
		if (TestParentFolderName(strSelName))
			return;
		SingleSelectedFileSize = fd.nFileSize;
	}

	ZoomedState=IsZoomed(Console().GetWindow());
	IconicState=IsIconic(Console().GetWindow());
	bool ShowTotalCopySize = Global->Opt->CMOpt.CopyShowTotal;
	int DestPlugin=ToPlugin;
	ToPlugin=FALSE;

	// ***********************************************************************
	// *** Prepare Dialog Controls
	// ***********************************************************************
	int DLG_HEIGHT=16, DLG_WIDTH=76;

	FARDIALOGITEMFLAGS no_tree = Global->Opt->Tree.TurnOffCompletely ? DIF_HIDDEN|DIF_DISABLE : 0;

	FarDialogItem CopyDlgData[]=
	{
		{DI_DOUBLEBOX,   3, 1,DLG_WIDTH-4,DLG_HEIGHT-2,0,nullptr,nullptr,0,MSG(lng::MCopyDlgTitle)},
		{DI_TEXT,        5, 2, 0, 2,0,nullptr,nullptr,0,MSG(Link? lng::MCMLTargetIN : lng::MCMLTargetTO)},
		{DI_EDIT,        5, 3,70, 3,0,L"Copy",nullptr,DIF_FOCUS|DIF_HISTORY|DIF_USELASTHISTORY|DIF_EDITPATH,L""},
		{DI_TEXT,       -1, 4, 0, 4,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_TEXT,        5, 5, 0, 5,0,nullptr,nullptr,0,MSG(lng::MCopySecurity)},
		{DI_RADIOBUTTON, 5, 5, 0, 5,0,nullptr,nullptr,DIF_GROUP,MSG(lng::MCopySecurityLeave)},
		{DI_RADIOBUTTON, 5, 5, 0, 5,0,nullptr,nullptr,0,MSG(lng::MCopySecurityCopy)},
		{DI_RADIOBUTTON, 5, 5, 0, 5,0,nullptr,nullptr,0,MSG(lng::MCopySecurityInherit)},
		{DI_TEXT,       -1, 6, 0, 6,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_TEXT,        5, 7, 0, 7,0,nullptr,nullptr,0,MSG(lng::MCopyIfFileExist)},
		{DI_COMBOBOX,   29, 7,70, 7,0,nullptr,nullptr,DIF_DROPDOWNLIST|DIF_LISTNOAMPERSAND|DIF_LISTWRAPMODE,L""},
		{DI_CHECKBOX,    5, 8, 0, 8,0,nullptr,nullptr,0,MSG(lng::MCopySymLinkContents)},
		{DI_CHECKBOX,    5, 9, 0, 9,0,nullptr,nullptr,0,MSG(lng::MCopyMultiActions)},
		{DI_TEXT,       -1,10, 0,10,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_CHECKBOX,    5,11, 0,11,(int)(m_UseFilter? BSTATE_CHECKED : BSTATE_UNCHECKED), nullptr, nullptr, DIF_AUTOMATION, MSG(lng::MCopyUseFilter)},
		{DI_TEXT,       -1,12, 0,12,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_BUTTON,      0,13, 0,13,0,nullptr,nullptr,DIF_DEFAULTBUTTON|DIF_CENTERGROUP,MSG(lng::MCopyDlgCopy)},
		{DI_BUTTON,      0,13, 0,13,0,nullptr,nullptr,no_tree|DIF_CENTERGROUP|DIF_BTNNOCLOSE,MSG(lng::MCopyDlgTree)},
		{DI_BUTTON,      0,13, 0,13,0,nullptr,nullptr,DIF_CENTERGROUP|DIF_BTNNOCLOSE|DIF_AUTOMATION|(m_UseFilter? 0 : DIF_DISABLE), MSG(lng::MCopySetFilter)},
		{DI_BUTTON,      0,13, 0,13,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(lng::MCopyDlgCancel)},
		{DI_TEXT,        5, 2, 0, 2,0,nullptr,nullptr,DIF_SHOWAMPERSAND,L""},
	};
	auto CopyDlg = MakeDialogItemsEx(CopyDlgData);
	CopyDlg[ID_SC_MULTITARGET].Selected=Global->Opt->CMOpt.MultiCopy;
	{
		const wchar_t *Str = MSG(lng::MCopySecurity);
		CopyDlg[ID_SC_ACLEAVE].X1 = CopyDlg[ID_SC_ACTITLE].X1 + wcslen(Str) - (wcschr(Str, L'&')?1:0) + 1;
		Str = MSG(lng::MCopySecurityLeave);
		CopyDlg[ID_SC_ACCOPY].X1 = CopyDlg[ID_SC_ACLEAVE].X1 + wcslen(Str) - (wcschr(Str, L'&') ? 1 : 0) + 5;
		Str = MSG(lng::MCopySecurityCopy);
		CopyDlg[ID_SC_ACINHERIT].X1 = CopyDlg[ID_SC_ACCOPY].X1 + wcslen(Str) - (wcschr(Str, L'&') ? 1 : 0) + 5;
	}

	if (Link)
	{
		CopyDlg[ID_SC_COMBOTEXT].strData=MSG(lng::MLinkType);
		CopyDlg[ID_SC_COPYSYMLINK].Selected=0;
		CopyDlg[ID_SC_COPYSYMLINK].Flags|=DIF_DISABLE|DIF_HIDDEN;
		m_CopySecurity=1;
	}
	else if (Move) // секция про перенос
	{
		CopyDlg[ID_SC_MULTITARGET].Selected = 0;
		CopyDlg[ID_SC_MULTITARGET].Flags |= DIF_DISABLE;

		//   2 - Default
		//   1 - Copy access rights
		//   0 - Inherit access rights
		m_CopySecurity=2;

		// ставить опцию "Inherit access rights"?
		// CSO_MOVE_SETINHERITSECURITY - двухбитный флаг
		if ((Global->Opt->CMOpt.CopySecurityOptions&CSO_MOVE_SETINHERITSECURITY) == CSO_MOVE_SETINHERITSECURITY)
			m_CopySecurity=0;
		else if (Global->Opt->CMOpt.CopySecurityOptions&CSO_MOVE_SETCOPYSECURITY)
			m_CopySecurity=1;

		// хотели сессионное запоминание?
		if (CopySecurityMove != -1 && (Global->Opt->CMOpt.CopySecurityOptions&CSO_MOVE_SESSIONSECURITY))
			m_CopySecurity=CopySecurityMove;
		else
			CopySecurityMove=m_CopySecurity;
	}
	else // секция про копирование
	{
		//   2 - Default
		//   1 - Copy access rights
		//   0 - Inherit access rights
		m_CopySecurity=2;

		// ставить опцию "Inherit access rights"?
		// CSO_COPY_SETINHERITSECURITY - двухбитный флаг
		if ((Global->Opt->CMOpt.CopySecurityOptions&CSO_COPY_SETINHERITSECURITY) == CSO_COPY_SETINHERITSECURITY)
			m_CopySecurity=0;
		else if (Global->Opt->CMOpt.CopySecurityOptions&CSO_COPY_SETCOPYSECURITY)
			m_CopySecurity=1;

		// хотели сессионное запоминание?
		if (CopySecurityCopy != -1 && Global->Opt->CMOpt.CopySecurityOptions&CSO_COPY_SESSIONSECURITY)
			m_CopySecurity=CopySecurityCopy;
		else
			CopySecurityCopy=m_CopySecurity;
	}

	// вот теперь выставляем
	if (m_CopySecurity)
	{
		if (m_CopySecurity == 1)
		{
			Flags|=FCOPY_COPYSECURITY;
			CopyDlg[ID_SC_ACCOPY].Selected=1;
		}
		else
		{
			Flags|=FCOPY_LEAVESECURITY;
			CopyDlg[ID_SC_ACLEAVE].Selected=1;
		}
	}
	else
	{
		Flags&=~(FCOPY_COPYSECURITY|FCOPY_LEAVESECURITY);
		CopyDlg[ID_SC_ACINHERIT].Selected=1;
	}

	string strCopyStr;

	if (SelCount==1)
	{
		if (SrcPanel->GetType() == panel_type::TREE_PANEL)
		{
			string strNewDir(strSelName);
			const auto pos = FindLastSlash(strNewDir);
			if (pos != string::npos)
			{
				strNewDir.resize(pos);

				if (!pos || strNewDir[pos-1]==L':')
					strNewDir += L"\\";

				FarChDir(strNewDir);
			}
		}

		string strSelNameShort(strSelName);
		QuoteOuterSpace(strSelNameShort);
		strCopyStr=MSG(Move? lng::MMoveFile : Link? lng::MLinkFile : lng::MCopyFile);
		TruncPathStr(strSelNameShort,static_cast<int>(CopyDlg[ID_SC_TITLE].X2-CopyDlg[ID_SC_TITLE].X1-strCopyStr.size()-7));
		strCopyStr+=L" "+strSelNameShort;

		// Если копируем одиночный файл, то запрещаем использовать фильтр
		if (!(m_FileAttr&FILE_ATTRIBUTE_DIRECTORY))
		{
			CopyDlg[ID_SC_USEFILTER].Selected=0;
			CopyDlg[ID_SC_USEFILTER].Flags|=DIF_DISABLE;
		}
	}
	else // Объектов несколько!
	{

		// коррекция языка - про окончания
		auto StrItems = str(SelCount);
		size_t LenItems=StrItems.size();
		auto NItems = lng::MCMLItemsA;

		if (LenItems > 0)
		{
			if ((LenItems >= 2 && StrItems[LenItems-2] == '1') ||
			        StrItems[LenItems-1] >= '5' ||
			        StrItems[LenItems-1] == '0')
				NItems = lng::MCMLItemsS;
			else if (StrItems[LenItems-1] == '1')
				NItems = lng::MCMLItems0;
		}
		strCopyStr = format(Move? lng::MMoveFiles : Link? lng::MLinkFiles : lng::MCopyFiles, SelCount, MSG(NItems));
	}

	CopyDlg[ID_SC_SOURCEFILENAME].strData=strCopyStr;
	CopyDlg[ID_SC_TITLE].strData = MSG(Move? lng::MMoveDlgTitle : Link? lng::MLinkDlgTitle : lng::MCopyDlgTitle);
	CopyDlg[ID_SC_BTNCOPY].strData = MSG(Move? lng::MCopyDlgRename: Link? lng::MCopyDlgLink : lng::MCopyDlgCopy);

	if (DestPanelMode == panel_mode::PLUGIN_PANEL)
	{
		// Если противоположная панель - плагин, то дисаблим OnlyNewer //?????
/*
		CopySecurity=2;
		CopyDlg[ID_SC_ACCOPY].Selected=0;
		CopyDlg[ID_SC_ACINHERIT].Selected=0;
		CopyDlg[ID_SC_ACLEAVE].Selected=1;
		CopyDlg[ID_SC_ACCOPY].Flags|=DIF_DISABLE;
		CopyDlg[ID_SC_ACINHERIT].Flags|=DIF_DISABLE;
		CopyDlg[ID_SC_ACLEAVE].Flags|=DIF_DISABLE;
*/
	}

	string strDestDir(DestPanel->GetCurDir());
	if(ToSubdir)
	{
		AddEndSlash(strDestDir);
		string strSubdir, strShort;
		DestPanel->GetCurName(strSubdir, strShort);
		strDestDir+=strSubdir;
	}
	string strSrcDir(SrcPanel->GetCurDir());

	if (CurrentOnly)
	{
		//   При копировании только элемента под курсором берем его имя в кавычки, если оно содержит разделители.
		CopyDlg[ID_SC_TARGETEDIT].strData = strSelName;

		if (!Move && CopyDlg[ID_SC_TARGETEDIT].strData.find_first_of(L",;") != string::npos)
		{
			// уберем все лишние кавычки
			// возьмем в кавычки, т.к. могут быть разделители
			InsertQuote(Unquote(CopyDlg[ID_SC_TARGETEDIT].strData));
		}
	}
	else
	{
		switch (DestPanelMode)
		{
		case panel_mode::NORMAL_PANEL:
			{
				if ((strDestDir.empty() || !DestPanel->IsVisible() || !StrCmpI(strSrcDir, strDestDir)) && SelCount==1)
					CopyDlg[ID_SC_TARGETEDIT].strData = strSelName;
				else
				{
					CopyDlg[ID_SC_TARGETEDIT].strData = strDestDir;
					AddEndSlash(CopyDlg[ID_SC_TARGETEDIT].strData);
				}

				/* $ 19.07.2003 IS
				   Если цель содержит разделители, то возьмем ее в кавычки, дабы не получить
				   ерунду при F5, Enter в панелях, когда пользователь включит MultiCopy
				*/
				if (!Move && CopyDlg[ID_SC_TARGETEDIT].strData.find_first_of(L",;") != string::npos)
				{
					// уберем все лишние кавычки
					// возьмем в кавычки, т.к. могут быть разделители
					InsertQuote(Unquote(CopyDlg[ID_SC_TARGETEDIT].strData));
				}

				break;
			}

		case panel_mode::PLUGIN_PANEL:
			{
				OpenPanelInfo Info;
				DestPanel->GetOpenPanelInfo(&Info);
				string strFormat = NullToEmpty(Info.Format);
				CopyDlg[ID_SC_TARGETEDIT].strData = strFormat+L":";

				while (CopyDlg[ID_SC_TARGETEDIT].strData.size()<2)
					CopyDlg[ID_SC_TARGETEDIT].strData += L":";

				strPluginFormat = Upper(CopyDlg[ID_SC_TARGETEDIT].strData);
				break;
			}
		}
	}

	string strInitDestDir = CopyDlg[ID_SC_TARGETEDIT].strData;
	// Для фильтра
	os::FAR_FIND_DATA fd;
	SrcPanel->GetSelName(nullptr,m_FileAttr);

	bool AddSlash=false;

	while (SrcPanel->GetSelName(&strSelName,m_FileAttr,nullptr,&fd))
	{
		if (m_UseFilter)
		{
			if (!m_Filter->FileInFilter(fd, nullptr, &fd.strFileName))
				continue;
		}

		if (m_FileAttr & FILE_ATTRIBUTE_DIRECTORY)
		{
			FolderPresent=true;
			AddSlash=true;
//      break;
		}
		else
		{
			FilesPresent=true;
		}
	}

	if (Link) // рулесы по поводу линков (предварительные!)
	{
		// задисаблим опцию про копирование права.
		CopyDlg[ID_SC_ACTITLE].Flags|=DIF_DISABLE|DIF_HIDDEN;
		CopyDlg[ID_SC_ACCOPY].Flags|=DIF_DISABLE|DIF_HIDDEN;
		CopyDlg[ID_SC_ACINHERIT].Flags|=DIF_DISABLE|DIF_HIDDEN;
		CopyDlg[ID_SC_ACLEAVE].Flags|=DIF_DISABLE|DIF_HIDDEN;
		CopyDlg[ID_SC_SEPARATOR2].Flags|=DIF_HIDDEN;

		for(int i=ID_SC_SEPARATOR2;i<=ID_SC_COMBO;i++)
		{
			CopyDlg[i].Y1-=2;
			CopyDlg[i].Y2-=2;
		}
		for(int i=ID_SC_MULTITARGET;i<=ID_SC_BTNCANCEL;i++)
		{
			CopyDlg[i].Y1-=3;
			CopyDlg[i].Y2-=3;
		}
		CopyDlg[ID_SC_TITLE].Y2-=3;
		DLG_HEIGHT-=3;
	}

	// корректируем позицию " to"
	CopyDlg[ID_SC_TARGETTITLE].X1=CopyDlg[ID_SC_TARGETTITLE].X2=CopyDlg[ID_SC_SOURCEFILENAME].X1+CopyDlg[ID_SC_SOURCEFILENAME].strData.size();

	/* $ 15.06.2002 IS
	   Обработка копирования мышкой - в этом случае диалог не показывается,
	   но переменные все равно инициализируются. Если произойдет неудачная
	   компиляция списка целей, то покажем диалог.
	*/
	string strCopyDlgValue;
	if (!Ask)
	{
		strCopyDlgValue = os::env::expand_strings(CopyDlg[ID_SC_TARGETEDIT].strData);
		m_DestList = split<std::vector<string>>(InsertQuote(Unquote(strCopyDlgValue)), STLF_UNIQUE);
		if (m_DestList.empty())
			Ask=TRUE;
	}

	// ***********************************************************************
	// *** Вывод и обработка диалога
	// ***********************************************************************
	if (Ask)
	{
		FarList ComboList={sizeof(FarList)};
		FarListItem LinkTypeItems[5]={},CopyModeItems[8]={};

		if (Link)
		{
			ComboList.ItemsNumber=std::size(LinkTypeItems);
			ComboList.Items=LinkTypeItems;
			ComboList.Items[0].Text=MSG(lng::MLinkTypeHardlink);
			ComboList.Items[1].Text=MSG(lng::MLinkTypeJunction);
			ComboList.Items[2].Text=MSG(lng::MLinkTypeSymlink);
			ComboList.Items[3].Text=MSG(lng::MLinkTypeSymlinkFile);
			ComboList.Items[4].Text=MSG(lng::MLinkTypeSymlinkDirectory);

			if (FilesPresent)
				ComboList.Items[0].Flags|=LIF_SELECTED;
			else
				ComboList.Items[1].Flags|=LIF_SELECTED;
		}
		else
		{
			ComboList.ItemsNumber=std::size(CopyModeItems);
			ComboList.Items=CopyModeItems;
			ComboList.Items[CM_ASK].Text=MSG(lng::MCopyAsk);
			ComboList.Items[CM_OVERWRITE].Text=MSG(lng::MCopyOverwrite);
			ComboList.Items[CM_SKIP].Text=MSG(lng::MCopySkipOvr);
			ComboList.Items[CM_RENAME].Text=MSG(lng::MCopyRename);
			ComboList.Items[CM_APPEND].Text=MSG(lng::MCopyAppend);
			ComboList.Items[CM_ONLYNEWER].Text=MSG(lng::MCopyOnlyNewerFiles);
			ComboList.Items[CM_ASKRO].Text=MSG(lng::MCopyAskRO);
			ComboList.Items[CM_ASK].Flags=LIF_SELECTED;
			ComboList.Items[CM_SEPARATOR].Flags=LIF_SEPARATOR;

			if (Global->Opt->Confirm.RO)
			{
				ComboList.Items[CM_ASKRO].Flags=LIF_CHECKED;
			}
		}

		CopyDlg[ID_SC_COMBO].ListItems=&ComboList;
		const auto Dlg = Dialog::create(CopyDlg, &ShellCopy::CopyDlgProc, this);
		Dlg->SetHelp(Link?L"HardSymLink":L"CopyFiles");
		Dlg->SetId(Link?HardSymLinkId:(Move?MoveFilesId:CopyFilesId));
		Dlg->SetPosition(-1,-1,DLG_WIDTH,DLG_HEIGHT);
		Dlg->SetAutomation(ID_SC_USEFILTER,ID_SC_BTNFILTER,DIF_DISABLE,DIF_NONE,DIF_NONE,DIF_DISABLE);
//    Dlg->Show();
		// $ 02.06.2001 IS + Проверим список целей и поднимем тревогу, если он содержит ошибки
		int DlgExitCode;

		for (;;)
		{
			Dlg->ClearDone();
			Dlg->Process();
			DlgExitCode=Dlg->GetExitCode();
			//Рефреш текущему времени для фильтра сразу после выхода из диалога
			m_Filter->UpdateCurrentTime();

			if (DlgExitCode == ID_SC_BTNCOPY)
			{
				/* $ 03.08.2001 IS
				   Запомним строчку из диалога и начинаем ее мучить в зависимости от
				   состояния опции мультикопирования
				*/
				auto tmp = strCopyDlgValue = CopyDlg[ID_SC_TARGETEDIT].strData;
				DeleteEndSlash(tmp);
				if (tmp != DestPanel->GetCurDir())
					strCopyDlgValue = os::env::expand_strings(strCopyDlgValue);

				if(!Move)
				{
					Global->Opt->CMOpt.MultiCopy=CopyDlg[ID_SC_MULTITARGET].Selected == BSTATE_CHECKED;
				}

				if (!CopyDlg[ID_SC_MULTITARGET].Selected || strCopyDlgValue.find_first_of(L",;") == string::npos) // отключено multi*
				{
					// уберем лишние кавычки
					// добавим кавычки, чтобы "список" удачно скомпилировался вне
					// зависимости от наличия разделителей в оном
					InsertQuote(Unquote(strCopyDlgValue));
				}

				m_DestList = split<std::vector<string>>(strCopyDlgValue, STLF_UNIQUE);
				if (!m_DestList.empty())
				{
					// Запомнить признак использования фильтра. KM
					m_UseFilter = CopyDlg[ID_SC_USEFILTER].Selected == BSTATE_CHECKED;
					break;
				}
				else
				{
					Message(MSG_WARNING,1,MSG(lng::MWarning),MSG(lng::MCopyIncorrectTargetList), MSG(lng::MOk));
				}
			}
			else
				break;
		}

		if (DlgExitCode == ID_SC_BTNCANCEL || DlgExitCode < 0 || (CopyDlg[ID_SC_BTNCOPY].Flags&DIF_DISABLE))
		{
			if (DestPlugin)
				ToPlugin=-1;

			return;
		}
	}

	// ***********************************************************************
	// *** Стадия подготовки данных после диалога
	// ***********************************************************************
	Flags&=~FCOPY_COPYPARENTSECURITY;

	if (CopyDlg[ID_SC_ACCOPY].Selected)
	{
		Flags|=FCOPY_COPYSECURITY;
	}
	else if (CopyDlg[ID_SC_ACINHERIT].Selected)
	{
		Flags&=~(FCOPY_COPYSECURITY|FCOPY_LEAVESECURITY);
	}
	else
	{
		Flags|=FCOPY_LEAVESECURITY;
	}

	if (Global->Opt->CMOpt.UseSystemCopy)
		Flags|=FCOPY_USESYSTEMCOPY;
	else
		Flags&=~FCOPY_USESYSTEMCOPY;

	if (!(Flags&(FCOPY_COPYSECURITY|FCOPY_LEAVESECURITY)))
		Flags|=FCOPY_COPYPARENTSECURITY;

	m_CopySecurity=Flags&FCOPY_COPYSECURITY?1:(Flags&FCOPY_LEAVESECURITY?2:0);

	// в любом случае сохраняем сессионное запоминание (не для Link, т.к. для Link временное состояние - "ВСЕГДА!")
	if (!Link)
	{
		if (Move)
			CopySecurityMove=m_CopySecurity;
		else
			CopySecurityCopy=m_CopySecurity;
	}

	ReadOnlyDelMode=ReadOnlyOvrMode=OvrMode=SkipEncMode=SkipMode=SkipDeleteMode=-1;
	SkipSecurityErrors = false;

	if (Link)
	{
		switch (CopyDlg[ID_SC_COMBO].ListPos)
		{
			case 0:
				RPT=RP_HARDLINK;
				break;
			case 1:
				RPT=RP_JUNCTION;
				break;
			case 2:
				RPT=RP_SYMLINK;
				break;
			case 3:
				RPT=RP_SYMLINKFILE;
				break;
			case 4:
				RPT=RP_SYMLINKDIR;
				break;
		}
	}
	else
	{
		ReadOnlyOvrMode=AskRO?-1:1;

		switch (CopyDlg[ID_SC_COMBO].ListPos)
		{
			case CM_ASK:
				OvrMode=-1;
				break;
			case CM_OVERWRITE:
				OvrMode=1;
				break;
			case CM_SKIP:
				OvrMode=3;
				ReadOnlyOvrMode=AskRO?-1:3;
				break;
			case CM_RENAME:
				OvrMode=5;
				break;
			case CM_APPEND:
				OvrMode=7;
				break;
			case CM_ONLYNEWER:
				Flags|=FCOPY_ONLYNEWERFILES;
				break;
		}
	}

	Flags|=CopyDlg[ID_SC_COPYSYMLINK].Selected? FCOPY_COPYSYMLINKCONTENTS : FCOPY_NONE;

	if (DestPlugin && CopyDlg[ID_SC_TARGETEDIT].strData == strInitDestDir)
	{
		ToPlugin=1;
		return;
	}

	if (DestPlugin==2)
	{
		if (PluginDestPath)
			strCopyDlgValue = PluginDestPath;

		return;
	}

	if ((Global->Opt->Diz.UpdateMode==DIZ_UPDATE_IF_DISPLAYED && SrcPanel->IsDizDisplayed()) ||
	        Global->Opt->Diz.UpdateMode==DIZ_UPDATE_ALWAYS)
	{
		Global->CtrlObject->Cp()->LeftPanel()->ReadDiz();
		Global->CtrlObject->Cp()->RightPanel()->ReadDiz();
	}

	CopyBuffer.reset(CopyBufferSize);
	DestPanel->CloseFile();
	strDestDizPath.clear();
	SrcPanel->SaveSelection();
	// TODO: Posix - bugbug
	ReplaceSlashToBackslash(strCopyDlgValue);
	// нужно ли показывать время копирования?
	// ***********************************************************************
	// **** Здесь все подготовительные операции закончены, можно приступать
	// **** к процессу Copy/Move/Link
	// ***********************************************************************
	int NeedDizUpdate=FALSE;
	int NeedUpdateAPanel=FALSE;
	// ПОКА! принудительно выставим обновление.
	// В последствии этот флаг будет выставляться в ShellCopy::CheckUpdatePanel()
	Flags|=FCOPY_UPDATEPPANEL;
	/*
	   ЕСЛИ ПРИНЯТЬ В КАЧЕСТВЕ РАЗДЕЛИТЕЛЯ ПУТЕЙ, НАПРИМЕР ';',
	   то нужно парсить CopyDlgValue на предмет MultiCopy и
	   вызывать CopyFileTree нужное количество раз.
	*/
	{
		Flags&=~FCOPY_MOVE;
		m_DestList = split<std::vector<string>>(strCopyDlgValue, STLF_UNIQUE);
		if (!m_DestList.empty())
		{
			string strNameTmp;
			// посчитаем количество целей.
			m_NumberOfTargets=m_DestList.size();

			if (m_NumberOfTargets > 1)
				Move=0;

			FOR_CONST_RANGE(m_DestList, i)
			{
				bool LastIteration = false;
				{
					if (i + 1 == m_DestList.end())
					{
						LastIteration = true;
					}
				}

				strNameTmp = *i;

				if ((strNameTmp.size() == 2) && IsAlpha(strNameTmp[0]) && (strNameTmp[1] == L':'))
					PrepareDiskPath(strNameTmp);

				if (CheckNulOrCon(strNameTmp.data()))
				{
					Flags|=FCOPY_COPYTONUL;
					strNameTmp = L"\\\\?\\nul\\";
				}
				else
					Flags&=~FCOPY_COPYTONUL;

				if (Flags&FCOPY_COPYTONUL)
				{
					Flags&=~FCOPY_MOVE;
					Move=0;
				}
				bool ShowCopyTime=(Global->Opt->CMOpt.CopyTimeRule&((Flags&FCOPY_COPYTONUL)?COPY_RULE_NUL:COPY_RULE_FILES))!=0;

				if (SelCount==1 || (Flags&FCOPY_COPYTONUL))
					AddSlash=false; //???


				if (LastIteration) // нужно учесть моменты связанные с операцией Move.
				{
					Flags |= FCOPY_COPYLASTTIME | (Move? FCOPY_MOVE : FCOPY_NONE); // только для последней операции
				}

				// Если выделенных элементов больше 1 и среди них есть каталог, то всегда
				// делаем так, чтобы на конце был '\\'
				// делаем так не всегда, а только когда NameTmp не является маской.
				if (AddSlash && strNameTmp.find_first_of(L"*?") == string::npos)
					AddEndSlash(strNameTmp);

				if (SelCount==1 && !FolderPresent)
				{
					ShowTotalCopySize = false;
				}

				if (Move) // при перемещении "тотал" так же скидывается для "того же диска"
				{
					if (GetPathRoot(strSrcDir) == GetPathRoot(strNameTmp))
						ShowTotalCopySize = false;
					if (SelCount==1 && FolderPresent && CheckUpdateAnotherPanel(SrcPanel,strSelName))
					{
						NeedUpdateAPanel=TRUE;
					}
				}

				if (!CP)
					CP = std::make_unique<copy_progress>(Move != 0, ShowTotalCopySize, ShowCopyTime);

				CP->m_Bytes.CurrCopied = 0;

				if (SelCount == 1 && !FolderPresent)
				{
					CP->m_Files.Total = 1;
					CP->m_Bytes.Total = SingleSelectedFileSize;
				}

				// Обнулим инфу про дизы
				strDestDizPath.clear();
				Flags&=~FCOPY_DIZREAD;
				// сохраним выделение
				SrcPanel->SaveSelection();
				strDestFSName.clear();
				int OldCopySymlinkContents=Flags&FCOPY_COPYSYMLINKCONTENTS;
				// собственно - один проход копирования
				// Mantis#45: Необходимо привести копирование ссылок на папки с NTFS на FAT к более логичному виду
				{
					DWORD FilesystemFlags;
					if (os::GetVolumeInformation(GetPathRoot(strNameTmp), nullptr, nullptr, nullptr, &FilesystemFlags, nullptr) && !(FilesystemFlags&FILE_SUPPORTS_REPARSE_POINTS))
						Flags|=FCOPY_COPYSYMLINKCONTENTS;
				}

				const auto I = CopyFileTree(strNameTmp);

				if (OldCopySymlinkContents)
					Flags|=FCOPY_COPYSYMLINKCONTENTS;
				else
					Flags&=~FCOPY_COPYSYMLINKCONTENTS;

				if (I == COPY_CANCEL)
				{
					NeedDizUpdate=TRUE;
					break;
				}

				// если "есть порох в пороховницах" - восстановим выделение
				if (!LastIteration)
					SrcPanel->RestoreSelection();

				// Позаботимся о дизах.
				if (!(Flags&FCOPY_COPYTONUL) && !strDestDizPath.empty())
				{
					const auto Attr = os::GetFileAttributes(DestDiz.GetDizName());
					int DestReadOnly=(Attr!=INVALID_FILE_ATTRIBUTES && (Attr & FILE_ATTRIBUTE_READONLY));

					if (LastIteration) // Скидываем только во время последней Op.
						if (Move && !DestReadOnly)
							SrcPanel->FlushDiz();

					DestDiz.Flush(strDestDizPath);
				}
			}
		}
		_LOGCOPYR(else SysLog(L"Error: DestList.Set(CopyDlgValue) return FALSE"));
	}
	// ***********************************************************************
	// *** заключительная стадия процесса
	// *** восстанавливаем/дизим/редравим
	// ***********************************************************************

	if (NeedDizUpdate) // при мультикопировании может быть обрыв, но нам все
	{                 // равно нужно апдейтить дизы!
		if (!(Flags&FCOPY_COPYTONUL) && !strDestDizPath.empty())
		{
			const auto Attr = os::GetFileAttributes(DestDiz.GetDizName());
			int DestReadOnly=(Attr!=INVALID_FILE_ATTRIBUTES && (Attr & FILE_ATTRIBUTE_READONLY));

			if (Move && !DestReadOnly)
				SrcPanel->FlushDiz();

			DestDiz.Flush(strDestDizPath);
		}
	}

#if 1
	SrcPanel->Update(UPDATE_KEEP_SELECTION);

	if (SelCount==1 && !strRenamedName.empty())
		SrcPanel->GoToFile(strRenamedName);

#if 1

	if (NeedUpdateAPanel && m_FileAttr != INVALID_FILE_ATTRIBUTES && (m_FileAttr&FILE_ATTRIBUTE_DIRECTORY) && DestPanelMode != panel_mode::PLUGIN_PANEL)
	{
		DestPanel->SetCurDir(SrcPanel->GetCurDir(), false);
	}

#else

	if (m_FileAttr != INVALID_FILE_ATTRIBUTES && (m_FileAttr&FILE_ATTRIBUTE_DIRECTORY) && DestPanelMode != panel_mode::PLUGIN_PANEL)
	{
		// если SrcDir содержится в DestDir...
		string strTmpDestDir;
		string strTmpSrcDir;
		DestPanel->GetCurDir(strTmpDestDir);
		SrcPanel->GetCurDir(strTmpSrcDir);

		if (CheckUpdateAnotherPanel(SrcPanel,strTmpSrcDir))
			DestPanel->SetCurDir(strTmpDestDir,false);
	}

#endif

	// проверим "нужность" апдейта пассивной панели
	if (Flags&FCOPY_UPDATEPPANEL)
	{
		DestPanel->SortFileList(TRUE);
		DestPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
	}

	if (SrcPanelMode == panel_mode::PLUGIN_PANEL)
		SrcPanel->SetPluginModified();

	Global->CtrlObject->Cp()->Redraw();
#else
	SrcPanel->Update(UPDATE_KEEP_SELECTION);

	if (SelCount==1 && strRenamedName.empty())
		SrcPanel->GoToFile(strRenamedName);

	SrcPanel->Redraw();
	DestPanel->SortFileList(TRUE);
	DestPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
	DestPanel->Redraw();
#endif
}

ShellCopy::~ShellCopy()
{
	_tran(SysLog(L"[%p] ShellCopy::~ShellCopy(), CopyBufer=%p",this,CopyBuffer));
}

COPY_CODES ShellCopy::CopyFileTree(const string& Dest)
{
	SCOPED_ACTION(ChangePriority)(THREAD_PRIORITY_NORMAL);
	//SaveScreen SaveScr;
	DWORD DestAttr = INVALID_FILE_ATTRIBUTES;
	size_t DestMountLen = 0;

	if (Dest.empty() || Dest == L".")
		return COPY_FAILURE; //????

	SetCursorType(false, 0);

	//Flags &= ~(FCOPY_STREAMSKIP|FCOPY_STREAMALL); // unused now...
	DWORD Flags0 = Flags;

	bool first = true;
	bool UseWildCards = Dest.find_first_of(L"*?") != string::npos;
	bool copy_to_null = (0 != (Flags & FCOPY_COPYTONUL));
	bool move_rename = (0 != (Flags & FCOPY_MOVE));
	bool SameDisk = false;

	if (!CP->m_Bytes.Total)
	{
		//  ! Не сканируем каталоги при создании линков
		if (CP->IsTotalVisible() && !(Flags&FCOPY_LINK) && !CalcTotalSize())
			return COPY_FAILURE;
	}
	else
	{
		CP->m_Bytes.CurrCopied = 0;
	}

	// Основной цикл копирования одной порции.
	//
	DWORD FileAttr;
	SrcPanel->GetSelName(nullptr, FileAttr);

	string strSelName, strSelShortName;
	while (SrcPanel->GetSelName(&strSelName, FileAttr, &strSelShortName))
	{
		string strDest(Dest);
		Flags = (Flags0 & ~FCOPY_DIZREAD) | (Flags & FCOPY_DIZREAD);

		bool src_abspath = IsAbsolutePath(strSelName);

		bool dst_abspath = copy_to_null || IsAbsolutePath(strDest);
		if (!dst_abspath && ((strDest.size() > 2 && strDest[1] == L':') || (!strDest.empty() && IsSlash(strDest[0]))))
		{
			strDest = ConvertNameToFull(strDest);
			dst_abspath = true;
		}

		SelectedFolderNameLength = (FileAttr & FILE_ATTRIBUTE_DIRECTORY)?(int)strSelName.size():0;
		if (UseWildCards)
			ConvertWildcards(strSelName, strDest, SelectedFolderNameLength);

		bool simple_rename = false;
		if (move_rename && first && SrcPanel->GetSelCount() == 1 && !src_abspath)
			simple_rename = PointToName(strDest) == strDest.data();

		if (!simple_rename && !dst_abspath && !IsAbsolutePath(strDest))
		{
			string tpath;
			if (!src_abspath)
			{
				tpath = SrcPanel->GetCurDir();
				AddEndSlash(tpath);
			}
			else
			{
				const auto SlashPos = FindLastSlash(strSelName);
				if (SlashPos)
				{
					tpath = strSelName.substr(0, SlashPos + 1);
				}
			}
			strDest = tpath + strDest;
		}

		bool check_samedisk = false, dest_changed = false;
		if (first || strSrcDriveRoot.empty() || (src_abspath && StrCmpNI(strSelName.data(), strSrcDriveRoot.data(), strSrcDriveRoot.size())))
		{
			strSrcDriveRoot = GetPathRoot(src_abspath? strSelName : SrcPanel->GetCurDir());
			SrcDriveType = FAR_GetDriveType(strSrcDriveRoot);
			check_samedisk = true;
		}
		if (!copy_to_null && (first || strDestDriveRoot.empty() || StrCmpNI(strDest.data(), strDestDriveRoot.data(), strDestDriveRoot.size())))
		{
			strDestDriveRoot = GetPathRoot(strDest);
			DestDriveType = FAR_GetDriveType(strDestDriveRoot);
			DestMountLen = GetMountPointLen(strDest, strDestDriveRoot);
			check_samedisk = dest_changed = true;
		}
		if (move_rename && !copy_to_null && check_samedisk)
		{
			SameDisk = GetPathRoot(src_abspath? strSelName : SrcPanel->GetCurDir()) == GetPathRoot(strDest);
		}

		if (first && !copy_to_null && (dst_abspath || !src_abspath) && !UseWildCards
		 && SrcPanel->GetSelCount() > 1
		 && !IsSlash(strDest.back())
		 && !os::fs::exists(strDest))
		{
			switch (Message(FMSG_WARNING,3,MSG(lng::MWarning),strDest.data(),MSG(lng::MCopyDirectoryOrFile),MSG(lng::MCopyDirectoryOrFileDirectory),MSG(lng::MCopyDirectoryOrFileFile),MSG(lng::MCancel)))
			{
			case 2: case -1: case -2: return COPY_CANCEL; // [Cancel]
			//case 1: break;                              // [File]
			case 0: AddEndSlash(strDest);	break;          // {Directory}
			}
		}

		if (dest_changed) // check destination drive ready
		{
			DestAttr = os::GetFileAttributes(strDest);
			if (INVALID_FILE_ATTRIBUTES == DestAttr)
			{
				const auto Exists_1 = os::fs::exists(strDestDriveRoot);
				auto Exists_2 = Exists_1;
				while ( !Exists_2 && SkipMode != 2)
				{
					Global->CatchError();
					const auto Result = OperationFailed(strDestDriveRoot, lng::MError, L"");
					if (Result == operation::retry)
					{
						continue;
					}
					else if (Result == operation::skip)
					{
						return COPY_SKIPPED;
					}
					else if (Result == operation::skip_all)
					{
						SkipMode = static_cast<int>(operation::skip);
						return COPY_SKIPPED;
					}
					else if (Result == operation::cancel)
					{
						return COPY_CANCEL;
					}

					Exists_2 = os::fs::exists(strDestDriveRoot);
				}
				if (!Exists_1 && Exists_2)
					DestAttr = os::GetFileAttributes(strDest);
			}
		}

		const auto pos = FindLastSlash(strDest);
		if (!copy_to_null && pos != string::npos && (!DestMountLen || pos > DestMountLen))
		{
			string strNewPath = strDest.substr(0, pos);
			os::fs::file_status NewPathStatus(strNewPath);
			if (!os::fs::exists(NewPathStatus))
			{
				if (os::CreateDirectory(strNewPath,nullptr))
					TreeList::AddTreeName(strNewPath);
				else
					CreatePath(strNewPath);

				DestAttr = os::GetFileAttributes(strDest);
			}
			else if (os::fs::is_file(NewPathStatus))
			{
				Message(MSG_WARNING,1,MSG(lng::MError),MSG(lng::MCopyCannotCreateFolder),strNewPath.data(),MSG(lng::MOk));
				return COPY_FAILURE;
			}
		}

		// копируем полный контент, независимо от опции (но не для случая переименования линка по сети)
		if ((DestDriveType == DRIVE_REMOTE || SrcDriveType == DRIVE_REMOTE) && StrCmpI(strSrcDriveRoot, strDestDriveRoot))
			Flags |= FCOPY_COPYSYMLINKCONTENTS;

		first = false;
		string strDestPath = strDest;

		os::FAR_FIND_DATA SrcData;
		int CopyCode = COPY_SUCCESS, KeepPathPos;
		Flags &= ~FCOPY_OVERWRITENEXT;

		KeepPathPos = (int)(size_t)(PointToName(strSelName) - strSelName.data());

		if (RPT==RP_JUNCTION || RPT==RP_SYMLINK || RPT==RP_SYMLINKFILE || RPT==RP_SYMLINKDIR)
		{
			switch (MkSymLink(strSelName, strDest, RPT))
			{
				case 2:
					break;
				case 1:

					// Отметим (Ins) несколько каталогов, ALT-F6 Enter - выделение с папок не снялось.
					if ((!(Flags&FCOPY_CURRENTONLY)) && (Flags&FCOPY_COPYLASTTIME))
						SrcPanel->ClearLastGetSelection();

					continue;
				case 0:
					return COPY_FAILURE;
			}
		}
		else
		{
			// проверка на вшивость ;-)
			if (!os::GetFindDataEx(strSelName,SrcData))
			{
				strDestPath = strSelName;
				CP->SetNames(strSelName,strDestPath);

				if (Message(MSG_WARNING,2,MSG(lng::MError),MSG(lng::MCopyCannotFind),
					          strSelName.data(),MSG(lng::MSkip),MSG(lng::MCancel)) == Message::second_button)
				{
					return COPY_FAILURE;
				}

				continue;
			}
		}

		if (move_rename)
		{
			if ((m_UseFilter || !SameDisk) || ((SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && (Flags&FCOPY_COPYSYMLINKCONTENTS)))
			{
				CopyCode=COPY_FAILURE;
			}
			else
			{
				do
				{
					CopyCode=ShellCopyOneFile(strSelName,SrcData,strDestPath,KeepPathPos,1);
				}
				while (CopyCode==COPY_RETRY);

				if (CopyCode==COPY_SUCCESS_MOVE)
				{
					if (!strDestDizPath.empty())
					{
						if (!strRenamedName.empty())
						{
							DestDiz.Erase(strSelName,strSelShortName);
							SrcPanel->CopyDiz(strSelName,strSelShortName,strRenamedName,strRenamedName,&DestDiz);
						}
						else
						{
							if (strCopiedName.empty())
								strCopiedName = strSelName;

							SrcPanel->CopyDiz(strSelName,strSelShortName,strCopiedName,strCopiedName,&DestDiz);
							SrcPanel->DeleteDiz(strSelName,strSelShortName);
						}
					}

					continue;
				}

				if (CopyCode==COPY_CANCEL)
					return COPY_CANCEL;

				if (CopyCode==COPY_SKIPPED)
				{
					CP->UpdateAllBytesInfo(SrcData.nFileSize);
					continue;
				}

				if (!(Flags&FCOPY_MOVE) || CopyCode==COPY_FAILURE)
					Flags|=FCOPY_OVERWRITENEXT;
			}
		}

		if (!(Flags&FCOPY_MOVE) || CopyCode==COPY_FAILURE)
		{
			string strCopyDest=strDest;

			do
			{
				CopyCode=ShellCopyOneFile(strSelName,SrcData,strCopyDest,KeepPathPos,0);
			}
			while (CopyCode==COPY_RETRY);

			Flags&=~FCOPY_OVERWRITENEXT;

			if (CopyCode==COPY_CANCEL)
				return COPY_CANCEL;

			if (CopyCode!=COPY_SUCCESS)
			{
				if (CopyCode == COPY_SKIPPED)
				{
					CP->UpdateAllBytesInfo(SrcData.nFileSize);
				}
				continue;
			}
		}

		if (CopyCode==COPY_SUCCESS && !(Flags&FCOPY_COPYTONUL) && !strDestDizPath.empty())
		{
			if (strCopiedName.empty())
				strCopiedName = strSelName;

			SrcPanel->CopyDiz(strSelName,strSelShortName,strCopiedName,strCopiedName,&DestDiz);
		}

		// Mantis#44 - Потеря данных при копировании ссылок на папки
		// если каталог (или нужно копировать симлинк) - придется рекурсивно спускаться...
		if (RPT!=RP_SYMLINKFILE && (SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			      (
			          !(SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
			          ((SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && (Flags&FCOPY_COPYSYMLINKCONTENTS))
			      )
			  )
		{
			int SubCopyCode;
			string strFullName;
			ScanTree ScTree(true, true, Flags & FCOPY_COPYSYMLINKCONTENTS);
			auto strSubName = strSelName + L"\\";

			if (DestAttr==INVALID_FILE_ATTRIBUTES)
				KeepPathPos=(int)strSubName.size();

			int NeedRename=!((SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && (Flags&FCOPY_COPYSYMLINKCONTENTS) && (Flags&FCOPY_MOVE));
			ScTree.SetFindPath(strSubName,L"*",FSCANTREE_FILESFIRST);

			while (ScTree.GetNextName(SrcData,strFullName))
			{
				if (m_UseFilter && (SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					// Просто пропустить каталог недостаточно - если каталог помечен в
					// фильтре как некопируемый, то следует пропускать и его и всё его
					// содержимое.
					if (!m_Filter->FileInFilter(SrcData, nullptr, &strFullName))
					{
						ScTree.SkipDir();
						continue;
					}
				}
				{
					int AttemptToMove=FALSE;

					if ((Flags&FCOPY_MOVE) && SameDisk && !(SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						AttemptToMove=TRUE;
						int Ret=COPY_SUCCESS;
						string strCopyDest=strDest;

						do
						{
							Ret=ShellCopyOneFile(strFullName,SrcData,strCopyDest,KeepPathPos,NeedRename);
						}
						while (Ret==COPY_RETRY);

						switch (Ret) // 1
						{
							case COPY_CANCEL:
								return COPY_CANCEL;
							case COPY_SKIPPED:
							{
								CP->UpdateAllBytesInfo(SrcData.nFileSize);
								continue;
							}
							case COPY_SUCCESS_MOVE:
							{
								continue;
							}
							case COPY_SUCCESS:

								if (!NeedRename) // вариант при перемещении содержимого симлинка с опцией "копировать содержимое сим..."
								{
									CP->UpdateAllBytesInfo(SrcData.nFileSize);
									continue;     // ...  т.к. мы ЭТО не мувили, а скопировали, то все, на этом закончим бадаться с этим файлов
								}
						}
					}

					int SaveOvrMode=OvrMode;

					if (AttemptToMove)
						OvrMode=1;

					string strCopyDest=strDest;

					do
					{
						SubCopyCode=ShellCopyOneFile(strFullName,SrcData,strCopyDest,KeepPathPos,0);
					}
					while (SubCopyCode==COPY_RETRY);

					if (AttemptToMove)
						OvrMode=SaveOvrMode;
				}

				if (SubCopyCode==COPY_CANCEL)
					return COPY_CANCEL;

				if (SubCopyCode==COPY_SKIPPED)
				{
					CP->UpdateAllBytesInfo(SrcData.nFileSize);
				}

				if (SubCopyCode==COPY_SUCCESS)
				{
					if (Flags&FCOPY_MOVE)
					{
						if (SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
						{
							if (ScTree.IsDirSearchDone() ||
								      ((SrcData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && !(Flags&FCOPY_COPYSYMLINKCONTENTS)))
							{
								if (SrcData.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
									os::SetFileAttributes(strFullName,FILE_ATTRIBUTE_NORMAL);

								if (os::RemoveDirectory(strFullName))
									TreeList::DelTreeName(strFullName);
							}
						}
						// здесь нужны проверка на FSCANTREE_INSIDEJUNCTION, иначе
						// при мувинге будет удаление файла, что крайне неправильно!
						else if (!ScTree.InsideJunction())
						{
							if (DeleteAfterMove(strFullName,SrcData.dwFileAttributes)==COPY_CANCEL)
								return COPY_CANCEL;
						}
					}
				}
			}

			if ((Flags&FCOPY_MOVE) && CopyCode==COPY_SUCCESS)
			{
				if (FileAttr & FILE_ATTRIBUTE_READONLY)
					os::SetFileAttributes(strSelName,FILE_ATTRIBUTE_NORMAL);

				if (os::RemoveDirectory(strSelName))
				{
					TreeList::DelTreeName(strSelName);

					if (!strDestDizPath.empty())
						SrcPanel->DeleteDiz(strSelName,strSelShortName);
				}
			}
		}
		else if ((Flags&FCOPY_MOVE) && CopyCode==COPY_SUCCESS)
		{
			int DeleteCode;

			if ((DeleteCode=DeleteAfterMove(strSelName,FileAttr))==COPY_CANCEL)
				return COPY_CANCEL;

			if (DeleteCode==COPY_SUCCESS && !strDestDizPath.empty())
				SrcPanel->DeleteDiz(strSelName,strSelShortName);
		}

		if ((!(Flags&FCOPY_CURRENTONLY)) && (Flags&FCOPY_COPYLASTTIME))
		{
			SrcPanel->ClearLastGetSelection();
		}
	}

	return COPY_SUCCESS; //COPY_SUCCESS_MOVE???
}



// абсолютно невменяемая функция. функция таких размеров вменяема быть не может. переписать ASAP

COPY_CODES ShellCopy::ShellCopyOneFile(
    const string& Src,
    const os::FAR_FIND_DATA &SrcData,
    string &strDest,
    int KeepPathPos,
    int Rename
)
{
	CP->m_Bytes.CurrCopied = 0; // Сбросить текущий прогресс

	if (CP->IsCancelled())
	{
		return COPY_CANCEL;
	}

	if (m_UseFilter)
	{
		if (!m_Filter->FileInFilter(SrcData, nullptr, &Src))
			return COPY_NOFILTER;
	}

	string strDestPath = strDest;

	DWORD DestAttr=INVALID_FILE_ATTRIBUTES;

	os::FAR_FIND_DATA DestData;
	if (!(Flags&FCOPY_COPYTONUL))
	{
		if (os::GetFindDataEx(strDestPath,DestData))
			DestAttr=DestData.dwFileAttributes;
	}

	int SameName=0, Append=0;

	if (!(Flags&FCOPY_COPYTONUL) && DestAttr!=INVALID_FILE_ATTRIBUTES && (DestAttr & FILE_ATTRIBUTE_DIRECTORY))
	{
		if(SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			int CmpCode=CmpFullNames(Src,strDestPath);

			if(CmpCode && SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT && RPT==RP_EXACTCOPY && !(Flags&FCOPY_COPYSYMLINKCONTENTS))
			{
				CmpCode = 0;
			}

			if (CmpCode==1) // TODO: error check
			{
				SameName=1;

				if (Rename)
				{
					CmpCode=!StrCmp(PointToName(Src),PointToName(strDestPath));
				}

				if (CmpCode==1)
				{
					Message(MSG_WARNING, MSG(lng::MError),
						{ MSG(lng::MCannotCopyFolderToItself1), Src, MSG(lng::MCannotCopyFolderToItself2) },
						{ MSG(lng::MOk) },
						L"ErrCopyItSelf");
					return COPY_CANCEL;
				}
			}
		}

		if (!SameName)
		{
			int Length=(int)strDestPath.size();

			if (!IsSlash(strDestPath[Length-1]) && strDestPath[Length-1]!=L':')
				strDestPath += L"\\";

			const wchar_t *PathPtr=Src.data()+KeepPathPos;

			if (*PathPtr && !KeepPathPos && PathPtr[1]==L':')
				PathPtr+=2;

			if (IsSlash(*PathPtr))
				PathPtr++;

			strDestPath += PathPtr;

			if (!os::GetFindDataEx(strDestPath,DestData))
				DestAttr=INVALID_FILE_ATTRIBUTES;
			else
				DestAttr=DestData.dwFileAttributes;
		}
	}

	if (!(Flags&FCOPY_COPYTONUL) && StrCmpI(strDestPath.data(),L"prn"))
		SetDestDizPath(strDestPath);

	CP->SetProgressValue(0,0);
	CP->SetNames(Src,strDestPath);

	int IsSetSecuty=FALSE;

	if (!(Flags&FCOPY_COPYTONUL))
	{
		// проверка очередного монстрика на потоки
		switch (CheckStreams(Src,strDestPath))
		{
			case COPY_SKIPPED:
				return COPY_SKIPPED;
			case COPY_CANCEL:
				return COPY_CANCEL;
			default:
				break;
		}

		bool dir = (SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		bool rpt = (SrcData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		bool cpc = (Flags & FCOPY_COPYSYMLINKCONTENTS) != 0;
		if (!dir && rpt && RPT==RP_EXACTCOPY && !cpc)
		{
			bool spf = (SrcData.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
			if (spf)
				cpc = true; // ???
			else
			{
				DWORD rpTag = 0;
				string strRptInfo;
				GetReparsePointInfo(Src, strRptInfo, &rpTag);
				cpc = (rpTag != IO_REPARSE_TAG_SYMLINK) && (rpTag != IO_REPARSE_TAG_MOUNT_POINT);
			}
			if (cpc)
				Flags |= FCOPY_COPYSYMLINKCONTENTS;
		}

		if (dir || (rpt && RPT==RP_EXACTCOPY && !cpc))
		{
			if (!Rename)
				strCopiedName = PointToName(strDestPath);

			if (DestAttr!=INVALID_FILE_ATTRIBUTES)
			{
				if ((DestAttr & FILE_ATTRIBUTE_DIRECTORY) && !SameName)
				{
					DWORD SetAttr=SrcData.dwFileAttributes;

					if (IsDriveTypeCDROM(SrcDriveType) && (SetAttr & FILE_ATTRIBUTE_READONLY))
						SetAttr&=~FILE_ATTRIBUTE_READONLY;

					if (SetAttr!=DestAttr)
						ShellSetAttr(strDestPath,SetAttr);

					return ConvertNameToFull(Src) == strDestPath? COPY_SKIPPED : COPY_SUCCESS;
				}

				int Type=os::GetFileTypeByName(strDestPath);

				if (Type==FILE_TYPE_CHAR || Type==FILE_TYPE_PIPE)
					return Rename? COPY_SKIPPED : COPY_SUCCESS;
			}

			if (Rename)
			{
				auto strSrcFullName = ConvertNameToFull(Src);
				os::FAR_SECURITY_DESCRIPTOR sd;

				// для Move нам необходимо узнать каталог родитель, чтобы получить его секьюрити
				if (!(Flags&(FCOPY_COPYSECURITY|FCOPY_LEAVESECURITY)))
				{
					IsSetSecuty=FALSE;

					if (CmpFullPath(Src,strDest)) // в пределах одного каталога ничего не меняем
						IsSetSecuty=FALSE;
					else if (!os::fs::exists(strDest)) // если каталога нет...
					{
						// ...получаем секьюрити родителя
						if (GetSecurity(GetParentFolder(strDest), sd))
							IsSetSecuty=TRUE;
					}
					else if (GetSecurity(strDest,sd)) // иначе получаем секьюрити Dest`а
						IsSetSecuty=TRUE;
				}

				// Пытаемся переименовать, пока не отменят
				for (;;)
				{
					if (os::MoveFile(Src, strDestPath))
					{
						if (IsSetSecuty)// && !strcmp(DestFSName,"NTFS"))
							SetRecursiveSecurity(strDestPath,sd);

						if (PointToName(strDestPath)==strDestPath.data())
							strRenamedName = strDestPath;
						else
							strCopiedName = PointToName(strDestPath);

						TreeList::RenTreeName(strSrcFullName, ConvertNameToFull(strDest));
						return SameName? COPY_SKIPPED : COPY_SUCCESS_MOVE;
					}
					else
					{
						Global->CatchError();
						int MsgCode = Message(MSG_WARNING|MSG_ERRORTYPE,3,MSG(lng::MError),
						                      MSG(lng::MCopyCannotRenameFolder),Src.data(),MSG(lng::MCopyRetry),
						                      MSG(lng::MCopyIgnore),MSG(lng::MCopyCancel));

						switch (MsgCode)
						{
							case 0:  continue;
							case 1:
							{
								int CopySecurity = Flags&FCOPY_COPYSECURITY;
								os::FAR_SECURITY_DESCRIPTOR tmpsd;

								if ((CopySecurity) && !GetSecurity(Src,tmpsd))
									CopySecurity = FALSE;
								SECURITY_ATTRIBUTES TmpSecAttr  ={sizeof(TmpSecAttr), tmpsd.get(), FALSE};
								if (os::CreateDirectory(strDestPath,CopySecurity?&TmpSecAttr:nullptr))
								{
									if (PointToName(strDestPath)==strDestPath.data())
										strRenamedName = strDestPath;
									else
										strCopiedName = PointToName(strDestPath);

									TreeList::AddTreeName(strDestPath);
									return COPY_SUCCESS;
								}
							}
							default:
								return COPY_CANCEL;
						} /* switch */
					} /* else */
				} /* while */
			} // if (Rename)

			os::FAR_SECURITY_DESCRIPTOR sd;

			if ((Flags&FCOPY_COPYSECURITY) && !GetSecurity(Src,sd))
				return COPY_CANCEL;
			SECURITY_ATTRIBUTES SecAttr = {sizeof(SecAttr), sd.get(), FALSE};
			if (RPT!=RP_SYMLINKFILE && SrcData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			{
				while (!os::CreateDirectoryEx(
					// CreateDirectoryEx preserves reparse points,
					// so we shouldn't use template when copying with content
					((SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && (Flags&FCOPY_COPYSYMLINKCONTENTS))? L"" : Src,
					strDestPath,(Flags&FCOPY_COPYSECURITY) ? &SecAttr:nullptr))
				{
					Global->CatchError();
					int MsgCode=Message(MSG_WARNING|MSG_ERRORTYPE,3,MSG(lng::MError),
					                MSG(lng::MCopyCannotCreateFolder),strDestPath.data(),MSG(lng::MCopyRetry),
					                MSG(lng::MCopySkip),MSG(lng::MCopyCancel));

					if (MsgCode)
						return (MsgCode==-2 || MsgCode==2)? COPY_CANCEL : COPY_SKIPPED;
				}

				DWORD SetAttr=SrcData.dwFileAttributes;

				if (IsDriveTypeCDROM(SrcDriveType) && (SetAttr & FILE_ATTRIBUTE_READONLY))
					SetAttr&=~FILE_ATTRIBUTE_READONLY;

				if ((SetAttr & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
				{
					// не будем выставлять компрессию, если мылимся в каталог
					// с выставленным FILE_ATTRIBUTE_ENCRYPTED (а он уже будет выставлен после CreateDirectory)
					// т.с. пропускаем лишний ход.
					if (os::fs::file_status(strDestPath).check(FILE_ATTRIBUTE_ENCRYPTED))
						SetAttr&=~FILE_ATTRIBUTE_COMPRESSED;

					if (SetAttr&FILE_ATTRIBUTE_COMPRESSED)
					{
						for (;;)
						{
							int MsgCode=ESetFileCompression(strDestPath,1,0,SkipMode);

							if (MsgCode)
							{
								if (MsgCode == SETATTR_RET_SKIP)
									Flags|=FCOPY_SKIPSETATTRFLD;
								else if (MsgCode == SETATTR_RET_SKIPALL)
								{
									Flags|=FCOPY_SKIPSETATTRFLD;
									SkipMode=SETATTR_RET_SKIP;
								}

								break;
							}

							if (MsgCode != SETATTR_RET_OK)
								return (MsgCode==SETATTR_RET_SKIP || MsgCode==SETATTR_RET_SKIPALL) ? COPY_SKIPPED:COPY_CANCEL;
						}
					}

					while (!ShellSetAttr(strDestPath,SetAttr))
					{
						Global->CatchError();
						int MsgCode=Message(MSG_WARNING|MSG_ERRORTYPE,4,MSG(lng::MError),
						                MSG(lng::MCopyCannotChangeFolderAttr),strDestPath.data(),
						                MSG(lng::MCopyRetry),MSG(lng::MCopySkip),MSG(lng::MCopySkipAll),MSG(lng::MCopyCancel));

						if (MsgCode)
						{
							if (MsgCode==1)
								break;

							if (MsgCode==2)
							{
								Flags|=FCOPY_SKIPSETATTRFLD;
								break;
							}

							os::RemoveDirectory(strDestPath);
							return (MsgCode==-2 || MsgCode==3)? COPY_CANCEL : COPY_SKIPPED;
						}
					}
				}
				else if (!(Flags & FCOPY_SKIPSETATTRFLD) && ((SetAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY))
				{
					while (!ShellSetAttr(strDestPath,SetAttr))
					{
						Global->CatchError();
						int MsgCode=Message(MSG_WARNING|MSG_ERRORTYPE,4,MSG(lng::MError),
						                MSG(lng::MCopyCannotChangeFolderAttr),strDestPath.data(),
						                MSG(lng::MCopyRetry),MSG(lng::MCopySkip),MSG(lng::MCopySkipAll),MSG(lng::MCopyCancel));

						if (MsgCode)
						{
							if (MsgCode==1)
								break;

							if (MsgCode==2)
							{
								Flags|=FCOPY_SKIPSETATTRFLD;
								break;
							}

							os::RemoveDirectory(strDestPath);
							return (MsgCode==-2 || MsgCode==3)? COPY_CANCEL : COPY_SKIPPED;
						}
					}
				}
			}

			// [ ] Copy contents of symbolic links
			// For file symbolic links only!!!
			// Directory symbolic links and junction points are handled by CreateDirectoryEx.
			if (!dir && rpt && !cpc && RPT==RP_EXACTCOPY)
			{
				switch (MkSymLink(Src, strDestPath, RPT))
				{
					case 2:
						return COPY_CANCEL;
					case 1:
						break;
					case 0:
						return COPY_FAILURE;
				}
			}

			TreeList::AddTreeName(strDestPath);
			return COPY_SUCCESS;
		}

		if (DestAttr!=INVALID_FILE_ATTRIBUTES && !(DestAttr & FILE_ATTRIBUTE_DIRECTORY))
		{
			if (SrcData.nFileSize==DestData.nFileSize)
			{
				int CmpCode=CmpFullNames(Src,strDestPath);

				if(CmpCode && SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT && RPT==RP_EXACTCOPY && !(Flags&FCOPY_COPYSYMLINKCONTENTS))
				{
					CmpCode = 0;
				}

				if (CmpCode==1) // TODO: error check
				{
					SameName=1;

					if (Rename)
					{
						CmpCode=!StrCmp(PointToName(Src),PointToName(strDestPath));
					}

					if (CmpCode==1 && !Rename)
					{
						string qSrc(Src);
						Message(MSG_WARNING,1,MSG(lng::MError),MSG(lng::MCannotCopyFileToItself1),
							    QuoteOuterSpace(qSrc).data(),MSG(lng::MCannotCopyFileToItself2),MSG(lng::MOk));
						return COPY_CANCEL;
					}
				}
			}

			int RetCode=COPY_CANCEL;
			string strNewName;

			if (!AskOverwrite(SrcData,Src,strDestPath,DestAttr,SameName,Rename,((Flags&FCOPY_LINK)?0:1),Append,strNewName,RetCode))
			{
				return (COPY_CODES)RetCode;
			}

			if (RetCode==COPY_RETRY)
			{
				strDest=strNewName;

				if (CutToSlash(strNewName) && !os::fs::exists(strNewName))
				{
					CreatePath(strNewName);
				}

				return COPY_RETRY;
			}
		}
	}
	else
	{
		if (SrcData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			return COPY_SUCCESS;
		}
	}

	bool NWFS_Attr = Global->Opt->Nowell.MoveRO && strDestFSName == L"NWFS";
	{
		for (;;)
		{
			int CopyCode=0;
			unsigned long long SaveTotalSize = CP->m_Bytes.Copied;

			if (!(Flags&FCOPY_COPYTONUL) && Rename)
			{
				int AskDelete;

				if (strDestFSName == L"NWFS" && !Append &&
				        DestAttr!=INVALID_FILE_ATTRIBUTES && !SameName)
				{
					os::DeleteFile(strDestPath); //BUGBUG
				}

				bool FileMoved = false;

				if (!Append)
				{
					const auto strSrcFullName = ConvertNameToFull(Src);

					if (NWFS_Attr)
						os::SetFileAttributes(strSrcFullName,SrcData.dwFileAttributes&(~FILE_ATTRIBUTE_READONLY));

					os::FAR_SECURITY_DESCRIPTOR sd;
					IsSetSecuty=FALSE;

					// для Move нам необходимо узнать каталог родитель, чтобы получить его секьюрити
					if (Rename && !(Flags&(FCOPY_COPYSECURITY|FCOPY_LEAVESECURITY)))
					{
						if (CmpFullPath(Src,strDest)) // в пределах одного каталога ничего не меняем
							IsSetSecuty=FALSE;
						else if (!os::fs::exists(strDest)) // если каталога нет...
						{
							// ...получаем секьюрити родителя
							if (GetSecurity(GetParentFolder(strDest), sd))
								IsSetSecuty=TRUE;
						}
						else if (GetSecurity(strDest, sd)) // иначе получаем секьюрити Dest`а
							IsSetSecuty=TRUE;
					}

					if (strDestFSName == L"NWFS")
						FileMoved = os::MoveFile(strSrcFullName, strDestPath);
					else
						FileMoved = os::MoveFileEx(strSrcFullName, strDestPath, SameName ? MOVEFILE_COPY_ALLOWED : MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);

					if (!FileMoved)
					{
						int MoveLastError=GetLastError();

						if (NWFS_Attr)
							os::SetFileAttributes(strSrcFullName,SrcData.dwFileAttributes);

						if (MoveLastError==ERROR_NOT_SAME_DEVICE)
							return COPY_FAILURE;

						SetLastError(MoveLastError);
						Global->CatchError();
					}
					else
					{
						if (IsSetSecuty)
							SetSecurity(strDestPath, sd);
					}

					if (NWFS_Attr)
						os::SetFileAttributes(strDestPath,SrcData.dwFileAttributes);

					if (FileMoved)
					{
						CP->m_Bytes.CurrCopied = SrcData.nFileSize;
						CP->UpdateAllBytesInfo(SrcData.nFileSize);
					}

					AskDelete=0;
				}
				else
				{
					do
					{
						DWORD Attr=INVALID_FILE_ATTRIBUTES;
						CopyCode=ShellCopyFile(Src,SrcData,strDestPath,Attr,Append);
					}
					while (CopyCode==COPY_RETRY);

					switch (CopyCode)
					{
						case COPY_SUCCESS:
							FileMoved = true;
							break;
						case COPY_FAILURE:
							FileMoved = false;
							break;
						case COPY_CANCEL:
							return COPY_CANCEL;
						case COPY_SKIPPED:
							return COPY_SKIPPED;
					}

					AskDelete=1;
				}

				if (FileMoved)
				{
					if (DestAttr==INVALID_FILE_ATTRIBUTES || !(DestAttr & FILE_ATTRIBUTE_DIRECTORY))
					{
						if (PointToName(strDestPath)==strDestPath.data())
							strRenamedName = strDestPath;
						else
							strCopiedName = PointToName(strDestPath);
					}

					if (IsDriveTypeCDROM(SrcDriveType) && (SrcData.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
						ShellSetAttr(strDestPath,SrcData.dwFileAttributes & (~FILE_ATTRIBUTE_READONLY));

					++CP->m_Files.Copied;

					if (AskDelete && DeleteAfterMove(Src,SrcData.dwFileAttributes)==COPY_CANCEL)
						return COPY_CANCEL;

					return COPY_SUCCESS_MOVE;
				}
			}
			else
			{
				do
				{
					CopyCode=ShellCopyFile(Src,SrcData,strDestPath,DestAttr,Append);
				}
				while (CopyCode==COPY_RETRY);

				if (CopyCode==COPY_SUCCESS)
				{
					strCopiedName = PointToName(strDestPath);

					if (!(Flags&FCOPY_COPYTONUL))
					{
						if (IsDriveTypeCDROM(SrcDriveType) && (SrcData.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
							ShellSetAttr(strDestPath,SrcData.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);

						if (DestAttr!=INVALID_FILE_ATTRIBUTES && !StrCmpI(strCopiedName, DestData.strFileName) &&
						        strCopiedName != DestData.strFileName)
							os::MoveFile(strDestPath,strDestPath); //???
					}

					++CP->m_Files.Copied;

					if (DestAttr!=INVALID_FILE_ATTRIBUTES && Append)
						os::SetFileAttributes(strDestPath,DestAttr);

					return COPY_SUCCESS;
				}
				else if (CopyCode==COPY_CANCEL || CopyCode==COPY_SKIPPED)
				{
					if (DestAttr!=INVALID_FILE_ATTRIBUTES && Append)
						os::SetFileAttributes(strDestPath,DestAttr);

					return (COPY_CODES)CopyCode;
				}

				if (DestAttr!=INVALID_FILE_ATTRIBUTES && Append)
					os::SetFileAttributes(strDestPath,DestAttr);
			}

			string strMsg1, strMsg2;
			lng MsgMCannot = (Flags&FCOPY_LINK)? lng::MCannotLink: (Flags&FCOPY_MOVE)? lng::MCannotMove : lng::MCannotCopy;
			strMsg1 = Src;
			strMsg2 = strDestPath;
			InsertQuote(strMsg1);
			InsertQuote(strMsg2);

			int MsgCode;
			if ((SrcData.dwFileAttributes&FILE_ATTRIBUTE_ENCRYPTED))
			{
				if (SkipEncMode != -1)
				{
					MsgCode = SkipEncMode;

					if (SkipEncMode == 1)
						Flags |= FCOPY_DECRYPTED_DESTINATION;
				}
				else
				{
					// Better to set it always, just in case
					//if (GetLastError() == ERROR_ACCESS_DENIED)
					{
						SetLastError(ERROR_ENCRYPTION_FAILED);
					}
					Global->CatchError();

					MsgCode = Message(MSG_WARNING | MSG_ERRORTYPE, 5, MSG(lng::MError),
						MSG(MsgMCannot),
						strMsg1.data(),
						MSG(lng::MCannotCopyTo),
						strMsg2.data(),
						MSG(lng::MCopyDecrypt),
						MSG(lng::MCopyDecryptAll),
						MSG(lng::MCopySkip),
						MSG(lng::MCopySkipAll),
						MSG(lng::MCopyCancel));
				}
				switch (MsgCode)
				{
				case 1:
					SkipEncMode = 1;
				case 0:
					Flags |= FCOPY_DECRYPTED_DESTINATION;
					break;

				case 3:
					SkipEncMode = 3;
				case 2:
					return COPY_SKIPPED;

				case -1:
				case -2:
				case 4:
					return COPY_CANCEL;
				}
			}
			else
			{
				if (SkipMode!=-1)
					MsgCode=SkipMode;
				else
				{
					Global->CatchError();
					MsgCode=Message(MSG_WARNING|MSG_ERRORTYPE,4,MSG(lng::MError),
									MSG(MsgMCannot),
									strMsg1.data(),
									MSG(lng::MCannotCopyTo),
									strMsg2.data(),
									MSG(lng::MCopyRetry),MSG(lng::MCopySkip),
									MSG(lng::MCopySkipAll),MSG(lng::MCopyCancel));
				}

				switch (MsgCode)
				{
				case  1:
					return COPY_SKIPPED;
				case  2:
					SkipMode=1;
					return COPY_SKIPPED;
				case -1:
				case -2:
				case  3:
					return COPY_CANCEL;
				}
			}
			CP->m_Bytes.Copied = SaveTotalSize;
			int RetCode = COPY_CANCEL;
			string strNewName;

			if (!AskOverwrite(SrcData,Src,strDestPath,DestAttr,SameName,Rename,((Flags&FCOPY_LINK)?0:1),Append,strNewName,RetCode))
				return (COPY_CODES)RetCode;

			if (RetCode==COPY_RETRY)
			{
				strDest=strNewName;

				if (CutToSlash(strNewName) && !os::fs::exists(strNewName))
				{
					CreatePath(strNewName);
				}

				return COPY_RETRY;
			}
		}
	}
}


// проверка очередного монстрика на потоки
COPY_CODES ShellCopy::CheckStreams(const string& Src,const string& DestPath)
{

#if 0
	int AscStreams=(Flags&FCOPY_STREAMSKIP)?2:((Flags&FCOPY_STREAMALL)?0:1);

	if (!(Flags&FCOPY_USESYSTEMCOPY) && AscStreams)
	{
		int CountStreams=EnumNTFSStreams(Src,nullptr,nullptr);

		if (CountStreams > 1 ||
		        (CountStreams >= 1 && (GetFileAttributes(Src)&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY))
		{
			if (AscStreams == 2)
			{
				return COPY_NEXT);
			}

			SetMessageHelp("WarnCopyStream");
			//char SrcFullName[NM];
			//ConvertNameToFull(Src,SrcFullName, sizeof(SrcFullName));
			//TruncPathStr(SrcFullName,ScrX-16);
			int MsgCode=Message(MSG_WARNING,5,MSG(lng::MWarning),
			                    MSG(lng::MCopyStream1),
			                    MSG(CanCreateHardLinks(DestPath, nullptr)? lng::MCopyStream2 : lng::MCopyStream3),
			                    MSG(lng::MCopyStream4),"\1",//SrcFullName,"\1",
			                    MSG(lng::MCopyResume),MSG(lng::MCopyOverwriteAll),MSG(lng::MCopySkipOvr),MSG(lng::MCopySkipAllOvr),MSG(lng::MCopyCancelOvr));

			switch (MsgCode)
			{
				case 0: break;
				case 1: Flags|=FCOPY_STREAMALL; break;
				case 2: return COPY_NEXT);
				case 3: Flags|=FCOPY_STREAMSKIP; return COPY_NEXT);
				default:
					return COPY_CANCEL;
			}
		}
	}

#endif
	return COPY_SUCCESS;
}

int ShellCopy::DeleteAfterMove(const string& Name,DWORD Attr)
{
	const auto FullName = ConvertNameToFull(Name);
	if (Attr & FILE_ATTRIBUTE_READONLY)
	{
		int MsgCode;

		if (!Global->Opt->Confirm.RO)
			ReadOnlyDelMode=1;

		if (ReadOnlyDelMode!=-1)
			MsgCode=ReadOnlyDelMode;
		else
			MsgCode=Message(MSG_WARNING,5,MSG(lng::MWarning),
			                MSG(lng::MCopyFileRO),FullName.data(),MSG(lng::MCopyAskDelete),
			                MSG(lng::MCopyDeleteRO),MSG(lng::MCopyDeleteAllRO),
			                MSG(lng::MCopySkipRO),MSG(lng::MCopySkipAllRO),MSG(lng::MCopyCancelRO));

		switch (MsgCode)
		{
			case 1:
				ReadOnlyDelMode=1;
				break;
			case 2:
				return COPY_SKIPPED;
			case 3:
				ReadOnlyDelMode=3;
				return COPY_SKIPPED;
			case -1:
			case -2:
			case 4:
				return COPY_CANCEL;
		}

		os::SetFileAttributes(FullName,FILE_ATTRIBUTE_NORMAL);
	}

	while ((Attr&FILE_ATTRIBUTE_DIRECTORY)?!os::RemoveDirectory(FullName):!os::DeleteFile(FullName))
	{
		Global->CatchError();
		operation MsgCode;

		if (SkipDeleteMode!=-1)
			MsgCode = static_cast<operation>(SkipDeleteMode);
		else
			MsgCode=OperationFailed(FullName, lng::MError, MSG(lng::MCannotDeleteFile));

		switch (MsgCode)
		{
		case operation::retry:
			break;

		case operation::skip:
			return COPY_SKIPPED;

		case operation::skip_all:
			SkipDeleteMode = static_cast<int>(operation::skip);
			return COPY_SKIPPED;

		case operation::cancel:
			return COPY_CANCEL;
		}
	}

	return COPY_SUCCESS;
}



int ShellCopy::ShellCopyFile(const string& SrcName,const os::FAR_FIND_DATA &SrcData,
                             string &strDestName,DWORD &DestAttr,int Append)
{
	if ((Flags&FCOPY_LINK))
	{
		if (RPT==RP_HARDLINK)
		{
			os::DeleteFile(strDestName); //BUGBUG
			return MkHardLink(SrcName,strDestName)? COPY_SUCCESS : COPY_FAILURE;
		}
		else
		{
			return MkSymLink(SrcName, strDestName, RPT)? COPY_SUCCESS : COPY_FAILURE;
		}
	}

	DWORD FilesystemFlags;
	if ((SrcData.dwFileAttributes&FILE_ATTRIBUTE_ENCRYPTED) &&
		os::GetVolumeInformation(GetPathRoot(strDestName), nullptr, nullptr, nullptr, &FilesystemFlags, nullptr) &&
		!(FilesystemFlags & FILE_SUPPORTS_ENCRYPTION))
	{
		int MsgCode;

		if (SkipEncMode!=-1)
		{
			MsgCode=SkipEncMode;

			if (SkipEncMode == 1)
				Flags|=FCOPY_DECRYPTED_DESTINATION;
		}
		else
		{
			string strSrcName(SrcName);
			InsertQuote(strSrcName);
			MsgCode = Message(MSG_WARNING, MSG(lng::MWarning),
				{ MSG(lng::MCopyEncryptWarn1), strSrcName, MSG(lng::MCopyEncryptWarn2), MSG(lng::MCopyEncryptWarn3) },
				{ MSG(lng::MCopyIgnore), MSG(lng::MCopyIgnoreAll), MSG(lng::MCopyCancel) },
				L"WarnCopyEncrypt");
		}

		switch (MsgCode)
		{
			case  0:
				_LOGCOPYR(SysLog(L"return COPY_NEXT -> %d",__LINE__));
				Flags|=FCOPY_DECRYPTED_DESTINATION;
				break;//return COPY_NEXT;
			case  1:
				SkipEncMode=1;
				Flags|=FCOPY_DECRYPTED_DESTINATION;
				_LOGCOPYR(SysLog(L"return COPY_NEXT -> %d",__LINE__));
				break;//return COPY_NEXT;
			case -1:
			case -2:
			case  2:
				_LOGCOPYR(SysLog(L"return COPY_CANCEL -> %d",__LINE__));
				return COPY_CANCEL;
		}
	}

	if ((Flags & FCOPY_USESYSTEMCOPY) && !Append)
	{
		if (!(SrcData.dwFileAttributes&FILE_ATTRIBUTE_ENCRYPTED) ||
		        ((SrcData.dwFileAttributes&FILE_ATTRIBUTE_ENCRYPTED) &&
		        (IsWindowsXPOrGreater() || !(Flags&(FCOPY_DECRYPTED_DESTINATION))))
		   )
		{
			if (!Global->Opt->CMOpt.CopyOpened)
			{
				if (!os::fs::file(SrcName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN))
				{
					_LOGCOPYR(SysLog(L"return COPY_FAILURE -> %d if (SrcHandle==INVALID_HANDLE_VALUE)",__LINE__));
					return COPY_FAILURE;
				}
			}

			//_LOGCOPYR(SysLog(L"call ShellSystemCopy('%s','%s',%p)",SrcName,DestName,SrcData));
			return ShellSystemCopy(SrcName,strDestName,SrcData);
		}
	}

	os::FAR_SECURITY_DESCRIPTOR sd;
	if ((Flags&FCOPY_COPYSECURITY) && !GetSecurity(SrcName,sd))
		return COPY_CANCEL;

	int OpenMode=FILE_SHARE_READ;

	if (Global->Opt->CMOpt.CopyOpened)
		OpenMode|=FILE_SHARE_WRITE;

	os::fs::file_walker SrcFile;
	bool Opened = SrcFile.Open(SrcName, GENERIC_READ, OpenMode, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);

	if (!Opened && Global->Opt->CMOpt.CopyOpened)
	{
		if (GetLastError() == ERROR_SHARING_VIOLATION)
		{
			Opened = SrcFile.Open(SrcName, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);
		}
	}

	if (!Opened)
	{
		return COPY_FAILURE;
	}

	os::fs::file DestFile;
	unsigned long long AppendPos=0;
	DWORD flags_attrs=0;

	bool CopySparse=false;

	if (!(Flags&FCOPY_COPYTONUL))
	{
		//if (DestAttr!=INVALID_FILE_ATTRIBUTES && !Append) //вот это портит копирование поверх хардлинков
		//api::DeleteFile(DestName);
		SECURITY_ATTRIBUTES SecAttr = {sizeof(SecAttr), sd.get(), FALSE};
		flags_attrs = SrcData.dwFileAttributes&(~((Flags&(FCOPY_DECRYPTED_DESTINATION))?FILE_ATTRIBUTE_ENCRYPTED|FILE_FLAG_SEQUENTIAL_SCAN:FILE_FLAG_SEQUENTIAL_SCAN));
		bool DstOpened = DestFile.Open(strDestName, GENERIC_WRITE, FILE_SHARE_READ, (Flags&FCOPY_COPYSECURITY) ? &SecAttr:nullptr, (Append ? OPEN_EXISTING:CREATE_ALWAYS), flags_attrs);
		Flags&=~FCOPY_DECRYPTED_DESTINATION;

		if (!DstOpened)
		{
			_LOGCOPYR(DWORD LastError=GetLastError();)
			SrcFile.Close();
			_LOGCOPYR(SysLog(L"return COPY_FAILURE -> %d CreateFile=-1, LastError=%d (0x%08X)",__LINE__,LastError,LastError));
			return COPY_FAILURE;
		}

		const auto strDriveRoot = GetPathRoot(strDestName);

		if (SrcData.dwFileAttributes&FILE_ATTRIBUTE_SPARSE_FILE)
		{
			DWORD VolFlags;
			if(os::GetVolumeInformation(strDriveRoot, nullptr, nullptr, nullptr, &VolFlags, nullptr) && VolFlags & FILE_SUPPORTS_SPARSE_FILES)
			{
				DWORD Temp;
				if (DestFile.IoControl(FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &Temp))
				{
					CopySparse=true;
				}
			}
		}

		if (Append)
		{
			if (!DestFile.SetPointer(0,&AppendPos,FILE_END))
			{
				SrcFile.Close();
				DestFile.SetEnd();
				DestFile.Close();
				return COPY_FAILURE;
			}
		}

		// если места в приёмнике хватает - займём сразу.
		UINT64 FreeBytes=0;
		if (os::GetDiskSize(strDriveRoot,nullptr,nullptr,&FreeBytes))
		{
			if (FreeBytes>SrcData.nFileSize)
			{
				const auto CurPtr = DestFile.GetPointer();
				DestFile.SetPointer(SrcData.nFileSize, nullptr, FILE_CURRENT);
				DestFile.SetEnd();
				DestFile.SetPointer(CurPtr, nullptr, FILE_BEGIN);
			}
		}
	}

	CP->SetProgressValue(0,0);

	if(SrcFile.InitWalk(CopyBufferSize))
	{
		bool AbortOp = false;

		do
		{
			if (CP->IsCancelled())
			{
				AbortOp=true;
			}

			CheckAndUpdateConsole();
			CP->SetProgressValue(CP->m_Bytes.CurrCopied, SrcData.nFileSize);

			if (AbortOp)
			{
				SrcFile.Close();

				if (!(Flags&FCOPY_COPYTONUL))
				{
					if (Append)
					{
						DestFile.SetPointer(AppendPos,nullptr,FILE_BEGIN);
					}

					DestFile.SetEnd();
					DestFile.Close();

					if (!Append)
					{
						os::SetFileAttributes(strDestName,FILE_ATTRIBUTE_NORMAL);
						os::DeleteFile(strDestName); //BUGBUG
					}
				}

				return COPY_CANCEL;
			}

			size_t BytesRead, BytesWritten;
			while (!SrcFile.Read(CopyBuffer.get(), SrcFile.GetChunkSize(), BytesRead))
			{
				Global->CatchError();
				int MsgCode = Message(MSG_WARNING|MSG_ERRORTYPE,2,MSG(lng::MError),
										MSG(lng::MCopyReadError),SrcName.data(),
										MSG(lng::MRetry),MSG(lng::MCancel));
				if (!MsgCode)
					continue;

				DWORD LastError=GetLastError();
				SrcFile.Close();

				if (!(Flags&FCOPY_COPYTONUL))
				{
					if (Append)
					{
						DestFile.SetPointer(AppendPos,nullptr,FILE_BEGIN);
					}

					DestFile.SetEnd();
					DestFile.Close();

					if (!Append)
					{
						os::SetFileAttributes(strDestName,FILE_ATTRIBUTE_NORMAL);
						os::DeleteFile(strDestName); //BUGBUG
					}
				}

				CP->SetProgressValue(0,0);
				SetLastError(LastError);
				Global->CatchError();
				CP->m_Bytes.CurrCopied = 0; // Сбросить текущий прогресс
				return COPY_FAILURE;
			}

			if (!BytesRead)
			{
				break;
			}

			if (!(Flags&FCOPY_COPYTONUL))
			{
				DestFile.SetPointer(SrcFile.GetChunkOffset() + (Append? AppendPos : 0), nullptr, FILE_BEGIN);
				while (!DestFile.Write(CopyBuffer.get(),BytesRead,BytesWritten,nullptr))
				{
					DWORD LastError=GetLastError();
					Global->CatchError();
					int Split=FALSE,SplitCancelled=FALSE,SplitSkipped=FALSE;

					if ((LastError==ERROR_DISK_FULL || LastError==ERROR_HANDLE_DISK_FULL) &&
						strDestName.size() > 1 && strDestName[1]==L':')
					{
						const auto strDriveRoot = GetPathRoot(strDestName);
						UINT64 FreeSize=0;

						if (os::GetDiskSize(strDriveRoot,nullptr,nullptr,&FreeSize))
						{
							if (FreeSize<BytesRead &&
								DestFile.Write(CopyBuffer.get(),(DWORD)FreeSize,BytesWritten,nullptr) &&
								SrcFile.SetPointer(FreeSize-BytesRead,nullptr,FILE_CURRENT))
							{
								DestFile.Close();
								int MsgCode=Message(MSG_WARNING|MSG_ERRORTYPE, MSG(lng::MError),
									{ strDestName },
									{ MSG(lng::MSplit), MSG(lng::MSkip), MSG(lng::MRetry), MSG(lng::MCancel) },
									L"CopyFiles");

								if (MsgCode==2)
								{
									SrcFile.Close();

									if (!Append)
									{
										os::SetFileAttributes(strDestName,FILE_ATTRIBUTE_NORMAL);
										os::DeleteFile(strDestName); //BUGBUG
									}

									return COPY_FAILURE;
								}

								if (!MsgCode)
								{
									Split=TRUE;

									for (;;)
									{
										if (os::GetDiskSize(strDriveRoot,nullptr,nullptr,&FreeSize))
										{
											if (FreeSize<BytesRead)
											{
												int MsgCode2 = Message(MSG_WARNING,2,MSG(lng::MWarning),
													MSG(lng::MCopyErrorDiskFull),strDestName.data(),
													MSG(lng::MRetry),MSG(lng::MCancel));

												if (MsgCode2)
												{
													Split=FALSE;
													SplitCancelled=TRUE;
												}
												else
													continue;
											}
										}
										break;
									}
								}

								if (MsgCode==1)
									SplitSkipped=TRUE;

								if (MsgCode==-1 || MsgCode==3)
									SplitCancelled=TRUE;
							}
						}
					}

					if (Split)
					{
						const auto FilePtr = SrcFile.GetPointer();
						os::FAR_FIND_DATA SplitData=SrcData;
						SplitData.nFileSize-=FilePtr;
						int RetCode = COPY_CANCEL;
						string strNewName;

						if (!AskOverwrite(SplitData, SrcName, strDestName, INVALID_FILE_ATTRIBUTES, FALSE, (Flags&FCOPY_MOVE) != 0, (Flags&FCOPY_LINK) == 0, Append, strNewName, RetCode))
						{
							SrcFile.Close();
							return COPY_CANCEL;
						}

						if (RetCode==COPY_RETRY)
						{
							strDestName=strNewName;

							if (CutToSlash(strNewName) && !os::fs::exists(strNewName))
							{
								CreatePath(strNewName);
							}

							return COPY_RETRY;
						}

						string strDestDir = strDestName;

						if (CutToSlash(strDestDir,true))
							CreatePath(strDestDir);


						if (!DestFile.Open(strDestName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, (Append ? OPEN_EXISTING:CREATE_ALWAYS), SrcData.dwFileAttributes|FILE_FLAG_SEQUENTIAL_SCAN) || (Append && !DestFile.SetPointer(0,nullptr,FILE_END)))
						{
							SrcFile.Close();
							DestFile.Close();
							return COPY_FAILURE;
						}
					}
					else
					{
						if (!SplitCancelled && !SplitSkipped &&
							Message(MSG_WARNING|MSG_ERRORTYPE,2,MSG(lng::MError),
							MSG(lng::MCopyWriteError),strDestName.data(),MSG(lng::MRetry),MSG(lng::MCancel)) == Message::first_button)
						{
							continue;
						}

						SrcFile.Close();

						if (Append)
						{
							DestFile.SetPointer(AppendPos,nullptr,FILE_BEGIN);
						}

						DestFile.SetEnd();
						DestFile.Close();

						if (!Append)
						{
							os::SetFileAttributes(strDestName,FILE_ATTRIBUTE_NORMAL);
							os::DeleteFile(strDestName); //BUGBUG
						}

						CP->SetProgressValue(0,0);
						SetLastError(LastError);

						if (SplitSkipped)
							return COPY_SKIPPED;

						return SplitCancelled? COPY_CANCEL : COPY_FAILURE;
					}

					break;
				}
			}
			else
			{
				BytesWritten=BytesRead; // не забудем приравнять количество записанных байт
			}

			CP->UpdateCurrentBytesInfo(SrcFile.GetChunkOffset() + SrcFile.GetChunkSize());
			CP->SetProgressValue(CP->m_Bytes.CurrCopied, SrcData.nFileSize);
		}
		while(SrcFile.Step());
	}

	SrcFile.Close();

	if (!(Flags&FCOPY_COPYTONUL))
	{
		DestFile.SetTime(nullptr, nullptr, &SrcData.ftLastWriteTime, nullptr);

		if (CopySparse)
		{
			INT64 Pos=SrcData.nFileSize;

			if (Append)
				Pos+=AppendPos;

			DestFile.SetPointer(Pos,nullptr,FILE_BEGIN);
			DestFile.SetEnd();
		}

		DestFile.Close();
		// TODO: ЗДЕСЯ СТАВИТЬ Compressed???
		Flags&=~FCOPY_DECRYPTED_DESTINATION;

		if (!IsWindowsVistaOrGreater() && IsWindowsServer()) // WS2003-Share SetFileTime BUG
		{
			if (FAR_GetDriveType(GetPathRoot(strDestName), 0) == DRIVE_REMOTE)
			{
				if (DestFile.Open(strDestName,GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,flags_attrs))
				{
					DestFile.SetTime(nullptr, nullptr, &SrcData.ftLastWriteTime, nullptr);
					DestFile.Close();
				}
			}
		}
	}

	return COPY_SUCCESS;
}

void ShellCopy::SetDestDizPath(const string& DestPath)
{
	if (!(Flags&FCOPY_DIZREAD))
	{
		strDestDizPath = ConvertNameToFull(DestPath);
		CutToSlash(strDestDizPath);

		if (strDestDizPath.empty())
			strDestDizPath = L".";

		if ((Global->Opt->Diz.UpdateMode==DIZ_UPDATE_IF_DISPLAYED && !SrcPanel->IsDizDisplayed()) ||
		        Global->Opt->Diz.UpdateMode==DIZ_NOT_UPDATE)
			strDestDizPath.clear();

		if (!strDestDizPath.empty())
			DestDiz.Read(strDestDizPath);

		Flags|=FCOPY_DIZREAD;
	}
}

enum WarnDlgItems
{
	WDLG_BORDER,
	WDLG_TEXT,
	WDLG_FILENAME,
	WDLG_SEPARATOR,
	WDLG_SRCFILEBTN,
	WDLG_DSTFILEBTN,
	WDLG_SEPARATOR2,
	WDLG_CHECKBOX,
	WDLG_SEPARATOR3,
	WDLG_OVERWRITE,
	WDLG_SKIP,
	WDLG_RENAME,
	WDLG_APPEND,
	WDLG_CANCEL,
};

enum
{
 DM_OPENVIEWER = DM_USER+33,
};


struct file_names_for_overwrite_dialog
{
	const string* Src;
	string* Dest;
	string* DestPath;
};

intptr_t ShellCopy::WarnDlgProc(Dialog* Dlg,intptr_t Msg,intptr_t Param1,void* Param2)
{
	switch (Msg)
	{
		case DM_OPENVIEWER:
		{
			const auto WFN = reinterpret_cast<file_names_for_overwrite_dialog*>(Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr));

			if (WFN)
			{
				LPCWSTR ViewName=nullptr;
				switch (Param1)
				{
					case WDLG_SRCFILEBTN:
						ViewName=WFN->Src->data();
						break;
					case WDLG_DSTFILEBTN:
						ViewName=WFN->Dest->data();
						break;
				}

				NamesList List;
				List.AddName(*WFN->Src);
				List.AddName(*WFN->Dest);
				List.SetCurName(*(Param1 == WDLG_SRCFILEBTN? WFN->Src : WFN->Dest));

				const auto Viewer = FileViewer::create(ViewName, FALSE, FALSE, TRUE, -1, nullptr, &List, false);
				Global->WindowManager->ExecuteModal(Viewer);
				Global->WindowManager->ResizeAllWindows();
			}
		}
		break;
		case DN_CTLCOLORDLGITEM:
		{
			if (Param1==WDLG_FILENAME)
			{
				FarColor Color=colors::PaletteColorToFarColor(COL_WARNDIALOGTEXT);
				FarDialogItemColors* Colors = static_cast<FarDialogItemColors*>(Param2);
				Colors->Colors[0] = Color;
				Colors->Colors[2] = Color;
			}
		}
		break;
		case DN_BTNCLICK:
		{
			switch (Param1)
			{
				case WDLG_SRCFILEBTN:
				case WDLG_DSTFILEBTN:
					Dlg->SendMessage(DM_OPENVIEWER, Param1, nullptr);
					break;
				case WDLG_RENAME:
				{
					const auto WFN = reinterpret_cast<file_names_for_overwrite_dialog*>(Dlg->SendMessage(DM_GETDLGDATA, 0, nullptr));
					string strDestName = *WFN->Dest;
					GenerateName(strDestName, *WFN->DestPath);

					if (Dlg->SendMessage(DM_GETCHECK, WDLG_CHECKBOX, nullptr) == BSTATE_UNCHECKED)
					{
						int All=BSTATE_UNCHECKED;

						if (GetString(MSG(lng::MCopyRenameTitle),MSG(lng::MCopyRenameText),nullptr,strDestName.data(), *WFN->Dest,L"CopyAskOverwrite",FIB_BUTTONS|FIB_NOAMPERSAND|FIB_EXPANDENV|FIB_CHECKBOX,&All,MSG(lng::MCopyRememberChoice)))
						{
							if (All!=BSTATE_UNCHECKED)
							{
								*WFN->DestPath = *WFN->Dest;
								CutToSlash(*WFN->DestPath);
							}

							Dlg->SendMessage(DM_SETCHECK,WDLG_CHECKBOX,ToPtr(All));
						}
						else
						{
							return TRUE;
						}
					}
					else
					{
						*WFN->Dest=strDestName;
					}
				}
				break;
			}
		}
		break;
		case DN_CONTROLINPUT:
		{
			const auto record = static_cast<const INPUT_RECORD*>(Param2);
			if (record->EventType==KEY_EVENT)
			{
				int key = InputRecordToKey(record);
				if ((Param1==WDLG_SRCFILEBTN || Param1==WDLG_DSTFILEBTN) && key==KEY_F3)
				{
					Dlg->SendMessage(DM_OPENVIEWER, Param1, nullptr);
				}
			}
		}
		break;

		default:
			break;
	}

	return Dlg->DefProc(Msg,Param1,Param2);
}

int ShellCopy::AskOverwrite(const os::FAR_FIND_DATA &SrcData,
                            const string& SrcName,
                            const string& DestName, DWORD DestAttr,
                            int SameName,int Rename,int AskAppend,
                            int &Append,string &strNewName,int &RetCode)
{
	enum
	{
		WARN_DLG_HEIGHT=13,
		WARN_DLG_WIDTH=76,
	};
	string qDst(DestName);
	QuoteOuterSpace(qDst);
	FarDialogItem WarnCopyDlgData[]=
	{
		{DI_DOUBLEBOX,3,1,WARN_DLG_WIDTH-4,WARN_DLG_HEIGHT-2,0,nullptr,nullptr,0,MSG(lng::MWarning)},
		{DI_TEXT,5,2,WARN_DLG_WIDTH-6,2,0,nullptr,nullptr,DIF_CENTERTEXT,MSG(lng::MCopyFileExist)},
		{DI_EDIT,5,3,WARN_DLG_WIDTH-6,3,0,nullptr,nullptr,DIF_READONLY,qDst.data()},
		{DI_TEXT,-1,4,0,4,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_BUTTON,5,5,WARN_DLG_WIDTH-6,5,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,L""},
		{DI_BUTTON,5,6,WARN_DLG_WIDTH-6,6,0,nullptr,nullptr,DIF_BTNNOCLOSE|DIF_NOBRACKETS,L""},
		{DI_TEXT,-1,7,0,7,0,nullptr,nullptr,DIF_SEPARATOR,L""},
		{DI_CHECKBOX,5,8,0,8,0,nullptr,nullptr,DIF_FOCUS,MSG(lng::MCopyRememberChoice)},
		{DI_TEXT,-1,9,0,9,0,nullptr,nullptr,DIF_SEPARATOR,L""},

		{DI_BUTTON,0,10,0,10,0,nullptr,nullptr,DIF_DEFAULTBUTTON|DIF_CENTERGROUP,MSG(lng::MCopyOverwrite)},
		{DI_BUTTON,0,10,0,10,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(lng::MCopySkipOvr)},
		{DI_BUTTON,0,10,0,10,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(lng::MCopyRename)},
		{DI_BUTTON,0,10,0,10,0,nullptr,nullptr,DIF_CENTERGROUP|(AskAppend?0:(DIF_DISABLE|DIF_HIDDEN)),MSG(lng::MCopyAppend)},
		{DI_BUTTON,0,10,0,10,0,nullptr,nullptr,DIF_CENTERGROUP,MSG(lng::MCopyCancelOvr)},
	};
	os::FAR_FIND_DATA DestData;
	int DestDataFilled=FALSE;
	Append=FALSE;

	if ((Flags&FCOPY_COPYTONUL))
	{
		RetCode=COPY_SKIPPED;
		return TRUE;
	}

	if (DestAttr==INVALID_FILE_ATTRIBUTES)
		if ((DestAttr=os::GetFileAttributes(DestName))==INVALID_FILE_ATTRIBUTES)
			return TRUE;

	if (DestAttr & FILE_ATTRIBUTE_DIRECTORY)
		return TRUE;

	const auto Format = L"{0:26} {1:20} {2} {3}";

	string strDestName = DestName;

	{
		int MsgCode = OvrMode;

		if (OvrMode == -1)
		{
			int Type;

			if ((!Global->Opt->Confirm.Copy && !Rename) || (!Global->Opt->Confirm.Move && Rename) ||
				SameName || (Type = os::GetFileTypeByName(DestName)) == FILE_TYPE_CHAR ||
				Type == FILE_TYPE_PIPE || (Flags&FCOPY_OVERWRITENEXT))
				MsgCode = 1;
			else
			{
				DestData = {};
				os::GetFindDataEx(DestName, DestData); // BUGBUG check result?
				DestDataFilled = TRUE;

				if ((Flags&FCOPY_ONLYNEWERFILES))
				{
					// сравним время
					if (DestData.ftLastWriteTime < SrcData.ftLastWriteTime)
						MsgCode = 0;
					else
						MsgCode = 2;
				}
				else
				{
					unsigned long long SrcSize = SrcData.nFileSize;
					FILETIME SrcLastWriteTime = SrcData.ftLastWriteTime;
					if (Flags&FCOPY_COPYSYMLINKCONTENTS && SrcData.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
					{
						os::FAR_FIND_DATA FindData;
						if (os::GetFindDataEx(ConvertNameToReal(SrcName), FindData))
						{
							SrcSize = FindData.nFileSize;
							SrcLastWriteTime = FindData.ftLastWriteTime;
						}
					}

					string strDateText, strTimeText;
					ConvertDate(SrcLastWriteTime, strDateText, strTimeText, 8, FALSE, FALSE, TRUE);
					const auto strSrcFileStr = format(Format, MSG(lng::MCopySource), SrcSize, strDateText, strTimeText);
					ConvertDate(DestData.ftLastWriteTime, strDateText, strTimeText, 8, FALSE, FALSE, TRUE);
					const auto strDestFileStr = format(Format, MSG(lng::MCopyDest), DestData.nFileSize, strDateText, strTimeText);

					WarnCopyDlgData[WDLG_SRCFILEBTN].Data = strSrcFileStr.data();
					WarnCopyDlgData[WDLG_DSTFILEBTN].Data = strDestFileStr.data();
					auto WarnCopyDlg = MakeDialogItemsEx(WarnCopyDlgData);
					const auto strFullSrcName = ConvertNameToFull(SrcName);
					file_names_for_overwrite_dialog WFN = { &strFullSrcName, &strDestName, &strRenamedFilesPath };
					const auto WarnDlg = Dialog::create(WarnCopyDlg, &ShellCopy::WarnDlgProc, &WFN);
					WarnDlg->SetDialogMode(DMODE_WARNINGSTYLE);
					WarnDlg->SetPosition(-1, -1, WARN_DLG_WIDTH, WARN_DLG_HEIGHT);
					WarnDlg->SetHelp(L"CopyAskOverwrite");
					WarnDlg->SetId(CopyOverwriteId);
					WarnDlg->Process();

					switch (WarnDlg->GetExitCode())
					{
					case WDLG_OVERWRITE:
						MsgCode = WarnCopyDlg[WDLG_CHECKBOX].Selected ? 1 : 0;
						break;
					case WDLG_SKIP:
						MsgCode = WarnCopyDlg[WDLG_CHECKBOX].Selected ? 3 : 2;
						break;
					case WDLG_RENAME:
						MsgCode = WarnCopyDlg[WDLG_CHECKBOX].Selected ? 5 : 4;
						break;
					case WDLG_APPEND:
						MsgCode = WarnCopyDlg[WDLG_CHECKBOX].Selected ? 7 : 6;
						break;
					case -1:
					case -2:
					case WDLG_CANCEL:
						MsgCode = 8;
						break;
					}
				}
			}
		}

		switch (MsgCode)
		{
		case 1:
			OvrMode = 1;
		case 0:
			break;
		case 3:
			OvrMode = 2;
		case 2:
			RetCode = COPY_SKIPPED;
			return FALSE;
		case 5:
			OvrMode = 5;
			GenerateName(strDestName, strRenamedFilesPath);
		case 4:
			RetCode = COPY_RETRY;
			strNewName = strDestName;
			break;
		case 7:
			OvrMode = 6;
		case 6:
			Append = TRUE;
			break;
		case -1:
		case -2:
		case 8:
			RetCode = COPY_CANCEL;
			return FALSE;
		}
	}

	if (RetCode!=COPY_RETRY)
	{
		if ((DestAttr & FILE_ATTRIBUTE_READONLY) && !(Flags&FCOPY_OVERWRITENEXT))
		{
			int MsgCode=0;

			if (!SameName)
			{
				if (ReadOnlyOvrMode!=-1)
				{
					MsgCode=ReadOnlyOvrMode;
				}
				else
				{
					if (!DestDataFilled)
					{
						DestData = {};
						os::GetFindDataEx(DestName,DestData); // BUGBUG check result
					}

					string strDateText, strTimeText;
					ConvertDate(SrcData.ftLastWriteTime,strDateText,strTimeText,8,FALSE,FALSE,TRUE);
					const auto strSrcFileStr = format(Format, MSG(lng::MCopySource), SrcData.nFileSize, strDateText, strTimeText);
					ConvertDate(DestData.ftLastWriteTime,strDateText,strTimeText,8,FALSE,FALSE,TRUE);
					const auto strDestFileStr = format(Format, MSG(lng::MCopyDest), DestData.nFileSize, strDateText, strTimeText);
					WarnCopyDlgData[WDLG_SRCFILEBTN].Data=strSrcFileStr.data();
					WarnCopyDlgData[WDLG_DSTFILEBTN].Data=strDestFileStr.data();
					WarnCopyDlgData[WDLG_TEXT].Data=MSG(lng::MCopyFileRO);
					WarnCopyDlgData[WDLG_OVERWRITE].Data=MSG(Append? lng::MCopyAppend : lng::MCopyOverwrite);
					WarnCopyDlgData[WDLG_RENAME].Type=DI_TEXT;
					WarnCopyDlgData[WDLG_RENAME].Data=L"";
					WarnCopyDlgData[WDLG_APPEND].Type=DI_TEXT;
					WarnCopyDlgData[WDLG_APPEND].Data=L"";
					auto WarnCopyDlg = MakeDialogItemsEx(WarnCopyDlgData);
					const auto strSrcName = ConvertNameToFull(SrcData.strFileName);
					file_names_for_overwrite_dialog WFN[] = { &strSrcName, &strDestName };
					const auto WarnDlg = Dialog::create(WarnCopyDlg, &ShellCopy::WarnDlgProc, &WFN);
					WarnDlg->SetDialogMode(DMODE_WARNINGSTYLE);
					WarnDlg->SetPosition(-1,-1,WARN_DLG_WIDTH,WARN_DLG_HEIGHT);
					WarnDlg->SetHelp(L"CopyFiles");
					WarnDlg->SetId(CopyReadOnlyId);
					WarnDlg->Process();

					switch (WarnDlg->GetExitCode())
					{
						case WDLG_OVERWRITE:
							MsgCode=WarnCopyDlg[WDLG_CHECKBOX].Selected?1:0;
							break;
						case WDLG_SKIP:
							MsgCode=WarnCopyDlg[WDLG_CHECKBOX].Selected?3:2;
							break;
						case -1:
						case -2:
						case WDLG_CANCEL:
							MsgCode=8;
							break;
					}
				}
			}

			switch (MsgCode)
			{
				case 1:
					ReadOnlyOvrMode=1;
				case 0:
					break;
				case 3:
					ReadOnlyOvrMode=2;
				case 2:
					RetCode=COPY_SKIPPED;
					return FALSE;
				case -1:
				case -2:
				case 8:
					RetCode=COPY_CANCEL;
					return FALSE;
			}
		}

		if (!SameName && (DestAttr & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)))
			os::SetFileAttributes(DestName,FILE_ATTRIBUTE_NORMAL);
	}

	return TRUE;
}



bool ShellCopy::GetSecurity(const string& FileName, os::FAR_SECURITY_DESCRIPTOR& sd)
{
	if (!os::GetFileSecurity(NTPath(FileName), DACL_SECURITY_INFORMATION, sd))
	{
		if (!SkipSecurityErrors)
		{
			Global->CatchError();
			switch (Message(MSG_WARNING | MSG_ERRORTYPE, 3, MSG(lng::MError),
				MSG(lng::MCannotGetSecurity),
				FileName.data(),
				MSG(lng::MSkip), MSG(lng::MCopySkipAll), MSG(lng::MCancel)))
			{
			case 0:
				break;

			case 1:
				SkipSecurityErrors = true;
				break;

			default:
				return false;
			}
		}
	}
	return true;
}


bool ShellCopy::SetSecurity(const string& FileName, const os::FAR_SECURITY_DESCRIPTOR& sd)
{
	if (!os::SetFileSecurity(NTPath(FileName), DACL_SECURITY_INFORMATION, sd))
	{
		if (!SkipSecurityErrors)
		{
			Global->CatchError();
			switch (Message(MSG_WARNING | MSG_ERRORTYPE, 3, MSG(lng::MError),
				MSG(lng::MCannotSetSecurity),
				FileName.data(),
				MSG(lng::MSkip), MSG(lng::MCopySkipAll), MSG(lng::MCancel)))
			{
			case 0:
				break;

			case 1:
				SkipSecurityErrors = true;
				break;

			default:
				return false;
			}
		}
	}
	return true;
}

// BUGBUG move to copy_progress
static BOOL ShellCopySecuryMsg(const copy_progress* CP, const string& Name)
{
	if (Name.empty() || CP->m_SecurityTimeCheck)
	{
		static int Width=30;
		int WidthTemp;
		if (!Name.empty())
		{
			WidthTemp=std::max(static_cast<int>(Name.size()),30);
		}
		else
			Width=WidthTemp=30;

		WidthTemp=std::min(WidthTemp, ScrX/2);
		Width=std::max(Width,WidthTemp);

		string strOutFileName = Name; //??? nullptr ???
		TruncPathStr(strOutFileName,Width);
		strOutFileName = fit_to_center(strOutFileName, Width + 4);
		Message(0,0,MSG(lng::MMoveDlgTitle),MSG(lng::MCopyPrepareSecury),strOutFileName.data());

		if (CP->IsCancelled())
		{
			return FALSE;
		}
	}

	return TRUE;
}


int ShellCopy::SetRecursiveSecurity(const string& FileName,const os::FAR_SECURITY_DESCRIPTOR& sd)
{
	if (SetSecurity(FileName, sd))
	{
		if (os::fs::is_directory(FileName))
		{
			ScanTree ScTree(true, true, Flags & FCOPY_COPYSYMLINKCONTENTS);
			ScTree.SetFindPath(FileName,L"*",FSCANTREE_FILESFIRST);

			string strFullName;
			os::FAR_FIND_DATA SrcData;
			while (ScTree.GetNextName(SrcData,strFullName))
			{
				if (!ShellCopySecuryMsg(CP.get(), strFullName))
					break;

				if (!SetSecurity(strFullName, sd))
				{
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	return FALSE;
}

int ShellCopy::ShellSystemCopy(const string& SrcName,const string& DestName,const os::FAR_FIND_DATA &SrcData)
{
	os::FAR_SECURITY_DESCRIPTOR sd;

	if ((Flags&FCOPY_COPYSECURITY) && !GetSecurity(SrcName, sd))
		return COPY_CANCEL;

	CP->SetNames(SrcName,DestName);
	CP->SetProgressValue(0,0);

	m_FileHandleForStreamSizeFix = nullptr;

	struct callback_wrapper
	{
		static DWORD WINAPI callback(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred, LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred, DWORD StreamNumber, DWORD CallbackReason, HANDLE SourceFile, HANDLE DestinationFile, LPVOID Data)
		{
			return static_cast<ShellCopy*>(Data)->CopyProgressRoutine(TotalFileSize.QuadPart, TotalBytesTransferred.QuadPart, StreamSize.QuadPart, StreamBytesTransferred.QuadPart, StreamNumber, CallbackReason, SourceFile, DestinationFile);
		}
	};

	if (!os::CopyFileEx(SrcName, DestName, callback_wrapper::callback, this, nullptr, Flags&FCOPY_DECRYPTED_DESTINATION ? COPY_FILE_ALLOW_DECRYPTED_DESTINATION : 0))
	{
		Flags&=~FCOPY_DECRYPTED_DESTINATION;
		return (GetLastError() == ERROR_REQUEST_ABORTED)? COPY_CANCEL : COPY_FAILURE;
	}

	Flags&=~FCOPY_DECRYPTED_DESTINATION;

	if ((Flags&FCOPY_COPYSECURITY) && !SetSecurity(DestName, sd))
		return COPY_CANCEL;

	return COPY_SUCCESS;
}

DWORD ShellCopy::CopyProgressRoutine(unsigned long long TotalFileSize, unsigned long long TotalBytesTransferred, unsigned long long StreamSize, unsigned long long StreamBytesTransferred, DWORD dwStreamNumber, DWORD dwCallbackReason, HANDLE hSourceFile, HANDLE hDestinationFile)
{
	// // _LOGCOPYR(CleverSysLog clv(L"CopyProgressRoutine"));
	// // _LOGCOPYR(SysLog(L"dwStreamNumber=%d",dwStreamNumber));
	bool Abort = false;
	if (CP->IsCancelled())
	{
		Abort=true;
	}

	CheckAndUpdateConsole();
	//fix total size
	if (dwStreamNumber == 1 && hSourceFile != m_FileHandleForStreamSizeFix)
	{
		CP->m_Bytes.Total -= StreamSize;
		CP->m_Bytes.Total += TotalFileSize;
		m_FileHandleForStreamSizeFix = hSourceFile;
	}

	CP->UpdateCurrentBytesInfo(TotalBytesTransferred);
	CP->SetProgressValue(TotalBytesTransferred, TotalFileSize);

	return Abort?PROGRESS_CANCEL:PROGRESS_CONTINUE;
}

bool ShellCopy::CalcTotalSize() const
{
	string strSelName, strSelShortName;
	DWORD FileAttr;
	unsigned long long FileSize;
	// Для фильтра
	os::FAR_FIND_DATA fd;

	CP->m_Bytes.Total = 0;
	CP->m_Bytes.CurrCopied = 0;
	CP->m_Files.Total = 0;
	SrcPanel->GetSelName(nullptr,FileAttr);

	while (SrcPanel->GetSelName(&strSelName,FileAttr,&strSelShortName,&fd))
	{
		if ((FileAttr&FILE_ATTRIBUTE_REPARSE_POINT) && !(Flags&FCOPY_COPYSYMLINKCONTENTS))
			continue;

		if (FileAttr & FILE_ATTRIBUTE_DIRECTORY)
		{
			{
				DirInfoData Data = {};
				CP->SetScanName(strSelName);
				int __Ret = GetDirInfo(L"", strSelName, Data, getdirinfo_infinite_delay, m_Filter.get(), (Flags&FCOPY_COPYSYMLINKCONTENTS ? GETDIRINFO_SCANSYMLINK : 0) | (m_UseFilter? GETDIRINFO_USEFILTER : 0));
				FileSize = Data.FileSize;
				if (__Ret <= 0)
				{
					return false;
				}

				if (Data.FileCount > 0)
				{
					CP->m_Bytes.Total += FileSize;
					CP->m_Files.Total += Data.FileCount;
				}
			}
		}
		else
		{
			//  Подсчитаем количество файлов
			if (m_UseFilter)
			{
				if (!m_Filter->FileInFilter(fd, nullptr, &fd.strFileName))
					continue;
			}

			FileSize = SrcPanel->GetLastSelectedSize();

			if (FileSize != (unsigned long long)-1)
			{
				CP->m_Bytes.Total += FileSize;
				++CP->m_Files.Total;
			}
		}
	}

	// INFO: Это для варианта, когда "ВСЕГО = общий размер * количество целей"
	CP->m_Bytes.Total *= m_NumberOfTargets;
	CP->m_Files.Total *= m_NumberOfTargets;
	return true;
}

/*
  Оболочка вокруг SetFileAttributes() для
  корректного выставления атрибутов
*/
bool ShellCopy::ShellSetAttr(const string& Dest, DWORD Attr)
{
	auto strRoot = GetPathRoot(Dest);
	if (!os::fs::exists(strRoot))
	{
		return false;
	}

	DWORD FileSystemFlagsDst;
	int GetInfoSuccess=os::GetVolumeInformation(strRoot,nullptr,nullptr,nullptr,&FileSystemFlagsDst,nullptr);

	if (GetInfoSuccess)
	{
		if (!(FileSystemFlagsDst&FILE_FILE_COMPRESSION))
		{
			Attr&=~FILE_ATTRIBUTE_COMPRESSED;
		}

		if (!(FileSystemFlagsDst&FILE_SUPPORTS_ENCRYPTION))
		{
			Attr&=~FILE_ATTRIBUTE_ENCRYPTED;
		}
	}

	if (!os::SetFileAttributes(Dest,Attr))
	{
		return false;
	}

	if ((Attr&FILE_ATTRIBUTE_COMPRESSED) && !(Attr&FILE_ATTRIBUTE_ENCRYPTED))
	{
		int Ret=ESetFileCompression(Dest,1,Attr&(~FILE_ATTRIBUTE_COMPRESSED),SkipMode);

		if (Ret==SETATTR_RET_ERROR)
		{
			return false;
		}
		else if (Ret==SETATTR_RET_SKIPALL)
		{
			SkipMode = SETATTR_RET_SKIP;
		}
	}

	// При копировании/переносе выставляем FILE_ATTRIBUTE_ENCRYPTED
	// для каталога, если он есть
	if (GetInfoSuccess && FileSystemFlagsDst&FILE_SUPPORTS_ENCRYPTION && Attr&FILE_ATTRIBUTE_ENCRYPTED && Attr&FILE_ATTRIBUTE_DIRECTORY)
	{
		int Ret=ESetFileEncryption(Dest, true, 0, SkipMode);

		if (Ret==SETATTR_RET_ERROR)
		{
			return false;
		}
		else if (Ret==SETATTR_RET_SKIPALL)
		{
			SkipMode=SETATTR_RET_SKIP;
		}
	}

	return true;
}
