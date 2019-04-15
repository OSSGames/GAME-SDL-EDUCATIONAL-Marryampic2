/* Marryampic II
 * Copyright (C) 2002-2005 Marc Le Douarain
 * http://www.multimania.com/mavati/marryampic2
 * December 2002
 * Last update : 29 December 2005
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define INCL_DOS
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SFont.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sound_params.h"

#define WIDTH 640
#define HEIGHT 480

#define NBR_CARDS_X 50
#define NBR_CARDS_Y 50
#define NBR_PAIRS 250

#define CARD_BACKDROP 9999
#define CARD_BLANK 9998
#define SAMPLE_FOUND 9999
#define SAMPLE_FINISHED 9998

#define NBR_PLAYERS 4

#define NBR_CARDSETS 200

#define BASE_Y ((HEIGHT-480)/2)

void MorphToPM();

void MorphToPM() {
   PPIB pib;
   PTIB tib;
 
   DosGetInfoBlocks(&tib, &pib);
 
   // Change flag from VIO to PM:
   if (pib->pib_ultype==2) pib->pib_ultype = 3;
}


void DisplayErrorAndExit( char * ErrorMsg );

// For the moment, use my function with IFF 8SVX support instead of one from SDL_mixer
#define My_LoadWAV(file) My_LoadWAV_RW( SDL_RWFromFile( file, "rb" ), 1 )
extern "C"
{
Mix_Chunk *My_LoadWAV_RW(SDL_RWops *src, int freesrc);
}


class ClSetup
{
	public:
	int NbrPlayers;
	char PlayerName[ NBR_PLAYERS ][ 20 ];
	int NumCardsetSelected;
	int NbrCardsetsInList;
	char * pCardsetList[ NBR_CARDSETS ];
	bool RuleOnlySoundListened;
	bool RuleKeepSameSound;
	int ShowCardsDuringSecs;
	private:
	SDL_Surface * Font;
	int CoordPlayersNamesX;
	int NumTextToDraw;
	int CurrentZone;
	int CurrentZoneBak;

	public:
	ClSetup( );
	~ClSetup( );
	void DrawZone( int NumZone, int PosiX, int PosiY, char * Text, bool bDrawIt );
	void DrawScreen( bool bDrawAll );
	bool Click( int WhichMouseButton );
	void DrawOneText( bool bNextOne, bool bNoUpdate );
	bool ReadCardsetsList( void );
	bool TheSetupPage( void );

	//TODO: LoadSetup( ) & SaveSetup( )
};

class ClBoard
{
	SDL_Surface * CardsImage;
	int CardWidth;
	int CardHeight;
	int NbrCardsX;
	int NbrCardsY;
	int SpacingX;
	int SpacingY;
	int DisplayWidth;
	int DisplayHeight;
	SDL_Surface * BackgroundImage;

	int BoardWidth;
	int BoardHeight;
	int BoardSpacingX;
	int BoardSpacingY;
	int BoardMessagesHeight;
	int NbrPairs;

	int CardNumber[ NBR_CARDS_X ][ NBR_CARDS_Y ];
	bool PairFound[ NBR_PAIRS ];
	int PrevCardX,PrevCardY;
	int FoundCardX[ 2 ],FoundCardY[ 2 ];
	bool bFinished;
	int PlayerNow;
	int NumPairListenedToFound;
	Mix_Chunk * SoundCard;
	public :
	char CardsetDirectory[ 500 ];
	bool bMakeTheTest;

	public:
	ClBoard( );
	~ClBoard( );
	bool LoadCardpic( char * CardsetToLoad );
	bool LoadInfos( char * CardsetToLoad );
	bool ToLowerCase( char * text );
	void RenameLowerCase( char * CardsetToLoad );
	bool LoadDatas( );
	bool VerifyAbort( void );
	void DisplayMessage( char * text );
	void DisplayPlayerMessage( int NumPlayer, bool bWin );
	void CleanCard( int PosiX, int PosiY );
	void DrawCard( int NumCard, int PosiX, int PosiY );
	void DrawCards( bool bHideCards );
	void PlaySoundCard( int NumCard );
	void ListenASound( void );
	void ClearScreen( bool DrawBackground );
	void InitGame( void );
	void CardClicked( int PosiX, int PosiY );
	void ClickedAt( int ClickX, int ClickY );
	bool MouseWait( void );
	void TheGame( void );
	void TestMode( void );
};

class ClScores
{
	int Found[ NBR_PLAYERS ];
	int Played[ NBR_PLAYERS ];

	public:
	ClScores( );
	void InitScores( void );
	void AddFound( int NumPlayer );
	void AddPlayed( int NumPlayer );
	void DisplayImage( SDL_Surface * Image, int Width, int Height );
	void ShowScores( char * Path, int DispWidth, int DispHeight );
};

ClSetup ObjSetup;
ClBoard ObjBoard;
ClScores ObjScores;
SDL_Surface * GameScreen;
SDL_Surface * GameFont;
SDL_Surface * MenuFont;
#ifndef __MORPHOS__ 
bool bSetFullScreen = false;
#else
bool bSetFullScreen = true;
#endif

ClSetup::ClSetup( )
{
	NbrCardsetsInList = 0;
	for( int iBalayCard=0; iBalayCard<NBR_CARDSETS; iBalayCard++)
		pCardsetList[ iBalayCard ] = NULL;
	NumCardsetSelected = 0;
	NbrPlayers = 1;
	for( int Player=0; Player<NBR_PLAYERS; Player++ )
		sprintf( PlayerName[ Player ], "Player#%d", Player+1 );
	RuleOnlySoundListened = false;
	RuleKeepSameSound = true;
	ShowCardsDuringSecs = 5;
	CurrentZone = 0;
	CurrentZoneBak = -1;
}

ClSetup::~ClSetup( )
{
	for( int iBalayCard=0; iBalayCard<NBR_CARDSETS; iBalayCard++)
	{
		if ( pCardsetList[ iBalayCard ] )
		{
			free( pCardsetList[ iBalayCard ] );
			pCardsetList[ iBalayCard ] = NULL;
		}
	}
}

void ClSetup::DrawZone( int NumZone, int PosiX, int PosiY, char * Text, bool bDrawIt )
{
	int MouseX,MouseY;
	SDL_GetMouseState( &MouseX, &MouseY );
	// in the zone?
	if ( MouseY>=PosiY+5 && MouseY<PosiY+32 )
		CurrentZone = NumZone;
	if ( bDrawIt )
	{
		bool bBright = CurrentZone==NumZone;
		PutString( GameScreen, PosiX, PosiY, Text );
		if ( bBright )
			PutString( GameScreen, PosiX-1, PosiY-1, Text );
	}
}

#define SETUP_RGB_COLORS SDL_MapRGB(GameScreen->format, 50, 10, 80)

void ClSetup::DrawScreen( bool bDrawAll )
{
	char Buff[ 100 ];

	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = WIDTH;
	rect.h = HEIGHT;
	if ( bDrawAll )
	{
		SDL_FillRect(GameScreen, &rect, SETUP_RGB_COLORS);
		CurrentZoneBak = -1;
		InitFont( Font );
		strcpy( Buff, "MARRYAMPIC II" );
		PutString( GameScreen, (WIDTH-TextWidth(Buff))/2,BASE_Y+10, Buff );
		PutString( GameScreen, (WIDTH-TextWidth(Buff))/2+1,BASE_Y+10+1, Buff );
	}
	sprintf( Buff, "Nbr. players : %d", NbrPlayers );
	DrawZone( 1, (WIDTH-TextWidth(Buff))/2,BASE_Y+80, Buff, bDrawAll );
	int CoordLinesX = (WIDTH-TextWidth("                             "))/2;
	CoordPlayersNamesX = CoordLinesX + TextWidth( "Name Player #0 : " );
	for( int Player=0; Player<NbrPlayers; Player++ )
	{
		sprintf( Buff, "Name Player #%d :", Player+1 );
		DrawZone( 10+Player, CoordLinesX, BASE_Y+125+Player*30, Buff, bDrawAll );
		if ( bDrawAll )
		{
			strcpy( Buff, PlayerName[ Player ] );
			PutString( GameScreen, CoordPlayersNamesX, BASE_Y+125+Player*30, Buff );
		}
	}
	strcpy( Buff, "Cardset : " );
	if ( NbrCardsetsInList>0 )
		strcat( Buff, pCardsetList[ NumCardsetSelected ] );
	DrawZone( 2, (WIDTH-TextWidth(Buff))/2,BASE_Y+260, Buff, bDrawAll );
	strcpy( Buff, RuleOnlySoundListened?"Only sound-pair listened rule":"Classic memory rule" );
	DrawZone( 3, (WIDTH-TextWidth(Buff))/2,BASE_Y+305, Buff, bDrawAll );
	if ( RuleOnlySoundListened )
	{
		strcpy( Buff, RuleKeepSameSound?"Keep same sound":"Do not keep same sound" );
		DrawZone( 4, (WIDTH-TextWidth(Buff))/2,BASE_Y+340, Buff, bDrawAll );
	}
	if ( ShowCardsDuringSecs>0 )
		sprintf( Buff, "Preview cards during %d sec%c", ShowCardsDuringSecs, ShowCardsDuringSecs>1?'s':' ' );
	else
		strcpy( Buff, "Do not preview cards" );
	DrawZone( 5, (WIDTH-TextWidth(Buff))/2,BASE_Y+385, Buff, bDrawAll );

	sprintf( Buff, "Play !" );
	DrawZone( 6, (WIDTH-TextWidth(Buff))/2,BASE_Y+435, Buff, bDrawAll );
	if ( bDrawAll )
	{
		DrawOneText( false /*bNextOne*/, true /*bNoUpdate*/ );
		SDL_UpdateRects( GameScreen, 1, &rect );
	}
}

bool ClSetup::Click( int WhichMouseButton )
{
	// receiving 2 events for the wheel...?
	static bool WheelBtn = false;
	if ( WhichMouseButton==SDL_BUTTON_WHEELDOWN || WhichMouseButton==SDL_BUTTON_WHEELUP )
	{
		if (WheelBtn)
		{
			WheelBtn = false;
			return false;
		}
		WheelBtn = true;
	}
	
	bool bStartGame = false;
	char Buff[ 100 ];
	if ( WhichMouseButton!=SDL_BUTTON_WHEELDOWN && WhichMouseButton!=SDL_BUTTON_WHEELUP )
	{
		switch( CurrentZone )
		{
			case 6:
				bStartGame = true;
				break;
			case 1:
				if ( WhichMouseButton==SDL_BUTTON_LEFT )
					NbrPlayers++;
				if ( WhichMouseButton==SDL_BUTTON_RIGHT )
					NbrPlayers--;
				if ( NbrPlayers>NBR_PLAYERS )
					NbrPlayers = 1;
				if ( NbrPlayers<1 )
					NbrPlayers = NBR_PLAYERS;
				break;
			case 3:
				RuleOnlySoundListened = RuleOnlySoundListened==true?false:true;
				break;
			case 4:
				RuleKeepSameSound = RuleKeepSameSound==true?false:true;
				break;
			case 5:
				if ( WhichMouseButton==SDL_BUTTON_LEFT )
					ShowCardsDuringSecs++;
				if ( WhichMouseButton==SDL_BUTTON_RIGHT )
					ShowCardsDuringSecs--;
				if ( ShowCardsDuringSecs>30 )
					ShowCardsDuringSecs = 30;
				if ( ShowCardsDuringSecs<0 )
					ShowCardsDuringSecs = 0;
				break;
		}
		for( int Player=0; Player<NbrPlayers; Player++ )
		{
			if ( CurrentZone==10+Player )
			{
				strcpy( PlayerName[ Player ], "" );
				DrawScreen( true );
				InitFont( Font );
				SFont_Input(GameScreen, CoordPlayersNamesX, BASE_Y+125+Player*30, TextWidth("            "), PlayerName[ Player ] );
				if ( PlayerName[ Player ][ 0 ]=='\0' )
					sprintf( PlayerName[ Player ], "Player#%d", Player+1 );
			}
		}
	}
	if ( CurrentZone==2 || WhichMouseButton==SDL_BUTTON_WHEELDOWN || WhichMouseButton==SDL_BUTTON_WHEELUP )
	{
		if ( NbrCardsetsInList>0 )
		{
			if ( WhichMouseButton==SDL_BUTTON_LEFT || WhichMouseButton==SDL_BUTTON_WHEELDOWN )
				NumCardsetSelected++;
			if ( WhichMouseButton==SDL_BUTTON_RIGHT || WhichMouseButton==SDL_BUTTON_WHEELUP )
				NumCardsetSelected--;
			if ( NumCardsetSelected<0 )
				NumCardsetSelected = NbrCardsetsInList-1;
			if ( NumCardsetSelected>=NbrCardsetsInList )
				NumCardsetSelected = 0;
		}
	}
	DrawScreen( true );
	return bStartGame;
}

void ClSetup::DrawOneText( bool bNextOne, bool bNoUpdate )
{
	char * msg[] = { "v0.6", "29 december 2005", "(C) 2002-2005 Marc Le Douarain",
			"The game engine is distributed under the terms of the GPL v2",
			"See http://www.multimania.com/mavati/marryampic2 for cardsets",
			"Do not forget to send me your new cardset,", "if you make one !",
			"marc.le-douarain \100 laposte . net",
		"", "", "", NULL };
	SDL_Rect rect;
	rect.x = 0;
	rect.y = BASE_Y+48;
	rect.w = WIDTH;
	rect.h = 20;
	SDL_FillRect(GameScreen, &rect, SETUP_RGB_COLORS);
	InitFont( GameFont );
	PutString( GameScreen, (WIDTH-TextWidth(msg[ NumTextToDraw ]))/2,rect.y, msg[ NumTextToDraw ] );
	if ( !bNoUpdate )
		SDL_UpdateRects( GameScreen, 1, &rect );
	if ( bNextOne )
	{
		NumTextToDraw++;
		if ( msg[ NumTextToDraw ]==NULL )
			NumTextToDraw = 0;
	}
}

bool ClSetup::ReadCardsetsList( void )
{
	DIR *pDir;
	struct dirent *pEnt;
	struct stat file_stat;
	char Buff[ 50 ];

	NbrCardsetsInList = 0;
	pDir = opendir("cards");
	if (pDir)
	{
		while ( ( pEnt = readdir(pDir) ) != NULL && NbrCardsetsInList<NBR_CARDSETS )
		{
			if ( strcmp(pEnt->d_name,".") && strcmp(pEnt->d_name,"..")
				/*&& pEnt->d_type==DT_DIR*/ )
			{
				// DT_DIR not working on every OS, so using stat() instead...
				strcpy( Buff, "cards/" );
				strcat( Buff, pEnt->d_name );
				if ( stat( Buff, &file_stat ) == 0 )
				{
					if ( (file_stat.st_mode & S_IFDIR) != 0 )
					{
						pCardsetList[ NbrCardsetsInList ] = (char *)malloc( strlen(pEnt->d_name)+1 );
						strcpy( pCardsetList[ NbrCardsetsInList ], pEnt->d_name );
						NbrCardsetsInList++;
					}
				}
			}
		}
	}
	closedir(pDir);
	if ( NbrCardsetsInList>1 )
	{
		// do alpha order...
		for( int iBalayA=0; iBalayA<NbrCardsetsInList-1; iBalayA++ )
		{
			for( int iBalayB=iBalayA+1; iBalayB<NbrCardsetsInList; iBalayB++ )
			{
				if ( strcmp( pCardsetList[ iBalayA ], pCardsetList[ iBalayB ] )>0 )
				{
					char * pSwap;
					pSwap = pCardsetList[ iBalayB ];
					pCardsetList[ iBalayB ] = pCardsetList[ iBalayA ];
					pCardsetList[ iBalayA ] = pSwap;
				}
			}
		}
	}
	return NbrCardsetsInList>0;
}

bool ClSetup::TheSetupPage( void )
{
	bool bEnd = false;
	bool bPlay = false;
	GameScreen = SDL_SetVideoMode( WIDTH, HEIGHT, 24, SDL_SWSURFACE | bSetFullScreen?SDL_FULLSCREEN:0 );
	if ( GameScreen==NULL )
		DisplayErrorAndExit( "Failed to set default video mode." );
	
	Uint32 PrevTime = SDL_GetTicks( );

	Font = MenuFont;
	if (!ReadCardsetsList( ))
	{
		DisplayErrorAndExit( "No cardsets found in the cards directory or no cards directory" );
		bEnd = true;
	}
	else
	{
		NumTextToDraw = 0;
		CurrentZone = 0;
		CurrentZoneBak = -1;
		ObjBoard.bMakeTheTest = false;
		DrawScreen( true );
		SDL_Event event;
		do
		{
			CurrentZone = 0;
			DrawScreen( false );
			if ( CurrentZone!=CurrentZoneBak )
				DrawScreen( true );
			CurrentZoneBak = CurrentZone;
			while( SDL_PollEvent( &event ) )
			{
				switch ( event.type )
				{
					case SDL_MOUSEBUTTONDOWN:
						if ( Click( event.button.button ) )
							bPlay = true;
						break;
					case SDL_KEYDOWN:
						if ( event.key.keysym.sym==SDLK_ESCAPE )
							bEnd = true;
						if ( event.key.keysym.sym==SDLK_SPACE )
						{
							bPlay = true;
							ObjBoard.bMakeTheTest = true;
						}
						if ( event.key.keysym.sym==SDLK_f )
						{
							bSetFullScreen = bSetFullScreen?false:true;
							GameScreen = SDL_SetVideoMode( WIDTH, HEIGHT, 24, SDL_SWSURFACE | bSetFullScreen?SDL_FULLSCREEN:0 );
							if ( GameScreen==NULL )
								DisplayErrorAndExit( "Failed to set default video mode." );
							DrawScreen( true );
						}
						break;
					case SDL_QUIT:
						bEnd = true;
						break;
				}
			}
			SDL_Delay( 30 );
			Uint32 CurrTime = SDL_GetTicks( );
			if ( (CurrTime-PrevTime)>2000 )
			{
				DrawOneText( true /*bNextOne*/, false /*bNoUpdate*/ );
				PrevTime = CurrTime;
			}
		}
		while( !bEnd && !bPlay );
		if ( NbrCardsetsInList>0 )
			sprintf( ObjBoard.CardsetDirectory, "cards/%s", pCardsetList[ NumCardsetSelected ] );
		for( int iBalayCard=0; iBalayCard<NBR_CARDSETS; iBalayCard++)
		{
			if ( pCardsetList[ iBalayCard ] )
			{
				free( pCardsetList[ iBalayCard ] );
				pCardsetList[ iBalayCard ] = NULL;
			}
		}
	}
	return bEnd;
}

ClBoard::ClBoard( )
{
	SoundCard = NULL;
	BackgroundImage = NULL;
	bMakeTheTest = false;
}
ClBoard::~ClBoard( )
{
	if ( SoundCard )
		Mix_FreeChunk( SoundCard );
	if ( CardsImage )
		SDL_FreeSurface( CardsImage );
	if ( BackgroundImage )
		SDL_FreeSurface( BackgroundImage );
}

bool ClBoard::LoadCardpic( char * CardsetToLoad )
{
	bool bLoadingOk = false;
	char Buffer[ 500 ];
	char * FileToLoad[ ] = { "cardpic.png", "cardpic.jpg",
#ifdef ILBM_24BITS_SUPPORT
	"cardpic_24bits.iff",
#endif
	"cardpic_aga.iff", "cardpic.iff", NULL };
	int ScanPic = 0;
	if ( CardsImage )
		SDL_FreeSurface( CardsImage );
	if ( BackgroundImage )
		SDL_FreeSurface( BackgroundImage );
	sprintf( Buffer, "%s/%s", CardsetToLoad, "picbackground.png" );
	BackgroundImage = IMG_Load( Buffer );
	if ( BackgroundImage==NULL )
	{
		sprintf( Buffer, "%s/%s", CardsetToLoad, "picbackground.jpeg" );
		BackgroundImage = IMG_Load( Buffer );
	}
	
	do
	{
		sprintf( Buffer, "%s/%s", CardsetToLoad, FileToLoad[ ScanPic ] );
		CardsImage = IMG_Load( Buffer );
		if ( CardsImage!=NULL )
		{
			bLoadingOk = true;
		}
		ScanPic++;
	}
	while( !bLoadingOk && FileToLoad[ ScanPic ]!=NULL );
	return bLoadingOk;
}

bool ClBoard::LoadInfos( char * CardsetToLoad )
{
	char Buffer[ 500 ];
	FILE * FileInfos;
	bool bLoadingOk = false;
	char Line[100];
	char * LineOk;
	CardWidth = -1;
	CardHeight = -1;
	NbrCardsX = -1;
	NbrCardsY = -1;
	SpacingX = -1;
	SpacingY = -1;
	DisplayWidth = WIDTH;
	DisplayHeight = HEIGHT;
	sprintf( Buffer, "%s/cardinfo.txt", CardsetToLoad );
	FileInfos = fopen( Buffer, "rt" );
	if ( FileInfos )
	{
	        do
        	{
			LineOk = fgets(Line,100,FileInfos);
			if (LineOk)
			{
				strcpy( Buffer,"WIDTH=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					CardWidth = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"HEIGHT=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					CardHeight = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"NUMBER_X=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					NbrCardsX = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"NUMBER_Y=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					NbrCardsY = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"SPACING_X=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					SpacingX = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"SPACING_Y=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					SpacingY = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"DISPLAY_WIDTH=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					DisplayWidth = atoi(&Line[strlen(Buffer)]);
				strcpy( Buffer,"DISPLAY_HEIGHT=" );
				if(strncmp(Line,Buffer,strlen(Buffer))==0)
					DisplayHeight = atoi(&Line[strlen(Buffer)]);
			}
		}
		while(LineOk);
		fclose( FileInfos );
		if ( CardWidth<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line WIDTH=" );
		}
		else if ( CardHeight<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line HEIGHT=" );
		}
		else if ( NbrCardsX<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line NUMBER_X=" );
		}
		else if ( NbrCardsY<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line NUMBER_Y=" );
		}
		else if ( SpacingX<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line SPACING_X=" );
		}
		else if ( SpacingY<0 )
		{
			DisplayErrorAndExit( "Error in file CardInfo.txt ; missing line SPACING_Y=" );
		}
		else
		{
			bLoadingOk = true;
		}
	}
	return bLoadingOk;
}

bool ClBoard::ToLowerCase( char * text )
{
	bool bDone = false;
	while( *text!='\0' )
	{
		if ( *text>='A' && *text<='Z' )
		{
			*text = *text+('a'-'A');
			bDone = true;
		}
		text++;
	}
	return bDone;
}

void ClBoard::RenameLowerCase( char * CardsetToLoad )
{
	DIR *pDir;
	struct dirent *pEnt;
	char BufferNew[ 500 ];
	char BufferOld[ 500 ];
	char Name[ 50 ];

	pDir = opendir(CardsetToLoad);
	if (pDir)
	{
		while ( ( pEnt = readdir(pDir) ) != NULL )
		{
			if ( strcmp(pEnt->d_name,".") && strcmp(pEnt->d_name,"..") )
			{
				strcpy( Name, pEnt->d_name );
				if ( ToLowerCase( Name ) )
				{
					sprintf( BufferNew, "%s/%s", CardsetToLoad, Name ) ;
					sprintf( BufferOld, "%s/%s", CardsetToLoad, pEnt->d_name ) ;
					if ( rename( BufferOld, BufferNew )<0 )
						DisplayErrorAndExit( "Failed to rename in lower case a file in the cardset directory" );
				}
			}
		}
	}
	closedir(pDir);
}

bool ClBoard::LoadDatas( )
{
	bool bOk;
	RenameLowerCase( CardsetDirectory );
	bOk = LoadInfos( CardsetDirectory );
	if ( bOk )
	{
		BoardWidth = DisplayWidth;
		BoardHeight = DisplayHeight;
		BoardMessagesHeight = 20;
		BoardSpacingX = (BoardWidth-(CardWidth*NbrCardsX))/(NbrCardsX+1);
		BoardSpacingY = (BoardHeight-BoardMessagesHeight-(CardHeight*NbrCardsY*2))/(NbrCardsY*2+1);
		NbrPairs = NbrCardsX*NbrCardsY;
		bOk = LoadCardpic( CardsetDirectory );
		if ( !bOk )
			DisplayErrorAndExit( "Failed to load a picture file in the cardset directory" );
	}
	else
	{
		DisplayErrorAndExit( "Failed to load file CardInfo.txt in the cardset directory" );
	}
	return bOk;
}

bool ClBoard::VerifyAbort( void )
{
	bool bAbort = false;
	SDL_Event event;
	if ( SDL_PollEvent( &event ) )
	{
		switch ( event.type )
		{
			case SDL_KEYDOWN:
				if ( event.key.keysym.sym==SDLK_ESCAPE )
					bAbort = true;
				break;
		}
	}
	return bAbort;
}

void ClBoard::DisplayMessage( char * text )
{
	SDL_Rect rect;
	rect.x = 0;
	rect.y = BoardHeight-BoardMessagesHeight;
	rect.w = BoardWidth;
	rect.h = BoardMessagesHeight;
	SDL_FillRect(GameScreen, &rect, SDL_MapRGB(GameScreen->format, 30, 30, 30));
	PutString(GameScreen,(BoardWidth-TextWidth(text))/2,BoardHeight-BoardMessagesHeight, text);
//	SDL_UpdateRect(GameScreen, 0, BoardHeight-BoardMessagesHeight, 0, BoardMessagesHeight);
	SDL_UpdateRects( GameScreen, 1, &rect );
}

void ClBoard::DisplayPlayerMessage( int NumPlayer, bool bWin )
{
	char BuffPlayer[ 50 ];
	if ( bWin )
	{
		sprintf( BuffPlayer, "Well done, %s !", ObjSetup.PlayerName[ NumPlayer ] );
	}
	else
	{
		if ( ObjSetup.RuleOnlySoundListened )
			sprintf( BuffPlayer, "%s, find the pair.", ObjSetup.PlayerName[ NumPlayer ] );
		else
			sprintf( BuffPlayer, "%s, find a pair.", ObjSetup.PlayerName[ NumPlayer ] );
	}
	DisplayMessage( BuffPlayer );
}

void ClBoard::CleanCard( int PosiX, int PosiY )
{
	int x = PosiX*(CardWidth+BoardSpacingX)+BoardSpacingX;
	int y = PosiY*(CardHeight+BoardSpacingY)+BoardSpacingY;
	int CoordX,CoordY;
	if ( y<0 )
		y = 0;
	if ( x<0 )
		x = 0;
	if ( BackgroundImage==NULL )
	{
		for( int Effect=0; Effect<2; Effect++ )
		{
			for( int ScanY=y; ScanY<y+CardHeight; ScanY=ScanY+2 )
			{
				for( int ScanX=x+Effect-2; ScanX<x+CardWidth+2; ScanX=ScanX+2 )
				{
					if ( ScanX>=0 && ScanX<DisplayWidth-1 && ScanY>=0 && ScanY<DisplayHeight-BoardMessagesHeight )
					{
						Uint8 * bufp;
						CoordX = ScanX;
						CoordY = ScanY;
						bufp = (Uint8 *)GameScreen->pixels+CoordY*GameScreen->pitch+CoordX*3;
						*bufp++ = 30;
						*bufp++ = 30;
						*bufp++ = 30;
						CoordX = ScanX+1;
						CoordY = ScanY+1;
						bufp = (Uint8 *)GameScreen->pixels+CoordY*GameScreen->pitch+CoordX*3;
						*bufp++ = 30;
						*bufp++ = 30;
						*bufp++ = 30;
					}
				}
				SDL_UpdateRect( GameScreen, x, ScanY, CardWidth, 2 );
			}
			SDL_Delay( 200 );
		}
	}
	else
	{
		for( int Effect=0; Effect<2; Effect++ )
		{
			for( int ScanY=y+Effect; ScanY<y+CardHeight; ScanY=ScanY+2 )
			{
				if ( ScanY<DisplayHeight-BoardMessagesHeight )
				{
					SDL_Rect rect;
					rect.x = x;
					rect.y = ScanY;
					rect.w = CardWidth;
					rect.h = 1;
					SDL_BlitSurface( BackgroundImage, &rect, GameScreen, &rect );
					SDL_UpdateRects( GameScreen, 1, &rect );
				}
			}
			SDL_Delay( 200 );
		}
	}
}

void ClBoard::DrawCard( int NumCard, int PosiX, int PosiY )
{
	if ( NumCard==CARD_BLANK )
	{
		CleanCard( PosiX, PosiY );
	}
	else
	{
		int SrcCardX = 0;
		int SrcCardY = 0;
		SDL_Rect src,dest;
		if ( NumCard==CARD_BACKDROP )
		{
			/* backdrop card position in source picture */
			SrcCardX = 0;
			SrcCardY = NbrCardsY;
		}
		else
		{
			/* search position card in source picture */
			SrcCardX = NumCard%NbrCardsX;
			SrcCardY = NumCard/NbrCardsX;
		}
		src.x = SrcCardX*(CardWidth+SpacingX);
		src.y = SrcCardY*(CardHeight+SpacingY);
		src.w = CardWidth;
		src.h = CardHeight;
		dest.x = PosiX*(CardWidth+BoardSpacingX)+BoardSpacingX;
		dest.y = PosiY*(CardHeight+BoardSpacingY)+BoardSpacingY;
		dest.w = CardWidth;
		dest.h = CardHeight;
		SDL_BlitSurface( CardsImage, &src, GameScreen, &dest );
		SDL_UpdateRects( GameScreen, 1, &dest );
	}
}

void ClBoard::DrawCards( bool bHideCards )
{
	if ( bHideCards )
		DisplayMessage( "Displaying the game..." );
	else
		DisplayMessage( "Displaying the preview..." );
	for ( int ScanY=0; ScanY<NbrCardsY*2; ScanY++ )
	{
		for ( int ScanX=0; ScanX<NbrCardsX; ScanX++ )
		{
			if ( bHideCards )
				DrawCard( CARD_BACKDROP, ScanX, ScanY );
			else
				DrawCard( CardNumber[ ScanX ][ ScanY ], ScanX, ScanY );
			SDL_Delay( 50 );
		}
	}
}

void ClBoard::PlaySoundCard( int NumCard )
{
	char Buffer[ 500 ];
	char BufferFinal[ 500 ];
	if ( NumCard==SAMPLE_FOUND )
		sprintf( Buffer, "%s/samplefound", CardsetDirectory );
	else if ( NumCard==SAMPLE_FINISHED )
		sprintf( Buffer, "%s/samplefinished", CardsetDirectory );
	else
		sprintf( Buffer, "%s/cardsample-%d", CardsetDirectory, NumCard+1 );
	if ( SoundCard )
	{
		Mix_HaltChannel(-1);
		Mix_FreeChunk( SoundCard );
	}
	// try to load .wav, and then .iff
	sprintf( BufferFinal, "%s.wav", Buffer );
// For the moment, use my function with IFF 8SVX support instead of one from SDL_mixer
//	SoundCard = Mix_LoadWAV( Buffer );
	SoundCard = My_LoadWAV( BufferFinal );
	if ( SoundCard==NULL )
	{
		sprintf( BufferFinal, "%s.iff", Buffer );
		SoundCard = My_LoadWAV( BufferFinal );
	}
	if ( SoundCard )
		Mix_PlayChannel(-1, SoundCard, 0);
	else
		printf("Failed to load sound file %s : %s\n", Buffer, SDL_GetError() );
}

void ClBoard::ListenASound( void )
{
	srand( SDL_GetTicks() );
	if ( NumPairListenedToFound==-1 || !ObjSetup.RuleKeepSameSound )
	{
		do
		{
			NumPairListenedToFound = rand()%NbrPairs;
		}
		while( PairFound[ NumPairListenedToFound ] );
		DisplayMessage( "Listen !" );
		PlaySoundCard( NumPairListenedToFound );
		SDL_Delay( 1*1000 );
	}
}

void ClBoard::ClearScreen( bool DrawBackground )
{
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = DisplayWidth;
	rect.h = DisplayHeight;
	SDL_FillRect( GameScreen, &rect, SDL_MapRGB(GameScreen->format, 30, 30, 30) );
	if ( DrawBackground && BackgroundImage )
		SDL_BlitSurface( BackgroundImage, &rect, GameScreen, &rect );
	SDL_UpdateRects( GameScreen, 1, &rect );
}

void ClBoard::InitGame( void )
{
	GameScreen = SDL_SetVideoMode( DisplayWidth, DisplayHeight, 24, SDL_SWSURFACE | bSetFullScreen?SDL_FULLSCREEN:0 );
	if ( GameScreen==NULL )
		DisplayErrorAndExit( "Failed to set video mode found from CardInfo.txt in the cardset directory" );
	
	ClearScreen( true/*DrawBackground*/ );
	InitFont(GameFont);

	int RawNumber[ NBR_PAIRS*2 ];
	for( int ScanPair=0; ScanPair<NBR_PAIRS; ScanPair++ )
		PairFound[ ScanPair ] = false;
	int ScanCard = 0;
	for( int ScanGene=0; ScanGene<2; ScanGene++ )
	{
		for( int NumPair=0; NumPair<NbrPairs; NumPair++ )
			RawNumber[ ScanCard++ ] = NumPair;
	}
	srand( SDL_GetTicks() );
	for( int ScanY=0; ScanY<NbrCardsY*2; ScanY++ )
	{
		for( int ScanX=0; ScanX<NbrCardsX; ScanX++ )
		{
			int RndInRaw;
			do
			{
				RndInRaw = rand()%(2*NbrPairs);
			}
			while( RawNumber[ RndInRaw ]==-1 );
			CardNumber[ ScanX ][ ScanY ] = RawNumber[ RndInRaw ];
			RawNumber[ RndInRaw ] = -1;
		}
	}
	PrevCardX = -1;
	PrevCardY = -1;
	FoundCardX[ 0 ] = -1;
	bFinished = false;
	NumPairListenedToFound = -1;
	PlayerNow = rand()%ObjSetup.NbrPlayers;
}

void ClBoard::CardClicked( int PosiX, int PosiY )
{
	int NumPairClicked = CardNumber[ PosiX ][ PosiY ];
	DrawCard( NumPairClicked, PosiX, PosiY );
	if ( PrevCardX==-1 && PrevCardY==-1 )
	{
		PrevCardX = PosiX;
		PrevCardY = PosiY;
	}
	else
	{
		ObjScores.AddPlayed( PlayerNow );
		bool bIsAPair = false;
		bool bIsTheSoundPair = false;
		if ( NumPairClicked==CardNumber[ PrevCardX ][ PrevCardY ] )
		{
			bIsAPair = true;
			if ( ObjSetup.RuleOnlySoundListened )
			{
				if ( NumPairClicked==NumPairListenedToFound )
					bIsTheSoundPair = true;
			}
		}

		/* a valid pair found ? (depending the rules selected) */
		if ( ( !ObjSetup.RuleOnlySoundListened && bIsAPair )
			|| ( ObjSetup.RuleOnlySoundListened && bIsAPair && bIsTheSoundPair ) )
		{
			CardNumber[ PosiX ][ PosiY ] = -1;
			CardNumber[ PrevCardX ][ PrevCardY ] = -1;
			PairFound[ NumPairClicked ] = true;
			DisplayPlayerMessage( PlayerNow, true );
			if ( !ObjSetup.RuleOnlySoundListened )
				PlaySoundCard( NumPairClicked );
			else
				PlaySoundCard( SAMPLE_FOUND );
			ObjScores.AddFound( PlayerNow );
			NumPairListenedToFound = -1;
//			SDL_Delay( 1000 );
//			DrawCard( CARD_BLANK, PosiX, PosiY );
//			DrawCard( CARD_BLANK, PrevCardX, PrevCardY );
			if ( FoundCardX[ 0 ]!=-1 )
			{
				DrawCard( CARD_BLANK, FoundCardX[ 0 ], FoundCardY[ 0 ] );
				DrawCard( CARD_BLANK, FoundCardX[ 1 ], FoundCardY[ 1 ] );
				FoundCardX[ 0 ] = -1;
			}
			else
			{
				SDL_Delay( 500 );
			}
			/* to clean this pair found later... */
			FoundCardX[ 0 ] = PosiX;
			FoundCardX[ 1 ] = PrevCardX;
			FoundCardY[ 0 ] = PosiY;
			FoundCardY[ 1 ] = PrevCardY;
			/* verify if all pairs found => end of game */
			bool bAllFound = true;
			for( int ScanPair=0; ScanPair<NbrPairs; ScanPair++ )
			{
				if ( !PairFound[ ScanPair ] )
					bAllFound = false;
			}
			if ( bAllFound )
				bFinished = true;
		}
		else
		{
			if ( ObjSetup.RuleOnlySoundListened && bIsAPair && !bIsTheSoundPair )
				DisplayMessage( "This is not the good sound-pair !!!" );
			else
				DisplayMessage( "This is not a pair..." );
			SDL_Delay( 1000 );
			DrawCard( CARD_BACKDROP, PosiX, PosiY );
			DrawCard( CARD_BACKDROP, PrevCardX, PrevCardY );
			PlayerNow = (PlayerNow+1)%ObjSetup.NbrPlayers;
		}
		PrevCardX = -1;
		PrevCardY = -1;
		if ( !bFinished )
		{
			if ( ObjSetup.RuleOnlySoundListened )
				ListenASound( );
			DisplayPlayerMessage( PlayerNow, false );
		}
	}
}

void ClBoard::ClickedAt( int ClickX, int ClickY )
{
	if ( ClickX>=BoardSpacingX && ClickY>=BoardSpacingY )
	{
		ClickX -= BoardSpacingX;
		ClickY -= BoardSpacingY;
		int PosiX = ClickX/(CardWidth+BoardSpacingX);
		int PosiY = ClickY/(CardHeight+BoardSpacingY);
		if ( ClickX-PosiX*(CardWidth+BoardSpacingX)<CardWidth && ClickY-PosiY*(CardHeight+BoardSpacingY)<CardHeight )
		{
			if ( !(PosiX==PrevCardX && PosiY==PrevCardY) && PosiX<NbrCardsX && PosiY<NbrCardsY*2)
			{
				if ( CardNumber[ PosiX ][ PosiY ]!=-1 )
					CardClicked( PosiX, PosiY );
			}
		}
	}
}

bool ClBoard::MouseWait( void )
{
	bool bExit = false;
	bool bAbort = false;
	SDL_Event event;
	// flush events before new one!
	while( SDL_PollEvent( &event ) );
	
	do
	{
		SDL_WaitEvent( &event );
		switch ( event.type )
		{
			case SDL_MOUSEBUTTONDOWN:
				bExit = true;
				break;
			case SDL_KEYDOWN:
				if ( event.key.keysym.sym==SDLK_ESCAPE )
				{
					bExit = true;
					bAbort = true;
				}
				break;
			case SDL_QUIT:
				bExit = true;
				bAbort = true;
				break;
		}
	}
	while( !bExit );
	return bAbort;
}

void ClBoard::TheGame( void )
{
	bool bAbort = false;
	InitGame( );
	ObjScores.InitScores( );
	if ( bMakeTheTest )
		TestMode( );
	/* show the cards */
	if ( ObjSetup.ShowCardsDuringSecs>0 )
	{
		char BufMsg[ 20 ];
		DrawCards( false );
		int DelayToShow = ObjSetup.ShowCardsDuringSecs*1000;
		Uint32 StartTime = SDL_GetTicks( );
		Uint32 EndTime = StartTime + DelayToShow;
		Uint32 CurrTime;
		do
		{
			CurrTime = SDL_GetTicks();
			int Disp = DelayToShow-(CurrTime-StartTime);
			sprintf( BufMsg, "< %d >", Disp>0?Disp/1000+1:0 );
			DisplayMessage( BufMsg );
			SDL_Delay( 10 );
		}
		while( CurrTime<EndTime && !VerifyAbort( ) );
	}

	DrawCards( true );
	if ( ObjSetup.RuleOnlySoundListened )
		ListenASound( );
	DisplayPlayerMessage( PlayerNow, false );

	SDL_Event event;
	do
	{
		// flush events before new one!
		while( SDL_PollEvent( &event ) );
		SDL_WaitEvent( &event );
		switch ( event.type )
		{
			case SDL_MOUSEBUTTONDOWN:
				ClickedAt( event.button.x, event.button.y );
				break;
			case SDL_KEYDOWN:
				if ( event.key.keysym.sym==SDLK_ESCAPE )
					bAbort = true;
				if ( event.key.keysym.sym==SDLK_SPACE && ObjSetup.RuleOnlySoundListened )
					PlaySoundCard( NumPairListenedToFound );
				break;
			case SDL_QUIT:
				bAbort = true;
				break;
		}
	}
	while( !bFinished && !bAbort );
	if ( bFinished )
	{
		SDL_Delay( 1000 );
		DisplayMessage("Click to see results...");
		MouseWait( );
		PlaySoundCard( SAMPLE_FINISHED );
		DisplayMessage("");
		ObjScores.ShowScores( CardsetDirectory, DisplayWidth, DisplayHeight );
		SDL_Delay( 3000 );
		MouseWait( );
	}
}

void ClBoard::TestMode( void )
{
	char BuffNumPair[ 100 ];
	int iPair = 0;
	bool bAbortTest = false;
	do
	{
		sprintf( BuffNumPair, "Test Mode : Picture & Sound Pair # %d", iPair+1 );
		DisplayMessage( BuffNumPair );
		DrawCard( iPair, NbrCardsX/2, NbrCardsY );
		PlaySoundCard( iPair );
		if ( MouseWait( ) )
			bAbortTest = true;
		iPair++;
	}
	while( iPair<NbrPairs && !bAbortTest );
	ClearScreen( true /*DrawBackground*/ );
}

ClScores::ClScores( )
{
	InitScores( );
}
void ClScores::InitScores( void )
{
	for ( int Player=0; Player<NBR_PLAYERS; Player++ )
	{
		Found[ Player ] = 0;
		Played[ Player ] = 0;
	}
}
void ClScores::AddFound( int NumPlayer )
{
	if ( NumPlayer>=0 && NumPlayer<NBR_PLAYERS-1 )
		Found[ NumPlayer ]++;
}
void ClScores::AddPlayed( int NumPlayer )
{
	if ( NumPlayer>=0 && NumPlayer<NBR_PLAYERS-1 )
		Played[ NumPlayer ]++;
}
void ClScores::DisplayImage( SDL_Surface * Image, int Width, int Height )
{
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = Width;
	rect.h = Height;
	SDL_BlitSurface( Image, &rect, GameScreen, &rect );
	SDL_UpdateRects( GameScreen, 1, &rect );
}
void ClScores::ShowScores( char * Path, int DispWidth, int DispHeight )
{
	char Buff[ 500 ];
	SDL_Surface * EndImage;
	int Winner = -1;
	if ( ObjSetup.NbrPlayers>1 )
	{
		int BestScore = 0;
		for( int Player=0; Player<ObjSetup.NbrPlayers; Player++ )
		{
			if (Found[ Player ]==BestScore )
				Winner = -1;
			if (Found[ Player ]>BestScore )
			{
				BestScore = Found[ Player ];
				Winner = Player;
			}
		}
	}
	ObjBoard.ClearScreen( false /*DrawBackground*/ );

	sprintf( Buff, "%s/%s", Path, "picfinished.png" );
	EndImage = IMG_Load( Buff );
	if ( EndImage==NULL )
	{
		sprintf( Buff, "%s/%s", Path, "picfinished.jpeg" );
		EndImage = IMG_Load( Buff );
	}
	if ( EndImage )
	{
		DisplayImage( EndImage, DispWidth, DispHeight );
		SDL_FreeSurface( EndImage );
	}
	
	InitFont( MenuFont );
	strcpy( Buff, "RESULTS :" );
	int PosiBaseX = (DispWidth-TextWidth(Buff))/2;
	PutString( GameScreen, PosiBaseX ,((DispHeight-HEIGHT)/2)+100, Buff );
	PosiBaseX = PosiBaseX-180;
	for( int Player=-1; Player<ObjSetup.NbrPlayers; Player++ )
	{
		int PosiY = ((DispHeight-HEIGHT)/2)+160 + (Player+1)*30;
		if ( Player>=0 )
		{
			PutString( GameScreen, PosiBaseX, PosiY, ObjSetup.PlayerName[ Player ] );
			if ( Winner==Player )
				PutString( GameScreen, PosiBaseX+1, PosiY+1, ObjSetup.PlayerName[ Player ] );
			sprintf( Buff, "%d", Found[ Player ] );
			PutString( GameScreen, PosiBaseX+210+30, PosiY, Buff );
			sprintf( Buff, "%d", Played[ Player ] );
			PutString( GameScreen, PosiBaseX+360+50, PosiY, Buff );

		}
		else
		{
			PutString( GameScreen, PosiBaseX, PosiY-10, "Name" );
			PutString( GameScreen, PosiBaseX+210, PosiY-10, "Found" );
			PutString( GameScreen, PosiBaseX+360, PosiY-10, "Attempts" );
		}
	}
	SDL_UpdateRect( GameScreen, 0, 0, DispWidth, DispHeight );
}

int main( int argc, char *argv[] )
{
    MorphToPM(); // Morph the VIO application to a PM one to be able to use Win* functions
	if ( argc>1 )
	{
		if ( strcmp( argv[ 1 ], "--fullscreen" )==0 || strcmp( argv[ 1 ], "-f" )==0 )
			bSetFullScreen = true;
	}

	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
	{
		fprintf( stderr, "Unable to init SDL: %s\n", SDL_GetError() );
		exit( 1 );
	}
	atexit( SDL_Quit );
	GameFont = IMG_Load("datas/SmallNeon.png");
	if ( GameFont==NULL )
	{
		fprintf( stderr, "Unable to load font file: %s\n", SDL_GetError() );
		exit( 1 );
	}
	MenuFont = IMG_Load("datas/NeonFont.png");
	if ( MenuFont==NULL )
	{
		fprintf( stderr, "Unable to load font file: %s\n", SDL_GetError() );
		exit( 1 );
	}
	GameScreen = SDL_SetVideoMode( WIDTH, HEIGHT, 24, SDL_SWSURFACE | bSetFullScreen?SDL_FULLSCREEN:0 );
	if ( GameScreen==NULL )
	{
		if ( bSetFullScreen )
			fprintf( stderr, "Unable to set 640x480 fullscreen video: %s\n", SDL_GetError() );
		else
			fprintf( stderr, "Unable to set 640x480 video: %s\n", SDL_GetError() );
		exit( 1 );
	}
	SDL_WM_SetCaption("Marryampic2", "Marryampic2");
	if (Mix_OpenAudio(AUDIO_FREQ, AUDIO_FORMAT, AUDIO_CHANNELS, AUDIO_SIZE) < 0)
		DisplayErrorAndExit( "Unable to setup audio..." );

	while ( !ObjSetup.TheSetupPage( ) )
	{
		if ( ObjBoard.LoadDatas( ) )
			ObjBoard.TheGame( );
	}

	Mix_CloseAudio( );
	SDL_FreeSurface( GameFont );
	SDL_FreeSurface( MenuFont );
}

void DisplayErrorAndExit( char * ErrorMsg )
{
	bool bExit = false;
	if ( GameScreen==NULL )
		GameScreen = SDL_SetVideoMode( WIDTH, HEIGHT, 24, SDL_SWSURFACE | bSetFullScreen?SDL_FULLSCREEN:0 );
	if ( GameScreen )
	{
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = WIDTH;
		rect.h = HEIGHT;
		SDL_FillRect( GameScreen, &rect, SDL_MapRGB(GameScreen->format, 70, 0, 0) );
		InitFont(GameFont);
		PutString( GameScreen, 0, 0, "Marryampic error : " );
		PutString( GameScreen, 0, 20, ErrorMsg );
		SDL_UpdateRects( GameScreen, 1, &rect );
	
		SDL_Event event;
		// flush events before new one!
		while( SDL_PollEvent( &event ) );
		do
		{
			SDL_WaitEvent( &event );
			switch ( event.type )
			{
				case SDL_MOUSEBUTTONDOWN:
					bExit = true;
					break;
				case SDL_KEYDOWN:
					if ( event.key.keysym.sym==SDLK_ESCAPE )
						bExit = true;
					break;
				case SDL_QUIT:
					bExit = true;
					break;
			}
		}
		while( !bExit );
	}
	else
	{
		printf( "Marryampic error:\n" );
		printf( "%s:\n", ErrorMsg );
	}

	Mix_CloseAudio( );
	SDL_FreeSurface( GameFont );
	SDL_FreeSurface( MenuFont );
	exit( 1 );
}


