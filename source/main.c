#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cjson.h"

#define BASE    "http://188.115.125.74:8090"
#define API_URL BASE "/api/collections/songs/records?perPage=100&sort=-created"
#define SW 1280
#define SH  720
#define MAX 200
#define ITEM_H 75
#define VISIBLE 9
#define LIST_W  740
#define PANEL_X 742

typedef struct { char *d; size_t n; } Buf;
typedef struct {
    char id[24], title[128], author[64], audio[128], cover[128], genre[48];
    int  duration, plays, likes;
    bool has_audio;
} Song;

static Song      songs[MAX];
static int       count=0, sel=0, scroll=0, now_idx=-1;
static Buf       audio_buf={0};
static Mix_Music *music=NULL;
static SDL_RWops *music_rw=NULL;
static SDL_Texture *cover_tex=NULL;
static SDL_Renderer *ren;
static TTF_Font *fBig, *fMed, *fSm;

static SDL_Color cTEXT  ={255,255,255,255};
static SDL_Color cSUB   ={160,160,160,255};
static SDL_Color cACCENT={220, 70, 45,255};
static SDL_Color cDIM   ={100, 65, 65,255};

/* ── curl ── */
static size_t wrcb(char *p, size_t s, size_t n, void *u){
    Buf *b=(Buf*)u; size_t t=s*n;
    b->d=realloc(b->d,b->n+t+1);
    memcpy(b->d+b->n,p,t); b->n+=t; b->d[b->n]=0;
    return t;
}
static Buf net_get(const char *url, int timeout){
    Buf b={0}; CURL *c=curl_easy_init(); if(!c)return b;
    curl_easy_setopt(c,CURLOPT_URL,url);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wrcb);
    curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,(long)timeout);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_perform(c); curl_easy_cleanup(c);
    return b;
}

/* ── load songs ── */
static void load_songs(void){
    Buf b=net_get(API_URL,15); if(!b.d)return;
    cJSON *root=cJSON_Parse(b.d); free(b.d); if(!root)return;
    cJSON *items=cJSON_GetObjectItem(root,"items");
    int total=cJSON_GetArraySize(items);
    for(int i=0;i<total&&count<MAX;i++){
        cJSON *it=cJSON_GetArrayItem(items,i);
        Song *s=&songs[count];
#define GS(f,dst,sz) do{cJSON *v=cJSON_GetObjectItem(it,f);if(v&&v->valuestring)strncpy(dst,v->valuestring,sz-1);}while(0)
#define GN(f,dst)    do{cJSON *v=cJSON_GetObjectItem(it,f);if(v)dst=v->valueint;}while(0)
        GS("id",s->id,24); GS("title",s->title,128); GS("author",s->author,64);
        GS("audio",s->audio,128); GS("cover",s->cover,128); GS("genre",s->genre,48);
        GN("duration",s->duration); GN("play_count",s->plays); GN("likes_count",s->likes);
        s->has_audio=(s->audio[0]!='\0');
        count++;
    }
    cJSON_Delete(root);
}

/* ── audio ── */
static void stop_audio(void){
    if(music){Mix_HaltMusic();Mix_FreeMusic(music);music=NULL;}
    if(music_rw){SDL_RWclose(music_rw);music_rw=NULL;}
    if(audio_buf.d){free(audio_buf.d);audio_buf.d=NULL;audio_buf.n=0;}
    if(cover_tex){SDL_DestroyTexture(cover_tex);cover_tex=NULL;}
}
static void play_song(int idx){
    stop_audio();
    if(idx<0||idx>=count||!songs[idx].has_audio)return;
    Song *s=&songs[idx];
    char url[512];
    snprintf(url,sizeof(url),"%s/api/files/songs/%s/%s",BASE,s->id,s->audio);
    audio_buf=net_get(url,60);
    if(!audio_buf.d||!audio_buf.n)return;
    music_rw=SDL_RWFromMem(audio_buf.d,(int)audio_buf.n);
    if(!music_rw)return;
    music=Mix_LoadMUS_RW(music_rw,0);
    if(!music)return;
    Mix_PlayMusic(music,1);
    now_idx=idx;
    if(s->cover[0]){
        snprintf(url,sizeof(url),"%s/api/files/songs/%s/%s",BASE,s->id,s->cover);
        Buf cb=net_get(url,10);
        if(cb.d&&cb.n){
            SDL_RWops *rw=SDL_RWFromMem(cb.d,(int)cb.n);
            SDL_Surface *sf=IMG_Load_RW(rw,1);
            if(sf){cover_tex=SDL_CreateTextureFromSurface(ren,sf);SDL_FreeSurface(sf);}
            free(cb.d);
        }
    }
}

/* ── render helpers ── */
static void fill(int x,int y,int w,int h,Uint8 r,Uint8 g,Uint8 b){
    SDL_SetRenderDrawColor(ren,r,g,b,255);
    SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(ren,&rc);
}
static void txt(TTF_Font *f,const char *t,int x,int y,SDL_Color c){
    if(!t||!*t)return;
    SDL_Surface *s=TTF_RenderUTF8_Blended(f,t,c); if(!s)return;
    SDL_Texture *tx=SDL_CreateTextureFromSurface(ren,s); SDL_FreeSurface(s);
    int w,h; SDL_QueryTexture(tx,NULL,NULL,&w,&h);
    SDL_Rect d={x,y,w,h}; SDL_RenderCopy(ren,tx,NULL,&d); SDL_DestroyTexture(tx);
}
static void txt_w(TTF_Font *f,const char *t,int x,int y,int mw,SDL_Color c){
    if(!t||!*t)return;
    SDL_Surface *s=TTF_RenderUTF8_Blended(f,t,c); if(!s)return;
    SDL_Texture *tx=SDL_CreateTextureFromSurface(ren,s); SDL_FreeSurface(s);
    int w,h; SDL_QueryTexture(tx,NULL,NULL,&w,&h);
    if(w>mw)w=mw;
    SDL_Rect src={0,0,w,h},dst={x,y,w,h};
    SDL_RenderCopy(ren,tx,&src,&dst); SDL_DestroyTexture(tx);
}
static const char* fmtd(int s){
    static char b[12];
    if(s<=0)return "--:--";
    snprintf(b,sizeof(b),"%d:%02d",s/60,s%60);
    return b;
}

/* ── main ── */
int main(int argc,char *argv[]){
    socketInitializeDefault();
    plInitialize(PlServiceType_User);

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK);
    IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
    TTF_Init();
    Mix_Init(MIX_INIT_MP3|MIX_INIT_OGG|MIX_INIT_FLAC);
    Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,4096);

    SDL_Window *win=SDL_CreateWindow("Jux Music",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        SW,SH,SDL_WINDOW_FULLSCREEN);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);

    PlFontData fd;
    plGetSharedFontByType(&fd,PlSharedFontType_Standard);
    SDL_RWops *frw=SDL_RWFromMem(fd.address,fd.size);
    fBig=TTF_OpenFontRW(frw,0,26); SDL_RWseek(frw,0,RW_SEEK_SET);
    fMed=TTF_OpenFontRW(frw,0,20); SDL_RWseek(frw,0,RW_SEEK_SET);
    fSm =TTF_OpenFontRW(frw,0,16); SDL_RWclose(frw);

    /* loading screen */
    fill(0,0,SW,SH,18,4,8);
    txt(fBig,"Jux Music — Chargement...",450,330,cTEXT);
    SDL_RenderPresent(ren);

    load_songs();

    PadState pad;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    bool run=true;
    while(run&&appletMainLoop()){
        padUpdate(&pad);
        u64 kd=padGetButtonsDown(&pad);
        if(kd&HidNpadButton_Plus) run=false;
        if((kd&HidNpadButton_AnyDown)&&sel<count-1){
            sel++; if(sel-scroll>=VISIBLE)scroll++;
        }
        if((kd&HidNpadButton_AnyUp)&&sel>0){
            sel--; if(sel<scroll)scroll--;
        }
        if(kd&HidNpadButton_A){
            fill(0,0,SW,SH,18,4,8);
            txt(fMed,"Chargement du morceau...",490,340,cSUB);
            SDL_RenderPresent(ren);
            play_song(sel);
        }
        if(kd&HidNpadButton_B){stop_audio();now_idx=-1;}

        /* background */
        fill(0,0,SW,SH,18,4,8);
        /* separator */
        fill(LIST_W,0,2,SH,50,15,20);

        /* ─── song list ─── */
        for(int i=0;i<VISIBLE&&(i+scroll)<count;i++){
            int idx=i+scroll;
            Song *s=&songs[idx];
            int y=i*ITEM_H;
            if(idx==sel)    fill(0,y,LIST_W,ITEM_H,38,12,17);
            if(idx==now_idx)fill(0,y,4,ITEM_H,220,70,45);

            char n[6]; snprintf(n,sizeof(n),"%d",idx+1);
            txt(fSm,n,12,y+28,cDIM);

            SDL_Color tc=idx==now_idx?cACCENT:(s->has_audio?cTEXT:cDIM);
            txt_w(fMed,s->title, 50,y+10,450,tc);
            txt_w(fSm, s->author,50,y+42,280,cSUB);
            if(s->genre[0])txt_w(fSm,s->genre,355,y+42,150,cDIM);
            txt(fSm,fmtd(s->duration),LIST_W-72,y+28,cSUB);

            SDL_SetRenderDrawColor(ren,35,10,14,255);
            SDL_RenderDrawLine(ren,50,y+ITEM_H-1,LIST_W-20,y+ITEM_H-1);
        }
        /* scrollbar */
        if(count>VISIBLE){
            int sh=(VISIBLE*ITEM_H*VISIBLE)/count;
            int sy=(scroll*VISIBLE*ITEM_H)/count;
            fill(LIST_W-5,0,5,VISIBLE*ITEM_H,45,14,18);
            fill(LIST_W-5,sy,5,sh,220,70,45);
        }

        /* ─── right panel ─── */
        fill(PANEL_X,0,SW-PANEL_X,SH,22,7,11);
        int px=PANEL_X+18, pw=SW-PANEL_X-36;

        if(now_idx>=0&&now_idx<count){
            Song *s=&songs[now_idx];
            txt(fSm,"▶  EN LECTURE",px,28,cACCENT);
            SDL_Rect cr={px,62,pw,(int)(pw*0.58f)};
            if(cover_tex) SDL_RenderCopy(ren,cover_tex,NULL,&cr);
            else{
                fill(cr.x,cr.y,cr.w,cr.h,35,10,14);
                txt(fBig,"♪",px+pw/2-16,cr.y+cr.h/2-18,cDIM);
            }
            int ty=cr.y+cr.h+18;
            txt_w(fBig,s->title, px,ty,   pw,cTEXT);
            txt_w(fMed,s->author,px,ty+34, pw,cSUB);
            if(s->genre[0])txt_w(fSm,s->genre,px,ty+62,pw,cDIM);
            char st[64];
            snprintf(st,sizeof(st),"♥ %d   ▶ %d   %s",s->likes,s->plays,fmtd(s->duration));
            txt(fSm,st,px,ty+88,cDIM);
            txt(fSm,"B : Arrêter la lecture",px,SH-38,cDIM);
        } else {
            txt(fBig,"JUX MUSIC",px+55,270,cTEXT);
            char ct[40]; snprintf(ct,sizeof(ct),"%d titres disponibles",count);
            txt(fMed,ct,px+30,314,cACCENT);
            txt(fSm,"↑↓  Naviguer",px+50,390,cDIM);
            txt(fSm,"A   Lire le morceau",px+50,416,cDIM);
            txt(fSm,"B   Arrêter",px+50,442,cDIM);
            txt(fSm,"+   Quitter",px+50,468,cDIM);
        }

        SDL_RenderPresent(ren);
    }

    stop_audio();
    TTF_CloseFont(fBig);TTF_CloseFont(fMed);TTF_CloseFont(fSm);
    Mix_CloseAudio();Mix_Quit();
    SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);
    TTF_Quit();IMG_Quit();SDL_Quit();
    plExit();socketExit();
    return 0;
}
