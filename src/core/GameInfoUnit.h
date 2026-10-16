// D-Fend Reborn: game information (name, genre, developer, ...) for profiles
//  - standard values of the game information fields (Bin/GameInfo.ini)
//  - search in the auto setup templates (offline)
//  - search in Wikidata (online, optional; https://www.wikidata.org/)
// Replaces the MobyGames / TheGamesDB readers of D-Fend Reloaded.
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

struct TGameInfo {
    QString Name;
    QString Genre;       // standard (English) genres, ";" separated
    QString Developer;
    QString Publisher;
    QString Year;
    QString Internet;
    QString Notes;
    QString Id;          // template file or Wikidata item ("Q...")
    bool DetailsLoaded = false;
};

// Kind: "Genre", "Language" or "License"; returns the standard spelling of
// each ";" separated value (unknown values unchanged)
QString StandardGameInfoValue(const QString& Kind, const QString& Value);

// Exported auto setup template: UTF-8, standard genre / language / license
// values, 4 digit year (docs/AutoSetupTemplates.md)
void NormaliseAutoSetupTemplateFile(const QString& FileName);

// Auto setup templates of the AutoSetup folder whose names match Name
QList<TGameInfo> SearchTemplateGameInfo(const QString& Name);

// Wikidata: games (and MS-DOS programs) matching Name; DOSOnly: only items
// with the platform MS-DOS
bool SearchWikidataGameInfo(const QString& Name, bool DOSOnly, QList<TGameInfo>& Result, QString& Error);
// Wikidata: developer, publisher, genre, year, web site and description of Info.Id
bool LoadWikidataGameInfo(TGameInfo& Info, QString& Error);
