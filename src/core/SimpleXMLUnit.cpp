// C++ port of SimpleXMLUnit.pas (implementation section).
#include "SimpleXMLUnit.h"

#include "CommonTools.h"
#include "PrgSetupUnit.h"
#include "DosBoxUnit.h"
#include "GameDBToolsUnit.h"
#include "LanguageSetupUnit.h"

// "If St2.Count>0 then begin St2[0]:=Indent+'<![CDATA['+St2[0]; St2.Add(']]>'); end; St.AddStrings(St2); St2.Free"
static void AddCDATA(TStringList* St, TStringList* St2, const QString& Indent)
{
    if (St2->Count() > 0) { (*St2)[0] = Indent + "<![CDATA[" + (*St2)[0]; St2->Add("]]>"); }
    St->AddStrings(*St2);
    delete St2;
}

static void WriteProfilesToDOGXML(TStringList* St, TList* GamesList)
{
    int K;
    TGame* G;
    TStringList* St2;
    QString S;
  St->Add("<Document>");
  St->Add("<Export>");
  St->Add("<Version>1.0</Version>");
  St->Add("<Generator>D.O.G.</Generator>");
  St->Add("<RealGenerator>D-Fend Reborn " + GetReleaseVersionAsString() + "</RealGenerator>");
  St->Add("</Export>");

  for (int I = 0; I < GamesList->size(); I++) {
    G = static_cast<TGame*>((*GamesList)[I]);

    St->Add("");
    St->Add("<Profile>");

    St->Add("  <Emulator>");
    St->Add("    <Type>DOSBox</Type>");
    St->Add("    <Version>0.72</Version>");
    if (G->CustomDOSBoxDir() == "") {
      St->Add("    <Path>" + IncludeTrailingPathDelimiter(MakeAbsPath(PrgSetup->DOSBoxSettings(0)->DosBoxDir(), PrgSetup->BaseDir())) + "DOSBox.exe</Path>");
      St->Add("    <Name>DOSBox 0.72</Name>");
    } else {
      St->Add("    <Path>" + IncludeTrailingPathDelimiter(MakeAbsPath(G->CustomDOSBoxDir(), PrgSetup->BaseDir())) + "DOSBox.exe</Path>");
      St->Add("    <Name>DOSBox 0.72 (Custom build)</Name>");
    }
    St->Add("    <Configuration>");

    St->Add("      <Glide>0</Glide>");
    St->Add("      <Innovation>0</Innovation>");
    St->Add("      <MT32SoundQuality>0</MT32SoundQuality>");
    St->Add("      <OverScan>0</OverScan>");
    St->Add("      <VGAChipSet>0</VGAChipSet>");
    St->Add("      <VSync>0</VSync>");
    St->Add("      <Printer>0</Printer>");
    St->Add("      <Pixelshader>0</Pixelshader>");
    St->Add("      <Scalers>");
    St->Add(QString("       <Display>\"No Scaler\",\"Normal 2x\",\"Normal 3x\",\"Advanced MAME 2x\",\"Advanced MAME 3x\",\"High-Quality 2x Magnification\",\"High-Quality 3x Magnification\",\"Scale and Interpolation 2x\",\"Super Scale and Interpolation 2x\",\"Super Eagle\",") + "\"Advanced Interpolation 2x\",\"Advanced Interpolation 3x\",\"TV Interlacing 2x\",\"TV Interlacing 3x\",\"RGB 2x\",\"RGB 3x\",\"Scan 2x\",\"Scan 3x\"</Display>");
    St->Add("       <Values>none,normal2x,normal3x,advmame2x,advmame3x,hq2x,hq3x,sai2x,super2xsai,supereagle,advinterp2x,advinterp3x,tv2x,tv3x,rgb2x,rgb3x,scan2x,scan3x</Values>");
    St->Add("      </Scalers>");
    St->Add("      <OutputDevices>");
    St->Add("       <Display>Surface,Overlay,OpenGL,\"OpenGL NB\",\"Direct Draw\"</Display>");
    St->Add("       <Values>surface,overlay,opengl,openglnb,ddraw</Values>");
    St->Add("      </OutputDevices>");
    St->Add("      <Machines>");
    St->Add("       <Display>Hercules,CGA,Tandy,\"IBM PCjr\",VGA</Display>");
    St->Add("       <Values>hercules,cga,tandy,pcjr,vga</Values>");
    St->Add("      </Machines>");
    St->Add("      <Memory>");
    St->Add("       <Display>1MB,2MB,4MB,8MB,16MB,24MB,32MB,63MB</Display>");
    St->Add("       <Values>1,2,4,8,16,24,32,63</Values>");
    St->Add("      </Memory>");
    St->Add("      <Core>");
    St->Add("       <Display>Auto,Normal,Full,Dynamic</Display>");
    St->Add("       <Values>auto,normal,full,dynamic</Values>");
    St->Add("      </Core>");
    St->Add("      <JoystickType>");
    St->Add("       <Display>None,\"2 axis\",\"4 axis\",Thrustmaster,Flightstick,Auto</Display>");
    St->Add("       <Values>none,2axis,4axis,fcs,ch,auto</Values>");
    St->Add("      </JoystickType>");
    St->Add("      <SoundQuality>");
    St->Add("       <Display>\"8000 Hz\",\"11025 Hz\",\"22050 Hz\",\"32000 Hz\",\"44100 Hz\"</Display>");
    St->Add("       <Values>8000,11025,22050,32000,44100</Values>");
    St->Add("      </SoundQuality>");
    St->Add("      <VGAChipset>");
    St->Add("       <Display>None,\"Paradise PVGA1A\",\"Tseng Labs ET3000\",\"New Tseng Labs ET4000\",\"Tseng Labs ET4000\",S3</Display>");
    St->Add("       <Values>none,pvga1a,et3000,et4000new,et4000,s3</Values>");
    St->Add("      </VGAChipset>");
    St->Add("      <Pixelshader>");
    St->Add("       <Display>None,\"Point Shader\",\"Bilinear Shader\",\"Scale 2x Shader\",\"Sai 2x Shader\"</Display>");
    St->Add("       <Values>none,point.fx,bilinear.fx,scale2x.fx,2xsai.fx</Values>");
    St->Add("      </Pixelshader>");
    St->Add("      <VSyncMode>");
    St->Add("       <Display>Off,On,Force,Host</Display>");
    St->Add("       <Values>off,on,force,host</Values>");
    St->Add("      </VSyncMode>");
    St->Add("      <VSyncRate>");
    St->Add("       <Display>60,65,70,75,80</Display>");
    St->Add("       <Values>60,65,70,75,80</Values>");
    St->Add("      </VSyncRate>");
    St->Add("      <PrintOutput>");
    St->Add("       <Display>\"Bitmap Image\",\"PNG Image\",\"Postscript Document\",Printer</Display>");
    St->Add("       <Values>bmp,png,ps,printer</Values>");
    St->Add("      </PrintOutput>");
    St->Add("      <PrinterDPIList>");
    St->Add("       <Display>300,360,600,800,1200,2400</Display>");
    St->Add("       <Values>300,300,600,800,1200,2400</Values>");
    St->Add("      </PrinterDPIList>");
    St->Add("      <InnovationQuality>");
    St->Add("       <Display>\"Very Low\",Low,Medium,High</Display>");
    St->Add("       <Values>0,1,2,3</Values>");
    St->Add("      </InnovationQuality>");
    St->Add("      <FullResolutions>");
    St->Add("       <Display>\"Original Resolution\",320x200,320x240,400x300,512x384,640x400,640x480,800x480,800x600,1024x480,1024x600,1024x768,1152x864,1280x600,1280x768,1280x1024,1400x1050,1600x1200,1792x1344,1800x1440,1920x1080,1920x1200,1920x1440,2048x1536</Display>");
    St->Add("       <Values>original,320x200,320x240,400x300,512x384,640x400,640x480,800x480,800x600,1024x480,1024x600,1024x768,1152x864,1280x600,1280x768,1280x1024,1400x1050,1600x1200,1792x1344,1800x1440,1920x1080,1920x1200,1920x1440,2048x1536</Values>");
    St->Add("      </FullResolutions>");
    St->Add("      <WindowResolutions>");
    St->Add("       <Display>\"Original Resolution\",320x200,320x240,400x300,512x384,640x400,640x480,800x480,800x600,1024x480,1024x600,1024x768,1152x864,1280x600,1280x768,1280x1024,1400x1050,1600x1200,1792x1344,1800x1440,1920x1080,1920x1200,1920x1440,2048x1536</Display>");
    St->Add("       <Values>original,320x200,320x240,400x300,512x384,640x400,640x480,800x480,800x600,1024x480,1024x600,1024x768,1152x864,1280x600,1280x768,1280x1024,1400x1050,1600x1200,1792x1344,1800x1440,1920x1080,1920x1200,1920x1440,2048x1536</Values>");
    St->Add("      </WindowResolutions>}");
    St->Add("    </Configuration>");
    St->Add("  </Emulator>");
    St->Add("  <Configuration>");
    St->Add("    <Information>");
    St->Add("      <Name>" + G->Name() + "</Name>");
    S = Trim(G->GameExe()); if (Copy(S, 1, 2) == ".\\") S = "&lt;DOGDRIVE&gt;" + Copy(S, 3, MaxInt);
    St->Add("      <ExeFilename>" + S + "</ExeFilename>");
    S = Trim(G->SetupExe()); if (Copy(S, 1, 2) == ".\\") S = "&lt;DOGDRIVE&gt;" + Copy(S, 3, MaxInt);
    St->Add("      <SetupFilename>" + S + "</SetupFilename>");
    St->Add("      <EXECommandLine>" + G->GameParameters() + "</EXECommandLine>");
    St->Add("      <SetupCommandLine>" + G->SetupParameters() + "</SetupCommandLine>");
    St->Add("      <ExeStartDrive>Z</ExeStartDrive>");
    St->Add("      <SetupStartDrive>Z</SetupStartDrive>");
    St->Add("      <Develloper>" + G->Developer() + "</Develloper>");
    St->Add("      <Developer>" + G->Developer() + "</Developer>");
    St->Add("      <Publisher>" + G->Publisher() + "</Publisher>");
    St->Add("      <Genre>" + G->Genre() + "</Genre>");
    St->Add("      <Year>" + G->Year() + "</Year>");
    St->Add("      <Working>1</Working>");
    St->Add("      <ConfFile>&lt;DOGHOME&gt;confs\\" + ExtractFileName(G->SetupFile()) + "</ConfFile>");
    St->Add("      <ProfileFile></ProfileFile>");
    St->Add("      <Language>" + G->Language() + "</Language>");
    S = Trim(G->Icon()); if (Copy(S, 1, 2) == ".\\") S = "&lt;DOGDRIVE&gt;" + Copy(S, 3, MaxInt);
    St->Add("      <Icon>" + S + "</Icon>");
    St->Add("      <Note>");
    St2 = StringToStringList(G->Notes());
    AddCDATA(St, St2, "      ");
    St->Add("      </Note>");
    St->Add("      <CustomInformation>");
    St2 = StringToStringList(G->UserInfo());
    {
      for (int J = 0; J < St2->Count(); J++) {
        S = (*St2)[J]; K = Pos("=", S);
        if (K == 0) {
          St->Add("<Title" + IntToStr(K) + "></Title" + IntToStr(K) + ">");
          St->Add("<Custom0" + IntToStr(K) + ">" + S + "</Custom0" + IntToStr(K) + ">");
        } else {
          St->Add("<Title" + IntToStr(K) + ">" + Copy(S, 1, K - 1) + "</Title" + IntToStr(K) + ">");
          St->Add("<Custom0" + IntToStr(K) + ">" + Copy(S, K + 1, MaxInt) + "</Custom0" + IntToStr(K) + ">");
        }
      }
      delete St2;
    }
    St->Add("      </CustomInformation>");
    St->Add("      <LinkedFiles>");
    for (int L = 1; L <= 9; L++) if (Trim(G->WWW(L)) != "") {
       St->Add("        <Title" + IntToStr(L - 1) + ">" + G->WWWName(L) + "</Title" + IntToStr(L - 1) + ">");
       St->Add("      <Link" + IntToStr(L - 1) + ">" + G->WWW(L) + "</Link" + IntToStr(L - 1) + ">");
    }
    St->Add("      </LinkedFiles>");
    St->Add("    </Information>");
    St->Add("    <Settings>");
    St->Add("      <Full-Configuration>");
    St2 = BuildConfFile(G, false, false, -1, nullptr, false); if (St2 == nullptr) return;
    AddCDATA(St, St2, "      ");
    St->Add("      </Full-Configuration>");
    St->Add("      <Manual-Configuration>");
    St2 = StringToStringList(G->CustomSettings());
    AddCDATA(St, St2, "      ");
    St->Add("      </Manual-Configuration>");
    St->Add("      <Manual-AutoExec>");
    St->Add("      <![CDATA[#INITIALIZATION");
    St2 = StringToStringList(G->Autoexec());
    St->AddStrings(*St2); delete St2;
    St->Add("#FINALIZATION]]>");
    St2 = StringToStringList(G->AutoexecFinalization());
    St->AddStrings(*St2); delete St2;
    St->Add("      </Manual-AutoExec>");
    St->Add("      <Addons>");
    St->Add("        <DOS32A>");
    if (G->UseDOS32A()) St->Add("          <Used>1</Used>"); else St->Add("          <Used>0</Used>");
    St->Add("        </DOS32A>");
    St->Add("        <i4DOS>");
    if (G->Use4DOS()) St->Add("          <Used>1</Used>"); else St->Add("          <Used>0</Used>");
    St->Add("        </i4DOS>");
    St->Add("        <CWSDPMI>");
    St->Add("          <Used>0</Used>");
    St->Add("          <SwapFile>0</SwapFile>");
    St->Add("          <DPMI10>0</DPMI10>");
    St->Add("        </CWSDPMI>");
    St->Add("        <Mouse>");
    if (G->AutoLockMouse()) St->Add("          <AutoLock>1</AutoLock>"); else St->Add("          <AutoLock>0</AutoLock>");
    St->Add("        </Mouse>");
    St->Add("        <Screen>");
    St->Add("          <RenderFix>0</RenderFix>");
    St->Add("        </Screen>");
    St->Add("      </Addons>");
    St->Add("    </Settings>");
    St->Add("    <AttachedFiles/>");
    St->Add("    <DataFiles/>");
    St->Add("  </Configuration>");
    St->Add("</Profile>");
  }

  St->Add("</Document>");
}


static void WriteProfilesToDBGLXML(TStringList* St, TList* GamesList)
{
    TGame* G;
    QString S;
    TStringList* St2;
    St->Add("<profiles>");
    St->Add("<export>");
    St->Add("<format-version>1.0</format-version>");
    St->Add("<title>D-Fend exported games</title>");
    if (GamesList->size() > 0) S = static_cast<TGame*>((*GamesList)[0])->Name();
    for (int I = 1; I < GamesList->size(); I++) S = S + ", " + static_cast<TGame*>((*GamesList)[I])->Name();
    St->Add("<notes><![CDATA[" + S + "]]></notes>");
    St->Add("<generator-title>DOSBox Game Launcher</generator-title>");
    St->Add("<generator-version>0.62</generator-version>");
    St->Add("<RealGenerator>D-Fend Reborn " + GetReleaseVersionAsString() + "</RealGenerator>");
    St->Add("<captures-available>false</captures-available>");
    St->Add("<gamedata-available>false</gamedata-available>");
    St->Add("</export>");

    for (int I = 0; I < GamesList->size(); I++) {
        G = static_cast<TGame*>((*GamesList)[I]);

        St->Add("");
        St->Add("<Profile>");
        St->Add("  <title>" + G->Name() + "</title>");
        St->Add("  <meta-info>");

        St->Add("    <developer>" + G->Developer() + "</developer>");
        St->Add("    <publisher>" + G->Publisher() + "</publisher>");
        St->Add("    <year>" + G->Year() + "</year>");
        St->Add("    <genre>" + G->Genre() + "</genre>");
        St->Add("    <status></status>");
        St->Add("    <ConfFile>&lt;DOGHOME&gt;confs\\" + ExtractFileName(G->SetupFile()) + "</ConfFile>");
        St->Add("      <ProfileFile></ProfileFile>");
        St->Add("      <Language>" + G->Language() + "</Language>");
        St->Add("      <notes>");
        St2 = StringToStringList(G->Notes());
        AddCDATA(St, St2, "      ");
        St->Add("      </notes>");
        St2 = StringToStringList(G->UserInfo());
        for (int J = 0; J < Min(St2->Count(), 10); J++)
            St->Add("      <custom" + IntToStr(J) + ">" + (*St2)[J] + "</custom" + IntToStr(J) + ">");
        delete St2;
        for (int K = 1; K <= 9; K++) if (Trim(G->WWW(K)) != "") {
            St->Add("    <link" + IntToStr(K) + ">" + G->WWW(K) + "</link" + IntToStr(K) + ">");
        }
        St->Add("  </meta-info>");
        St->Add("  <full-configuration>");
        St2 = BuildConfFile(G, false, false, -1, nullptr, false); if (St2 == nullptr) return;
        AddCDATA(St, St2, "    ");
        St->Add("  </full-configuration>");
        St->Add("  <incremental-configuration>");
        St2 = BuildConfFile(G, false, false, -1, nullptr, false); if (St2 == nullptr) return;
        AddCDATA(St, St2, "    ");
        St->Add("  </incremental-configuration>");
        St->Add("  <dosbox>");
        St->Add("    <title>DOSBox v0.72</title>");
        St->Add("    <version>0.72</version>");
        St->Add("    <captures>" + G->CaptureFolder() + "</captures>");
        St->Add("    <game>" + G->GameExe() + "</game>");
        St->Add("    <game-parameters>" + G->GameParameters() + "</game-parameters>");
        St->Add("    <setup>" + G->SetupExe() + "</setup>");
        St->Add("    <setup-parameters>" + G->SetupParameters() + "</setup-parameters>");
        if (G->GameExe() == "") S = ""; else {
            S = MakeRelPath(ExtractFilePath(MakeAbsPath(G->GameExe(), PrgSetup->BaseDir())), PrgSetup->BaseDir());
            if (Copy(S, 1, 2) == ".\\") S = "&lt;DOGDRIVE&gt;" + Copy(S, 3, MaxInt);
        }
        St->Add("    <game-dir>" + S + "</game-dir>");
        St->Add("  </dosbox>");
        St->Add("</profile>");
    }
    St->Add("</profiles>");
}

bool WriteProfilesToXML(TList* GamesList, const QString& FileName, TSimpleXMLFileStyle FileStyle)
{
    bool result;
    TStringList St;
    St.Add("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    St.Add("");
    switch (FileStyle) {
        case sxfsDOG: WriteProfilesToDOGXML(&St, GamesList); break;
        case sxfsDBGL: WriteProfilesToDBGLXML(&St, GamesList); break;
        default: result = false; return result;
    }
    result = true;
    if (!St.SaveToFile(FileName)) result = false;
    return result;
}

enum TXMLType { xtDOG, xtDBGL };

// IXMLNode.ChildNodes['Name']: TXMLDocument has doNodeAutoCreate set by
// default, so a missing child node is created (empty) instead of returning nil.
static QDomElement ChildNode(QDomElement N, const QString& Name)
{
    QDomElement E = N.firstChildElement(Name);
    if (E.isNull()) {
        E = N.ownerDocument().createElement(Name);
        N.appendChild(E);
    }
    return E;
}

// IXMLNode.ChildNodes (element nodes)
static QList<QDomElement> ChildNodes(const QDomElement& N)
{
    QList<QDomElement> result;
    for (QDomElement E = N.firstChildElement(); !E.isNull(); E = E.nextSiblingElement()) result.append(E);
    return result;
}

static TGame* MakeGameProfileFromXML(TGameDB* GameDB, QDomElement Profile, TXMLType XMLType)
{
    QDomElement N;
    QString S;
    TGame* result = nullptr;

    switch (XMLType) {
        case xtDBGL: N = ChildNode(Profile, "title"); break;
        case xtDOG:
            N = ChildNode(Profile, "Configuration");
            if (!N.isNull()) N = ChildNode(N, "Information");
            if (!N.isNull()) N = ChildNode(N, "Name");
            break;
        default: return result;
    }
    // The Delphi code used N.NodeName here (which always gave "title" / "Name");
    // the text of the node is the profile name.
    if (N.isNull()) S = "Game"; else S = N.text();
    result = GameDB->Game(GameDB->Add(S));
    return result;
}

static void LoadMetaInfoFromXML(TGame* G, QDomElement Profile, TXMLType XMLType)
{
    TStringList St;
    QDomElement N, N2, N3;
    QString S;
    if (XMLType == xtDBGL) {
        N = ChildNode(Profile, "meta-info");
        if (!N.isNull()) {
            const QList<QDomElement> Children = ChildNodes(N);
            for (int I = 0; I < Children.size(); I++) {
                N2 = Children[I];
                S = Trim(ExtUpperCase(N2.nodeName()));
                if (S == "DEVELOPER") { G->SetDeveloper(N2.text()); continue; }
                if (S == "PUBLISHER") { G->SetPublisher(N2.text()); continue; }
                if (S == "YEAR") { G->SetYear(N2.text()); continue; }
                if (S == "GENRE") { G->SetGenre(N2.text()); continue; }
                if (S == "NOTES") { G->SetNotes(N2.text()); continue; }
                for (int K = 1; K <= 9; K++) if (S == "LINK" + IntToStr(K)) { G->SetWWW(K, N2.text()); continue; }
                St.Add(N2.nodeName() + "=" + N2.text());
            }
        }
    }
    if (XMLType == xtDOG) {
        N = ChildNode(Profile, "Configuration");
        if (!N.isNull()) N = ChildNode(N, "Information");
        if (!N.isNull()) {
            const QList<QDomElement> Children = ChildNodes(N);
            for (int I = 0; I < Children.size(); I++) {
                N2 = Children[I];
                S = Trim(ExtUpperCase(N2.nodeName()));
                if (S == "DEVELOPER") { G->SetDeveloper(N2.text()); continue; }
                if (S == "PUBLISHER") { G->SetPublisher(N2.text()); continue; }
                if (S == "YEAR") { G->SetYear(N2.text()); continue; }
                if (S == "GENRE") { G->SetGenre(N2.text()); continue; }
                if (S == "NOTES") { G->SetNotes(N2.text()); continue; }
                if (S == "LANGUAGE") { G->SetLanguage(N2.text()); continue; }
                if (S == "ICON") { G->SetIcon(N2.text()); continue; }
                if (S == "WORKING") { St.Add(N2.nodeName() + "=" + N2.text()); continue; }
                if (S == "CUSOMINFORMATION") {
                    const QList<QDomElement> Children2 = ChildNodes(N2);
                    for (int J = 0; J < Children2.size(); J++) { N3 = Children2[J]; St.Add(N3.nodeName() + "=" + N3.text()); }
                }
            }
        }
    }
    G->SetUserInfo(StringListToString(&St));
}

static void LoadFullConfiguration(TGameDB* AGameDB, TGame* G, QDomElement Profile, TXMLType XMLType)
{
    QDomElement N;
    if (XMLType == xtDBGL) {
        N = ChildNode(Profile, "full-configuration");
    }

    if (XMLType == xtDOG) {
        N = ChildNode(Profile, "Configuration");
        if (!N.isNull()) N = ChildNode(N, "Settings");
        if (!N.isNull()) N = ChildNode(N, "Full-Configuration");
    }

    if (!N.isNull()) {
        ImportConfData(AGameDB, G, N.text());
    }
}

static void LoadSpecialDBGLData(TGame* G, QDomElement Profile)
{
    QDomElement N, N2;
    QString S;
    N = ChildNode(Profile, "dosbox");
    if (N.isNull()) return;

    const QList<QDomElement> Children = ChildNodes(N);
    for (int I = 0; I < Children.size(); I++) {
        N2 = Children[I];
        S = Trim(ExtUpperCase(N2.nodeName()));
        if (S == "CAPTURES") {
            S = N2.text();
            if ((Length(S) < 2) || (S[1] != ':')) {
                if (Length(S) >= 1) {
                    if (S[0] == '\\') S = "." + S;
                    if (S[0] != '.') S = ".\\" + S;
                }
            }
            G->SetCaptureFolder(S);
            continue;
        }
        if (S == "SETUP") { G->SetSetupExe(N2.text()); continue; }
        if (S == "SETUP-PARAMETERS") { G->SetSetupParameters(N2.text()); continue; }
    }
}

static QString DOGFileNameToFileName(const QString& DOGFileName)
{
    const QString DOGString1 = "&lt;DOGDRIVE&gt;";
    const QString DOGString2 = "<DOGDRIVE>";
    QString result = Trim(DOGFileName);
    if (ExtUpperCase(Copy(result, 1, Length(DOGString1))) == ExtUpperCase(DOGString1)) result = Trim(Copy(result, Length(DOGString1) + 1, MaxInt));
    if (ExtUpperCase(Copy(result, 1, Length(DOGString2))) == ExtUpperCase(DOGString2)) result = Trim(Copy(result, Length(DOGString2) + 1, MaxInt));
    if (Length(result) >= 1) {
        if (result[0] == '\\') result = "." + result;
        if (result[0] != '.') result = ".\\" + result;
    }
    return result;
}

static bool ReadValue(const QDomElement& N, const QString& IntegerKey, const QString& DataKey, QString& Data)
{
    int ValNr;
    QDomElement N2, Values;
    TStringList* St;
    bool result = false;

    ValNr = -1;
    const QList<QDomElement> Children = ChildNodes(N);
    for (int I = 0; I < Children.size(); I++) {
        N2 = Children[I];
        if (Trim(ExtUpperCase(N2.nodeName())) == ExtUpperCase(IntegerKey)) {
            if (!TryStrToInt(N2.text(), ValNr)) { ValNr = -1; continue; }
            break;
        }
    }
    if (ValNr < 0) return result;

    for (int I = 0; I < Children.size(); I++) {
        N2 = Children[I];
        if (Trim(ExtUpperCase(N2.nodeName())) == ExtUpperCase(DataKey)) {
            Values = ChildNode(N2, "Values");
            if (!Values.isNull()) {
                St = ValueToList(Values.text(), ",");
                result = (St->Count() > ValNr);
                if (result) Data = Trim((*St)[ValNr]);
                delete St;
                return result;
            }
        }
    }
    return result;
}

static void LoadSpecialDOGData(TGame* G, QDomElement Profile)
{
    QDomElement N, N2, N3;
    QString S;
    N = ChildNode(Profile, "Emulator");
    if (!N.isNull()) {

        N2 = ChildNode(N, "Path");
        if (!N2.isNull()) G->SetCustomDOSBoxDir(MakeRelPath(N2.text(), PrgSetup->BaseDir(), true));

        N2 = ChildNode(N, "Configuration");
        if (!N2.isNull()) {
            N3 = ChildNode(N2, "Glide");
            if (!N3.isNull()) {
                if (N3.text() == "0") G->SetGlideEmulation("flase"); else G->SetGlideEmulation("true");
            }
            if (ReadValue(N2, "VGAChipSet", "VGAChipset", S)) G->SetVGAChipset(S);
            if (ReadValue(N2, "Printer", "PrintOutput", S)) {
                G->SetPrinterOutputFormat(S);
                G->SetEnablePrinterEmulation(ExtUpperCase(S) != "NONE");
            }
        }

    }

    N = ChildNode(Profile, "Configuration");
    if (!N.isNull()) {

        N2 = ChildNode(N, "Information");
        if (!N2.isNull()) {
            N3 = ChildNode(N2, "ExeFilename");
            if (!N3.isNull()) G->SetGameExe(DOGFileNameToFileName(N3.text()));
            N3 = ChildNode(N2, "SetupFilename");
            if (!N3.isNull()) G->SetSetupExe(DOGFileNameToFileName(N3.text()));
            N3 = ChildNode(N2, "EXECommandLine");
            if (!N3.isNull()) G->SetGameParameters(N3.text());
            N3 = ChildNode(N2, "SetupCommandLine");
            if (!N3.isNull()) G->SetSetupParameters(N3.text());
        }

        N2 = ChildNode(N, "Settings");
        if (!N2.isNull()) N2 = ChildNode(N2, "Addons");
        if (!N2.isNull()) {
            N3 = ChildNode(N2, "DOS32A");
            if (!N3.isNull()) {
                N3 = ChildNode(N3, "Used");
                if ((!N3.isNull()) && (N3.text() != "0")) G->SetUseDOS32A(true);
            }
            N3 = ChildNode(N2, "i4DOS");
            if (!N3.isNull()) {
                N3 = ChildNode(N3, "Used");
                if ((!N3.isNull()) && (N3.text() != "0")) G->SetUse4DOS(true);
            }
            N3 = ChildNode(N2, "Mouse");
            if (!N3.isNull()) {
                N3 = ChildNode(N3, "AutoLock");
                if ((!N3.isNull()) && (N3.text() != "0")) G->SetAutoLockMouse(true);
            }
        }

    }
}

static QString ImportProfileFromXML(TGameDB* GameDB, QDomElement Profile, TXMLType XMLType)
{
    TGame* G;
    QString result = "";

    G = MakeGameProfileFromXML(GameDB, Profile, XMLType);
    if (G == nullptr) return result;

    LoadMetaInfoFromXML(G, Profile, XMLType);
    LoadFullConfiguration(GameDB, G, Profile, XMLType);

    if (XMLType == xtDBGL) LoadSpecialDBGLData(G, Profile);
    if (XMLType == xtDOG) LoadSpecialDOGData(G, Profile);

    G->StoreAllValues();
    G->LoadCache();
    return result;
}

static QString ImportXMLFileIntern(TGameDB* GameDB, QDomElement Root, TXMLType XMLType)
{
    QString result = "";

    const QList<QDomElement> Children = ChildNodes(Root);
    for (int I = 0; I < Children.size(); I++) if (Trim(ExtUpperCase(Children[I].nodeName())) == "PROFILE") {
        result = ImportProfileFromXML(GameDB, Children[I], XMLType);
        if (result != "") return result;
    }
    return result;
}

QString ImportXMLFile(TGameDB* GameDB, const QString& FileName)
{
    QDomDocument XMLDoc;
    QByteArray MSt;
    QString S;
    QString result = "";

    {
        QFile F(FileName);
        if (!F.open(QIODevice::ReadOnly)) { result = Format(LanguageSetup->MessageCouldNotOpenFile(), {FileName}); return result; }
        MSt = F.readAll();
    }

    if (!XMLDoc.setContent(MSt)) {
        /* result:=LanguageSetup.MessageCouldNotActivateXMLDocument; */
        result = "Could not activte the XML document.";
        return result;
    }

    S = Trim(ExtUpperCase(XMLDoc.documentElement().nodeName()));
    if (S == "DOCUMENT") { result = ImportXMLFileIntern(GameDB, XMLDoc.documentElement(), xtDOG); return result; }
    if (S == "PROFILES") { result = ImportXMLFileIntern(GameDB, XMLDoc.documentElement(), xtDBGL); return result; }
    /* result:=Format(LanguageSetup.MessageUnknownXMLType,[XMLDoc.DocumentElement.NodeName]); */
    result = Format("Unknown document type \"%s\".", {XMLDoc.documentElement().nodeName()});
    return result;
}
