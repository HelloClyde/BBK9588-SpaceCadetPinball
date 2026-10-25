/* Portrait composition. The original table and sidebar are never redrawn. */
#include "engine/pch.h"
#include "engine/render.h"
#include "engine/pb.h"
#include "engine/GroupData.h"
#include "engine/TPinballTable.h"
#include "engine/TTextBox.h"
#include "platform.h"
#include "ui.h"

static unsigned short *pixels;
static unsigned short rgb(unsigned r,unsigned g,unsigned b){unsigned short c=(unsigned short)(((r&248)<<8)|((g&252)<<3)|(b>>3));return c?c:1;}
static void dot(int x,int y,unsigned short c){if((unsigned)x<240u&&(unsigned)y<320u)pixels[y*240+x]=c;}
static void rect(int x,int y,int w,int h,unsigned short c){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)dot(i,j,c);}
static void border(int x,int y,int w,int h,unsigned short c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static unsigned glyph(char c){
    static const unsigned short letters[26]={0x2bed,0x6bae,0x3923,0x6b6e,0x79a7,0x79a4,0x397b,0x5bed,0x7497,0x126a,0x5bad,0x4927,0x5fed,0x5f6d,0x2b6a,0x6ba4,0x2b7b,0x6bad,0x388e,0x7492,0x5b6f,0x5b6a,0x5b7d,0x5aad,0x5a92,0x72a7};
    static const unsigned short digits[10]={0x7b6f,0x2c97,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
    if(c>='a'&&c<='z')c-=32;
    if(c>='A'&&c<='Z')return letters[c-'A'];
    if(c>='0'&&c<='9')return digits[c-'0'];
    if(c=='-')return 0x01c0;if(c=='+')return 0x05d0;if(c=='/')return 0x12a4;
    if(c==':')return 0x0410;if(c=='.')return 2;if(c=='<')return 0x1511;if(c=='>')return 0x4454;
    return 0;
}
static void text(int x,int y,const char*s,unsigned short c,int scale=1){
    while(s&&*s){unsigned g=glyph(*s++);for(int j=0;j<5;++j)for(int i=0;i<3;++i)if(g&(1u<<(14-j*3-i)))rect(x+i*scale,y+j*scale,scale,scale,c);x+=scale*4;}
}
static void number(int x,int y,unsigned value,unsigned short c,int scale=1){char buf[16];snprintf(buf,sizeof(buf),"%u",value);text(x,y,buf,c,scale);}
static void blit(int dx,int dy,int dw,int dh,int sx,int sy,int sw,int sh,bool filter=false){
    gdrv_bitmap8 *src=render::vscreen;if(!src||!src->BmpBufPtr1||dw<=0||dh<=0||sw<=0||sh<=0)return;
    for(int y=0;y<dh;++y){int yy=sy+y*sh/dh;if((unsigned)yy>=(unsigned)src->Height)continue;
        for(int x=0;x<dw;++x){int xx=sx+x*sw/dw;if((unsigned)xx>=(unsigned)src->Width)continue;
            const ColorRgba&c=src->BmpBufPtr1[yy*src->Stride+xx];
            if(filter&&sw>dw&&sh>dh&&xx+1<src->Width&&yy+1<src->Height){
                const ColorRgba&b=src->BmpBufPtr1[yy*src->Stride+xx+1];
                const ColorRgba&d=src->BmpBufPtr1[(yy+1)*src->Stride+xx];
                const ColorRgba&e=src->BmpBufPtr1[(yy+1)*src->Stride+xx+1];
                dot(dx+x,dy+y,rgb((c.GetRed()+b.GetRed()+d.GetRed()+e.GetRed())/4,
                    (c.GetGreen()+b.GetGreen()+d.GetGreen()+e.GetGreen())/4,
                    (c.GetBlue()+b.GetBlue()+d.GetBlue()+e.GetBlue())/4));
            }else dot(dx+x,dy+y,rgb(c.GetRed(),c.GetGreen(),c.GetBlue()));}}
}
static void button(int x,int w,const char*s){rect(x,296,w,20,rgb(15,32,54));border(x,296,w,20,rgb(87,127,159));text(x+(w-(int)strlen(s)*4)/2,303,s,rgb(232,218,166));}

/* Sidebar messages are engine text, typeset at the LCD's native resolution.
 * Filtering the photographic logo helps at this scale, but small bitmap-font
 * stems would still disappear when the complete original panel is reduced. */
static const unsigned char* message_glyph(char c){
    static const unsigned char glyphs[36][7]={
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
        {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14}
    };
    static const unsigned char blank[7]={0,0,0,0,0,0,0};
    static const unsigned char punctuation[][7]={
        {0,4,4,0,4,4,0},{0,0,0,31,0,0,0},{0,0,0,0,0,4,4},
        {0,0,0,0,0,4,8},{4,4,4,4,4,0,4},{0,4,4,31,4,4,0},
        {1,2,2,4,8,8,16},{4,4,8,0,0,0,0},{2,4,8,8,8,4,2},
        {8,4,2,2,2,4,8},{24,25,2,4,8,19,3},{14,17,1,2,4,0,4}
    };
    if(c>='a'&&c<='z')c-=32;
    if(c>='A'&&c<='Z')return glyphs[c-'A'];
    if(c>='0'&&c<='9')return glyphs[26+c-'0'];
    const char* marks=":-.,!+/'()%?";
    for(unsigned i=0;marks[i];++i)if(c==marks[i])return punctuation[i];
    return blank;
}
static void message_text(int x,int y,const char*s,unsigned short color){
    while(s&&*s){const unsigned char*g=message_glyph(*s++);
        for(int j=0;j<7;++j)for(int i=0;i<5;++i)if(g[j]&(16>>i))dot(x+i,y+j,color);
        x+=6;}
}
static void message_box(TTextBox*box,int px,int py,int pw,int ph,const gdrv_bitmap8*bg){
    if(!box)return;
    int x=px+(box->OffsetX-bg->XPosition)*pw/bg->Width;
    int y=py+(box->OffsetY-bg->YPosition)*ph/bg->Height;
    int w=box->Width*pw/bg->Width,h=box->Height*ph/bg->Height;
    if(w<8||h<8)return;
    rect(x,y,w,h,rgb(7,9,14));
    if(!box->CurrentMessage||!box->CurrentMessage->Text)return;
    const char*s=box->CurrentMessage->Text;int yy=y+2;
    int maxChars=(w-4)/6;if(maxChars>60)maxChars=60;
    while(*s&&yy+7<=y+h-1){
        while(*s==' '||*s=='\r')++s;
        if(*s=='\n'){++s;yy+=9;continue;}
        int count=0,lastSpace=-1;
        while(s[count]&&s[count]!='\n'&&s[count]!='\r'&&count<maxChars){if(s[count]==' ')lastSpace=count;++count;}
        if(count==maxChars&&s[count]&&s[count]!='\n'&&s[count]!='\r'&&lastSpace>0)count=lastSpace;
        char line[64];for(int i=0;i<count;++i)line[i]=s[i];line[count]=0;
        message_text(x+2,yy,line,rgb(181,221,255));
        s+=count;if(*s=='\r')++s;if(*s=='\n')++s;while(*s==' ')++s;
        yy+=9;
    }
}

void scp_render_ui(unsigned short*out,int popup){
    pixels=out;rect(0,0,240,320,rgb(6,10,18));
    TPinballTable *table=pb::MainTable;if(!table)return;
    int w=table->Width,h=table->Height;
    if(w>0&&h>0){
        int dw=240,dh=h*240/w;if(dh>278){dh=278;dw=w*278/h;}
        blit((240-dw)/2,16+(278-dh)/2,dw,dh,-render::ScreenOffsetX(),-render::ScreenOffsetY(),w,h);
    }
    rect(0,0,240,15,rgb(12,25,43));
    text(5,5,"SCORE",rgb(113,168,197));number(30,5,table->CurScore<0?0:(unsigned)table->CurScore,rgb(255,202,105));
    text(155,5,"BALL",rgb(113,168,197));number(177,5,table->BallCount>0?(unsigned)(table->MaxBallCount-table->BallCount+1):0,rgb(255,202,105));
    text(196,5,"UP INFO",rgb(172,205,224));
    rect(0,286,240,8,rgb(6,10,18));text(8,287,"UP SCORE/PAUSE   DOWN NUDGE   ESC EXIT",rgb(120,147,167));
    button(3,70,"LEFT");button(78,84,pb::game_mode==GameModes::GameOver?"NEW GAME":"LAUNCH");button(167,70,"RIGHT");
    if(popup){
        for(int i=0;i<240*320;++i){unsigned c=pixels[i];pixels[i]=(unsigned short)(((c>>1)&0x7bef)|1);}
        rect(12,12,216,296,rgb(8,15,24));border(12,12,216,296,rgb(164,188,205));
        text(29,22,"SCOREBOARD - PAUSED",rgb(250,203,108),2);
        auto bg=pb::record_table->GetBitmap(pb::record_table->record_labeled("background"));
        char best[16];snprintf(best,sizeof(best),"%u",scp_platform_best());
        int bestX=(240-20-(int)strlen(best)*4)/2;
        text(bestX,34,"BEST",rgb(126,166,195));text(bestX+20,34,best,rgb(224,234,244));
        if(bg&&bg->Width>0&&bg->Height>0){int dh=242,dw=bg->Width*dh/bg->Height,dx=(240-dw)/2,dy=43;
            blit(dx,dy,dw,dh,bg->XPosition,bg->YPosition,bg->Width,bg->Height,true);
            message_box(pb::InfoTextBox,dx,dy,dw,dh,bg);
            message_box(pb::MissTextBox,dx,dy,dw,dh,bg);}
        text(30,295,"UP / ENTER / ESC : CLOSE",rgb(157,202,220));
        if(pb::game_mode==GameModes::GameOver)text(42,286,"ENTER STARTS NEW GAME",rgb(255,210,110));
    }
}

void scp_render_notice(unsigned short*out,const char*title,const char*message){
    pixels=out;rect(0,0,240,320,rgb(7,13,25));border(9,52,222,210,rgb(80,135,163));
    text(21,69,title,rgb(255,205,113),2);int x=21,y=98;
    while(message&&*message&&y<239){char c=*message++;if(c=='\n'||x>215){x=21;y+=10;if(c=='\n')continue;}
        char buf[2]={c,0};text(x,y,buf,rgb(183,208,224));x+=4;}
    text(62,273,"ESC TO RETURN",rgb(138,173,195));
}
