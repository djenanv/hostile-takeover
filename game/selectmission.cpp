#include "game/ht.h"
#include "base/format.h"
#include "yajl/wrapper/jsontypes.h"
#include "game/httppackinfomanager.h"
#include "game/completemanager.h"

namespace wi {

#define kcUnlockAhead 1
#define kfItemLocked 0x8000

// Should be moved into PackInfoManager, but in a better form,
// for getting specific properties without needing to know the key.

const char *GetString(const json::JsonMap *map, const char *key) {
    const json::JsonObject *obj = map->GetObject(key);
    if (obj == NULL || obj->type() != json::JSONTYPE_STRING) {
        return NULL;
    }
    const json::JsonString *s = (json::JsonString *)obj;

    bool fWhitespace = true;
    char ch; 
    const char *psz = s->GetString();
    while ((ch = *psz++) != 0) {
        if (!isspace(ch)) {
            fWhitespace = false;
            break;
        }
    }
    if (fWhitespace) {
        return NULL;
    }
    return s->GetString();
}

SelectMissionForm::SelectMissionForm(MissionList *pml,
        const MissionIdentifier *pmiidFind) {
    m_pml = pml;
    m_pmiidFind = pmiidFind;
    m_mt = kmtStory;
    m_cMagic = 0;
    m_fMagicUnlock = false;
#ifdef DEBUG
    m_fMagicUnlock = true;
#endif
    memset(m_aplstc, 0, sizeof(m_aplstc));
    memset(m_arcTabs, 0, sizeof(m_arcTabs));
    memset(m_arcCards, 0, sizeof(m_arcCards));
    m_rcPlay.SetEmpty();
    m_rcBack.SetEmpty();
    m_iPressedZone = -1;
}

bool SelectMissionForm::Init(FormMgr *pfrmm, IniReader *pini, word idf) {
	if (!ShellForm::Init(pfrmm, pini, idf))
		return false;

    // Format the lists. 3 lists are used as a simple cache.

    int aidcList[] = { kidcStoryList, kidcChallengeList, kidcAddOnList };
    for (int i = 0; i < ARRAYSIZE(aidcList); i++) {
        ListControl *plstc = (ListControl *)GetControlPtr(aidcList[i]);
        m_aplstc[i] = plstc;
        Rect rc;
        plstc->GetRect(&rc);
        Font *pfnt = gapfnt[kifntShadow];
        int cxComplete = pfnt->GetTextExtent("Complete");
        int xTitle = rc.Width() / 2 - cxComplete * 3 / 2;
        int xRightComplete = rc.Width() - 10;
        int xLeftComplete = xRightComplete - cxComplete - cxComplete / 2;
        plstc->SetTabStops(xTitle, xLeftComplete, xRightComplete);
        plstc->SetTabFlags(kfLstTabEllipsis, kfLstTabCenter, 0);
        plstc->SetFlags(plstc->GetFlags() | kfLstcKeepInteriorPositioning);
    }

    // If asked to find a certain mission, find it first to see what
    // type it is, and switch the radio button bar to that type.

    int iPack = -1;
    int iMission = -1;
	int cMissions = m_pml->GetCount();
    if (m_pmiidFind != NULL) {
        for (int i = 0; i < cMissions; i++) {
            MissionIdentifier miid;
            m_pml->GetMissionIdentifier(i, &miid);
            if (memcmp(&miid.packid, &m_pmiidFind->packid,
                    sizeof(miid.packid)) == 0) {
                if (iPack == -1) {
                    iPack = i;
                }
                if (strcmp(miid.szLvlFilename,
                        m_pmiidFind->szLvlFilename) == 0) {
                    iMission = i;
                    break;
                }
            }
        }
        if (iMission == -1) {
            iMission = iPack;
        }
    }
    int iMissionSelect = iMission;

    // Init the lists

    MissionType mt = InitLists(iMissionSelect);
    SwitchToMissionType(mt);
	HideLegacyControls();

    // Hide this label - only show it if there are no addon packs
    GetControlPtr(kidcAddOnMessage)->Show(false);

	return true;
}

void SelectMissionForm::HideLegacyControls() {
	for (int i = 0; i < ARRAYSIZE(m_aplstc); i++) {
		if (m_aplstc[i] != NULL) {
			m_aplstc[i]->Show(false);
		}
	}
	word aidc[] = { kidcCategories, kidcMissionPackInfo, kidcAddOnMessage,
			kidcOk, kidcCancel };
	for (int i = 0; i < ARRAYSIZE(aidc); i++) {
		Control *pctl = GetControlPtr(aidc[i]);
		if (pctl != NULL) {
			pctl->Show(false);
		}
	}
}

void SelectMissionForm::UpdateLayout() {
	int cx = m_rc.Width();
	int cy = m_rc.Height();
	int xPad = _max(PcFromFc(6), cx / 40);
	int gap = _max(PcFromFc(3), cx / 80);
	int yTabs = PcFromFc(22);
	int cyTab = PcFromFc(13);
	int cxTab = (cx - xPad * 2 - gap * 2) / 3;
	for (int i = 0; i < 3; i++) {
		m_arcTabs[i].Set(xPad + i * (cxTab + gap), yTabs,
				xPad + i * (cxTab + gap) + cxTab, yTabs + cyTab);
	}

	int yBody = yTabs + cyTab + gap * 2;
	int yFooter = cy - PcFromFc(25);
	int cyCard = _max(PcFromFc(54), yFooter - yBody);
	int cxFeature = (cx - xPad * 2) * 3 / 5;
	int cxPreview = cx - xPad * 2 - cxFeature - gap;
	m_arcCards[0].Set(xPad, yBody, xPad + cxFeature, yBody + cyCard);
	for (int i = 1; i < 4; i++) {
		int y = yBody + (i - 1) * (cyCard + gap) / 3;
		int yNext = yBody + i * cyCard / 3;
		m_arcCards[i].Set(xPad + cxFeature + gap, y,
				xPad + cxFeature + gap + cxPreview, yNext - gap);
	}

	int cyButton = PcFromFc(13);
	int yButton = cy - cyButton - PcFromFc(5);
	int cxPlay = _max(PcFromFc(54), cxFeature - gap);
	m_rcPlay.Set(xPad, yButton, xPad + cxPlay, yButton + cyButton);
	m_rcBack.Set(cx - xPad - PcFromFc(49), yButton,
		cx - xPad, yButton + cyButton);
}

static void DrawMissionText(DibBitmap *pbm, Font *pfnt, const char *psz,
		int x, int y, int cx, int cy, bool fEllipsis) {
	char sz[128];
	strncpyz(sz, psz != NULL ? psz : "", sizeof(sz));
	pfnt->DrawText(pbm, sz, x, y, cx, cy, fEllipsis);
}

static void DrawMissionBox(DibBitmap *pbm, Rect *prc, Color clrFill,
		Color clrBorder, int cxyBorder) {
	pbm->Fill(prc->left, prc->top, prc->Width(), prc->Height(), clrFill);
	DrawBorder(pbm, prc, cxyBorder, clrBorder);
}

bool SelectMissionForm::GetCardMission(int iCard, int *piMission,
		MissionDescription *pmd, bool *pfLocked, bool *pfComplete) {
	if (iCard < 0 || iCard >= 4 || piMission == NULL || pmd == NULL ||
			pfLocked == NULL || pfComplete == NULL) {
		return false;
	}
	ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
	if (plstc == NULL || iCard >= plstc->GetCount()) {
		return false;
	}
	int iTarget = plstc->GetSelectedItemIndex();
	if (iTarget < 0) {
		iTarget = 0;
	}
	iTarget += iCard;
	if (iTarget >= plstc->GetCount()) {
		return false;
	}

	int iList = 0;
	int iliLastCompleteStory = -1;
	for (int i = 0; i < m_pml->GetCount(); i++) {
		MissionDescription md;
		if (!m_pml->GetMissionDescription(i, &md) ||
				md.mt != m_mt) {
			continue;
		}
		MissionIdentifier miid;
		if (!m_pml->GetMissionIdentifier(i, &miid)) {
			continue;
		}
		bool fLocked = false;
		if (md.mt == kmtStory) {
			int iliMissionLocked = iliLastCompleteStory + 1 + kcUnlockAhead;
			if (iList >= iliMissionLocked) {
				fLocked = true;
			}
		}
		bool fComplete = gpcptm->IsComplete(&miid);
		if (md.mt == kmtStory && fComplete) {
			iliLastCompleteStory = iList;
		}
		if (iList == iTarget) {
			*piMission = i;
			*pmd = md;
			*pfLocked = fLocked;
			*pfComplete = fComplete;
			return true;
		}
		iList++;
	}
	return false;
}

void SelectMissionForm::OnPaint(DibBitmap *pbm) {
	UpdateLayout();
	int xForm = m_rc.left;
	int yForm = m_rc.top;
	Color clrPanel = GetColor(kiclrListBackground);
	Color clrPanelAlt = GetColor(kiclrFormBackground);
	Color clrAccent = GetColor(kiclrCyanSideFirst);
	Color clrAccentBright = GetColor(kiclrWhite);
	Color clrMuted = GetColor(kiclrMediumGray);
	Font *pfntTitle = gapfnt[kifntTitle];
	Font *pfnt = gapfnt[kifntShadow];
	Font *pfntButton = gapfnt[kifntButton];

	int xPad = _max(PcFromFc(8), m_rc.Width() / 40);
	DrawMissionText(pbm, pfntTitle, "CAMPAIGN HUB", xForm + xPad,
			yForm + PcFromFc(7), m_rc.Width() - xPad * 2, PcFromFc(13), false);
	DrawMissionText(pbm, gapfnt[kifntDefault], "SELECT YOUR NEXT OPERATION",
			xForm + xPad, yForm + PcFromFc(20),
			m_rc.Width() - xPad * 2, PcFromFc(7), false);

	const char *aszTabs[] = { "STORY", "CHALLENGE", "ADD-ON" };
	for (int i = 0; i < 3; i++) {
		Rect rc = m_arcTabs[i];
		rc.Offset(xForm, yForm);
		bool fSelected = (i == IndexFromMissionType(m_mt));
		DrawMissionBox(pbm, &rc, fSelected ? clrAccent : clrPanel,
				fSelected ? clrAccentBright : clrMuted, PcFromFc(1));
		DrawMissionText(pbm, pfnt, aszTabs[i], rc.left, rc.top + PcFromFc(3),
				rc.Width(), rc.Height() - PcFromFc(3), false);
	}

	ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
	int iSelected = plstc != NULL ? plstc->GetSelectedItemIndex() : -1;
	MissionDescription mdSelected;
	int iMissionSelected = -1;
	bool fSelectedLocked = false;
	bool fSelectedComplete = false;
	GetCardMission(0, &iMissionSelected, &mdSelected, &fSelectedLocked,
			&fSelectedComplete);

	for (int i = 0; i < 4; i++) {
		int iMission;
		MissionDescription md;
		bool fLocked;
		bool fComplete;
		if (!GetCardMission(i, &iMission, &md, &fLocked, &fComplete)) {
			continue;
		}
		Rect rc = m_arcCards[i];
		rc.Offset(xForm, yForm);
		bool fFeature = (i == 0);
		// Card zero is always the selected mission; the compact cards are the
		// missions immediately following it in the current category.
		bool fCurrent = (i == 0);
		Color clrFill = fFeature ? clrPanelAlt : clrPanel;
		Color clrBorder = fCurrent ? clrAccentBright :
				(fLocked ? clrMuted : clrAccent);
		DrawMissionBox(pbm, &rc, clrFill, clrBorder, fCurrent ? PcFromFc(2) : PcFromFc(1));

		char szNumber[16];
		sprintf(szNumber, "M%02d", (plstc != NULL ? iSelected : 0) + i);
		if (fFeature) {
			DrawMissionText(pbm, pfntTitle, szNumber, rc.left + PcFromFc(6),
				rc.top + PcFromFc(6), rc.Width() - PcFromFc(12), PcFromFc(13), false);
			DrawMissionText(pbm, pfntTitle, md.szLvlTitle,
				rc.left + PcFromFc(6), rc.top + PcFromFc(22),
				rc.Width() - PcFromFc(12), PcFromFc(16), true);
			DrawMissionText(pbm, pfnt, md.szPackName, rc.left + PcFromFc(6),
				rc.top + PcFromFc(39), rc.Width() - PcFromFc(12), PcFromFc(8), true);
			const char *pszStatus = fLocked ? "LOCKED" :
					(fComplete ? "COMPLETE" : "READY TO DEPLOY");
			DrawMissionText(pbm, pfnt, pszStatus, rc.left + PcFromFc(6),
				rc.bottom - PcFromFc(12), rc.Width() - PcFromFc(12), PcFromFc(8), false);
		} else {
			DrawMissionText(pbm, pfnt, szNumber, rc.left + PcFromFc(4),
				rc.top + PcFromFc(3), PcFromFc(30), PcFromFc(8), false);
			DrawMissionText(pbm, pfnt, md.szLvlTitle, rc.left + PcFromFc(34),
				rc.top + PcFromFc(3), rc.Width() - PcFromFc(38),
				rc.Height() - PcFromFc(5), true);
			if (fLocked) {
				DrawMissionText(pbm, pfnt, "LOCKED", rc.left + PcFromFc(4),
					rc.bottom - PcFromFc(9), rc.Width() - PcFromFc(8), PcFromFc(7), false);
			}
		}
	}

	Rect rcPlay = m_rcPlay;
	rcPlay.Offset(xForm, yForm);
	bool fCanPlay = iSelected >= 0 && (!fSelectedLocked || m_fMagicUnlock);
	DrawMissionBox(pbm, &rcPlay, fCanPlay ? clrAccent : clrPanel,
			fCanPlay ? clrAccentBright : clrMuted, PcFromFc(1));
	DrawMissionText(pbm, pfntButton, fCanPlay ? "PLAY" : "SELECT A MISSION",
			rcPlay.left, rcPlay.top + PcFromFc(3), rcPlay.Width(),
		rcPlay.Height() - PcFromFc(3), false);

	Rect rcBack = m_rcBack;
	rcBack.Offset(xForm, yForm);
	DrawMissionBox(pbm, &rcBack, clrPanel, clrAccent, PcFromFc(1));
	DrawMissionText(pbm, pfntButton, "BACK", rcBack.left,
			rcBack.top + PcFromFc(3), rcBack.Width(),
		rcBack.Height() - PcFromFc(3), false);
}

void SelectMissionForm::OnPaintControls(DibBitmap *pbm, UpdateMap *pupd) {
	// The legacy form controls remain the data and navigation source for this
	// screen, but their compact renderer is intentionally not painted. The
	// campaign hub above owns the complete visual layer and touch handling.
}

int SelectMissionForm::IndexFromMissionType(MissionType mt) {
    switch (mt) {
    case kmtStory:
        return 0;
    case kmtChallenge:
        return 1;
    case kmtAddOn:
        return 2;
    default:
        return -1;
    }
}

MissionType SelectMissionForm::MissionTypeFromIndex(int i) {
    switch (i) {
    case 0:
        return kmtStory;
    case 1:
        return kmtChallenge;
    case 2:
        return kmtAddOn;
    default:
        return kmtUnknown;
    }
}

void SelectMissionForm::SwitchToMissionType(MissionType mt) {
    m_mt = mt;
    RadioButtonBarControl *prbbc =
            (RadioButtonBarControl *)GetControlPtr(kidcCategories);
    prbbc->SetSelectionIndex(IndexFromMissionType(mt));
    for (int i = 0; i < ARRAYSIZE(m_aplstc); i++) {
        bool fShow = false;
        if (i == IndexFromMissionType(mt)) {
            fShow = true;
        }
        m_aplstc[i]->Show(fShow);
    }
    UpdateDescription();
}

MissionType SelectMissionForm::InitLists(int iMissionSelect) {
    // Fill in the lists, and along the way keep track of useful selection
    // indexes.

    int ailiFirstIncomplete[kmtUnknown + 1];
    memset(ailiFirstIncomplete, 0xff, sizeof(ailiFirstIncomplete));
    int iliLastCompleteStory = -1;
    int iliSelectSpecial = -1;
    MissionType mtSelectSpecial = kmtUnknown;

    int cMissions = m_pml->GetCount();
	for (int i = 0; i < cMissions; i++) {
        MissionDescription md;
        if (!m_pml->GetMissionDescription(i, &md)) {
            continue;
        }
        if (md.mt != kmtStory && md.mt != kmtChallenge && md.mt != kmtAddOn) {
            continue;
        }

        // The first locked mission is kcUnlockAhead missions ahead of the
        // last complete story mission.

        ListControl *plstc = m_aplstc[IndexFromMissionType(md.mt)];
        bool fLocked = false;
        if (md.mt == kmtStory) {
            int iliMissionLocked = iliLastCompleteStory + 1 + kcUnlockAhead;
            int iliNext = plstc->GetCount();
            if (iliNext >= iliMissionLocked) {
                fLocked = true;
            }
        }

        // Get the status - LOCKED, Complete, or nothing

        MissionIdentifier miid;
        m_pml->GetMissionIdentifier(i, &miid);
        dword dw = (dword)i;
        const char *pszStatus = "";
        if (fLocked) {
            pszStatus = "LOCKED";
            dw |= kfItemLocked;
        } else {
            if (gpcptm->IsComplete(&miid)) {
                pszStatus = "Complete";
            }
        }

        // Add the item

        plstc->Add(base::Format::ToString("%s\t%s", md.szLvlTitle,
                pszStatus), (void *)(pword)dw);

        // Track the first incomplete for each mission type.

        if (ailiFirstIncomplete[md.mt] == -1) {
            if (!gpcptm->IsComplete(&miid)) {
                ailiFirstIncomplete[md.mt] = plstc->GetCount() - 1;
            }
        }

        // Track the last complete for story missions, for mission unlocking

        if (md.mt == kmtStory) {
            if (gpcptm->IsComplete(&miid)) {
                iliLastCompleteStory = plstc->GetCount() - 1;
            }
        }

        // This is passed in when the form needs to select a certain
        // mission when it first shows.

        if (i == iMissionSelect) {
            iliSelectSpecial = plstc->GetCount() - 1;
            mtSelectSpecial = md.mt;
        }
    }

    // The initially selected missions are the first incomplete missions
    // for each mission type.

    MissionType mtSelect = kmtStory;
    for (int i = 0; i < ARRAYSIZE(m_aplstc); i++) {
        ListControl *plstc = m_aplstc[i];

        // Is this the list that is awarded the special selection?

        if (i == IndexFromMissionType(mtSelectSpecial)) {
            plstc->Select(iliSelectSpecial, true, true);
            mtSelect = mtSelectSpecial;
            continue;
        }

        int iliSelect = ailiFirstIncomplete[MissionTypeFromIndex(i)];
        if (iliSelect < 0) {
            iliSelect = 0;
        }
        plstc->Select(iliSelect, true, true);
    }
    return mtSelect;
}

bool SelectMissionForm::OnPenEvent(Event *pevt) {
	UpdateLayout();
	int x = pevt->x - m_rc.left;
	int y = pevt->y - m_rc.top;
	int iZone = -1;
	for (int i = 0; i < 3; i++) {
		Rect rc = m_arcTabs[i];
		if ((pevt->ff & kfEvtFinger) != 0) {
			rc.Inflate(PcFromFc(2), PcFromFc(2));
		}
		if (rc.PtIn(x, y)) {
			iZone = i;
			break;
		}
	}
	if (iZone == -1) {
		for (int i = 0; i < 4; i++) {
			Rect rc = m_arcCards[i];
			if ((pevt->ff & kfEvtFinger) != 0) {
				rc.Inflate(PcFromFc(2), PcFromFc(2));
			}
			if (rc.PtIn(x, y)) {
				iZone = 10 + i;
				break;
			}
		}
	}
	if (iZone == -1) {
		Rect rc = m_rcPlay;
		if ((pevt->ff & kfEvtFinger) != 0) {
			rc.Inflate(PcFromFc(2), PcFromFc(2));
		}
		if (rc.PtIn(x, y)) {
			iZone = 20;
		}
	}
	if (iZone == -1) {
		Rect rc = m_rcBack;
		if ((pevt->ff & kfEvtFinger) != 0) {
			rc.Inflate(PcFromFc(2), PcFromFc(2));
		}
		if (rc.PtIn(x, y)) {
			iZone = 21;
		}
	}

	if (pevt->eType == penDownEvent) {
		m_iPressedZone = iZone;
		return iZone != -1;
	}
	if (pevt->eType != penUpEvent) {
		return m_iPressedZone != -1;
	}
	int iPressedZone = m_iPressedZone;
	m_iPressedZone = -1;
	if (iPressedZone < 0 || iPressedZone != iZone) {
		return iPressedZone != -1;
	}

	if (iPressedZone >= 0 && iPressedZone < 3) {
		MissionType mtNew = MissionTypeFromIndex(iPressedZone);
		if (mtNew != m_mt) {
			SwitchToMissionType(mtNew);
			InvalidateRect(NULL);
		}
		return true;
	}
	if (iPressedZone >= 10 && iPressedZone < 14) {
		ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
		int iItem = plstc != NULL ? plstc->GetSelectedItemIndex() : -1;
		if (plstc != NULL && iItem >= 0) {
			iItem += iPressedZone - 10;
			if (iItem < plstc->GetCount()) {
				plstc->Select(iItem, true, true);
				UpdateDescription();
				InvalidateRect(NULL);
			}
		}
		return true;
	}
	if (iPressedZone == 20) {
		ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
		if (plstc != NULL && plstc->GetSelectedItemIndex() >= 0 &&
				(!IsSelectedMissionLocked(plstc) || m_fMagicUnlock)) {
			OnControlSelected(kidcOk);
		}
		return true;
	}
	if (iPressedZone == 21) {
		// Preserve the legacy debug/QA unlock gesture even though the visual
		// Back button is now custom-painted.
		if (m_mt == kmtStory) {
			m_cMagic++;
			if (m_cMagic >= 5) {
				m_cMagic = 0;
				m_fMagicUnlock = true;
			}
		}
		OnControlSelected(kidcCancel);
		return true;
	}
	return false;
}

void SelectMissionForm::OnControlSelected(word idc) {
    switch (idc) {
    case kidcOk:
    case kidcCancel:
        EndForm(idc);
        break;

    case kidcSetupGame:
        {
            ShellForm *pfrm = (ShellForm *)gpmfrmm->LoadForm(gpiniForms,
                    kidfInGameOptions, new InGameOptionsForm());
            if (pfrm != NULL) {
                pfrm->DoModal();
                delete pfrm;
            }
        }
        break;
    }
}

void SelectMissionForm::OnControlNotify(word idc, int nNotify) {
    if (idc == kidcCategories) {
        RadioButtonBarControl *prbbc =
                (RadioButtonBarControl *)GetControlPtr(kidcCategories);
        int iButtonSelected = prbbc->GetSelectionIndex();
        if (iButtonSelected < 0) {
            iButtonSelected = 0;
        }
        MissionType mtNew = MissionTypeFromIndex(iButtonSelected);
        if (mtNew == m_mt) {
            return;
        }
        SwitchToMissionType(mtNew);

        // If in Add-On category, and there is nothing there, show this
        // label, otherwise hide it

        bool fShowLabel = false;
        ListControl *plstc = m_aplstc[IndexFromMissionType(kmtAddOn)];
        if (m_mt == kmtAddOn) {
            if (plstc->GetCount() == 0) {
                fShowLabel = true;
            }
        }

        LabelControl *plbl = (LabelControl *)GetControlPtr(kidcAddOnMessage);
        if (fShowLabel) {
            plbl->Show(true);
            if (m_mt == kmtAddOn) {
                plstc->Show(false);
            }
        } else {
            plbl->Show(false);
            if (m_mt == kmtAddOn) {
                plstc->Show(true);
            }
        }
    }

    if (idc == kidcStoryList || idc == kidcChallengeList ||
            idc == kidcAddOnList) {
        // Update the mission pack description
        UpdateDescription();
    }

    // Handle button hiding

    bool fShow = true;
    ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
    if (plstc->GetSelectedItemIndex() == -1) {
        fShow = false;
    }
    if (m_mt == kmtStory) {
        if (IsSelectedMissionLocked(plstc)) {
            fShow = false;
        }
    }
    if (m_fMagicUnlock) {
        fShow = true;
    }
    GetControlPtr(kidcOk)->Show(fShow);
}

int SelectMissionForm::GetSelectedMissionIndex(ListControl *plstc) {
    pword pw = (pword)plstc->GetSelectedItemData();
    return (int)(pw & ~kfItemLocked);
}

bool SelectMissionForm::IsSelectedMissionLocked(ListControl *plstc) {
    pword pw = (pword)plstc->GetSelectedItemData();
    return (pw & kfItemLocked) != 0;
}

void SelectMissionForm::UpdateDescription() {
    LabelControl *plbl = (LabelControl *)GetControlPtr(kidcMissionPackInfo);
    ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
    if (plstc->GetSelectedItemIndex() == -1) {
        plbl->SetText("");
        return;
    }
    int i = GetSelectedMissionIndex(plstc);
    MissionIdentifier miid;
    if (!m_pml->GetMissionIdentifier(i, &miid)) {
        plbl->SetText("");
        return;
    }
    json::JsonMap *map = gppim->GetInfoMap(&miid.packid);
    if (map == NULL) {
        MissionDescription md;
        if (!m_pml->GetMissionDescription(i, &md)) {
            plbl->SetText("");
            return;
        }
        const char *s;
        if (miid.packid.id == PACKID_MAIN) {
            s = base::Format::ToString("%s by Spiffcode, Inc.", md.szPackName);
        } else {
            s = md.szPackName;
        }
        plbl->SetText(s);
        return;
    }
    const char *szAuthor = GetString(map, "a");
    const char *szTitle = GetString(map, "t");
    const char *s = base::Format::ToString("%s by %s", szTitle, szAuthor);
    plbl->SetText(s);
    delete map;
}

bool SelectMissionForm::GetSelectedMission(MissionIdentifier *pmiid) {
    ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
    int i = GetSelectedMissionIndex(plstc);
    return m_pml->GetMissionIdentifier(i, pmiid);
}

} // namespace wi
