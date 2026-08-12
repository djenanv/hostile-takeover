#include "game/ht.h"
#include "base/format.h"
#include "yajl/wrapper/jsontypes.h"
#include "game/httppackinfomanager.h"
#include "game/completemanager.h"

namespace wi {

#define kcUnlockAhead 1
#define kfItemLocked 0x8000

static void DrawMissionUiText(DibBitmap *pbm, Font *pfnt, const char *psz,
        int x, int y, int cx = -1, int cy = -1, bool fEllipsis = false) {
    char sz[256];
    strncpyz(sz, psz != NULL ? psz : "", sizeof(sz));
    if (cx < 0) {
        pfnt->DrawText(pbm, sz, x, y);
    } else {
        pfnt->DrawText(pbm, sz, x, y, cx, cy, fEllipsis);
    }
}

static void DrawMissionUiPanel(DibBitmap *pbm, Rect *prc, Color clrFill,
        Color clrBorder) {
    pbm->Fill(prc->left, prc->top, prc->Width(), prc->Height(), clrFill);
    DrawBorder(pbm, prc, PcFromFc(1), clrBorder);
}

static void DrawMissionUiFrame(DibBitmap *pbm, Rect *prc, Color clrBorder) {
    DrawBorder(pbm, prc, PcFromFc(1), clrBorder);
    int c = PcFromFc(4);
    pbm->DrawLine(prc->left, prc->top, prc->left + c, prc->top,
            GetColor(kiclrWhite));
    pbm->DrawLine(prc->right - c, prc->bottom - 1, prc->right - 1,
            prc->bottom - 1, GetColor(kiclrWhite));
}

static void DrawMissionUiBackdrop(DibBitmap *pbm, int cx, int cy) {
    Color clrGrid = GetColor(kiclrBlueSideFirst);
    Color clrAccent = GetColor(kiclrCyanSideFirst);
    int c = PcFromFc(20);
    if (c < 8) {
        c = 8;
    }
    for (int x = c / 2; x < cx; x += c) {
        pbm->DrawLine(x, 0, x, cy, clrGrid);
    }
    for (int y = c / 2; y < cy; y += c) {
        pbm->DrawLine(0, y, cx, y, clrGrid);
    }
    pbm->DrawLine(0, cy / 2, cx, cy / 2, clrAccent);
}

static void DrawMissionUiLock(DibBitmap *pbm, Rect *prc, Color clr) {
    int cx = prc->Width() / 2;
    int x = prc->left + cx - PcFromFc(4);
    int y = prc->top + PcFromFc(4);
    Rect rc;
    rc.Set(x, y + PcFromFc(5), x + PcFromFc(8), y + PcFromFc(12));
    DrawBorder(pbm, &rc, PcFromFc(1), clr);
    pbm->DrawLine(x + PcFromFc(2), y + PcFromFc(5),
            x + PcFromFc(2), y + PcFromFc(2), clr);
    pbm->DrawLine(x + PcFromFc(2), y + PcFromFc(2),
            x + PcFromFc(6), y + PcFromFc(2), clr);
    pbm->DrawLine(x + PcFromFc(6), y + PcFromFc(2),
            x + PcFromFc(6), y + PcFromFc(5), clr);
}

static void DrawMissionUiSignal(DibBitmap *pbm, Rect *prc, Color clrAccent,
        Color clrDim) {
    int x = prc->left + PcFromFc(5);
    int y = prc->top + PcFromFc(5);
    int cx = prc->Width() - PcFromFc(10);
    for (int i = 0; i < 4; i++) {
        int x1 = x + cx * i / 4;
        int y1 = y + (i & 1 ? PcFromFc(9) : PcFromFc(2));
        int x2 = x + cx * (i + 1) / 4;
        int y2 = y + ((i + 1) & 1 ? PcFromFc(9) : PcFromFc(2));
        pbm->DrawLine(x1, y1, x2, y2, i == 0 ? clrAccent : clrDim);
    }
}

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
    memset(m_arcPreview, 0, sizeof(m_arcPreview));
    m_rcFeature.SetEmpty();
    m_rcPlay.SetEmpty();
    m_rcBack.SetEmpty();
    m_rcPrevious.SetEmpty();
    m_rcNext.SetEmpty();
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

    // Hide this label - only show it if there are no addon packs
    GetControlPtr(kidcAddOnMessage)->Show(false);
    HideLegacyControls();
    UpdateLayout();

	return true;
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
    HideLegacyControls();
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

void SelectMissionForm::HideLegacyControls() {
    for (int i = 0; i < m_cctl; i++) {
        m_apctl[i]->Show(false);
    }
}

void SelectMissionForm::UpdateLayout() {
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    int pad = PcFromFc(5);
    int gap = PcFromFc(3);
    int yTabs = PcFromFc(29);
    int cyTabs = PcFromFc(12);
    int yContent = PcFromFc(45);
    int yBottom = cy - PcFromFc(28);

    int cxTab = (cx - pad * 2 - gap * 2) / 3;
    for (int i = 0; i < 3; i++) {
        int x = pad + i * (cxTab + gap);
        m_arcTabs[i].Set(x, yTabs, x + cxTab, yTabs + cyTabs);
    }

    int cxContent = cx - pad * 2;
    int cxFeature = (cxContent - gap) * 59 / 100;
    int xPreview = pad + cxFeature + gap;
    m_rcFeature.Set(pad, yContent, pad + cxFeature, yBottom);

    int cyPreview = (yBottom - yContent - gap * 2) / 3;
    for (int i = 0; i < 3; i++) {
        int y = yContent + i * (cyPreview + gap);
        m_arcPreview[i].Set(xPreview, y, cx - pad, y + cyPreview);
    }

    m_rcPlay.Set(m_rcFeature.right - PcFromFc(53),
            m_rcFeature.bottom - PcFromFc(23), m_rcFeature.right - PcFromFc(7),
            m_rcFeature.bottom - PcFromFc(7));
    m_rcBack.Set(cx - pad - PcFromFc(43), cy - PcFromFc(24),
            cx - pad, cy - PcFromFc(8));

    int yArrow = yContent + (yBottom - yContent) / 2 - PcFromFc(8);
    m_rcPrevious.Set(pad, yArrow, pad + PcFromFc(12), yArrow + PcFromFc(16));
    m_rcNext.Set(cx - pad - PcFromFc(12), yArrow, cx - pad,
            yArrow + PcFromFc(16));
}

bool SelectMissionForm::GetMissionCard(int iOffset, int *piGlobal,
        int *piCategory, MissionDescription *pmd, bool *pfLocked,
        bool *pfComplete) {
    int iType = IndexFromMissionType(m_mt);
    if (iType < 0 || m_aplstc[iType] == NULL) {
        return false;
    }
    int iSelected = m_aplstc[iType]->GetSelectedItemIndex();
    if (iSelected < 0) {
        iSelected = 0;
    }
    int iTarget = iSelected + iOffset;
    if (iTarget < 0 || iTarget >= m_aplstc[iType]->GetCount()) {
        return false;
    }

    int iCategory = 0;
    int iStoryCategory = 0;
    int iliLastCompleteStory = -1;
    for (int i = 0; i < m_pml->GetCount(); i++) {
        MissionDescription md;
        if (!m_pml->GetMissionDescription(i, &md)) {
            continue;
        }
        if (md.mt != kmtStory && md.mt != kmtChallenge &&
                md.mt != kmtAddOn) {
            continue;
        }

        MissionIdentifier miid;
        if (!m_pml->GetMissionIdentifier(i, &miid)) {
            continue;
        }
        bool fComplete = gpcptm->IsComplete(&miid);
        bool fLocked = false;
        if (md.mt == kmtStory) {
            int iliMissionLocked = iliLastCompleteStory + 1 + kcUnlockAhead;
            fLocked = iStoryCategory >= iliMissionLocked;
        }

        if (md.mt == m_mt) {
            if (iCategory == iTarget) {
                if (piGlobal != NULL) {
                    *piGlobal = i;
                }
                if (piCategory != NULL) {
                    *piCategory = iCategory;
                }
                if (pmd != NULL) {
                    *pmd = md;
                }
                if (pfLocked != NULL) {
                    *pfLocked = fLocked;
                }
                if (pfComplete != NULL) {
                    *pfComplete = fComplete;
                }
                return true;
            }
            iCategory++;
        }

        if (md.mt == kmtStory && fComplete) {
            iliLastCompleteStory = iStoryCategory;
        }
        if (md.mt == kmtStory) {
            iStoryCategory++;
        }
    }
    return false;
}

bool SelectMissionForm::SelectMissionOffset(int iOffset) {
    int iType = IndexFromMissionType(m_mt);
    if (iType < 0 || m_aplstc[iType] == NULL) {
        return false;
    }
    int iSelected = m_aplstc[iType]->GetSelectedItemIndex();
    if (iSelected < 0) {
        iSelected = 0;
    }
    int iTarget = iSelected + iOffset;
    if (iTarget < 0 || iTarget >= m_aplstc[iType]->GetCount()) {
        return false;
    }
    m_aplstc[iType]->Select(iTarget, true, true);
    UpdateDescription();
    HideLegacyControls();
    InvalidateRect(NULL);
    return true;
}

void SelectMissionForm::OnPaint(DibBitmap *pbm) {
    UpdateLayout();
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    Color clrPanel = GetColor(kiclrListBackground);
    Color clrDeep = GetColor(kiclrMenuBack);
    Color clrAccent = GetColor(kiclrCyanSideFirst);
    Color clrMuted = GetColor(kiclrMediumGray);
    Color clrGrid = GetColor(kiclrBlueSideFirst);
    Font *pfnt = gapfnt[kifntShadow];

    // The inherited shell background supplies the established sci-fi texture;
    // this dark header and the opaque cards keep the new layout legible over it.
    DrawMissionUiBackdrop(pbm, cx, cy);
    pbm->Fill(0, 0, cx, m_arcTabs[0].bottom + PcFromFc(4), clrDeep);

    Rect rcBadge;
    rcBadge.Set(PcFromFc(5), PcFromFc(5), PcFromFc(122), PcFromFc(17));
    DrawMissionUiPanel(pbm, &rcBadge, clrPanel, clrAccent);
    pbm->Fill(rcBadge.left + PcFromFc(3), rcBadge.top + PcFromFc(3),
            PcFromFc(5), PcFromFc(5), clrAccent);
    DrawMissionUiText(pbm, gapfnt[kifntDefault], "MISSION 01 / SYSTEMS CHECK",
            rcBadge.left + PcFromFc(11), rcBadge.top + PcFromFc(1),
            rcBadge.Width() - PcFromFc(13), pfnt->GetHeight(), true);
    const char *pszTitle = "PLAY SINGLE PLAYER";
    int cxTitle = gapfnt[kifntTitle]->GetTextExtent(pszTitle);
    DrawMissionUiText(pbm, gapfnt[kifntTitle], pszTitle,
            (cx - cxTitle) / 2, PcFromFc(7), cxTitle,
            gapfnt[kifntTitle]->GetHeight(), false);

    const char *aszTabs[] = { "STORY", "CHALLENGE", "ADD-ON" };
    for (int i = 0; i < 3; i++) {
        Rect rc = m_arcTabs[i];
        bool fSelected = i == IndexFromMissionType(m_mt);
        DrawMissionUiPanel(pbm, &rc, fSelected ? clrAccent : clrDeep,
                fSelected ? GetColor(kiclrWhite) : clrMuted);
        DrawMissionUiText(pbm, gapfnt[kifntButton], aszTabs[i],
                rc.left + PcFromFc(6), rc.top + PcFromFc(1),
                rc.Width() - PcFromFc(12), gapfnt[kifntButton]->GetHeight(),
                false);
    }

    MissionDescription md;
    bool fLocked = false;
    bool fComplete = false;
    bool fHaveFeature = GetMissionCard(0, NULL, NULL, &md, &fLocked,
            &fComplete);
    int iCategory = 0;
    if (fHaveFeature) {
        GetMissionCard(0, NULL, &iCategory, &md, &fLocked, &fComplete);
    }

    DrawMissionUiFrame(pbm, &m_rcFeature, fHaveFeature ? clrAccent : clrMuted);
    if (!fHaveFeature) {
        DrawMissionUiText(pbm, gapfnt[kifntButton], "NO MISSIONS AVAILABLE",
                m_rcFeature.left + PcFromFc(10),
                m_rcFeature.top + PcFromFc(18),
                m_rcFeature.Width() - PcFromFc(20),
                gapfnt[kifntButton]->GetHeight(), true);
        DrawMissionUiText(pbm, pfnt,
                m_mt == kmtAddOn ? "DOWNLOAD AN ADD-ON PACK TO BEGIN."
                                  : "CHECK THE CAMPAIGN DATA AND TRY AGAIN.",
                m_rcFeature.left + PcFromFc(10),
                m_rcFeature.top + PcFromFc(42),
                m_rcFeature.Width() - PcFromFc(20), PcFromFc(20), true);
    } else {
        Rect rcImage;
        rcImage.Set(m_rcFeature.left + PcFromFc(2), m_rcFeature.top + PcFromFc(2),
                m_rcFeature.right - PcFromFc(2),
                m_rcFeature.bottom - PcFromFc(58));
        pbm->Fill(rcImage.left, rcImage.top, rcImage.Width(), rcImage.Height(),
                clrPanel);
        for (int y = rcImage.top + PcFromFc(6); y < rcImage.bottom;
                y += PcFromFc(6)) {
            pbm->DrawLine(rcImage.left, y, rcImage.right, y, clrGrid);
        }
        int xCenter = rcImage.left + rcImage.Width() * 3 / 5;
        int yCenter = rcImage.top + rcImage.Height() / 2;
        int r = _min(rcImage.Width(), rcImage.Height()) / 3;
        pbm->DrawLine(xCenter - r, yCenter, xCenter + r, yCenter, clrAccent);
        pbm->DrawLine(xCenter, yCenter - r / 2, xCenter, yCenter + r / 2,
                clrAccent);
        pbm->DrawLine(xCenter - r, yCenter, xCenter - r / 3,
                yCenter - r / 2, GetColor(kiclrWhite));
        pbm->DrawLine(xCenter - r / 3, yCenter - r / 2, xCenter + r,
                yCenter, GetColor(kiclrWhite));
        pbm->DrawLine(xCenter + r, yCenter, xCenter - r / 3,
                yCenter + r / 2, GetColor(kiclrWhite));
        pbm->DrawLine(xCenter - r / 3, yCenter + r / 2, xCenter - r,
                yCenter, GetColor(kiclrWhite));
        DrawMissionUiSignal(pbm, &rcImage, clrAccent, clrMuted);

        char szMission[32];
        snprintf(szMission, sizeof(szMission), "M%d", iCategory);
        DrawMissionUiText(pbm, gapfnt[kifntDefault], szMission,
                m_rcFeature.left + PcFromFc(9), m_rcFeature.top + PcFromFc(8),
                PcFromFc(24), pfnt->GetHeight(), false);
        DrawMissionUiText(pbm, gapfnt[kifntDefault],
                fLocked ? "ACCESS RESTRICTED" : "LIVE MISSION FEED",
                m_rcFeature.left + PcFromFc(40), m_rcFeature.top + PcFromFc(8),
                m_rcFeature.Width() - PcFromFc(50), pfnt->GetHeight(), true);

        DrawMissionUiText(pbm, gapfnt[kifntTitle], md.szLvlTitle,
                m_rcFeature.left + PcFromFc(9), m_rcFeature.bottom - PcFromFc(48),
                m_rcFeature.Width() - PcFromFc(72),
                gapfnt[kifntTitle]->GetHeight(), true);
        DrawMissionUiText(pbm, pfnt, md.szPackName,
                m_rcFeature.left + PcFromFc(9), m_rcFeature.bottom - PcFromFc(31),
                m_rcFeature.Width() - PcFromFc(72), pfnt->GetHeight(), true);
        DrawMissionUiText(pbm, gapfnt[kifntDefault],
                fComplete ? "COMPLETE" : (fLocked ? "LOCKED" : "UNLOCKED"),
                m_rcFeature.left + PcFromFc(9), m_rcFeature.bottom - PcFromFc(16),
                PcFromFc(52), pfnt->GetHeight(), false);
        if (fLocked && !m_fMagicUnlock) {
            DrawMissionUiLock(pbm, &m_rcFeature, clrMuted);
        }

        bool fCanPlay = !fLocked || m_fMagicUnlock;
        DrawMissionUiPanel(pbm, &m_rcPlay, fCanPlay ? clrAccent : clrDeep,
                fCanPlay ? GetColor(kiclrWhite) : clrMuted);
        DrawMissionUiText(pbm, gapfnt[kifntButton], fCanPlay ? "PLAY" : "LOCKED",
                m_rcPlay.left, m_rcPlay.top + PcFromFc(1), m_rcPlay.Width(),
                gapfnt[kifntButton]->GetHeight(), false);
    }

    for (int i = 0; i < 3; i++) {
        MissionDescription mdPreview;
        bool fPreviewLocked = false;
        bool fPreviewComplete = false;
        int iPreviewCategory = 0;
        bool fHavePreview = GetMissionCard(i + 1, NULL, &iPreviewCategory,
                &mdPreview, &fPreviewLocked, &fPreviewComplete);
        Rect rc = m_arcPreview[i];
        DrawMissionUiPanel(pbm, &rc, fHavePreview ? clrPanel : clrDeep,
                fHavePreview && !fPreviewLocked ? clrAccent : clrMuted);
        if (!fHavePreview) {
            DrawMissionUiText(pbm, pfnt, "END OF CAMPAIGN", rc.left + PcFromFc(8),
                    rc.top + PcFromFc(8), rc.Width() - PcFromFc(16),
                    pfnt->GetHeight(), false);
            continue;
        }

        char szMission[32];
        snprintf(szMission, sizeof(szMission), "M%d", iPreviewCategory);
        DrawMissionUiText(pbm, gapfnt[kifntDefault], szMission,
                rc.left + PcFromFc(8), rc.top + PcFromFc(5), PcFromFc(24),
                pfnt->GetHeight(), false);
        DrawMissionUiText(pbm, pfnt, mdPreview.szLvlTitle,
                rc.left + PcFromFc(35), rc.top + PcFromFc(5),
                rc.Width() - PcFromFc(48), pfnt->GetHeight(), true);

        Rect rcSignal;
        rcSignal.Set(rc.left + PcFromFc(8), rc.top + PcFromFc(24),
                rc.right - PcFromFc(8), rc.bottom - PcFromFc(8));
        DrawMissionUiSignal(pbm, &rcSignal,
                fPreviewLocked ? clrMuted : clrAccent, clrGrid);
        pbm->DrawLine(rcSignal.left, rcSignal.bottom - PcFromFc(4),
                rcSignal.right, rcSignal.top + PcFromFc(2),
                fPreviewLocked ? clrMuted : clrAccent);
        if (fPreviewLocked && !m_fMagicUnlock) {
            DrawMissionUiLock(pbm, &rcSignal, clrMuted);
            DrawMissionUiText(pbm, gapfnt[kifntDefault], "LOCKED",
                    rc.left + PcFromFc(8), rc.bottom - PcFromFc(14),
                    rc.Width() - PcFromFc(16), pfnt->GetHeight(), false);
        } else {
            DrawMissionUiText(pbm, gapfnt[kifntDefault],
                    fPreviewComplete ? "COMPLETE" : "READY",
                    rc.left + PcFromFc(8), rc.bottom - PcFromFc(14),
                    rc.Width() - PcFromFc(16), pfnt->GetHeight(), false);
        }
    }

    if (GetMissionCard(1, NULL, NULL, NULL, NULL, NULL)) {
        pbm->DrawLine(m_rcNext.left + PcFromFc(3), m_rcNext.top + PcFromFc(4),
                m_rcNext.left + PcFromFc(8), m_rcNext.top + PcFromFc(8), clrAccent);
        pbm->DrawLine(m_rcNext.left + PcFromFc(8), m_rcNext.top + PcFromFc(8),
                m_rcNext.left + PcFromFc(3), m_rcNext.top + PcFromFc(12), clrAccent);
    }
    if (GetMissionCard(-1, NULL, NULL, NULL, NULL, NULL)) {
        pbm->DrawLine(m_rcPrevious.right - PcFromFc(3),
                m_rcPrevious.top + PcFromFc(4), m_rcPrevious.right - PcFromFc(8),
                m_rcPrevious.top + PcFromFc(8), clrAccent);
        pbm->DrawLine(m_rcPrevious.right - PcFromFc(8),
                m_rcPrevious.top + PcFromFc(8), m_rcPrevious.right - PcFromFc(3),
                m_rcPrevious.top + PcFromFc(12), clrAccent);
    }

    DrawMissionUiPanel(pbm, &m_rcBack, clrDeep, clrAccent);
    DrawMissionUiText(pbm, gapfnt[kifntButton], "BACK", m_rcBack.left,
            m_rcBack.top + PcFromFc(1), m_rcBack.Width(),
            gapfnt[kifntButton]->GetHeight(), false);
}

void SelectMissionForm::OnPaintControls(DibBitmap *pbm, UpdateMap *pupd) {
}

bool SelectMissionForm::OnPenEvent(Event *pevt) {
    UpdateLayout();
    int x = pevt->x - m_rc.left;
    int y = pevt->y - m_rc.top;
    int iZone = -1;

    if (m_rcNext.PtIn(x, y) &&
            GetMissionCard(1, NULL, NULL, NULL, NULL, NULL)) {
        iZone = 23;
    } else if (m_rcPrevious.PtIn(x, y) &&
            GetMissionCard(-1, NULL, NULL, NULL, NULL, NULL)) {
        iZone = 22;
    } else {
        for (int i = 0; i < 3; i++) {
            if (m_arcTabs[i].PtIn(x, y)) {
                iZone = i;
                break;
            }
        }
        if (iZone < 0 && m_rcPlay.PtIn(x, y)) {
            iZone = 20;
        }
        if (iZone < 0 && m_rcBack.PtIn(x, y)) {
            iZone = 21;
        }
        if (iZone < 0 && m_rcFeature.PtIn(x, y)) {
            iZone = 10;
        }
        if (iZone < 0) {
            for (int i = 0; i < 3; i++) {
                if (m_arcPreview[i].PtIn(x, y)) {
                    iZone = 11 + i;
                    break;
                }
            }
        }
    }

    if (pevt->eType == penDownEvent) {
        m_iPressedZone = iZone;
        if (iZone >= 0) {
            InvalidateRect(NULL);
        }
        return iZone >= 0;
    }
    if (pevt->eType != penUpEvent) {
        return m_iPressedZone >= 0;
    }

    int iPressed = m_iPressedZone;
    m_iPressedZone = -1;
    if (iPressed < 0 || iPressed != iZone) {
        return iPressed >= 0;
    }

    if (iPressed >= 0 && iPressed < 3) {
        SwitchToMissionType(MissionTypeFromIndex(iPressed));
        InvalidateRect(NULL);
    } else if (iPressed >= 11 && iPressed <= 13) {
        SelectMissionOffset(iPressed - 10);
    } else if (iPressed == 20 || iPressed == 10) {
        MissionDescription md;
        bool fLocked = false;
        if (GetMissionCard(0, NULL, NULL, &md, &fLocked, NULL) &&
                (!fLocked || m_fMagicUnlock)) {
            EndForm(kidcOk);
        }
    } else if (iPressed == 21) {
        m_cMagic++;
        if (m_cMagic >= 5) {
            m_cMagic = 0;
            m_fMagicUnlock = true;
        }
        EndForm(kidcCancel);
    } else if (iPressed == 22) {
        SelectMissionOffset(-1);
    } else if (iPressed == 23) {
        SelectMissionOffset(1);
    }
    return true;
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
    HideLegacyControls();
}

int SelectMissionForm::GetSelectedMissionIndex(ListControl *plstc) {
    if (plstc == NULL || plstc->GetSelectedItemData() == NULL) {
        return -1;
    }
    pword pw = (pword)plstc->GetSelectedItemData();
    return (int)(pw & ~kfItemLocked);
}

bool SelectMissionForm::IsSelectedMissionLocked(ListControl *plstc) {
    if (plstc == NULL || plstc->GetSelectedItemData() == NULL) {
        return true;
    }
    pword pw = (pword)plstc->GetSelectedItemData();
    return (pw & kfItemLocked) != 0;
}

void SelectMissionForm::UpdateDescription() {
    LabelControl *plbl = (LabelControl *)GetControlPtr(kidcMissionPackInfo);
    ListControl *plstc = m_aplstc[IndexFromMissionType(m_mt)];
    if (plbl == NULL || plstc == NULL) {
        return;
    }
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
    if (i < 0) {
        return false;
    }
    return m_pml->GetMissionIdentifier(i, pmiid);
}

} // namespace wi
