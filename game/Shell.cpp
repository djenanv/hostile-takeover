#include "game/ht.h"
#include "game/lobby.h"
#include "game/serviceurls.h"
#include "base/misc.h"

namespace wi {

static void DrawShellText(DibBitmap *pbm, Font *pfnt, const char *psz,
        int x, int y, int cx = -1, int cy = -1, bool fEllipsis = false) {
    char sz[256];
    strncpyz(sz, psz != NULL ? psz : "", sizeof(sz));
    if (cx < 0) {
        pfnt->DrawText(pbm, sz, x, y);
    } else {
        pfnt->DrawText(pbm, sz, x, y, cx, cy, fEllipsis);
    }
}

static void DrawShellPanel(DibBitmap *pbm, Rect *prc, Color clrFill,
        Color clrBorder) {
    pbm->Fill(prc->left, prc->top, prc->Width(), prc->Height(), clrFill);
    DrawBorder(pbm, prc, PcFromFc(1), clrBorder);
}

static void DrawShellFrame(DibBitmap *pbm, Rect *prc, Color clrBorder) {
    DrawBorder(pbm, prc, PcFromFc(1), clrBorder);
    int c = PcFromFc(4);
    pbm->DrawLine(prc->left, prc->top, prc->left + c, prc->top,
            GetColor(kiclrWhite));
    pbm->DrawLine(prc->right - c, prc->bottom - 1, prc->right - 1,
            prc->bottom - 1, GetColor(kiclrWhite));
}

static void DrawShellBackdrop(DibBitmap *pbm, int cx, int cy) {
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
    pbm->DrawLine(0, cy / 2 - PcFromFc(34), cx / 3,
            cy / 2 - PcFromFc(34) - cx / 8, clrGrid);
    pbm->DrawLine(cx, cy / 2 + PcFromFc(42), cx * 2 / 3,
            cy / 2 + PcFromFc(42) + cx / 8, clrGrid);
}

static void DrawShellStatus(DibBitmap *pbm, Rect *prc, const char *pszLabel,
        const char *pszValue) {
    Font *pfnt = gapfnt[kifntShadow];
    int x = prc->left;
    int y = prc->top;
    DrawShellText(pbm, gapfnt[kifntDefault], pszLabel, x, y,
            prc->Width(), pfnt->GetHeight(), false);
    DrawShellText(pbm, pfnt, pszValue, x, y + pfnt->GetHeight() + PcFromFc(1),
            prc->Width(), pfnt->GetHeight(), false);
}

// A simple form handler for the main menu, for buttons that want to
// be processed without ending the modal form loop.

class MainMenuForm : public ShellForm
{
public:
    MainMenuForm() {
        m_iPressedZone = -1;
        memset(m_arcActions, 0, sizeof(m_arcActions));
        memset(m_arcUtility, 0, sizeof(m_arcUtility));
        m_rcHero.SetEmpty();
    }

    virtual bool Init(FormMgr *pfrmm, IniReader *pini, word idf) {
        if (!ShellForm::Init(pfrmm, pini, idf)) {
            return false;
        }
        HideLegacyControls();
        UpdateLayout();
        return true;
    }

    virtual void OnPaint(DibBitmap *pbm);
    virtual void OnPaintControls(DibBitmap *pbm, UpdateMap *pupd) {}
    virtual bool OnPenEvent(Event *pevt);

    virtual void OnControlSelected(word idc) {
        // Catch this here so the form doesn't get re-created (with associated
        // sound effect).
        if (idc == kidcLeaderboard) {
            LoginHandler handler;
            std::string d = base::StringEncoder::QueryEncode(gszDeviceId);
            std::string o(base::StringEncoder::QueryEncode(HostGetPlatformString()));
            const char *url; 
            if (strlen(handler.StatsUsername()) == 0) {
                url = base::Format::ToString("%s?d=%s&o=%s", kszLeaderboardUrl,
                        d.c_str(), o.c_str());
            } else {
                std::string q = base::StringEncoder::QueryEncode(
                        handler.StatsUsername());
                url = base::Format::ToString("%s?p=%s&d=%s&o=%s", kszLeaderboardUrl,
                        q.c_str(), d.c_str(), o.c_str());
            }
            HostInitiateWebView("Hostile Takeover Statistics", url);
            return;
        }
        ShellForm::OnControlSelected(idc);
    }

private:
    void HideLegacyControls() {
        for (int i = 0; i < m_cctl; i++) {
            m_apctl[i]->Show(false);
        }
    }
    void UpdateLayout();

    Rect m_rcHero;
    Rect m_arcActions[4];
    Rect m_arcUtility[3];
    int m_iPressedZone;
};

void MainMenuForm::UpdateLayout() {
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    int pad = PcFromFc(6);
    int gap = PcFromFc(4);
    int yBottom = cy - PcFromFc(28);
    int xHeroRight = cx * 43 / 100;
    m_rcHero.Set(pad, PcFromFc(42), xHeroRight, yBottom);

    int xActions = xHeroRight + gap;
    int cxActions = cx - xActions - pad;
    int y = PcFromFc(32);
    int cyAction = PcFromFc(22);
    for (int i = 0; i < 4; i++) {
        m_arcActions[i].Set(xActions, y + i * (cyAction + gap),
                xActions + cxActions, y + i * (cyAction + gap) + cyAction);
    }

    int cxUtility = (cxActions - gap * 2) / 3;
    for (int i = 0; i < 3; i++) {
        int x = xActions + i * (cxUtility + gap);
        m_arcUtility[i].Set(x, yBottom + PcFromFc(4), x + cxUtility,
                yBottom + PcFromFc(17));
    }
}

void MainMenuForm::OnPaint(DibBitmap *pbm) {
    UpdateLayout();
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    Color clrDeep = GetColor(kiclrMenuBack);
    Color clrAccent = GetColor(kiclrCyanSideFirst);
    Color clrMuted = GetColor(kiclrMediumGray);
    Font *pfnt = gapfnt[kifntShadow];

    DrawShellBackdrop(pbm, cx, cy);
    DrawShellText(pbm, gapfnt[kifntTitle], "HOSTILE TAKEOVER", PcFromFc(6),
            PcFromFc(5), cx - PcFromFc(12), gapfnt[kifntTitle]->GetHeight(),
            false);
    DrawShellText(pbm, gapfnt[kifntDefault],
            "TACTICAL COMMAND  /  SECURE CHANNEL", PcFromFc(6),
            PcFromFc(21), cx - PcFromFc(12), pfnt->GetHeight(), false);

    DrawShellFrame(pbm, &m_rcHero, clrAccent);
    pbm->Fill(m_rcHero.left, m_rcHero.top, PcFromFc(3), m_rcHero.Height(),
            clrAccent);
    DrawShellText(pbm, gapfnt[kifntDefault], "OPERATIONS DECK",
            m_rcHero.left + PcFromFc(8), m_rcHero.top + PcFromFc(7),
            m_rcHero.Width() - PcFromFc(16), pfnt->GetHeight(), false);
    DrawShellText(pbm, gapfnt[kifntTitle], "READY",
            m_rcHero.left + PcFromFc(8), m_rcHero.top + PcFromFc(25),
            m_rcHero.Width() - PcFromFc(16), gapfnt[kifntTitle]->GetHeight(),
            false);
    DrawShellText(pbm, pfnt, "YOUR NEXT OPERATION IS WAITING.",
            m_rcHero.left + PcFromFc(8), m_rcHero.top + PcFromFc(46),
            m_rcHero.Width() - PcFromFc(16), pfnt->GetHeight(), false);

    int xMid = m_rcHero.left + m_rcHero.Width() / 2;
    int yMid = m_rcHero.top + m_rcHero.Height() / 2;
    int r = _min(m_rcHero.Width(), m_rcHero.Height()) / 4;
    pbm->DrawLine(xMid - r, yMid, xMid + r, yMid, clrAccent);
    pbm->DrawLine(xMid, yMid - r, xMid, yMid + r, clrAccent);
    pbm->DrawLine(xMid - r, yMid, xMid, yMid - r / 2, clrMuted);
    pbm->DrawLine(xMid, yMid - r / 2, xMid + r, yMid, clrMuted);
    pbm->DrawLine(xMid + r, yMid, xMid, yMid + r / 2, clrMuted);
    pbm->DrawLine(xMid, yMid + r / 2, xMid - r, yMid, clrMuted);
    pbm->Fill(xMid - PcFromFc(1), yMid - PcFromFc(1), PcFromFc(2),
            PcFromFc(2), GetColor(kiclrWhite));

    Rect rcStatus;
    rcStatus.Set(m_rcHero.left + PcFromFc(8), m_rcHero.bottom - PcFromFc(45),
            m_rcHero.right - PcFromFc(8), m_rcHero.bottom - PcFromFc(8));
    DrawShellStatus(pbm, &rcStatus, "SYSTEM STATUS", "ONLINE  /  ALL SYSTEMS NOMINAL");
    pbm->Fill(rcStatus.left, rcStatus.bottom - PcFromFc(2),
            rcStatus.Width() * 3 / 4, PcFromFc(1), clrAccent);

    const char *aszAction[] = { "PLAY", "LOAD SAVED GAME", "MISSION PACKS",
            "LEADERBOARD" };
    const char *aszHint[] = { "START A NEW OPERATION", "RESUME YOUR LAST RUN",
            "EXPAND THE CAMPAIGN", "VIEW GLOBAL STATS" };
    if (gfDemo) {
        aszAction[1] = "PURCHASE";
        aszHint[1] = "UNLOCK THE FULL CAMPAIGN";
    }
    for (int i = 0; i < 4; i++) {
        Rect rc = m_arcActions[i];
        bool fPrimary = i == 0;
        DrawShellPanel(pbm, &rc, fPrimary ? clrAccent : clrDeep,
                fPrimary ? GetColor(kiclrWhite) : clrMuted);
        DrawShellText(pbm, gapfnt[kifntButton], aszAction[i],
                rc.left + PcFromFc(7), rc.top + PcFromFc(1),
                rc.Width() - PcFromFc(14), gapfnt[kifntButton]->GetHeight(), true);
        DrawShellText(pbm, pfnt, aszHint[i], rc.left + PcFromFc(7),
                rc.bottom - pfnt->GetHeight() - PcFromFc(1),
                rc.Width() - PcFromFc(14), pfnt->GetHeight(), true);
    }

    const char *aszUtility[] = { "OPTIONS", "HELP", "FORUMS" };
    for (int i = 0; i < 3; i++) {
        Rect rc = m_arcUtility[i];
        DrawShellPanel(pbm, &rc, clrDeep, clrAccent);
        DrawShellText(pbm, gapfnt[kifntButton], aszUtility[i], rc.left,
                rc.top + PcFromFc(1), rc.Width(),
                gapfnt[kifntButton]->GetHeight(), true);
    }

    DrawShellText(pbm, pfnt, "v1.0  /  SPiFFCODE COMMAND SYSTEM",
            PcFromFc(6), cy - PcFromFc(9), cx - PcFromFc(12),
            pfnt->GetHeight(), false);
}

bool MainMenuForm::OnPenEvent(Event *pevt) {
    UpdateLayout();
    int x = pevt->x - m_rc.left;
    int y = pevt->y - m_rc.top;
    int iZone = -1;
    if (m_rcHero.PtIn(x, y)) {
        iZone = 0;
    } else {
        for (int i = 0; i < 4; i++) {
            if (m_arcActions[i].PtIn(x, y)) {
                iZone = i;
                break;
            }
        }
        if (iZone < 0) {
            for (int i = 0; i < 3; i++) {
                if (m_arcUtility[i].PtIn(x, y)) {
                    iZone = 10 + i;
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
    switch (iPressed) {
    case 0:
        OnControlSelected(kidcPlay);
        break;
    case 1:
        OnControlSelected(gfDemo ? kidcBuyMe : kidcLoadSavedGame);
        break;
    case 2:
        OnControlSelected(kidcDownloadMissions);
        break;
    case 3:
        OnControlSelected(kidcLeaderboard);
        break;
    case 10:
        OnControlSelected(kidcSetupGame);
        break;
    case 11:
        OnControlSelected(kidcHelp);
        break;
    case 12:
        OnControlSelected(kidcForums);
        break;
    }
    return true;
}

class PlayMenuForm : public ShellForm {
public:
    PlayMenuForm() {
        m_iPressedZone = -1;
        memset(m_arcChoices, 0, sizeof(m_arcChoices));
        m_rcBack.SetEmpty();
    }

    virtual bool Init(FormMgr *pfrmm, IniReader *pini, word idf) {
        if (!ShellForm::Init(pfrmm, pini, idf)) {
            return false;
        }
        for (int i = 0; i < m_cctl; i++) {
            m_apctl[i]->Show(false);
        }
        UpdateLayout();
        return true;
    }
    virtual void OnPaint(DibBitmap *pbm);
    virtual void OnPaintControls(DibBitmap *pbm, UpdateMap *pupd) {}
    virtual bool OnPenEvent(Event *pevt);

private:
    void UpdateLayout();
    Rect m_arcChoices[2];
    Rect m_rcBack;
    int m_iPressedZone;
};

void PlayMenuForm::UpdateLayout() {
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    int pad = PcFromFc(8);
    int gap = PcFromFc(5);
    int y = PcFromFc(53);
    int bottom = cy - PcFromFc(31);
    int cxChoice = (cx - pad * 2 - gap) / 2;
    for (int i = 0; i < 2; i++) {
        int x = pad + i * (cxChoice + gap);
        m_arcChoices[i].Set(x, y, x + cxChoice, bottom);
    }
    m_rcBack.Set(cx - pad - PcFromFc(43), cy - PcFromFc(24),
            cx - pad, cy - PcFromFc(8));
}

void PlayMenuForm::OnPaint(DibBitmap *pbm) {
    UpdateLayout();
    int cx = m_rc.Width();
    int cy = m_rc.Height();
    Color clrPanel = GetColor(kiclrListBackground);
    Color clrDeep = GetColor(kiclrMenuBack);
    Color clrAccent = GetColor(kiclrCyanSideFirst);
    Color clrMuted = GetColor(kiclrMediumGray);
    Font *pfnt = gapfnt[kifntShadow];

    DrawShellBackdrop(pbm, cx, cy);
    DrawShellText(pbm, gapfnt[kifntDefault], "COMMAND MODE  /  SELECT DEPLOYMENT",
            PcFromFc(8), PcFromFc(7), cx - PcFromFc(16), pfnt->GetHeight(), false);
    DrawShellText(pbm, gapfnt[kifntTitle], "PLAY GAME", 0, PcFromFc(20), cx,
            gapfnt[kifntTitle]->GetHeight(), false);
    pbm->Fill(PcFromFc(8), PcFromFc(38), cx - PcFromFc(16), PcFromFc(1),
            clrAccent);

    const char *aszTitle[] = { "SINGLE PLAYER", "MULTIPLAYER" };
    const char *aszSub[] = { "CAMPAIGN OPERATIONS", "SKIRMISH NETWORK" };
    const char *aszBody[] = {
        "PLAY THE STORY AND MASTER THE SYSTEMS.",
        "HOST OR JOIN A MATCH."
    };
    for (int i = 0; i < 2; i++) {
        Rect rc = m_arcChoices[i];
        DrawShellPanel(pbm, &rc, i == 0 ? clrAccent : clrPanel,
                i == 0 ? GetColor(kiclrWhite) : clrMuted);
        DrawShellText(pbm, gapfnt[kifntButton], aszTitle[i],
                rc.left + PcFromFc(10), rc.top + PcFromFc(9),
                rc.Width() - PcFromFc(20), gapfnt[kifntButton]->GetHeight(), true);
        DrawShellText(pbm, pfnt, aszSub[i], rc.left + PcFromFc(10),
                rc.top + PcFromFc(27), rc.Width() - PcFromFc(20),
                pfnt->GetHeight(), false);
        DrawShellText(pbm, pfnt, aszBody[i], rc.left + PcFromFc(10),
                rc.top + PcFromFc(39), rc.Width() - PcFromFc(20),
                PcFromFc(11), true);

        int ySignal = rc.bottom - PcFromFc(23);
        pbm->Fill(rc.left + PcFromFc(10), ySignal,
                rc.Width() - PcFromFc(20), PcFromFc(1),
                i == 0 ? GetColor(kiclrWhite) : clrAccent);
        DrawShellText(pbm, gapfnt[kifntDefault],
                i == 0 ? "PRIMARY ROUTE" : "NETWORK ROUTE",
                rc.left + PcFromFc(10), ySignal + PcFromFc(3),
                rc.Width() - PcFromFc(20), pfnt->GetHeight(), false);
    }

    DrawShellPanel(pbm, &m_rcBack, clrDeep, clrAccent);
    DrawShellText(pbm, gapfnt[kifntButton], "BACK", m_rcBack.left,
            m_rcBack.top + PcFromFc(1), m_rcBack.Width(),
            gapfnt[kifntButton]->GetHeight(), false);
}

bool PlayMenuForm::OnPenEvent(Event *pevt) {
    UpdateLayout();
    int x = pevt->x - m_rc.left;
    int y = pevt->y - m_rc.top;
    int iZone = -1;
    for (int i = 0; i < 2; i++) {
        if (m_arcChoices[i].PtIn(x, y)) {
            iZone = i;
            break;
        }
    }
    if (iZone < 0 && m_rcBack.PtIn(x, y)) {
        iZone = 2;
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
    if (iPressed == 0) {
        EndForm(kidcPlaySinglePlayer);
    } else if (iPressed == 1) {
        EndForm(kidcPlayMultiPlayer);
    } else {
        EndForm(kidcCancel);
    }
    return true;
}

Shell gshl;
Shell::Shell()
{
	m_ppal = NULL;
	m_mpiclriclrShadow = NULL;
}

bool Shell::Init()
{
	m_ppal = (Palette *)gpakr.MapFile("shell.palbin", &m_fmapPalette);
	Assert(m_ppal != NULL);
	m_mpiclriclrShadow = (byte *)gpakr.MapFile("shell.palbin.shadowmap", &m_fmapShadowMap);
	Assert(m_mpiclriclrShadow != NULL);
	return true;
}

void Shell::Exit()
{
	if (m_ppal != NULL)
		gpakr.UnmapFile(&m_fmapPalette);
	if (m_mpiclriclrShadow != NULL)
		gpakr.UnmapFile(&m_fmapShadowMap);
}

void Shell::SetPalette()
{
	gmpiclriclrShadow = m_mpiclriclrShadow;
	SetHslAdjustedPalette(m_ppal, gnHueOffset, gnSatMultiplier, gnLumOffset);
}

int Shell::PlayGame(PlayMode pm, MissionIdentifier *pmiid, Stream *pstm,
        int nRank)
{
	// UNDONE: free up space-taking Shell resources

	int nGo;

	do {
		nGo = knGoSuccess;

		switch (pm) {
		case kpmNormal:
			nGo = ggame.PlayLevel(pmiid, NULL, nRank);
			break;

		case kpmSavedGame:
			nGo = ggame.PlaySavedGame(pstm);
			break;
		}

		if (nGo == knGoLoadSavedGame) {
			pm = kpmSavedGame;
			pstm = gpstmSavedGame;
 		}
	} while (nGo == knGoLoadSavedGame);

	// UNDONE: reload space-taking Shell resources

	gmpiclriclrShadow = m_mpiclriclrShadow;

	return nGo;
}

void Shell::Launch(bool fLoadReinitializeSave, MissionIdentifier *pmiid)
{
#ifdef STRESS
	static char *s_aszStressLevels[] = {
		"S_01.lvl", "S_02.lvl", "S_03.lvl", "S_04.lvl", "S_05.lvl", "S_06.lvl", "S_07.lvl", 
		"S_08.lvl", "S_09.lvl", "S_10.lvl", "S_11.lvl", "S_12.lvl", "S_13.lvl", "S_14.lvl"
	};

	if (gfStress) {
		while (true) {
			gtStressTimeout = 30 * 60 * 100L;		// 30 minutes
			int ilvl = GetAsyncRandom() % ARRAYSIZE(s_aszStressLevels);
            strncpyz(pmiid->szLvlFilename, s_aszStressLevels[ilvl],
                    sizeof(pmiid->szLvlFilename));
			int nGo = PlayGame(kpmNormal, pmiid, NULL, 0);
			if (nGo == knGoAppStop)
				return;
		}
	}
#endif // def STRESS

#if 0
#if defined(DEBUG) && defined(CE)
	WCHAR wszDeviceName[128];
	SystemParametersInfo(SPI_GETOEMINFO, sizeof(wszDeviceName), &wszDeviceName, 0);
   char tst[500];
   int cch = wcslen(wszDeviceName);
   char *pch = (char *)wszDeviceName;
   char *pdst = tst;
   while (cch > 0) {
	   if (*pch != 0)
		   *pdst++ = *pch++;
		else
			pch++;
	   cch--;
   }
   *pdst = 0;
   HtMessageBox(kfMbClearDib, "Device ID String", tst); 
#endif
#endif

#ifdef BETA_TIMEOUT
	HtMessageBox(kfMbWhiteBorder | kfMbClearDib, "HOSTILE TAKEOVER",
			"Hostile Takeover\n\n"
			"Copyright 2003-2008 Spiffcode, Inc.\n\n"
			"http://www.spiffcode.com\n\n"
			"Test Release. Please keep private until out of beta. Thank you :)");
#endif

	// Handle DRM

	if (!DrmValidate())
		return;

	if (gevm.IsAppStopping())
		return;

	// Are we launching immediately into a saved game?

	Stream *pstm = HostOpenSaveGameStream(knGameReinitializeSave, true);

	if (pstm != NULL) {

		// Stream is closed/deleted upon return
		
		int nGo = PlayGame(kpmSavedGame, NULL, pstm, 0);
		if (nGo == knGoAppStop)
			return;

		//if (nGo == knGoInitFailure)
		// silently fall into a normal game open

	} else if (pmiid != NULL) {
		int nGo = PlayGame(kpmNormal, pmiid, NULL, 0);
		if (nGo == knGoAppStop)
			return;

		if (nGo == knGoInitFailure)
			HtMessageBox(kfMbWhiteBorder, "Load Game", "Error launch level!");
	}

	while (true) {
        if (gevm.IsAppStopping()) {
            return;
        }

        // Show startup form (new single player, new multi player, load saved
        // game, etc)

		ShellForm *pfrm = (ShellForm *)gpmfrmm->LoadForm(gpiniForms,
                kidfStartup, new MainMenuForm());
		Assert(pfrm != NULL);
		if (pfrm == NULL)
			return;

#ifdef V100_MENUS
		pfrm->GetControlPtr(kidcPlayMultiPlayer)->Show(false);
		pfrm->GetControlPtr(kidcPlaySinglePlayer)->Show(false);
		if (gfDemo) {
			pfrm->GetControlPtr(kidcBeginNewGame)->Show(false);   
			pfrm->GetControlPtr(kidcPlayMission)->Show(false);   
		} else {   
			pfrm->GetControlPtr(kidcPlayDemo)->Show(false);   
			Control *pctl = pfrm->GetControlPtr(kidcBuyMe);   
			Rect rc;   
			pctl->GetRect(&rc);   
			pctl->Show(false);   
			pctl = pfrm->GetControlPtr(kidcLoadSavedGame);   
			pctl->SetRect(&rc);   
		} 
#else
		if (gfDemo)
			pfrm->GetControlPtr(kidcLoadSavedGame)->Show(false);
		else
			pfrm->GetControlPtr(kidcBuyMe)->Show(false);
#endif

		// Make Shell palette and shadow map active

		gshl.SetPalette();

		int idc;
		pfrm->DoModal(&idc);
		delete pfrm;

		// While the dialog was up the player might have exited the app

		if (gevm.IsAppStopping())
			return;

        // Before beta check

        switch (idc) {
        case kidcPlay:
            if (DoPlay()) {
                return;
            }
            continue;

		case kidcBuyMe:
			DrmValidate();
            continue;

		case kidcSetupGame:
			DoModalGameOptionsForm(m_ppal, false);
            continue;
	
        case kidcForums:
            HostOpenUrl(kszForumUrl);
            continue;

		case kidcHelp:
			Help();
			continue;

		case kidcCredits:
			Help("credits");
			continue;
        }

#ifdef BETA_TIMEOUT
        if (!CheckBetaTimeout()) {
            continue;
        }
#endif

		switch (idc) {
		case kidcPlayDemo:
		case kidcBeginNewGame:
			if (BeginNewGame())
				return;
			break;

		case kidcPlayMission:
		case kidcPlaySinglePlayer:
			if (PlaySinglePlayer(NULL)) {
				return;
            }
			break;

		case kidcPlayMultiPlayer:
            if (PlayMultiplayer(NULL)) {
                return;
            }
			break;

		case kidcLoadSavedGame:
			{
				Stream *pstm = PickLoadGameStream();
				if (pstm == NULL)
					break;

				// Stream is closed/deleted upon return

				int nGo = PlayGame(kpmSavedGame, NULL, pstm, 0);

				if (nGo == knGoAppStop)
					return;

				if (nGo == knGoInitFailure)
					HtMessageBox(kfMbWhiteBorder | kfMbClearDib, "Load Game", "Error loading saved game!");
			}
			break;

        case kidcDownloadMissions:
            DownloadMissionPack();
            break;

		case kidcExitGame:
			return;
		}
	}
}

bool Shell::DoPlay()
{
    while (true) {
        ShellForm *pfrm = (ShellForm *)gpmfrmm->LoadForm(gpiniForms,
                kidfPlay, new PlayMenuForm());
        if (pfrm == NULL) {
            return kidcCancel;
        }
        int idc;
        pfrm->DoModal(&idc);
        delete pfrm;

        if (gevm.IsAppStopping()) {
            return true;
        }

        if (idc == kidcCancel) {
            return false;
        }

#ifdef BETA_TIMEOUT
        if (!CheckBetaTimeout()) {
            continue;
        }
#endif

        if (idc == kidcPlaySinglePlayer) {
			if (PlaySinglePlayer(NULL)) {
                return true;
            }
        }

        if (idc == kidcPlayMultiPlayer) {
            if (PlayMultiplayer(NULL)) {
                return true;
            }
        }
    }
}

void Shell::DownloadMissionPack()
{
    // Show the download form. True is returned if the user wants to
    // play the first mission in the pack identified by packid.

    PackId packid;
    if (!ShowDownloadMissionPackForm(&packid)) {
        return;
    }

    // The downloaded a pack identified by packid, and wants to play it.
    // Find the first mission and determine if it's single or multi
    // player. Then, launch the appropriate Play form with this info.
    // It will show, with the appropriate mission highlighted.

    MissionList *pml = CreateMissionList(&packid, kmltAll);
    if (pml == NULL) {
        return;
    }
    if (pml->GetCount() == 0) {
        delete pml;
        return;
    }
    MissionDescription md;
    pml->GetMissionDescription(0, &md);

    // Show the appropriate form. This will cause the first mission from
    // this pack to be highlighted.
 
    if (pml->IsMultiplayerMissionType(md.mt)) {
        PlayMultiplayer(&packid);
    } else {
        PlaySinglePlayer(&packid);
    }
	
    delete pml;
}

bool Shell::PlayMultiplayer(const PackId *ppackid)
{
    Lobby lobby;
    dword result = lobby.Shell(ppackid);
    return result == knShellResultAppStop;
}

bool Shell::PlaySinglePlayer(const PackId *ppackid)
{
    MissionIdentifier miidFind;
    memset(&miidFind, 0, sizeof(miidFind));
    if (ppackid != NULL) {
        miidFind.packid = *ppackid;
    }

    while (true) {
        // First, create a mission list, which is an enumerator for the
        // missions we want to show in this form.

        MissionList *pml = CreateMissionList(NULL, kmltSinglePlayer);
        if (pml == NULL) {
            return true;
        }
		SelectMissionForm *pfrm = (SelectMissionForm *)gpmfrmm->LoadForm(
                gpiniForms, kidfSelectMissionWide,
                new SelectMissionForm(pml, &miidFind));
		if (pfrm == NULL) {
            delete pml;
			return true;
        }

		int idc;
		pfrm->DoModal(&idc);

        MissionIdentifier miid;
        bool fMissionSelected = false;
        if (idc == kidcOk) {
            if (pfrm->GetSelectedMission(&miid)) {
                miidFind = miid;
                fMissionSelected = true;
            }
        }
            
		delete pfrm;
        delete pml;

		// While the dialog was up the player might have exited the app

		if (gevm.IsAppStopping())
			return true;

		if (!fMissionSelected)
			return false;

		int nGo = PlayGame(kpmNormal, &miid, NULL, 0);

        // The next time the SelectMission form shows, highlight the last
        // mission the user was playing.

        miidFind = ggame.GetLastMissionIdentifier();

		if (nGo == knGoAppStop) {
			return true;
        }

		if (nGo == knGoInitFailure) {
			HtMessageBox(kfMbWhiteBorder, "Error", "Unable to load and initialize the mission.");
			continue;
		}
	}

    return false;
}

#if 0
// Returns true if the app is stopping

bool Shell::PlaySinglePlayer()
{
	while (true) {
		ShellForm *pfrm = (ShellForm *)gpmfrmm->LoadForm(gpiniForms, kidfPlaySolo, new ShellForm());
		if (pfrm == NULL)
			return false;
#if !defined(DEV_BUILD)
		pfrm->GetControlPtr(kidcLoadSavedGame)->Show(false);
		pfrm->GetControlPtr(kidcPlaybackGame)->Show(false);
#endif
		int idc;
		pfrm->DoModal(&idc);
		delete pfrm;

		// While the dialog was up the player might have exited the app

		if (gevm.IsAppStopping())
			return true;

		switch (idc) {
		case kidcCancel:
			return false;

		case kidcBeginNewGame:
			return BeginNewGame();

		case kidcPlayChallengeLevel:
			if (PlayChallengeLevel(false))
				return true;
			break;

		case kidcPlayStoryMission:
			if (PlayChallengeLevel(true))
				return true;
			break;

		case kidcPlaybackGame:
			break;

		case kidcLoadSavedGame:
			{
				Stream *pstm = PickLoadGameStream();
				if (pstm == NULL)
					break;

				// Stream is closed/deleted upon return

				int nGo = PlayGame(kpmSavedGame, pstm);

				if (nGo == knGoAppStop)
					return true;

				if (nGo == knGoInitFailure)
					HtMessageBox(kfMbWhiteBorder | kfMbClearDib, "Load Game", "Error loading saved game!");
			}
			break;
		}
	}

	return false;
}
#endif

bool Shell::PlayChallengeLevel(bool fStory)
{
#if 0
	while (true) {
		// Have the player select a level to play

		PickLevelForm *pfrm = (PickLevelForm *)gpmfrmm->LoadForm(gpiniForms, kidfPickLevel, 
				new PickLevelForm(fStory ? kltStory : kltChallenge));
		Assert(pfrm != NULL);
		if (pfrm == NULL)
			return true;

		if (gfDemo) {
			Control *pctl = pfrm->GetControlPtr(kidcOk);
			pctl->Show(false);
		} else {
#if 0
			// UNDONE:
			pctl = GetControlPtr(kidcPleaseRegister);
			pctl->Show(false);
#endif
		}

		int idc;
		pfrm->DoModal(&idc);

		char szLevel[kcbFilename];
		if (idc != kidcCancel)
			strcpy(szLevel, pfrm->m_szLevel);
		delete pfrm;

		// While the dialog was up the player might have exited the app

		if (gevm.IsAppStopping())
			return true;

		if (idc == kidcCancel)
			return false;

		int nGo = PlayGame(kpmNormal, szLevel);
		if (nGo == knGoAppStop)
			return true;

		if (nGo == knGoInitFailure) {
			HtMessageBox(kfMbWhiteBorder, "Error", "Unable to load and initialize the mission.");
			continue;
		}
	}
#endif
    return false;
}

bool Shell::BeginNewGame()
{
#if 0
	bool fNewGame = true;
	int nRank = 0;
	if ((gnDemoRank != 0) && (!gfDemo)) {

		// would you like to replay missions?  
		// UNDONE: Expand HtMessageBox and use it here instead of all this.

		DialogForm *pfrm = (DialogForm *)gpmfrmm->LoadForm(gpiniForms, kidfContinueGame, new DialogForm());
		if (pfrm != NULL) {
				pfrm->SetBorderColorIndex(kiclrWhite);
				pfrm->SetTitleColor(GetColor(kiclrSide1));
				pfrm->SetBackgroundColorIndex(kiclrShadow2x);
				pfrm->SetClearDibFlag();
				
				// position the form

				Rect rcForm;
				pfrm->GetRect(&rcForm);
				DibBitmap *pbm = pfrm->GetFormMgr()->GetDib();
				Size siz;
				pbm->GetSize(&siz);
				int yNew = (siz.cy - rcForm.Height()) / 2;
				int xNew = (siz.cx - rcForm.Width()) / 2;
				rcForm.Offset(xNew - rcForm.left, yNew - rcForm.top);
				pfrm->SetRect(&rcForm);

				int idc;
				pfrm->DoModal(&idc);
				gpmfrmm->RemoveForm(pfrm);

				if (gevm.IsAppStopping())
					return false;


				fNewGame = (idc == kidcOk);
				delete pfrm;
		}

		nRank = fNewGame ? 0 : gnDemoRank;
		gnDemoRank = 0;
		ggame.SavePreferences();
	}
	int nGo = PlayGame(kpmNormal, (void *)(fNewGame ? "S_00.lvl" : "S_03.lvl"), nRank);
	if (nGo == knGoAppStop)
		return true;

	if (nGo == knGoInitFailure)
		HtMessageBox(kfMbWhiteBorder, "Error", "Unable to load and initialize the mission.");

#endif
	return false;
}

#if 0
//
// PickLevelForm implementation
//

PickLevelForm::PickLevelForm(LevelType lt)
{
	m_lt = lt;
	m_szLevel[0] = 0;
}

int CreateLevelList(char **ppsz)
{
	// Get all the .lvl files

	char szFn[kcbFilename];
	int cFiles = 0;

	Enum enm;
	while (gpakr.EnumFiles(&enm, szFn, sizeof(szFn))) {
		int cch = strlen(szFn);
		if (cch < 4)
			continue;
		if (szFn[cch - 4] == '.' && szFn[cch - 3] == 'l' && szFn[cch - 2] == 'v' && szFn[cch - 1] == 'l') {
			strncpyz((char *)&gpbScratch[cFiles * kcbFilename], szFn, kcbFilename);
			cFiles++;
		}
	}

	// Sort them based on filename!

	for (int i = cFiles - 1; i >= 0; i--) {
		for (int j = 1; j <= i; j++) {
			char *pszBack = (char *)&gpbScratch[(j - 1) * kcbFilename];
			char *pszAhead = (char *)&gpbScratch[j * kcbFilename];

			if (strcmp(pszBack, pszAhead) > 0) {
				char szT[kcbFilename];
				strcpy(szT, pszBack);
				strcpy(pszBack, pszAhead);
				strcpy(pszAhead, szT);
			}
		}
	}

	// Alloc a chunk and copy them in

	char *pszT = new char[cFiles * kcbFilename];
	if (pszT == NULL)
		return 0;
	memcpy(pszT, gpbScratch, cFiles * kcbFilename);
	*ppsz = pszT;
	return cFiles;
}

bool PickLevelForm::Init(FormMgr *pfrmm, IniReader *pini, word idf)
{
	if (!ShellForm::Init(pfrmm, pini, idf))
		return false;

	// Create a entry for each level

	char szLevel[kcbFilename];
	char szTitle[100];

	ListControl *plstc = (ListControl *)GetControlPtr(kidcLevelList);

	char *aszLevels = NULL;
	int cLevels = CreateLevelList(&aszLevels);
	for (int nLevel = 0; nLevel < cLevels; nLevel++) {
		strncpyz(szLevel, (char *)&aszLevels[nLevel * kcbFilename], sizeof(szLevel));

		// Only show the requested level types

		LevelType lt;
		if (strnicmp(szLevel, "m_", 2) == 0)
			lt = kltMultiplayer;
		else if (strnicmp(szLevel, "s_", 2) == 0)
			lt = kltStory;
		else
			lt = kltChallenge;

		if (lt != m_lt)
			continue;

		// Pull title from level.

#if 1
//faster
		strcpy(szTitle, "<untitled>");
		IniReader *pini = LoadIniFile(gpakr, szLevel);
		if (pini != NULL) {
			pini->GetPropertyValue("General", "Title", szTitle, sizeof(szTitle));
			delete pini;
		}
		plstc->Add(szTitle, (void *)nLevel);
#else
		Level *plvl = new Level();
		plvl->LoadLevelInfo(szLevel);
		plstc->Add(plvl->GetTitle(), (void *)nLevel);
		delete plvl;
#endif
	}

	delete aszLevels;
	return true;
}

void PickLevelForm::OnControlSelected(word idc)
{
	if (idc == kidcOk) {
		ListControl *plstc = (ListControl *)GetControlPtr(kidcLevelList);
		int nLevel = (int)plstc->GetSelectedItemData();
		char *aszLevels = NULL;
		int cLevels = CreateLevelList(&aszLevels);
		if (nLevel >= 0 && nLevel <= cLevels) {
			strcpy(m_szLevel, (char *)&aszLevels[nLevel * kcbFilename]);
			delete aszLevels;
		} else {
			HtMessageBox(kfMbWhiteBorder, "Error!", "First you must select a level to play.");
			delete aszLevels;
			return;
		}
	} else if (idc == kidcLevelList) {
		return;
	}
	EndForm(idc);
}
#endif

//
// ShellForm implementation
//

ShellForm::ShellForm()
{
	m_fCached = false;
	m_fAnimate = true;
	m_fTimerEnabled = false;
}

ShellForm::~ShellForm()
{
	if (m_fTimerEnabled)
		gtimm.RemoveTimer(this);
}

bool ShellForm::Init(FormMgr *pfrmm, IniReader *pini, word idf)
{
	if (!Form::Init(pfrmm, pini, idf))
		return false;

	// Keep invisible until the controls are at the right place

	Show(false);

	// Shell forms draw over the whole screen, so size the form to be full screen.
	// This way it is a full screen opaquing form when it comes up, which means things
	// won't try to draw behind it, slowing the game down

	DibBitmap *pbm = m_pfrmm->GetDib();
	Size siz;
	pbm->GetSize(&siz);

	int xNew = (siz.cx - m_rc.Width()) / 2;
	int yNew = (siz.cy - m_rc.Height()) / 2;

	// Reposition the controls

	for (int n = 0; n < m_cctl; n++) {
		Control *pctl = m_apctl[n];
		Rect rcCtl;
		pctl->GetRect(&rcCtl);
		int xNewCtl = rcCtl.left + xNew;
		int yNewCtl = rcCtl.top + yNew;
		pctl->SetPosition(xNewCtl, yNewCtl);
	}
	Rect rcNew;
	rcNew.Set(0, 0, siz.cx, siz.cy);
	SetRect(&rcNew);

	// Set version string - hack

	if (m_idf == kidfStartup) {
		LabelControl *pctl = (LabelControl *)GetControlPtr(kidcVersion);
		if (pctl != NULL) {
			// If no version string, exe+data are unmarked. Show date/time
			// Use the version string verbatim so we show extra text at the end

			char szT[64];
			if (!ggame.GetFormattedVersionString(gszVersion, szT)) {
				szT[0] = 0;
#ifdef DEV_BUILD
				strcat(szT, "DEV BUILD ");
#endif
				strcat(szT, __DATE__);
				strcat(szT, ", ");
				strcat(szT, __TIME__);
			} else {
				szT[0] = 'v';
				strncpyz(&szT[1], gszVersion, sizeof(szT));
			}
#if __LP64__
			strcat(szT, " (64 bit)");
#endif
			pctl->SetText(szT);
		}
	}

	return true;
}

#define kctRate 2
#define kcFcZip 18
bool ShellForm::DoModal(int *pnResult, bool fAnimate, bool fShowSound)
{
	m_fAnimate = fAnimate;
	if (!m_fAnimate) {
		if (fShowSound)
			gsndm.PlaySfx(ksfxGuiFormShow);
		return Form::DoModal(pnResult, (Sfx)-1, (Sfx)-1);
	}

	// Take over form show sound playing

	Size siz;
	ggame.GetPlayfieldSize(&siz);

	// Reposition all the form's controls of the form so they can be zipped in

	for (int i = 0; i < m_cctl; i++) {
		Control *pctl = m_apctl[i];
		
		Rect rcCtl;
		pctl->GetRect(&rcCtl);

		// Remember where the control is supposed to end up

		m_axDst[i] = (rcCtl.left & ~1);

		// Odd index controls fly in from the left, even from the right

		if (i & 1) {
			pctl->SetPosition(rcCtl.left - siz.cx - (i * PcFromFc(kcFcZip)), rcCtl.top);
		} else {
			pctl->SetPosition(rcCtl.left + siz.cx + (i * PcFromFc(kcFcZip)), rcCtl.top);
		}
	}

	// Now that the controls are positioned right make the form visible

	Show(true);

	if (fShowSound)
		m_wf |= kfFrmShowSound;

	// Start timer

	gtimm.AddTimer(this, kctRate);
	m_fTimerEnabled = true;
	m_fCached = false;
	m_tLast = 0;

	// Some controls might be added on the fly after ShellForm::Init. Since we 
	// won't have their destination stashed away we'll just ignore them.

	m_cctlToZip = m_cctl;

	return Form::DoModal(pnResult, (Sfx)-1, (Sfx)-1);
}

void ShellForm::OnTimer(long tCurrent)
{
	Assert(m_fAnimate);

	int ct = (int)(tCurrent - m_tLast);
	m_tLast = tCurrent;

	// Waiting until all the pieces have been cached before doing absolute
	// timing. This gives smoother movement.

	int pcMove = PcFromFc(kcFcZip);
	if (m_fCached)
		pcMove = ((ct + kctRate / 2) / kctRate) * PcFromFc(kcFcZip);

	bool fZipping = false;

	for (int i = 0; i < m_cctlToZip; i++) {
		Control *pctl = m_apctl[i];

		Rect rcCtl;
		pctl->GetRect(&rcCtl);

		int xDst = m_axDst[i];
		if (rcCtl.left == xDst)
			continue;

		fZipping = true;

		int x;
		if (xDst > rcCtl.left)
			x = _min(rcCtl.left + pcMove, xDst);
		else
			x = _max(rcCtl.left - pcMove, xDst);

		pctl->SetPosition(x & ~1, rcCtl.top);
	}

	gevm.SetRedrawFlags(kfRedrawDirty | kfRedrawBeforeTimer);

	if (!fZipping) {
		gtimm.RemoveTimer(this);
		m_fTimerEnabled = false;
		if (m_wf & kfFrmShowSound)
			gsndm.PlaySfx(ksfxGuiFormShow);
        OnZipDone();
	}
}

void ShellForm::OnPaintBackground(DibBitmap *pbm, UpdateMap *pupd)
{
	// Draw the fancy background bitmap

	Size sizDib;
	pbm->GetSize(&sizDib);
	RawBitmap *prbm = LoadRawBitmap("titlescreenbkgd.rbm");
	Size sizBmp;
	prbm->GetSize(&sizBmp);
    
    // Draw middle
	Rect rcBmp;
	rcBmp.left = ((sizDib.cx - sizBmp.cx) / 2) & ~1;
	rcBmp.top = (sizDib.cy - sizBmp.cy) / 2;
	rcBmp.right = rcBmp.left + sizBmp.cx;
	rcBmp.bottom = rcBmp.top + sizBmp.cy;
	BltHelper(pbm, prbm, pupd, rcBmp.left, rcBmp.top);

#if 0
    // Draw left
    if (rcBmp.left > 0) {
        BltHelper(pbm, prbm, pupd, rcBmp.left - sizBmp.cx, rcBmp.top);
    }
    
    // Draw right
    if (rcBmp.right < sizDib.cx) {
        BltHelper(pbm, prbm, pupd, rcBmp.right, rcBmp.top);
    }
#endif
    
	delete prbm;
    
	// If the screen is wider than the form we clear those areas
	// out first to the form's background color

	Size siz;
	pbm->GetSize(&siz);
	if (rcBmp.top > 0) {
		Rect rc;
		rc.Set(0, 0, siz.cx, rcBmp.top);
		FillHelper(pbm, pupd, &rc, GetColor(kiclrBlack));
	}
	if (rcBmp.bottom < siz.cy) {
		Rect rc;
		rc.Set(0, rcBmp.bottom, siz.cx, siz.cy);
		FillHelper(pbm, pupd, &rc, GetColor(kiclrBlack));
	}
    
	if (rcBmp.left > 0) {
		Rect rc;
		rc.Set(0, rcBmp.top, rcBmp.left, rcBmp.bottom);
		FillHelper(pbm, pupd, &rc, GetColor(kiclrBlack));
	}
	if (rcBmp.right < siz.cx) {
		Rect rc;
		rc.Set(rcBmp.right, rcBmp.top, siz.cx, rcBmp.bottom);
		FillHelper(pbm, pupd, &rc, GetColor(kiclrBlack));
	}

	// Don't start timing for absolute positioned animation until we've
	// loaded the cache at least once. Gives smoother animation

	if (!m_fCached) {
		m_fCached = true;
		m_tLast = gtimm.GetTickCount();
	}
}

//
// RegisterNowForm
//

class RegisterNowForm : public ShellForm
{
public:
	virtual bool OnPenEvent(Event *pevt) secGameOptionsForm;
};

void DoRegisterNowForm()
{
	ShellForm *pfrm = (ShellForm *)gpmfrmm->LoadForm(gpiniForms, kidfRegisterNow, new RegisterNowForm());
	if (pfrm != NULL) {
		pfrm->DoModal();
		delete pfrm;
	}
}

bool RegisterNowForm::OnPenEvent(Event *pevt)
{
	if (pevt->eType == penDownEvent) {
		EndForm(kidcOk);
		return true;
	}
	return false;
}

} // namespace wi
