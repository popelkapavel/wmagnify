// wmagnify - magnify part of screen under cursor
// windres -o wmagnifr.o wmagnifr.rc
// gcc -fomit-frame-pointer -O3 -o wmagnify.exe wmagnifr.o wmagnify.c -mwindows -mno-cygwin 
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <math.h>

#include "getopt.h"

#define TimerId 1

#define WM_TASKBAR (WM_APP+124)
#define SCALE 4

HWND hWnd1,hWnd2;
HDC hDC,hDCDesktop,hDC2;
int DesktopWidth,DesktopHeight;
UINT Timer;
HINSTANCE hInstance;
HICON hIcon;

int timeout=100;
int zoom=2,scale=SCALE;
int sticky=8;
int halftone=0,not=0,cliptab=0;
int cross=0;
int ControlKey=0;
int InitialX=128,InitialY=128,InitialWidth=256,InitialHeight=128;
int show=0,quit=0;
int xyrgb=0;
int pause=0;
int rgb2;
int full=0;
int m3d=0;
int m3di=0,cycle;
int *xorbits,*lastbits,lastsize;char xorop=0,diff=0;
char op1=0;
int flags1,flags2,flags3;
BOOL showcursor=1;
POINT rel;
int findex=0,fmode=0,fcolor[]={0,0xff0000,0xff7f00,0xffff00,0x7fff00,0x00ff00,0x00ff7f,0x00ffff,0x007fff,0x0000ff,0x7f00ff,0xff00ff,0xff007f},fcolors=sizeof(fcolor)/sizeof(*fcolor);

void PrintHelp();

void UpdateHalftone() {
  // BLACKONWHITE,COLORONCOLOR,HALFTONE,WHITEONBLACK
  SetStretchBltMode(hDC,halftone|zoom*SCALE%scale?HALFTONE:COLORONCOLOR);
/*  if(halftone) {
    SetBrushOrgEx(hDC,0,0,NULL);
    SelectObject(hDC,GetStockObject(BLACK_BRUSH));
  } */
}

void ShowCursor2(BOOL bShow) {
  if(bShow==showcursor) return;
  ShowCursor(showcursor=bShow);
}

int IntToRange(int x,int low,int high) {
  if(x<low) return low;
  else if(x>high) return high;
  return x;
}

int CF_fmt=CF_OEMTEXT; // CF_TEXT,CF_OEMTEXT

int StrSetClipboard(char *str) {
  HANDLE r,sh;
  char *s2;
  int sl;

  if(!str||!OpenClipboard(NULL))
    return 0;
  EmptyClipboard();
  sl=strlen(str);
  sh=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,sl+1);
  s2=GlobalLock(sh);
  memcpy(s2,str,sl+1);
  GlobalUnlock(sh);
  r=SetClipboardData(CF_fmt,sh);
  CloseClipboard();
  return !!r;
}

char xyrgbText[64]="";
char clipboard[2048]="",clipshift=0;

void strrep(char *str,const char *search,char replace) {
  while(*str)
    if(strchr(search,*str++)) str[-1]=replace;
}

int rgbsum(int c) {
  return (c&255)+((c>>8)&255)+((c>>16)&255);
}
int rgbsum2(int c) {
  return ((c&255)+3*((c>>8)&255)+2*((c>>16)&255)+1)/2;
}
int rgbmin(int c) {
  int r=c&255,g=(c>>8)&255,b=(c>>16)&255;
  if(g<r) r=g;
  return b<r?b:r;
}
int rgbmax(int c) {
  int r=c&255,g=(c>>8)&255,b=(c>>16)&255;
  if(g>r) r=g;
  return b>r?b:r;
}
inline int rgbgray(int c,char mode) {
  return (mode>0?rgbmax(c):mode<0?rgbmin(c):(rgbsum2(c)+1)/3);
}
char rgbbw(int c,char mode,unsigned char level) {
  return (mode>0?rgbmax(c):mode<0?rgbmin(c):(rgbsum(c)+1)/3)>=level;
}
char rgbbw2(int c,char mode,char shift,char and,unsigned char level) {
  return (((mode>0?rgbmax(c):mode<0?rgbmin(c):(rgbsum(c)+1)/3)>>shift)&and)>=level;
}
int rgbdiv(int c,int n) {
  int a,d,r=c&255,g=(c>>8)&255,b=(c>>16)&255;
  if(n<3) r=r>127?255:0,g=g>127?255:0,b=b>127?255:0;
  else {
    d=255/(n-1),a=d/2;
    r=(r+a)/d*255/(n-1);
    g=(g+a)/d*255/(n-1);
    b=(b+a)/d*255/(n-1);
  }
  return r|(g<<8)|(b<<16);
}
int rgbperm(int c,char perm) {
  int r,g,b,rs,gs,bs;
  if(!perm) return c;
  r=c&255,g=(c>>8)&255,b=(c>>16)&255;
  if(perm==1) rs=0,gs=16;
  else if(perm==2) rs=8,gs=16;
  else if(perm==3) rs=8,gs=0;
  else if(perm==4) rs=16,gs=0;
  else rs=16,gs=8;
  bs=24-rs-gs;
  return (r<<rs)|(g<<gs)|(b<<bs);
}
int rgbmix(int c1,int c2,int i,int n) {
  int a0,a1,a2,j;
  if(i<=0) return c1;
  if(i>=n) return c2;
  j=n-i;
  a0=(j*(c1&255)+i*(c2&255))/n;
  a1=(j*((c1>>8)&255)+i*((c2>>8)&255))/n;
  a2=(j*((c1>>16)&255)+i*((c2>>16)&255))/n;
  return a0|(a1<<8)|(a2<<16);
}
int rgb2cmy(int c) {
  unsigned char r0,r1,r2,c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
  if(c0>c1&&c0>c2) {
    if(c1>c2) r0=c2,r1=c0-c1+c2,r2=c0;
    else r0=c1,r1=c0,r2=c0-c2+c1;
  } else if(c1>c0&&c1>c2) {
    if(c0>c2) r0=c1-c0+c2,r1=c2,r2=c1;
    else r0=c1,r1=c0,r2=c1-c2+c0;
  } else {
    if(c0>c1) r0=c2-c0+c1,r1=c2,r2=c1;
    else r0=c2,r1=c2-c1+c0,r2=c0;
  }
  return r0|(r1<<8)|(r2<<16);
}

int rgb2cmy2(int c) {
  unsigned char i,a,c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
  if(c0<c1) i=c0,a=c1;else i=c1,a=c0;
  if(c2<i) i=c2;else if(c2>a) a=c2;
  if(i<a) c0=i+a-c0,c1=i+a-c1,c2=i+a-c2;
  return c0|(c1<<8)|(c2<<16);
}

int huergb(int h) {
  if(h>=1536) h=h%1536;
  else if(h<0) h=h%1536+1536;
  int d=h&255,r,g,b;
  h>>=8;
  if(h==0) {r=255;g=d;b=0;}
  else if(h==1) {r=255^d;g=255;b=0;}
  else if(h==2) {r=0;g=255;b=d;}
  else if(h==3) {r=0;g=255^d;b=255;}
  else if(h==4) {r=d;g=0;b=255;}
  else {r=255;g=0;b=255^d;}
  return b|(g<<8)|(r<<16);
}

int rgbhue(int color,char rev,int shift) {
  int b=color&255,g=(color>>8)&255,r=(color>>16)&255,i=b,a=b,d,h;
  if(r<g) {i=r;a=g;} else {i=g;a=r;};
  if(b<i) i=b;else if(b>a) a=b;
  if(i==a) return rev||shift!=0?color:-1;
  d=a-=i;
  if(rev) h=r,r=b,b=h;
  if(d!=255) {b=(b-i)*255/d;g=(g-i)*255/d;r=(r-i)*255/d;}
  if(g==255) h=512+(r>0?-r:b);
  else if(b==255) h=1024+(g>0?-g:r);
  else h=b>0?1536-b:g;
  if(shift==0&&!rev) return h;
  //if(rev) h=1536-h;
  h+=shift;
  color=huergb(h);
  if(d==255) return color;
  b=color&255;g=(color>>8)&255;r=(color>>16)&255;
  r=i+r*d/255;g=i+g*d/255;b=i+b*d/255;
  return b|(g<<8)|(r<<16);
}

int dia(int x,int half,int max) {
  char neg=x<0;
  if(neg) x=-x;
  if(x<max) x=2*x>max?max-(max-x)*(max-half)*2/max:x*half*2/max;
  return neg?-x:x;
}

int abs2(int a,int b) {
  return a>b?a-b:b-a;
}

int rgbdiff(int c1,int c2,int mul) {
  int r0=mul*abs2(c1&255,c2&255),r1=mul*abs2((c1>>8)&255,(c2>>8)&255),r2=mul*abs2((c1>>8)&255,(c2>>8)&255);
  if(r0>255) r0=255;if(r1>255) r1=255;if(r2>255) r2=255;
  return r0|(r1<<8)|(r2<<16);
}

int rgbembo(int c1,int c2,int mul) {
  int r0=128+mul*((c1&255)-(c2&255)),r1=128+mul*(((c1>>8)&255)-((c2>>8)&255)),r2=128+mul*(((c1>>8)&255)-((c2>>8)&255));
  if(r0<0) r0=0;else if(r0>255) r0=255;
  if(r1<0) r1=0;else if(r1>255) r1=255;
  if(r2<0) r2=0;else if(r2>255) r2=255;
  return r0|(r1<<8)|(r2<<16);
}

int rgbxmix(int c1,int c2) {
  return (((c1&255)+((c1>>16)&255))/2)|(((((c1>>8)&255)+((c2>>8)&255))/2)<<8)|((((c2&255)+((c2>>16)&255))/2)<<16);
}

int rgbmin3(int c1,int c2) {
  unsigned char r0=c1&255,r1=(c1>>8)&255,r2=(c1>>16)&255;
  unsigned char r;
  r=c2&255;if(r<r0) r0=r;
  r=(c2>>8)&255;if(r<r1) r1=r;
  r=(c2>>16)&255;if(r<r2) r2=r;
  return r0|(r1<<8)|(r2<<16);
}

int rgbmax3(int c1,int c2) {
  unsigned char r0=c1&255,r1=(c1>>8)&255,r2=(c1>>16)&255;
  unsigned char r;
  r=c2&255;if(r>r0) r0=r;
  r=(c2>>8)&255;if(r>r1) r1=r;
  r=(c2>>16)&255;if(r>r2) r2=r;
  return r0|(r1<<8)|(r2<<16);
}

int rgbmap(int c,unsigned char *map) {
  unsigned char c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
  return map[c0]|(map[c1]<<8)|(map[c2]<<16);
}

unsigned char lightmap[256],darkmap[256];

void gama(char inv,float gamma,unsigned char *map) {
  int i,x;
  map[0]=0,map[255]=255;
  for(i=1;i<255;i++) {
    float f=i/255.0;
    if(gamma==0)
      x=(int)(255*(1-sqrt(1.0-f*f))+0.49);
    else 
      x=(int)(255*pow(f,gamma)+0.49);
      //x=(int)(255*f*f+0.49);
    map[inv?255-i:i]=inv?255-x:x;
  }
}


int rgb4(int c0,int c1,int c2,int c3,int sum) {
  return sum<256?rgbmix(c0,c1,sum,255):sum<511?rgbmix(c1,c2,sum-255,255):rgbmix(c2,c3,sum-510,255);
}
int rainbow[]={0,0xff00ff,0x0000ff,0x00ffff,0x00ff00,0xffff00,0xff0000,0xffffff};
int rainbow2[]={0,0xff00ff,0x000044,0x00ffff,0x004400,0xffff00,0x440000,0xffffff};
int layers2[]={0,0x00ffff,0,0x00ff00,0,0xffff00,0,0xffffff};
int bluered[]={0xffffff,0x00ffff,0x0000ff,0x0,0xff0000,0xffff00};
int bluered2[]={0x0,0x00ffff,0x0000ff,0xffffff,0xff0000,0xffff00};
int rgbn(int n,int *c,int i,int m) {
  if(i<=0) return c[0];
  if(i>=m) return c[n-1];
  i=i*(n-1);
  c+=i/m;
  return rgbmix(*c,c[1],i%m,m);
}
int rgbaggr(int c,int *min,int *max) {
  int c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255,mi=*min,ma=*max;
  char i=0,a=0;
  if(c0<mi) mi=c0,i=1;else if(c0>ma) ma=c0,a=1;
  if(c1<mi) mi=c1,i=1;else if(c1>ma) ma=c1,a=1;
  if(c2<mi) mi=c2,i=1;else if(c2>ma) ma=c2,a=1;
  if(i) *min=mi;
  if(a) *max=ma;
  return c0+c1+c2;
}
int rgbaggr3(int c,int *min,int *max) {
  int c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255,mi=*min,ma=*max;
  int i0=mi&255,i1=(mi>>8)&255,i2=(mi>>16)&255;
  int a0=ma&255,a1=(ma>>8)&255,a2=(ma>>16)&255;
  char i=0,a=0;
  if(c0<i0) i0=c0,i=1;else if(c0>a0) a0=c0,a=1;
  if(c1<i1) i1=c1,i=1;else if(c1>a1) a1=c1,a=1;
  if(c2<i2) i2=c2,i=1;else if(c2>a2) a2=c2,a=1;
  if(i) *min=i0|(i1<<8)|(i2<<16);
  if(a) *max=a0|(a1<<8)|(a2<<16);
  return c0+c1+c2;
}

int i765(int color,int i,char satur) {
  if(i<=0) return 0;else if(i>=765) return 0xffffff;
  int r=color&255,g=(color>>8)&255,b=(color>>16)&255;
  int s=r+g+b;
  if(s==i) goto end;
  if(r==b&&b==g) { r=g=b=i/3;/*s-=3*r;if(s>0) {b++;if(s>1) r++;}*/;goto end2;}          
  if(satur) {
    if(i<s) {
      int mi=r<g?r:g;
      if(b<mi) mi=b;
      if(mi>0) {
        if(satur>1)
          r=255-(255-r)*255/(255-mi),g=255-(255-g)*255/(255-mi),b=255-(255-b)*255/(255-mi);
        else {
          if(3*mi>s-i) mi=(s-i)/3;
          r-=mi;g-=mi;b-=mi;
        }
      }
    } else {
      int ma=r>g?r:g;
      if(b>ma) ma=b;
      if(ma<255) {
        if(satur>1) 
          r=r*255/ma,g=g*255/ma,b=b*255/ma;
	else {
	  ma^=255;
	  if(3*ma>i-s) ma=(i-s)/3;
	  r+=ma;g+=ma;b+=ma;
	}                  

      }            
    }
    s=r+g+b;
  }
  if(i<s) {
    if(i+2<s) r=r*i/s,g=g*i/s;
    b=i-r-g;
  } else {
    i=765-i;s=765-s;
    if(i+2<s) r=255-((255-r)*i/s),g=255-((255-g)*i/s);
    b=(765-i)-r-g;
  }
 end2:
  if(b<0) {
    b++;if(r>0) r--;else if(g>0) g--;
    if(b<0) {b++;if(g>0) g--;else if(r>0) r--;}
  } else if(b>255) {
    b--;if(r<255) r++;else if(g<255) g++;
    if(b>255) {b--;if(g<255) g++;else if(r<255) r++;}
  }
 end: 
  return r|(g<<8)|(b<<16);
}

int dark(char light,int c,char satur) {
  int s=rgbsum(c);
  float s2=(light?765-s:s)/765.0;
  s2*=s2;
  s=(int)(765*s2+0.49);
  if(light) s=765-s;
  return i765(c,s,satur);
}

void Contrast(unsigned char *bits,int width,int height,char gray) {
  unsigned char *h=bits,*he=bits+4*width*height,b,i0,i1,i2,a0,a1,a2,d0,d1,d2;
  i0=a0=h[0],i1=a1=h[1],i2=a2=h[2];
  for(h=(void*)bits;h<he;h++) {
    b=*h++;if(b<i0) i0=b;else if(b>a0) a0=b;
    b=*h++;if(b<i1) i1=b;else if(b>a1) a1=b;
    b=*h++;if(b<i2) i2=b;else if(b>a2) a2=b;
  }
  if(gray) {
    if(i1<i0) i0=i1;if(i2<i0) i0=i2; 
    if(a1>a0) a0=a1;if(a2>a0) a0=a2; 
    i1=i2=i0;
    a1=a2=a0;
  }
  d0=a0-i0,d1=a1-i1,d2=a2-i2;
  if((d0==255||!d0)||(d1==255||!d1)||(d2==255||!d2)) return;
  for(h=(void*)bits;h<he;h++) {
    if(d0) {b=*h;*h=(b-i0)*255/d0;} h++;
    if(d1) {b=*h;*h=(b-i1)*255/d1;} h++;
    if(d2) {b=*h;*h=(b-i2)*255/d2;} h++;
  }
}

void c765bw2(unsigned char *pixel,int bpl,int width,int height) {
  unsigned char *h=pixel,*he2,*he=h+bpl*height,*h2;
  int x,y,si[766],s,s2,i,a;
  memset(si,0,sizeof(si));
  bpl-=4*width;
  for(y=height,h=pixel;h<he;h+=bpl) {
    --y;
    for(x=width,he2=h+4*width;h<he2;) {
      s=*h++;s+=*h++;s+=*h++;h++;
      if(--x) {
        s2=h[0]+h[1]+h[2];
        if(s<s2) i=s,a=s2;else i=s2,a=s;
        if(i<a) si[i]++,si[a]--;
      }
      if(y) {
        h2=h+4*width+bpl;
        s2=h2[0]+h2[1]+h2[2];
        if(s<s2) i=s,a=s2;else i=s2,a=s;
        if(i<a) si[i]++,si[a]--;
      }
    }
  }
  for(i=0,s=0,x=-1,s2=0;i<766;i++) {
    s+=si[i];
    if(s>s2) x=i,s2=s;
  }
  if(x<0) h=pixel,x=h[0]+h[1]+h[2]>765/2?-1:765;
  for(h=pixel;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h+=4) {
      int c;
      s=h[0]+h[1]+h[2];
      c=s<=x?0:0xff;
      h[0]=h[1]=h[2]=c;
    }
}

int gravg(int *si,int f,int t) {
  int i,n=0,s=0;
  for(i=f;i<t;i++) {n+=si[i];s+=i*si[i];}
  return n?s/n:0;
}
int grerr(int *si,int f,int t) {
  int i,n=0,s=0,a;
  for(i=f;i<t;i++) {n+=si[i];s+=i*si[i];}
  if(!n) return 0;
  a=s/n;
  for(i=f,s=0;i<a;i++) s+=(a-i)*si[i];
  for(i=a+1;i<t;i++) s+=(i-a)*si[i];
  return s;
}
int grerr2(int *si,int f,int m,int t) {
  return grerr(si,f,m)+grerr(si,m,t);
}

void gropt(int *si,short *ll,int lev) {
  int i,j;
  char chg;
  j=0;
  do {
    chg=i=0;
    for(;i<lev-1&&j<4096;j++) {
      int li=ll[i],l1=ll[i+1],l2=ll[i+2];
      int e2,e=grerr2(si,li,l1,l2),mi=l1;
      for(l1=li+1;l1<l2;l1++) 
        if(si[l1]) {
  	  e2=grerr2(si,li,l1,l2);
	  if(e2<e) e=e2,mi=l1;
        }
      //printf("%d. %d %d %d \n",j,i,l1,ll[i+1]);
      if(mi!=ll[i+1]) {
	ll[i+1]=mi;
	chg=1;
      }
      i++;
    }
  } while(chg);
}

void c765gro(unsigned char *data,int bpl,int width,int height,int lev) {
  unsigned char *h=data,*he2,*he=h+bpl*height;
  int y,si[766],s,i,l;
  short ll[lev+1];
  memset(si,0,sizeof(si));
  bpl-=4*width;
  for(y=height,h=data;h<he;h+=bpl) {
    --y;
    for(he2=h+4*width;h<he2;) {
      s=*h++;s+=*h++;s+=*h++;h++;
      si[s]++;
    }
  }
  for(l=0;l<=lev;l++)
    ll[l]=l*766/lev;
  gropt(si,ll,lev);
  for(i=0,s=0,l=0;i<766;i++) {
    if(ll[l]==i) s=gravg(si,ll[l],ll[l+1])/3,l++;
    si[i]=s;
  }
  for(h=data;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h+=4) {
      int c;
      s=h[0]+h[1]+h[2];
      c=si[s];
      h[0]=h[1]=h[2]=c;
    }
}

int xpal_dist(int *map,int n,int c,int *ix,char ps2,char max) {
  unsigned char *x=(void*)(map+4),c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255,x0,x1,x2;
  register int m=766,d=0,r1,r2,i=n,s;
  while(i--) {
    if(ps2) {
      x0=x[0],x1=x[1],x2=x[2];
      d=(c0>x0?c0-x0:x0-c0)+(c1>x1?c1-x1:x1-c1)+(c2>x2?c2-x2:x2-c2);
    } else {
      d=0;
      x0=x[0],x1=x[1];
      d+=x1-x0;
      if((s=x0-c0)>0) d+=s;else if((s=c0-x1)>0) d+=s;
      x0=x[2],x1=x[3];
      r1=x1-x0;
      if((s=x0-c1)>0) r1+=s;else if((s=c1-x1)>0) r1+=s;
      x0=x[4],x1=x[5];
      r2=x1-x0;
      if((s=x0-c2)>0) r2+=s;else if((s=c2-x1)>0) r2+=s;
      if(max) {
        if(r1>d) d=r1;
        if(r2>d) d=r2;
      } else 
        d+=r1+r2;
    }
    if(d<m) m=d,*ix=n-1-i;
    x+=24;
  }
  return m;
}

int xpal_join(int *map,int n,int *dist,char max) {
  int i,im=-1,j,jm=-1,d,r1,r2,mi=766,ix,jx;
  unsigned char *ib,*jb,i0,j0,i1,j1;
  
  for(i=0,ib=(void*)(map+4);i<n-1;i++,ib+=24) {
    for(j=i+1,jb=ib+24;j<n;j++,jb+=24) {
      i0=ib[0],i1=ib[1],j0=jb[0],j1=jb[1];
      d=(j1>i1?j1:i1)-(j0<i0?j0:i0);
      i0=ib[2],i1=ib[3],j0=jb[2],j1=jb[3];
      r1=(j1>i1?j1:i1)-(j0<i0?j0:i0);
      i0=ib[4],i1=ib[5],j0=jb[4],j1=jb[5];
      r2=(j1>i1?j1:i1)-(j0<i0?j0:i0);
      if(max) {
        if(r1>d) d=r1;
        if(r2>d) d=r2;
      } else
        d+=r1+r2;
      if(d<mi) mi=d,im=i,jm=j;
    }
  }
  ix=6*im,jx=6*jm;
  map[ix]+=map[jx];
  map[ix+1]+=map[jx+1];
  map[ix+2]+=map[jx+2];
  map[ix+3]+=map[jx+3];
  ib=(void*)(map+ix+4);
  jb=(void*)(map+jx+4);
  if(jb[0]<ib[0]) ib[0]=jb[0];
  if(jb[1]>ib[1]) ib[1]=jb[1];
  if(jb[2]<ib[2]) ib[2]=jb[2];
  if(jb[3]>ib[3]) ib[3]=jb[3];
  if(jb[4]<ib[4]) ib[4]=jb[4];
  if(jb[5]>ib[5]) ib[5]=jb[5];
  d=ib[1]-ib[0]+ib[3]-ib[2]+ib[5]-ib[4];
  if(d>*dist) *dist=d;
  return jm;
}

void maxcount(unsigned char *data,int bpl,int width,int height,int count,char max) {
  unsigned char *ph,*pe2,*pe,*x,*data2=data,c0,c1,c2;
  int xpal[count*6],n=0,dist=0,ix=0,i,c,d=1;
  memset(xpal,0,sizeof(xpal));
  for(pe2=data+height*bpl;data<pe2;data+=bpl)
    for(ph=data,pe=ph+4*width;ph<pe;ph+=4) {
      c0=ph[0],c1=ph[1],c2=ph[2];
      if(n) {
        c=c0|(c1<<8)|(c2<<16);
        if(n==count) {
          x=(void*)(xpal+ix+4);
          if(c0<x[0]||c0>x[1]||c1<x[2]||c1>x[3]||c2<x[4]||c2>x[5]) ix=-1;
          else d=dist;
        } else ix=-1;
        if(ix<0) {
          d=xpal_dist(xpal,n,c,&ix,0,max);
          ix*=6;
        }
      }
      if(n<count) {
	if(d) {
	  ix=6*n++;
	  x=(void*)(xpal+ix+4);
	  x[0]=x[1]=c0;
	  x[2]=x[3]=c1;
	  x[4]=x[5]=c2;
	}
      } else if(d>dist) {
	ix=6*xpal_join(xpal,n,&dist,max);
	xpal[ix]=xpal[ix+1]=xpal[ix+2]=xpal[ix+3]=0;
	x=(void*)(xpal+ix+4);
	x[0]=x[1]=c0;x[2]=x[3]=c1;x[4]=x[5]=c2;
      }
      xpal[ix]++;
      xpal[ix+1]+=c0;
      xpal[ix+2]+=c1;
      xpal[ix+3]+=c2;
      x=(void*)(xpal+ix+4);
      if(c0<x[0]) x[0]=c0;
      if(c0>x[1]) x[1]=c0;
      if(c1<x[2]) x[2]=c1;
      if(c1>x[3]) x[3]=c1;
      if(c2<x[4]) x[4]=c2;
      if(c2>x[5]) x[5]=c2;
    }
  for(i=0;i<n;i++) {
    ix=6*i;
    x=(void*)(xpal+ix+4);
    d=xpal[ix];
    x[0]=xpal[ix+1]/d;
    x[1]=xpal[ix+2]/d;
    x[2]=xpal[ix+3]/d;
  }
  data=data2;
  for(pe2=data+height*bpl;data<pe2;data+=bpl)
    for(ph=data,pe=ph+4*width;ph<pe;ph+=4) {
      c=ph[0]|(ph[1]<<8)|(ph[2]<<16);
      xpal_dist(xpal,n,c,&ix,1,max);
      x=(void*)(xpal+6*ix+4);
      ph[0]=x[0],ph[1]=x[1],ph[2]=x[2];
    }
}



void BalanceBW(unsigned char *bits,int bpl,int width,int height) {
  unsigned char *h=bits,*he2,*he=bits+bpl*height;
  int i,s,l,n,si[766],min=h[0]+h[1]+h[2],max=min;
  memset(si,0,sizeof(si));
  bpl-=4*width;
  for(h=bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h++) {
      s=*h++;s+=*h++;s+=*h++;
      si[s]++;
      if(s<min) min=s;else if(s>max) max=s;
    }
  if(min==max) return;
  n=width*height-si[min]-si[max];
  if(n) {
    int min2=min,max2=max;
    double t=1.0*(max-min)*n,ti,ta;
    l=1+n/(max-min);
    if(0) for(i=min+1;i<max;i++) 
      if((s=si[i])) 
        if(s>l) n-=s-l,si[i]=l;
    l=0;
    si[min]=0,si[max]=n;
    for(i=min+1;i<max;i++) {
      s=si[i];
      l+=s;
      si[i]=l;
    }
    while(min2<max2) {
      if(si[min2+1]==si[min2]) {min2++;continue;}
      if(si[max2-1]==si[max2]) {max2--;continue;}
      t=1.0*(min2-min)*si[min2]+1.0*(max-max2)*(n-si[max2])+1.0*(max2-min2)*(si[min2]-si[max2]);
      ti=1.0*(min2-min+1)*si[min2+1]+1.0*(max-max2)*(n-si[max2])+1.0*(max2-min2-1)*(si[min2+1]-si[max2]);
      ta=1.0*(min2-min)*si[min2]+1.0*(max-max2+1)*(n-si[max2-1])+1.0*(max2-min2-1)*(si[min2]-si[max2-1]);
      if(ti<t&&ti<ta) { min2++;continue;}
      if(ta<t) { max2++;continue;}
      break;
    }
    l=(si[min2]+si[max2])/2;
    for(i=min+1;i<max;i++) 
      if(si[i]>=l) {
          l=i;
          break;
        }
  } else
    l=max;
  for(h=bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h++) {
      int c,*g=(int*)h;
      s=*h++;s+=*h++;s+=*h++;
      c=s<l?0:0xffffff;
      *g=c;
    }
}
void Balance(unsigned char *bits,int bpl,int width,int height,char mode) {
  unsigned char *h=bits,*he2,*he=bits+bpl*height;
  int i,s,l,n,si[766],min=h[0]+h[1]+h[2],max=min;
  memset(si,0,sizeof(si));
  bpl-=4*width;
  for(h=bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h++) {
      s=*h++;s+=*h++;s+=*h++;
      si[s]++;
      if(s<min) min=s;else if(s>max) max=s;
    }
  if(min==max) return;
  n=width*height-si[min]-si[max];
  if(n) {
    l=1+n/(max-min);
    for(i=min+1;i<max;i++) 
      if((s=si[i])) 
        if(s>l) n-=s-l,si[i]=l;
    l=0;
    for(i=min+1;i<max;i++) 
      if((s=si[i])) {
        l+=s;
        si[i]=1+l*763/n;
      }
  }
  for(h=bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h++) {
      int c,*g=(int*)h;
      s=*h++;s+=*h++;s+=*h++;
      if(s==min) c=0;
      else if(s==max) c=0xffffff;
      else c=i765(*g,si[s],mode);
      *g=c;
    }
}

void BalanceRGB(unsigned char *bits,int bpl,int width,int height) {
  unsigned char *h=bits,*he2,*he=bits+bpl*height,h0,h1,h2;
  int s0[768],*s1=s0+256,*s2=s1+256,s0i,s0a,s1i,s1a,s2i,s2a,n0,n1,n2,i,s,si,l0,l1,l2;
  s0i=s0a=h[0];s1i=s1a=h[1];s2i=s2a=h[2];

  memset(s0,0,sizeof(s0));
  bpl-=4*width;
  for(h=(void*)bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h+=4) {
      s0[h0=h[0]]++,s1[h1=h[1]]++,s2[h2=h[2]]++;
      if(h0<s0i) s0i=h0;else if(h0>s0a) s0a=h0;
      if(h1<s1i) s1i=h1;else if(h1>s1a) s1a=h1;
      if(h2<s2i) s2i=h2;else if(h2>s2a) s2a=h2;
    }
  n0=n1=n2=width*height;
  if(s0i==s0a&&s1i==s1a&&s2i==s2a) return;
  n0-=s0[s0i]+s0[s0a];
  n1-=s1[s1i]+s1[s1a];
  n2-=s2[s2i]+s2[s2a];
  if(n0<1&&n1<1&&n2<1) return;
  if(s0i<s0a) s0[s0i]=0,s0[s0a]=255,l0=1+n0/(s0a-s0i);else s0a=l0=0;
  if(s1i<s1a) s1[s1i]=0,s1[s1a]=255,l1=1+n1/(s1a-s1i);else s1a=l1=0;
  if(s2i<s2a) s2[s2i]=0,s2[s2a]=255,l2=1+n2/(s2a-s2i);else s2a=l2=0;
  for(i=0;i<255;i++) {
    if(i>s0i&&i<s0a&&(si=s0[i])>l0) {n0-=si-l0;s0[i]=l0;}
    if(i>s1i&&i<s1a&&(si=s1[i])>l1) {n1-=si-l1;s1[i]=l1;}
    if(i>s2i&&i<s2a&&(si=s2[i])>l2) {n2-=si-l2;s2[i]=l2;}
  } 
  for(s=0,i=s0i+1;i<s0a;i++)
    if((si=s0[i])) { s+=si; s0[i]=1+s*254/n0; }
  for(s=0,i=s1i+1;i<s1a;i++)
    if((si=s1[i])) { s+=si; s1[i]=1+s*254/n1; }
  for(s=0,i=s2i+1;i<s2a;i++)
    if((si=s2[i])) { s+=si; s2[i]=1+s*254/n2; }
  for(h=(void*)bits;h<he;h+=bpl)
    for(he2=h+4*width;h<he2;h+=4) {
      if(s0a) h[0]=s0[h[0]];
      if(s1a) h[1]=s1[h[1]];
      if(s2a) h[2]=s2[h[2]];
    }
}

  static const unsigned char matrix[]={
    0,192,48,240,12,204,60,252,3,195,51,243,15,207,63,255,
    128,64,176,112,140,76,188,124,131,67,179,115,143,79,191,127,
    32,224,16,208,44,236,28,220,35,227,19,211,47,239,31,223,
    160,96,144,80,172,108,156,92,163,99,147,83,175,111,159,95,
    8,200,56,248,4,196,52,244,11,203,59,251,7,199,55,247,
    136,72,184,120,132,68,180,116,139,75,187,123,135,71,183,119,
    40,232,24,216,36,228,20,212,43,235,27,219,39,231,23,215,
    168,104,152,88,164,100,148,84,171,107,155,91,167,103,151,87,
    2,194,50,242,14,206,62,254,1,193,49,241,13,205,61,253,
    130,66,178,114,142,78,190,126,129,65,177,113,141,77,189,125,
    34,226,18,210,46,238,30,222,33,225,17,209,45,237,29,221,
    162,98,146,82,174,110,158,94,161,97,145,81,173,109,157,93,
    10,202,58,250,6,198,54,246,9,201,57,249,5,197,53,245,
    138,74,186,122,134,70,182,118,137,73,185,121,133,69,181,117,
    42,234,26,218,38,230,22,214,41,233,25,217,37,229,21,213,
    170,106,154,90,166,102,150,86,169,105,153,89,165,101,149,85,
  };

void Matrix(unsigned char *bits,int bpl,int width,int height,char rgb) {
  register int i,j;
  unsigned char *h;
  const unsigned char *mx;

  for(j=0;j<height;j++) {
    h=bits+j*bpl;
    mx=matrix+((j&15)<<4);
    for(i=0;i<width;h++,i++) {
      unsigned char m=mx[i&15];
      if(rgb) {
        *h=*h>m||*h==255?255:0;h++;
        *h=*h>m||*h==255?255:0;h++;
        *h=*h>m||*h==255?255:0;h++;
      } else {
        int s=(h[0]+h[1]+h[2]+1)/3;
        h[0]=h[1]=h[2]=s>m||s==255?255:0;
        h+=3;
      }
    }
  }
}

void Diffusion(unsigned char *bits,int bpl,int width,int height,char rgb) {
  register int i,j,k,e,e2,de,de3,*emem,*eline,el,*ep;
  unsigned char *ph;
  el=(rgb?3:1)*(width+2)*sizeof(*eline);
  emem=malloc(el);
  memset(emem,0,el);

  for(j=height;j-->0;) {
    if(rgb) {
      for(k=0,eline=emem;k<3;k++,eline+=width+2) {
	ph=bits+j*bpl+k;
	ep=eline+1,e2=e=eline[0]=0;
	for(i=0;i<width;i++,ph+=4) {
	  e+=*ep+*ph;
	  if(e>127) *ph=255,e-=255;else *ph=0;
	  de=e/9;
	  ep[-1]+=de;
	  de3=e/3;
	  *ep++=e2+de3;
	  e-=de+de3+de;
	  e2=de;
	}
      }
    } else {
      ph=bits+j*bpl;
      ep=emem+1,e2=e=emem[0]=0;
      for(i=0;i<width;i++,ph+=4) {
        unsigned char b=(ph[0]+ph[1]+ph[2])/3;
	e+=*ep+b;
	if(e>127) e-=(b=255);else b=0;
        ph[0]=ph[1]=ph[2]=b;
	de=e/9;
	ep[-1]+=de;
	de3=e/3;
	*ep++=e2+de3;
	e-=de+de3+de;
	e2=de;
      }
    }
  }
  free(emem);
}

void Mirror(unsigned char *bits,int bpl,int width,int height,char mode) {
  register int x,j,*h,*he,*g,y;
  for(j=0;j<height;j++) { 
    h=(int*)(bits+j*bpl);
    if(mode==1) {
      char odd=width&1;
      for(he=h+width/2,g=he,y=*g,g+=odd;h<he;h++,g++) 
        x=*h,*h=*g,g[-odd]=x;
      if(odd) g[-1]=y;
    } else 
      for(g=h+width-1;h<g;h++,g--) 
        x=*h,*h=*g,*g=x;
  }
}

void Filter33(unsigned char *bits,int bpl,int width,int height,char mode) {
  int b,w4=width*4;
  unsigned char *h=bits,*he2,*he=bits+bpl*(height-1),*p=malloc(w4),*ph,*g;
  memcpy(p,bits,w4);
  for(h=bits+bpl,g=h+bpl,bpl-=w4;h<he;h+=bpl+4,g+=bpl+4) {
    for(b=*(int*)h,he2=h+w4-4,h+=4,g+=4,ph=p+4;h<he2;ph+=4,h+=4,g+=4) {
      int c0=h[0],c1=h[1],c2=h[2],b0=b&255,b1=(b>>8)&255,b2=(b>>16)&255,bp=*(int*)h;
      if(mode=='b') {
        c0=(c0+ph[0]+b0+h[4+0]+g[0])/5;
        c1=(c1+ph[1]+b1+h[4+1]+g[1])/5;
        c2=(c2+ph[2]+b2+h[4+2]+g[2])/5;
      } else if(mode=='s') {
        c0=(8*c0-ph[0]-b0-h[4+0]-g[0])/4;
        if(c0<0) c0=0;else if(c0>255) c0=255;
        c1=(8*c1-ph[1]-b1-h[4+1]-g[1])/4;
        if(c1<0) c1=0;else if(c1>255) c1=255;
        c2=(8*c2-ph[2]-b2-h[4+2]-g[2])/4;
        if(c2<0) c2=0;else if(c2>255) c2=255;
      }
      h[0]=c0,h[1]=c1,h[2]=c2;
      *(int*)(ph-4)=b;
      b=bp;
    }
    *(int*)(ph-4)=b;
    *(int*)ph=*(int*)h;
  }
  free(p);
}

void CFilter(int *bits,int width,int height,int color,char mode) {
  unsigned char *h,*he,c0=color&255,c1=(color>>8)&255,c2=(color>>16)&255;
  for(h=(void*)bits;height--;) 
    for(he=h+4*width;h<he;h+=4) {
      if(mode) {
        int c;
        unsigned char h0=h[0],h1=h[1],h2=h[2];
        if(mode==1)
          c=i765(color,(h0+3*h1+2*h2+1)/2,0);
        else {
          unsigned char i,a;
          if(h0<h1) i=h0,a=h1;else i=h1,a=h0;
          if(h2<i) i=h2;else if(h2>a) a=h2;
          a-=i;
          if(!a) continue;
          h[0]=i+a*c0/255;
          h[1]=i+a*c1/255;
          h[2]=i+a*c2/255;
          continue;
        }
        h[0]=c&255;
        h[1]=(c>>8)&255;
        h[2]=(c>>16)&255;
      } else {
	h[0]=h[0]*c0/255;
	h[1]=h[1]*c1/255;
	h[2]=h[2]*c2/255;
      }
    }
}

unsigned char sharp1(unsigned char c,unsigned char a,char mode) {
  register int x;
  if(mode==2) x=dia(4*(c-a),96,128)+128;
  else if(mode==6) x=6*(c-a)+128;
  else if(mode==3) x=dia(4*(c-a),192,256);
  else if(mode==7) x=dia(4*(a-c),192,256);
  else if(mode==4) x=255-dia(4*(c-a),192,256);
  else if(mode==5) x=255-dia(4*(a-c),192,256);
  else x=2*c-a;
  return x<0?0:x>255?255:x;
}

int sharp3(int c,int a,char mode,char satur) {
  int sc=rgbsum(c),sa=rgbsum(a),x;
  if(mode==2) x=dia(4*(sc-sa),256,384)+383;
  else if(mode==6) x=6*(sc-sa)+383;
  else if(mode==3) x=dia(4*(sc-sa),512,765);
  else if(mode==7) x=dia(4*(sa-sc),512,765);
  else if(mode==4) x=765-dia(4*(sc-sa),512,765);
  else if(mode==5) x=765-dia(4*(sa-sc),512,765);
  else x=2*sc-sa;
  c=i765(c,x,satur);
  return c;
}

unsigned char max3(unsigned char a,unsigned char b,unsigned char c) { if(b>a) a=b; return a>c?a:c; }
unsigned char min3(unsigned char a,unsigned char b,unsigned char c) { if(b<a) a=b; return a<c?a:c; }

void Blur(int *bits,int width,int height,int size,char hori,char vert,char sharp) {
  unsigned char *pixel=(void*)bits,*h,*g,*copy,c0,c1,c2;
  int x,y,s0,s1,s2,n,s4,bpl=4*width;
  if(size<1) return;
  copy=malloc(bpl*height);
  if(hori) {
    s4=4*size;
    for(y=0;y<height;y++) {
       for(x=s0=s1=s2=n=0,h=pixel+y*bpl;x<width&&x<size;x++,h+=4) 
	 s0+=h[0],s1+=h[1],s2+=h[2],n++;
       for(x=0,h=pixel+y*bpl,g=copy+y*bpl;x<width;x++,h+=4,g+=4) {
	 if(x+size<width) s0+=h[s4],s1+=h[s4+1],s2+=h[s4+2],n++;
	 c0=(s0+size)/n,c1=(s1+size)/n,c2=(s2+size)/n;
         if(sharp&&!vert) {
           if(sharp>=8) {
             int c=sharp3(h[0]|(h[1]<<8)|(h[2]<<16),c0|(c1<<8)|(c2<<16),sharp-8,0);
             c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
           } else {
             c0=sharp1(h[0],c0,sharp),c1=sharp1(h[1],c1,sharp),c2=sharp1(h[2],c2,sharp);
             if(sharp==5) {unsigned char mima=min3(c0,c1,c2)+max3(c0,c1,c2);c0=mima-c0;c1=mima-c1;c2=mima-c2;}
           }
         }
         g[0]=c0,g[1]=c1,g[2]=c2;
	 if(x>=size) s0-=h[-s4],s1-=h[-s4+1],s2-=h[-s4+2],n--;
       }
    }
  } else
    memcpy(copy,pixel,bpl*height);
  if(vert) {
    s4=bpl*size;
    for(x=0;x<width;x++) {
       for(y=s0=s1=s2=n=0,h=copy+4*x;y<height&&y<size;y++,h+=bpl) 
	 s0+=h[0],s1+=h[1],s2+=h[2],n++;
       for(y=0,h=copy+4*x,g=pixel+4*x;y<height;y++,h+=bpl,g+=bpl) {
	 if(y+size<height) s0+=h[s4],s1+=h[s4+1],s2+=h[s4+2],n++;
	 c0=(s0+size)/n,c1=(s1+size)/n,c2=(s2+size)/n;
         if(sharp) {
           if(sharp>=8) {
             int c=sharp3(g[0]|(g[1]<<8)|(g[2]<<16),c0|(c1<<8)|(c2<<16),sharp-8,0);
             c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
           } else {
             c0=sharp1(g[0],c0,sharp),c1=sharp1(g[1],c1,sharp),c2=sharp1(g[2],c2,sharp);
             if(sharp==5) {unsigned char mima=min3(c0,c1,c2)+max3(c0,c1,c2);c0=mima-c0;c1=mima-c1;c2=mima-c2;}
           }
         }
         g[0]=c0,g[1]=c1,g[2]=c2;
	 if(y>=size) s0-=h[-s4],s1-=h[-s4+1],s2-=h[-s4+2],n--;
       }
    }
  } else
    memcpy(pixel,copy,bpl*height);
  free(copy);
}

void hdr4(int *bits,int width,int height,char flags,char size,int n0) {
  unsigned char *pixel=(void*)bits,*pix2=malloc(4*width*height);
  int y,x,j,k0=size<4?1:size/2,k,d,bpl=4*width;
  char rgb=flags&2,satur=flags&1,diag=flags&4;
  if(size<1) size=2;
  if(n0<0) n0=0;
  memcpy(pix2,pixel,height*bpl);
  for(y=size-1;y<height-size;y++) {
    unsigned char *g=pixel+y*bpl+4*(size-1),*h=pix2+y*bpl+4*(size-1);
    for(x=2*size-2;x<width;x++,h+=4,g+=4) {
      unsigned char c0=*h,c1=h[1],c2=h[2],mi0=c0,ma0=c0,mi1=c1,ma1=c1,mi2=c2,ma2=c2;
      int c,n=n0,s0=n*c0,s1=n*c1,s2=n*c2,mi=c0+c1+c2,ma=mi,s=n*mi;
      for(k=1;k<size;k++) {
       if(diag) {
        unsigned char *hh0=h-k*bpl,*hh1=h+k*bpl,*hh2=h+k*4,*hh3=h-k*4;
        for(j=0;j<k;j++,hh0+=bpl+4,hh1+=-bpl-4,hh2+=bpl-4,hh3+=-bpl+4)
          for(d=0;d<4;d++) {
            unsigned char *hh=d<2?d?hh1:hh0:d<3?hh2:hh3;
            if(rgb) {
              register unsigned char z0,z1,z2;
              z0=*hh;if(z0<mi0) mi0=z0;else if(z0>ma0) ma0=z0;
              z1=hh[1];if(z1<mi1) mi1=z1;else if(z1>ma1) ma1=z1;
              z2=hh[2];if(z2<mi2) mi2=z2;else if(z2>ma2) ma2=z2;
            } else {
              register int z=*hh+hh[1]+hh[2];if(z<mi) mi=z; else if(z>ma) ma=z;
            }
          }
       } else {
        unsigned char *hh0=h+k*4+(1-k)*bpl,*hh1=h-k*bpl-k*4,*hh2=h+k*bpl+(1-k)*4,*hh3=h-k*4-k*bpl;
        for(j=0;j<2*k;j++,hh0+=bpl,hh1+=4,hh2+=4,hh3+=bpl)
          for(d=0;d<4;d++) {
            unsigned char *hh=d<2?d?hh1:hh0:d<3?hh2:hh3;
            if(rgb) {
              register unsigned char z0,z1,z2;
              z0=*hh;if(z0<mi0) mi0=z0;else if(z0>ma0) ma0=z0;
              z1=hh[1];if(z1<mi1) mi1=z1;else if(z1>ma1) ma1=z1;
              z2=hh[2];if(z2<mi2) mi2=z2;else if(z2>ma2) ma2=z2;
            } else {
              register int z=*hh+hh[1]+hh[2];if(z<mi) mi=z; else if(z>ma) ma=z;
              //c=*hh+hh[1]+hh[2];if(c<mi) {mi=c;goto check;} else if(c>ma) {ma=c;check: if(ma-mi==765) goto skip;}
            }
          }
        }
       //skip:
        if(k<k0) continue;
        n++;
        if(rgb) {
          if(satur) {
            int i=mi0<mi1?mi0:mi1,a=ma0>ma1?ma0:ma1;
            if(mi2<i) i=mi2;if(ma2>a) a=ma2;
            s0+=i==a?c0:(c0-i)*255/(a-i);
            s1+=i==a?c1:(c1-i)*255/(a-i);
            s2+=i==a?c2:(c2-i)*255/(a-i);
          } else {
            s0+=mi0==ma0?c0:(c0-mi0)*255/(ma0-mi0);
            s1+=mi1==ma1?c1:(c1-mi1)*255/(ma1-mi1);
            s2+=mi2==ma2?c2:(c2-mi2)*255/(ma2-mi2);
          }
        } else {
          c=c0+c1+c2;
          s+=mi==ma?c:(c-mi)*765/(ma-mi);
        }
      }
      if(rgb) {
        g[0]=s0/n,g[1]=s1/n,g[2]=s2/n;
      } else {
        c=i765(c0|(c1<<8)|(c2<<16),s/n,satur);
        g[0]=c&255,g[1]=(c>>8)&255,g[2]=(c>>16)&255;
      }
    }
  }
  free(pix2);
}

void Emboss(int *bits,int width,int height) {
  unsigned char *g,*ge;
  int y,a,b,c,d,p,n,bpl=4*width;
  for(y=0;y<height-1;y++)
    for(ge=3+(unsigned char*)(bits+y*width),g=ge+bpl-4;g>ge;g--) {
      if((((int)g)&3)==3) g--;
      a=*g,b=g[-4],c=g[bpl],d=g[bpl-4];
      b-=a,c-=a,d-=a;
      b*=3*(256-abs(b)),c*=3*(256-abs(c)),d*=2*(256-abs(d));
      p=n=0;
      if(b>0) p+=b;else n+=b;
      if(c>0) p+=c;else n+=c;
      if(d>0) p+=d;else n+=d;
      a=128+(p>-n?p:n);
      *g=a<0?0:a>255?255:a;
    }
}

void Map1(int *bits,int width,int height,unsigned char *m) {
  unsigned char *g,*ge;
  for(g=(unsigned char*)bits,ge=g+4*width*height;g<ge;g++)
    *g=m[*g];
}

void Mapi(int *bits,int width,int height,unsigned short *m,char satur) {
  unsigned int *g,*ge;
  for(g=(unsigned int*)bits,ge=g+width*height;g<ge;g++)
    *g=i765(*g,m[rgbsum(*g)],satur);
}

void MapiN(unsigned short *mi,int c) {
  if(c>0) {
    unsigned short i,j,ma[766];
    for(i=0;i<766;i++) j=mi[i],ma[i]=j>765?765:mi[i];
    while(c-->0) for(i=0;i<766;i++) mi[i]=ma[mi[i]];
  }
}

void AddBits(int *bits,int width,int height,char cnt,unsigned char d) {
  int a;
  unsigned char m[256],x=(1<<cnt)-1;
  for(a=0;a<256;a++) 
    m[a]=(a&~x)|((a+d)&x);
  Map1(bits,width,height,m);
}

void RotBits(int *bits,int width,int height,char cnt,unsigned int d) {
  int a;
  unsigned char m[256],x=(1<<cnt)-1,x2;
  d%=cnt;
  if(!d) return;
  x2=(1<<d)-1;
  for(a=0;a<256;a++) 
    m[a]=(a&~x)|((a<<d)&x)|((a>>(cnt-d))&x2);
  Map1(bits,width,height,m);
}

void SomBits(int *bits,int width,int height,unsigned char x,char shift) {
  int a;
  unsigned char m[256],b,c=8-shift;
  for(a=0;a<256;a++) {
    b=a-x;
    m[a]=(b>>shift)?a>>shift:b<<c;
  }
  Map1(bits,width,height,m);
}

void SomBits2(int *bits,int width,int height,unsigned short x,char mul) {
  int a,c=765/mul;
  unsigned short mi[766],b;
  for(a=0;a<766;a++) {
    b=a-x;
    b%=766;
    mi[a]=b<mul?b*c:a/mul;
  }
  Mapi(bits,width,height,mi,0);
}

void SomBits3(int *bits,int width,int height,int x,int y,int z) {
  int a,u=765-y,v=765-z,b,o;
  unsigned short mi[766];
  x%=u;
  if(x<0) x+=u;
  o=x*v/u;
  for(a=0,b=-x;a<766;a++,b++) {
    mi[a]=b<0?a*v/u:b<y?o+b*z/y:765-(765-a)*v/u;
  }
  Mapi(bits,width,height,mi,0);
}

void SomBits4(int *bits,int width,int height,int x) {
  int a,t,y;
  unsigned short mi[766];
  x%=766;
  if(x<0) x+=766;
  y=765-x,t=x*x+y*y;
  for(a=0;a<766;a++) {
    mi[a]=(a<x?a*a:t-(765-a)*(765-a))*(long long)765/t;
  }
  Mapi(bits,width,height,mi,0);
}

void Oil(int *bits,int width,int height,char x) {
  int a,b,l2,l1,a1,a2;
  unsigned char m[256],l=(1<<x)-1,h=~l;
  l1=l/3,l2=1+l1+(l-1)/3;
  a1=1+3*l2,a2=3*(l2+1)-2;
  for(a=0;a<256;a++) {
    b=a&l,b=(a&h)+(b>l2?3*b-a2:b>l1?a1-3*b:3*b);
    //if(x6) b=a&31,b=(a&0xe0)+(b>21?3*b-64:b>10?64-3*b:3*b);
    //else b=a&15,b=(a&0xf0)+(b>10?3*b-31:b>5?31-3*b:3*b);
    m[a]=b;
  }
  Map1(bits,width,height,m);
}

void Oil2m(unsigned short *map,char x,char m) {
  int i,d=m+1;
  for(i=0;i<766;i++)
    map[i]=(m*i+((i<<x)%766))/d;
}

void Oil2(int *bits,int width,int height,char x,char m) {
  unsigned short mi[766];
  Oil2m(mi,x,m);
  Mapi(bits,width,height,mi,0);
}

int dent2f(int i,int m,int n,int c) {
  int d,b,a;
  if(i<=0) return 0;if(i>=m) return m;
  d=m/n,b=i%d,a=i/d;
  return c*b+(m-c*(m%d))*a/(m/d);
}

void Dent2(int *bits,int width,int height,char n,char c) {
  unsigned short i,mi[766];
  for(i=0;i<766;i++) mi[i]=dent2f(i,765,n,2);
  //MapiN(mi,c);
  Mapi(bits,width,height,mi,0);
}

void Diff2(int *last,int *bits,int width,int height,char mode) {
  int *h=last,*g=bits,s=width*height,*ge=g+s,a,b,c,x;
  unsigned char d,e=0,a0,b0,i;
  for(;g<ge;g++,h++) {
    a=*g,b=*h,*h=a;
    e|=(d=a!=b);
    if(mode>1) { 
      c=0;
      for(i=0;i<24;i+=8) {
        a0=(a>>i)&255,b0=(b>>i)&255;
        if(mode==3) x=128+2*(a0-b0);
        else x=255-2*abs(a0-b0);
        if(x<0) x=0;else if(x>255) x=255;
        c|=x<<i;
      }
    } else c=d?rgbmix(a,0,16,255):0xffffff;
    //} else c=rgbmix(h[s],d?rgbmix(a,0,16,255):0xffffff,1,2);
    *g=c;
  }
  a=4*width*height;
  if(e) memcpy(h,bits,a);
  else memcpy(bits,h,a);
}

unsigned char sigma3(unsigned char x) {
  return x<=32?2*x:x>=224?255-2*(255-x):64+(x-32)*2/3;
}
unsigned int sigma5(int s) {
  return s<=96?2*s:s>=765-96?765-2*(765-s):192+(s-96)*2/3;
}

int rgbsig(int c) {
  unsigned char c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255;
  unsigned char s0=sigma3(c0),s1=sigma3(c1),s2=sigma3(c2);
  c0=(c0+3*s0)/4,c1=(c1+3*s1)/4,c2=(c2+3*s2)/4;
  return c0|(c1<<8)|(c2<<16);
}

int sumsig(int c,char satur) {
  int s=rgbsum(c),s2=s,s3=sigma5(s=sigma5(s));
  return i765(c,(s+2*s2+1*s3)/4,satur);
}

void Flags(int *bits,int width,int height,int flags) {
 int c,*h,*he=bits+width*height;
 if(!flags) return;
 if(flags&0x10000) Filter33((void*)bits,4*width,width,height,'b');
 if(flags&0x20000) Filter33((void*)bits,4*width,width,height,'s');
 if(flags&0xef0077c3) {
  char perm=(flags>>12)&7;
  for(h=bits;h<he;h++) {
   c=*h;
   if(flags&1) c^=0xffffff;
   if(flags&2) c=rgb2cmy(c);
   if(perm) c=rgbperm(c,perm);
   if(flags&0x100) c^=0xff0000;
   if(flags&0x200) c^=0x00ff00;
   if(flags&0x400) c^=0x0000ff;
   if(flags&0x80) c=rgbdiv(c,6);
   if(flags&0x3000000) c=0x10101*rgbgray(c,((flags>>24)&3)-2);
   if(flags&0x4000000) c=dark(1,c,1);//c=rgbmap(c,lightmap);
   if(flags&0x8000000) c=dark(0,c,1);//c=rgbmap(c,darkmap);
   if(flags&0x40) c=rgbbw(c,0,128)?0xffffff:0;
   if(flags&0x20000000) c=rgbbw2(c,0,4,3,2)?0xffffff:0;
   if(flags&0x40000000) c=0xffffff^rgb2cmy2(c);
   if(flags&0x80000000) c=rgbdiv(c,2);
   *h=c;
  }
 }
 if(flags&4) Balance((void*)bits,4*width,width,height,1);
 if(flags&8) BalanceRGB((void*)bits,4*width,width,height);
 if(flags&0x10000000) BalanceBW((void*)bits,4*width,width,height);
 //if(flags&0x10000000) c765bw2((void*)bits,4*width,width,height);
 if(flags&0x30) {
   int n=9,brick=0,i,j,h,h2,w,w2,n2;
   for(j=h=0;j++<n;) {
     h2=h,h=height*j/n;
     for(i=w=0,n2=n+(brick&&(j&1));i++<n2;) {
       w2=w,w=width*i/n;
       if(brick&&(j&1)) { w-=width/n/2;if(w>width) w=width;}
       if(flags&0x10) Balance((void*)(bits+h2*width+w2),4*width,w-w2,h-h2,1);
       else BalanceRGB((void*)(bits+h2*width+w2),4*width,w-w2,h-h2);
     }
   }
 }
 if(flags&0x800) Matrix((void*)bits,4*width,width,height,0);
 if(flags&0x40000) Matrix((void*)bits,4*width,width,height,1);
 if(flags&0x80000) Diffusion((void*)bits,4*width,width,height,0);
 if(flags&0x100000) Diffusion((void*)bits,4*width,width,height,1);
 if(flags&0x200000) Mirror((void*)bits,4*width,width,height,0);
 if(flags&0x400000) Mirror((void*)bits,4*width,width,height,1);
}

void Flags3(int *bits,int width,int height,int flags) {
  if(!flags) return;
  if(flags&0xf) Blur(bits,width,height,4,1,1,flags&15);
  if(flags&0xf0) hdr4(bits,width,height,(flags>>4)&7,6,8);
  if(flags&0x1ff00) {
    int c,*h,*he=bits+width*height;
    int csh=((flags>>11)&31)*255/5;
    char cm=!!(flags&0x10000);
    for(h=bits;h<he;h++) {
      c=*h;
      if(flags&0x100) c=rgbsig(c);
      if(flags&0x200) c=sumsig(c,0);
      if(flags&0x400) c=sumsig(c,1);
      if(csh||cm) c=rgbhue(c,cm,csh);
      *h=c;
    }
  }
  if(flags&0x20000) Emboss(bits,width,height);
  if(flags&0x6000000) {
    char x=(flags>>25)&3;
    if(x==1) AddBits(bits,width,height,5,2*cycle);
    else if(x==2) RotBits(bits,width,height,6,cycle);
    //else SomBits(bits,width,height,-cycle*16,5);
    //else SomBits2(bits,width,height,cycle*34,85);
    else SomBits3(bits,width,height,cycle*24,33,255);
    //else SomBits4(bits,width,height,cycle*24);
  }
  if(flags&0x8000000) Dent2(bits,width,height,7,3);
  if(flags&0x1000000) Oil2(bits,width,height,2+((flags>>18)&3),2);
  if(flags&0xc0000) Oil(bits,width,height,4+((flags>>18)&3));
  if(flags&0xf00000) {
    int size=4*width*height;
    if(!lastbits||lastsize!=size) {
       free(lastbits);
       lastbits=malloc(2*(lastsize=size));
       memcpy(lastbits,bits,size);
       memcpy(lastbits+width*height,bits,size);
    } else {
      Diff2(lastbits,bits,width,height,flags>>20);
    }
  }
}

int *GetBits(HDC dc,int x,int y,int width,int height,int *bits2,int width2,char xorop,char op1,int flags1,int flags2,int flags3) {
  BITMAPINFOHEADER bi;
  int *data,*bits,*copy=NULL; //,*h2;
  int bpl,size;
  HBITMAP hbmp,hbmp2;
  HDC dc2;

  memset(&bi,0,sizeof(bi));
  bi.biSize=sizeof(bi);
  bi.biWidth=width;
  bi.biHeight=height;
  bi.biPlanes=1;
  bi.biBitCount=32;
  bi.biCompression=BI_RGB;
  bpl=bi.biWidth*4,size=bpl*height;
  dc2=CreateCompatibleDC(dc);
  hbmp=CreateDIBSection(dc2,(BITMAPINFO*)(void*)&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
  data=bits;
  hbmp2=SelectObject(dc2,hbmp);
  BitBlt(dc2,0,0,bi.biWidth,bi.biHeight,dc,x,y,SRCCOPY);
  if(!xorop&&(op1||flags1||flags2||flags3||findex)) {
    if(op1=='8') maxcount((void*)bits,4*width,width,height,64,0);
    //else if(op1=='6') c765gro((void*)bits,4*width,width,height,16);
    else {
    int b,i,j,*p=malloc(4*width);
    memcpy(p,bits,4*width);
    cycle++;
    Flags(data,width,height,flags1);
    Flags3(data,width,height,flags3);
    for(j=1,bits+=width;j<height-1;j++,bits+=width) {
     for(i=1,b=*bits;i<width-1;i++) {
       int c=bits[i]&0xffffff,b2=c,c0=c&255,c1=(c>>8)&255,c2=(c>>16)&255,s=c0+c1+c2,d0=c0,d1=c1,d2=c2,min,max,avg;
       char c3=0;
       if(op1=='i') c1=c2=c0=s/3,c3=1;
       else if(op1=='g') {if(c1<c0) c0=c1;if(c2<c0) c0=c2;c1=c2=c0,c3=1;}
       else if(op1=='G') {if(c1>c0) c0=c1;if(c2>c0) c0=c2;c1=c2=c0,c3=1;}
       else if(op1=='p') c=rgb4(0,0x00ffff,0xff0000,0xffff00,s);
       else if(op1=='P') c=rgbn(sizeof(rainbow2)/sizeof(*rainbow2),rainbow2,s,765);
       else if(op1=='L') c=rgbn(sizeof(rainbow)/sizeof(*rainbow),rainbow,s,765);
       else if(op1=='l') c=rgbn(sizeof(layers2)/sizeof(*layers2),layers2,s,765);
       else if(op1=='K') c=rgbn(sizeof(bluered)/sizeof(*bluered),bluered,s,765);
       else if(op1=='k') c=rgbn(sizeof(bluered2)/sizeof(*bluered2),bluered2,s,765);
       else if(op1=='h') {
         min=max=c0;
         avg=rgbaggr(c,&min,&max);
         avg+=rgbaggr(b,&min,&max);
         avg+=rgbaggr(bits[i+1],&min,&max);
         avg+=rgbaggr(p[i],&min,&max);
         avg+=rgbaggr(bits[i+width],&min,&max);
         avg/=15;
         if(min<max&&(min>0||max<255)) {
           c0=c0==min?0:c0==max?255:c0<avg?(c0-min)*127/(avg-min):128+(c0-avg)*127/(max-avg);
           c0=(3*d0+c0)/4;
           c1=c1==min?0:c1==max?255:c1<avg?(c1-min)*127/(avg-min):128+(c1-avg)*127/(max-avg);
           c1=(3*d1+c1)/4;
           c2=c2==min?0:c2==max?255:c2<avg?(c2-min)*127/(avg-min):128+(c2-avg)*127/(max-avg);
           c2=(3*d2+c2)/4;
           c3=1;
         }
       } else if(op1=='n'||op1=='N'||op1=='M') {
         int l=32,s0=s/l,s1=rgbsum(b)/l,s2=rgbsum(bits[i+1])/l,s3=rgbsum(p[i])/l,s4=rgbsum(bits[i+width])/l;
         if(s0<s1||s0<s2||s0<s3||s0<s4) c=0;
         else if(op1=='M') c=0xffffff;
         else if(op1=='N') c=i765(c,s0*l,0);
       } else if(op1=='e') {
         int div=2,p1=p[i-1],p2=p[i];
         c0=128+(24*c0-8*((b&255)+(p1&255)+(p2&255)))/div;
         if(c0<0) c0=0;else if(c0>255) c0=255;
         c1=128+(24*c1-8*(((b>>8)&255)+((p1>>8)&255)+((p2>>8)&255)))/div;
         if(c1<0) c1=0;else if(c1>255) c1=255;
         c2=128+(24*c2-8*(((b>>16)&255)+((p1>>16)&255)+((p2>>16)&255)))/div;
         if(c2<0) c2=0;else if(c2>255) c2=255;
         c3=1;
       } else if(op1=='d') {
         min=max=c;
         rgbaggr3(b,&min,&max);
         rgbaggr3(bits[i+1],&min,&max);
         rgbaggr3(p[i],&min,&max);
         rgbaggr3(bits[i+width],&min,&max);
         c0=255-8*((max&255)-c0);if(c0<0) c0=0;
         c1=255-8*(((max>>8)&255)-c1);if(c1<0) c1=0;
         c2=255-8*(((max>>16)&255)-c2);if(c2<0) c2=0;
         c3=1;
       } else if(op1=='s') {
         c0=(3*c0*767/765)&511;if(c0>255) c0=511-c0;
         c1=(3*c1*767/765)&511;if(c1>255) c1=511-c1;
         c2=(3*c2*767/765)&511;if(c2>255) c2=511-c2;
         c3=1;
       } else if(op1=='t') {
         c=i765(c,s<256?3*s:s<510?765-3*(s-255):3*(s-510),1);
       } else if(op1=='T') {
         c=i765(c,s<256?765-3*s:s<510?3*(s-255):765-3*(s-510),1);
       } else if(op1=='u'||op1=='U') {
         int m=s,d=0,mm;
         if(m>255) m-=255,d++;
         if(m>255) m-=255,d++;
         if((d==1)^(op1=='U')) m^=255;
         mm=c0;if(c1>mm) mm=c1;if(c2>mm) mm=c2;
         if(mm==0) c=0;
         else {
           c0=c0*m/mm;
           c1=c1*m/mm;
           c2=c2*m/mm;
           c3=1;
         }
       } else if(op1=='4') {
         c0=c0/16*17;
         c1=c1/16*17;
         c2=c2/16*17;
         c3=1;
       } else if(op1=='6') {
         c=(s/48)*0x111111;
       } else if(op1=='I') {
         c=(rgbsum(c)*8/766)&1?0xffffff:0;
       }
       bits[i]=c3?c0|(c1<<8)|(c2<<16):c;
       p[i-1]=b;
       b=b2;
     }
     p[i-1]=b;
     p[i]=bits[i];
    }
    free(p);
    }
    Flags(data,width,height,flags2);
    if(findex) CFilter(data,width,height,fcolor[findex],fmode);
    BitBlt(dc,x,y,bi.biWidth,bi.biHeight,dc2,0,0,SRCCOPY);
  } else if(bits2==NULL) {
    copy=malloc(size);
    memcpy(copy,bits,size);
  } else {
    int i,j,t=GetTickCount(),d=0;
    for(j=0;j<height;j++,bits+=width,bits2+=width2)
     for(i=0;i<width;i++) {
      int c2=bits[i],c=(c2^bits2[i])&0xffffff;
      d|=c;
      if(xorop=='e') c=c?0x0:0xffffff;
      else if(xorop=='a') c=c?bits[i]^0x808080:0xffffff;
      else if(xorop=='b') c=c?bits2[i]^0x808080:0xffffff;
      else if(xorop=='c') c=c?rgbmin(bits[i])>240?0:bits[i]:0xffffff;
      else if(xorop=='d') c=c?rgbmin(bits2[i])>240?0:bits2[i]:0xffffff;
      else if(xorop=='i') c=c^0xffffff;
      else if(xorop=='X') c=rgbmix(bits[i],bits2[i],1,3);
      else if(xorop=='x') c=rgbmix(bits[i],bits2[i],2,3);
      else if(xorop=='m') c=rgbmix(bits[i],bits2[i],1,2);
      else if(xorop=='-') c=0xffffff^rgbdiff(bits[i],bits2[i],4);
      else if(xorop=='+') c=rgbembo(bits[i],bits2[i],4);
      else if(xorop=='|') c=c?rgbxmix(bits[i],bits2[i]):bits[i];
      else if(xorop=='<') c=c?rgbmin3(bits[i],bits2[i]):bits[i];
      else if(xorop=='>') c=c?rgbmax3(bits[i],bits2[i]):bits[i];
      else if(xorop=='=') c=c?rgbmix(bits[i],0x0,1,2):rgbmix(bits[i],0xffffff,1,2);
      else if(xorop=='2') c=bits2[i];
      else if(xorop=='~') c=t&512?bits[i]:bits2[i];
      bits[i]=c;
      if(diff) bits2[i]=c2;
     }
    if(diff&&!d) copy--;
    else BitBlt(dc,x,y,bi.biWidth,bi.biHeight,dc2,0,0,SRCCOPY);
  }
  SelectObject(dc2,hbmp2);
  DeleteObject(hbmp);
  DeleteDC(dc2);
  return copy;
}

char Shift() { return 0!=(0x8000&(GetKeyState(VK_SHIFT)));}
char Ctrl() { return 0!=(0x8000&(GetKeyState(VK_CONTROL)));}
char Alt() { return 0!=(0x8000&(GetKeyState(VK_MENU)));}
char LShift() { return 0!=(GetKeyState(VK_LSHIFT)&0x8000); }
char RShift() { return 0!=(GetKeyState(VK_RSHIFT)&0x8000); }
char LCtrl() { return 0!=(GetKeyState(VK_LCONTROL)&0x8000);}
char RCtrl() { return 0!=(GetKeyState(VK_RCONTROL)&0x8000);}
char LAlt() { return 0!=(GetKeyState(VK_LMENU)&0x8000);}
char RAlt() { return 0!=(GetKeyState(VK_RMENU)&0x8000);}
char CapsLock() { return 0!=(GetKeyState(VK_CAPITAL)&0x1);}
char NumLock() { return 0!=(GetKeyState(VK_NUMLOCK)&0x1);}  char NumLockI() { return !NumLock();}
char space,tab;
int curx,cury;

void Redraw() {
  int w,h,sw,sh;
  RECT r;
  int sx,sy,rgb;
  
  if(pause) return;
  if(!CapsLock()) {
    POINT cur;
    GetCursorPos(&cur);
    curx=cur.x,cury=cur.y;
  }
  GetClientRect(hWnd1,&r);
  rgb=xyrgb?GetPixel(hDCDesktop,curx,cury):0;
  w=r.right;
  h=r.bottom;
  sw=(scale*w/SCALE+zoom-1)/zoom;
  sh=(scale*h/SCALE+zoom-1)/zoom;
  w=w;
  h=h;
  if(m3d>1) sw/=2;
  sx=curx-sw/2;sy=cury-sh/2;
  sx=IntToRange(sx,0,DesktopWidth-sw),sy=IntToRange(sy,0,DesktopHeight-sh);
  
 // UpdateHalftone();
  if(m3d>2) {
    StretchBlt(
      hDC,m3di?w/2:0,0,w/2,h
      ,full?hDC2:hDCDesktop,sx,sy+sh/2,sw,sh/2
      ,not?NOTSRCCOPY:SRCCOPY
    );
    StretchBlt(
      hDC,m3di?0:w/2,0,w/2,h
      ,full?hDC2:hDCDesktop,sx,sy,sw,sh/2
      ,not?NOTSRCCOPY:SRCCOPY
    );
  } else if(m3d>0) {
    StretchBlt(
      hDC,m3di?w/2:0,0,w/2,h
      ,full?hDC2:hDCDesktop,sx+sw/2,sy,sw/2,sh
      ,not?NOTSRCCOPY:SRCCOPY
    );
    StretchBlt(
      hDC,m3di?0:w/2,0,w/2,h
      ,full?hDC2:hDCDesktop,sx,sy,sw/2,sh
      ,not?NOTSRCCOPY:SRCCOPY
    );
  } else {
    char proc=xorop||op1||flags1||flags2||flags3||findex,skip=0;
    if(proc&&!full) {
      BitBlt(hDC2,sx,sy,sw,sh,hDCDesktop,sx,sy,SRCCOPY);
      skip=!!GetBits(hDC2,sx,sy,sw,sh,xorbits+(DesktopHeight-sy-sh)*DesktopWidth+sx,DesktopWidth,xorop,op1,flags1,flags2,flags3);
    }
    if(!skip) StretchBlt(
      hDC,0,0,w,h
      ,full||proc?hDC2:hDCDesktop,sx,sy,sw,sh
      ,not?NOTSRCCOPY:SRCCOPY
    );
  }
  rgb2=GetPixel(hDC,w/2,h/2);
  if(cross) {
    int cx=(curx-sx)*zoom*SCALE/scale+zoom/2,cy=(cury-sy)*zoom*SCALE/scale+zoom/2;
    MoveToEx(hDC,cx,0,NULL);LineTo(hDC,cx,h);
    MoveToEx(hDC,0,cy,NULL);LineTo(hDC,w,cy);
  }
  if(xyrgb) {
    char l;
    register int r,g,b,y;
    SIZE ts;

    l=sprintf(xyrgbText,xyrgb==2?"left:%ld;top:%ld;":xyrgb==3?"%ld,%ld":"%ld,%ld:",curx-rel.x,cury-rel.y);
    //rgb=GetPixel(hDCDesktop,curx,cury);
    if(rgb==-1) rgb=rgb2;
    r=rgb&255,g=(rgb>>8)&255,b=(rgb>>16)&255;
    switch(xyrgb) {
     case 1:l+=sprintf(xyrgbText+l,"#%02X%02X%02X",r,g,b);break;
     case 4:l+=sprintf(xyrgbText+l,"%d,%d,%d",r,g,b);break;
     case 5:l+=sprintf(xyrgbText+l,"%.3f,%.3f,%.3f",r/255.0,g/255.0,b/255.0);break;
     case 6:l+=sprintf(xyrgbText+l,"%d",rgb);break;
    }
    GetTextExtentPoint32(hDC,xyrgbText,l,&ts);
    y=h-ts.cy;
    TextOut(hDC,0,y,xyrgbText,l);
    //BitBlt(hDC,0,y,ts.cx,ts.cy,NULL,0,0,DSTINVERT);
    //BitBlt(hDC,0,y,ts.cx,ts.cy,NULL,0,0,DSTINVERT);
  }
}

void PrintHelp() {
  MessageBox(NULL,
     "Options:\n"
     "  -t TIMEOUT : -t 333 ( timeout in milliseconds (-t 3x) [100])\n"
     "  -z ZOOM : -z 4 ( zoom [2])\n"
     "  -H : halftone\n"
     "  -g WIDTHxHEIGHT+X+Y : -g 128x64+100+100 initial window position\n"
     "  -c : draw cross\n"
     "  -x : draw x,y (-f 24[n] font size)\n"
     "  -h : this help\n"
     "\n"
     "Buttons:\n"
     "Left: drag to move\n"
     "Right: click to hide\n"
     "\n"
     "Systray Icon:\n"
     "Left click: show window\n"
     "Right click: hide window/exit WMagnify\n"
     "\n"
     "Keys:\n"
     "F1: this help\n"
     "F12: mouse speed (shift=>slow ctrl=>fast)\n"
     "1..9: set zoom to 1..9\n"
     "<Ctrl> 0..9: when <Ctrl> is down type zoom as number \n"
     "+,-: increase/decrease zoom \n"
     "<Ctrl>+,-: multiple/divide zoom 2x (ctrl with unzoom)\n"
     "h: toggle halftone\n"
     "i: toggle invert\n"
     "c: toggle cross draw\n"
     "x: toggle x,y,rgb draw\n"
     "r: toggle relative coords\n"
     "z: zoom fullscreen mode (F11)\n"
     "o: switch off xor screen (+shift or +ctrl to switch on)\n"
     "p: switch off filters (+shift,ctrl,... main filter)\n"
     "f: color filter (ctrl change mode)\n"
     "CapsLock: lock position (on video center)\n"
     "<Ctrl,Shift,Alt> k,l,m,F9,F10: filters input/output filters\n"
     "<Ctrl> C: copy text (-T tab separated)\n"
     "Esc,q: Exit\n"
    ,"Options",MB_OK
  );
}

NOTIFYICONDATA pnid;

void add_icon() {
  pnid.cbSize=sizeof(pnid);
  pnid.hWnd=hWnd1;
  pnid.uID=13;
  pnid.uFlags=NIF_ICON|NIF_MESSAGE;
  pnid.uCallbackMessage=WM_TASKBAR;
  pnid.hIcon=hIcon;
  Shell_NotifyIcon(NIM_ADD,&pnid); // NIM_ADD,NIM_MODIFY,NIM_DELETE
}   

void mod_icon() {
  RECT r;
  pnid.uFlags=NIF_TIP;
  GetWindowRect(hWnd1,&r);
  sprintf(pnid.szTip,"%dx %ld,%ld %ldx%ld",
    zoom,r.left,r.top,r.right-r.left,r.bottom-r.top);
  Shell_NotifyIcon(NIM_MODIFY,&pnid); 
}

void del_icon() {
  Shell_NotifyIcon(NIM_DELETE,&pnid);
}

long FAR PASCAL WindowProc (HWND hWnd, UINT iMessage,
    WPARAM wParam, LPARAM lParam) {
  int r=0;
 
  switch(iMessage)  {
   case WM_TASKBAR:
    switch(lParam) {
     case WM_LBUTTONDOWN:
      if(full) BitBlt(hDC2,0,0,DesktopWidth,DesktopHeight,hDCDesktop,0,0,SRCCOPY);
      ShowWindow(hWnd1,(show=SW_NORMAL)|(full?SW_MAXIMIZE:0));
      SetFocus(hWnd1);
     break;
/*     case WM_RBUTTONDBLCLK:
      quit=1;
      break; */
     case WM_RBUTTONDOWN:
      if(show!=SW_HIDE)
        ShowWindow(hWnd1,show=SW_HIDE);
      else
        quit=1;
     break;
     case WM_RBUTTONUP:
      if(quit)
        PostQuitMessage(0);
     break;
    }
    break;
   case WM_DESTROY:
    PostQuitMessage(0);
    break;
   case WM_SETFOCUS:
    *clipboard=0;
    if((HWND)wParam==hWnd2) {
      SetFocus(hWnd1);
    }
   default:
    r=DefWindowProc(hWnd, iMessage, wParam, lParam);
  }
  return r;
}

void MoveCursor(char color,int dx,int dy) {
  POINT pt;
  if(GetCursorPos(&pt)) {
    if(color&&pt.x>=0&&pt.x<DesktopWidth&&pt.y>=0&&pt.y<DesktopHeight) {
      int *bits,c,i,e=dy?DesktopHeight:DesktopWidth,n=0;
      dx=dx<0?-1:dx>0?1:0;dy=dx?0:dy<0?-1:dy>0?1:0;
      bits=GetBits(full?hDC2:hDCDesktop,dy?pt.x:0,dx?pt.y:0,dy?1:DesktopWidth,dx?1:DesktopHeight,NULL,0,0,0,0,0,0);
      i=dx?pt.x:pt.y;
      if(dy) i=e-1-i;dy=-dy;
      c=bits[i]&0xffffff;
      for(;i+=dx+dy,i>=0&&i<e;) {
        int c2=bits[i]&0xffffff;
        if(c2!=c) { if(n) i-=dx+dy;break;}
        n++;
      }
      free(bits);
      if(dx) pt.x=i;else pt.y=e-1-i;
      /*int c=GetPixel(hDCDesktop,pt.x,pt.y),n=0;
      dx=dx<0?-1:dx>0?1:0;dy=dy<0?-1:dy>0?1:0;
      if(c!=CLR_INVALID) do {
        pt.x+=dx;pt.y+=dy;
        int c2=GetPixel(hDCDesktop,pt.x,pt.y);
        if(c2!=c) {if(n) pt.x-=dx,pt.y-=dy;break;}
        n++;
      } while(pt.x>=0&&pt.y>=0&&pt.x<DesktopWidth&&pt.y<DesktopHeight);
      */
    } else {
      pt.x+=dx;pt.y+=dy;
    }
    if(pt.x<0) pt.x=0;else if(pt.x>=DesktopWidth) pt.x=DesktopWidth-1;
    if(pt.y<0) pt.y=0;else if(pt.y>=DesktopHeight) pt.y=DesktopHeight-1;
    SetCursorPos(pt.x,pt.y);
  }
}

void XORMove(char dx,char dy) {
  if(dx>0) memmove(xorbits+1,xorbits,DesktopWidth*DesktopHeight*4-1);
  else if(dx<0) memmove(xorbits,xorbits+1,DesktopWidth*DesktopHeight*4-1);
  if(dy<0) memmove(xorbits+DesktopWidth,xorbits,DesktopWidth*(DesktopHeight-1)*4);
  else if(dy>0) memmove(xorbits,xorbits+DesktopWidth,DesktopWidth*(DesktopHeight-1)*4);
}

void Make2() {
  hDC2=CreateCompatibleDC(hDCDesktop);
  SelectObject(hDC2,CreateCompatibleBitmap(hDCDesktop,DesktopWidth,DesktopHeight));
  gama(1,2,lightmap);
  gama(0,2,darkmap);
}

void Full(int value) {
  if(value==full) return;
  if(!value) {
    ShowCursor2(TRUE);
    ShowWindow(hWnd1,SW_NORMAL);
    full=0;
  } else {
    if(!hDC2) Make2();
    ShowWindow(hWnd1,SW_HIDE);
    Sleep(250);
    //InvalidateRect(NULL,NULL,FALSE);
    BitBlt(hDC2,0,0,DesktopWidth,DesktopHeight,hDCDesktop,0,0,SRCCOPY);
    if(xorop||op1||flags1||flags2||flags3||findex) GetBits(hDC2,0,0,DesktopWidth,DesktopHeight,xorbits,DesktopWidth,xorop,op1,flags1,flags2,flags3);
    ShowWindow(hWnd1,SW_SHOWMAXIMIZED);
    full=1;
    ShowCursor2(zoom<2);
  } 
}

void MouseSpeed(int speed) {
  if(speed<1||speed>20) speed=10;
  SystemParametersInfo(0x71,0,(void*)speed,0);//SPI_SETMOUSESPEED
}

//short curand[]={0x7ffd,0x7ffd,0x7ffd,0x7ffd,0x7ffd,0x7ffd,0x0101,0xffff,0x0101,0x7ffd,0x7ffd,0x7ffd,0x7ffd,0x7ffd,0x7ffd};
//short curxor[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
short curand[]={0x1ff1,0x1ff1,0x1ff1,0x1ff1,0x0101,0x0101,0x0101,0xffff,0x0101,0x0101,0x0101,0x1ff1,0x1ff1,0x1ff1,0x1ff1};
short curxor[]={0x0000,0x4004,0x4004,0x4004,0x4004,0x7c7c,0x0000,0x0000,0x0000,0x7c7c,0x4004,0x4004,0x4004,0x4004,0x0000};

char rotate(char ch,char *flags) {
  char *h;
  for(h=flags;*h;h++)
    if(ch==*h&&h[1]) return h[1];
  return *flags;
}

int Timeout(char **hh) {
  char *h=*hh;
  int ms=strtol(h,&h,0);
  if(*h=='x') {
    h++;
    if(ms) ms=1000/ms;
  } 
  *hh=h;
  return IntToRange(ms,10,60000);
}

void ColorShift(char d) {
  int s=(flags3>>11)&31;
  if(d<0) d+=30;
  s=(s+d)%30;
  flags3=(flags3&~0xf800)|(s<<11);
}

int PASCAL WinMain(HINSTANCE hCurInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow) {
  MSG Message;
  WNDCLASS WndClass;
  char *h,fontb=1;
  HKL hkl=LoadKeyboardLayout("00000409",0);
  int font=20;


  hInstance=hCurInstance;
  hDCDesktop=GetWindowDC(GetDesktopWindow());
  DesktopWidth=GetDeviceCaps(hDCDesktop,HORZRES);
  DesktopHeight=GetDeviceCaps(hDCDesktop,VERTRES);
  hDC2=NULL;

  //MessageBox(NULL,lpCmdLine,"param",MB_OK);
  for(h=lpCmdLine;*h;h++) {
    if(*h=='-') {
     next:
      h++;
      switch(*h) {
       case 'f':
        font=IntToRange(strtol(h+1,&h,0),6,48);
        if(*h=='n'||*h=='N') h++,fontb=0;
	break;
       case 't':
        h++,timeout=Timeout(&h);
	break;
       case 'z':
        zoom=IntToRange(strtol(h+1,&h,0),1,16);
        break;
       case 'Z':
        scale=strtol(h+1,&h,0);
        scale=IntToRange(scale<=SCALE?scale*=SCALE:scale,SCALE,16*SCALE);
        break;
       case 's':
        sticky=IntToRange(strtol(h+1,&h,0),0,128);
        break;
       case 'g':
        InitialWidth=IntToRange(strtol(h+1,&h,0),1,DesktopWidth);
        InitialHeight=IntToRange(strtol(h+1,&h,0),1,DesktopWidth);
        if(*h!=' '&&*h) {
          InitialX=IntToRange(strtol(h+1,&h,0),0,DesktopWidth-InitialWidth);
          InitialY=IntToRange(strtol(h+1,&h,0),0,DesktopHeight-InitialHeight);
        }
        break;
       case 'H':halftone=1;goto next;
       case 'T':cliptab=1;goto next;
       case 'I':not=1;goto next;
       case 'c': cross=1; goto next;
       case 'x': xyrgb=1; goto next;
       case '\t':
       case ' ':
       case 0:
	break;
       case 'h':
       default:
	PrintHelp();
        exit(*h!='h');
	break;
      }
    }
    if(*h!=' ') {
      if(*h) timeout=Timeout(&h);
      break;
    }
  }

  hIcon = LoadIcon(hInstance,"IconWMagnify"); 

  WndClass.cbClsExtra = 0;
  WndClass.cbWndExtra = 0;
  WndClass.hbrBackground = NULL;//GetStockObject(WHITE_BRUSH);
  WndClass.hCursor = CreateCursor(hInstance,7,7,15,15,curand,curxor);//LoadCursor(NULL, IDC_CROSS);
  WndClass.hIcon = hIcon;
  WndClass.hInstance = NULL;
  WndClass.lpfnWndProc = WindowProc;
  WndClass.lpszClassName = "WMagnify";
  WndClass.lpszMenuName = NULL;
  WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  RegisterClass(&WndClass);


  hWnd2 = CreateWindowEx (WS_EX_TOPMOST,"WMagnify","WMagnify"
    ,WS_POPUP|WS_SIZEBOX,0,0,8,8,NULL,NULL,NULL,NULL
  );
  
  hWnd1 = CreateWindowEx (WS_EX_TOPMOST,"WMagnify","WMagnify"
    ,WS_POPUP|WS_SIZEBOX,InitialX,InitialY,InitialWidth,InitialHeight
    ,hWnd2,NULL,NULL,NULL
  );
  hDC=GetDC(hWnd1);
  // for Cross drawing
  SelectObject(hDC,CreatePen(PS_SOLID,0,0x808080));
  SetROP2(hDC,R2_XORPEN);
  SelectObject(hDC,CreateFont(font,0,0,0,fontb?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,"MS Sans Serif"));
  

  ShowWindow (hWnd1,show=SW_NORMAL);
  SetFocus(hWnd1);
  UpdateHalftone();
  Timer=SetTimer(hWnd1,TimerId,timeout,NULL);

  add_icon();
  mod_icon();
  rel.x=rel.y=0;

  while (GetMessage (&Message, 0, 0, 0)) {
    static int pmx,pmy,lmx,lmy,nmx,nmy;
    static int lwx,lwy;
    RECT r;
    int digit;
    static int ControlZoom;

    TranslateMessage(&Message);
    //printf("Message %d %w=%d %l=%d\n",Message.message,Message.wParam,Message.lParam);
    switch(Message.message)  {
     case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(Message.hwnd,&ps);
        Redraw();
        EndPaint(Message.hwnd,&ps);
      } 
      break;
     case WM_TIMER:
      Redraw();
      while(PeekMessage(&Message,NULL,WM_TIMER,WM_TIMER,PM_REMOVE));
      break;
     case WM_COMMAND:
      switch(LOWORD(Message.wParam)) {
      }
      break;
     case WM_SIZE:
      mod_icon();
      Redraw();
      break;
//     case WM_DESTROY:
//      goto quit;
     case WM_SYSKEYDOWN:
     case WM_KEYDOWN:
      /* { char s[32];
        sprintf(s,"%c %d %x",Message.wParam,Message.wParam,Message.wParam);
        MessageBox(NULL,s,"KeyDown",MB_OK);
      } */
      if(Message.wParam=='Y'||Message.wParam=='Z'
          ||(Message.wParam>=VK_OEM_1&&Message.wParam<=VK_OEM_3)||(Message.wParam>=VK_OEM_4&&Message.wParam<=VK_OEM_8)) {
        char scan=(Message.lParam>>16)&127;
        int vk=MapVirtualKeyEx(scan,3,hkl);
        //printf("%d %d %d\n",scan,vk,Msg.wParam);
        Message.wParam=vk;//scan<40?'Y':'Z';
      }
      switch(Message.wParam) {
       case VK_SPACE:space=1;break;
       case VK_TAB:tab=1;break;
       case VK_F1:
        PrintHelp();
        break;
       case VK_F3:
        m3d=(m3d+1)%4;
        break;
       case VK_F4:if(Alt()) goto q;m3di^=1;break;
       case VK_F12:MouseSpeed(Ctrl()?15:Shift()?5:10);break;
       case VK_CONTROL:
        ControlKey=1;
        ControlZoom=0;
        break;
       case VK_HOME:MoveCursor(LCtrl(),-100,0);break;
       case VK_END:MoveCursor(LCtrl(),+100,0);break;
       case VK_PRIOR:MoveCursor(LCtrl(),0,-100);break;
       case VK_NEXT:MoveCursor(LCtrl(),0,+100);break;
       case VK_LEFT:if(xorbits&&Alt()) XORMove(-1,0);else MoveCursor(LCtrl(),ControlKey?Shift()?-50:-10:Shift()?-5:-1,0);break;
       case VK_RIGHT:if(xorbits&&Alt()) XORMove(+1,0);else MoveCursor(LCtrl(),ControlKey?Shift()?50:10:Shift()?5:1,0);break;
       case VK_UP:if(xorbits&&Alt()) XORMove(0,-1);else MoveCursor(LCtrl(),0,ControlKey?Shift()?-50:-10:Shift()?-5:-1);break;
       case VK_DOWN:if(xorbits&&Alt()) XORMove(0,+1);else MoveCursor(LCtrl(),0,ControlKey?Shift()?50:10:Shift()?5:1);break;
       case VK_NUMPAD0:
       case VK_NUMPAD1:
       case VK_NUMPAD2:
       case VK_NUMPAD3:
       case VK_NUMPAD4:
       case VK_NUMPAD5:
       case VK_NUMPAD6:
       case VK_NUMPAD7:
       case VK_NUMPAD8:
       case VK_NUMPAD9:
        digit=Message.wParam-VK_NUMPAD0;
        goto number;
       case '0':
       case '1':
       case '2':
       case '3':
       case '4':
       case '5':
       case '6':
       case '7':
       case '8':
       case '9':
        digit=Message.wParam-'0';
       number:
        if(ControlKey) {
          ControlZoom=ControlZoom*10+digit;
          break;
        } else
          zoom=digit;
       rezoom:
        zoom=IntToRange(zoom,1,64);
        scale=IntToRange(scale,SCALE,16*SCALE);
        if(full) ShowCursor2(zoom<2);
        mod_icon();
        Redraw();
        break;
       case VK_ADD:
       case 0xbb:
        if(ControlKey) scale--;
        else if(Shift()) zoom<<=1; else zoom++;
        goto rezoom;
       case VK_SUBTRACT:
       case 0xbd:
        if(ControlKey) scale++;
        else if(Shift()) zoom>>=1; else zoom--;
        goto rezoom;
       case 'C':
        if(ControlKey) {
          char *h;
          if(Shift()) clipshift=(clipshift+1)%3;
          if(xyrgb==0) { 
            POINT cur;
            GetCursorPos(&cur);cur.x-=rel.x;cur.y-=rel.y;
            sprintf(h=xyrgbText,"%ld,%ld",cur.x,cur.y);
          } else if(clipshift||xyrgb==2||xyrgb==3) h=xyrgbText;
          else {
            h=xyrgbText;
            if(!cliptab) {
              h=strchr(h,':');
              if(h) h++;
            }
          }
          if(h&&*h&&strlen(clipboard)+strlen(h)+4<sizeof(clipboard)) {
            if(*clipboard) strcat(clipboard,xyrgb==3?" l":"\r\n"); 
            strcat(clipboard,h);
            if(cliptab) strrep(clipboard,",:;",'\t');
            StrSetClipboard(clipboard);
          }
        } else {
          cross=!cross;
          Redraw();
        } break;
       /*case 'V':
        if(ControlKey) {
          POINT pt;
          HWND wnd;
          GetCursorPos(&pt);
          wnd=WindowFromPoint(pt);
          //PostMessage(wnd,WM_KEYDOWN,Message.wParam,Message.lParam);
          //SetForegroundWindow(wnd);
          //ControlKey=0;
          PostMessage(wnd,WM_KEYUP,Message.wParam,Message.lParam);
        }
        break;*/
       case 'X':
        xyrgb=(xyrgb+(Shift()?6:1))%7;
        Redraw();
        break;
       case 'R':{
        int x=rel.x,y=rel.y;
        GetCursorPos(&rel);
        if(rel.x==x&&rel.y==y) rel.x=rel.y=0;
        if(!xyrgb) xyrgb=2,clipshift=1;
        cross=1;
        Redraw();
        } break;
       case 'U':
        diff=!diff;
        break;
       case 'W':flags3=(flags3&~0xf00000)|((Shift()+2*Ctrl()+4*Alt()+8*space)<<20);goto op;
       case 'O':
        if(Ctrl()||Shift()||Alt()||space||tab) {
          char xp2=xorop;
          xorop=tab?space?'2':Shift()?'<':Ctrl()?'>':'=':space?Shift()?'+':Ctrl()?'-':Alt()?'|':rotate(xp2,"~Xmx2"):Ctrl()?(Shift()?2:1)+(RCtrl()?'b':'a'):RShift()?'e':'i';
          if(!xp2) {
            Make2();
            if(xorbits) free(xorbits);
            xorbits=GetBits(full?hDC2:hDCDesktop,0,0,DesktopWidth,DesktopHeight,NULL,0,0,0,0,0,0);
          }
        } else
          xorop=diff=0;
        Redraw();
        break;
       case 'S':
        flags3=(flags3&~15)|(NumLock()*8+(Ctrl()?Shift()?RShift()?4:5:RCtrl()?7:3:Shift()?RShift()?6:2:1));
        goto op;
       case 'N':
        flags3^=Shift()?RShift()?0x400:0x200:0x100;
        goto op;
       case VK_F2:flags3=(flags3&~(3<<25))|((flags3+(1<<25))&(3<<25));goto op;
       case 'T':flags3^=0x8000000;goto op;
       case 'E':{
        char x=2*Ctrl()+Shift();
        if(Alt()) flags3^=1<<24;
        else flags3^=(x?x*2:1)<<17;};
        goto op;
       case 'D':
        flags3=(flags3&~0xf0)|0x80|((4*NumLock()+2*Ctrl()+Shift())<<4);
        goto op;
       case 'P':
        if(Ctrl()||Shift()||Alt()||space||tab) {
          char shf=Shift(),rshf=RShift();
          op1=tab?'I':Ctrl()?RCtrl()?LCtrl()?'4':shf?'s':space?rotate(op1,"PL"):rotate(op1,"plKk"):rshf?space?'U':'T':shf?space?'u':'t':'h':Alt()?RAlt()?LAlt()?'8':'d':space?'M':'N':space?'e':rshf?LShift()?'6':'G':'g';
        } else op1=flags1=flags2=flags3=findex=0;
       op:
        Make2();
        Redraw();
        break;
       case 'L':
        if(Shift()) {if(RShift()) flags2^=4<<space; else flags1^=4<<space;}
        if(Ctrl()) {if(RCtrl()) flags2^=16<<space; else flags1^=16<<space;}
        if(Alt()) {if(RAlt()) flags2^=0x10000000; else flags1^=0x10000000;}
        goto op;
       case 'J':
        if(Shift()) ColorShift(1-2*RShift());
        else flags3^=0x10000;
        goto op;
       case 'K':
        if(Shift()) {if(RShift()) flags2^=2;else flags1^=2;}
        if(Ctrl()) {if(RCtrl()) flags2^=0x20000000; else flags1^=0x20000000;}
        goto op;
       case 'I':
        if(Ctrl()&&Shift()) { if(RShift()) flags2^=0x40000000;else flags1^=0x40000000;}
        else if(Shift()) { if(RShift()) flags2^=1;else flags1^=1;}
        else if(Ctrl()) { if(RCtrl()) flags2^=0x4000000;else flags1^=0x4000000;}
        else if(Alt()) { if(RAlt()) flags2^=0x8000000;else flags1^=0x8000000;}
        else {not=!not;Redraw();break;}
        goto op;
       case VK_F5:
       case VK_F6:
       case VK_F7:{
        short f=0x100<<(Message.wParam-VK_F5);
        if(Shift()) {if(RShift()) flags2^=f; else flags1^=f;}
        } goto op;
       case VK_F8:{
        int *f=Shift()?RShift()?&flags2:&flags1:NULL;
        if(f) {
          char p=(*f>>12)&7;
          *f=((*f)&~0x7000)|(((p+1)%6)<<12);
        } else flags1&=~0x7000,flags2&=~0x7000;
        } goto op;
       case VK_F9:
        if(Shift()) {if(RShift()) flags2^=0x40; else flags1^=0x40;}
        if(Ctrl()) {if(RCtrl()) flags2^=0x80; else flags1^=0x80;}
        if(Alt()) {if(RAlt()) flags2^=0x800; else flags1^=0x800;}
        if(space) flags2^=0x80000;
        if(tab) flags2^=0x80000000;
        goto op;
       case VK_F10:
        if(Shift()) {if(RShift()) flags2^=0x20000; else flags1^=0x20000;}
        if(Ctrl()) {if(RCtrl()) flags2^=0x10000; else flags1^=0x10000;}
        if(Alt()) {if(RAlt()) flags2^=0x40000; else flags1^=0x40000;}
        if(space) flags2^=0x100000;
        goto op;
       case 'F':
        if(Ctrl()) fmode=(fmode+1)%3;
        else findex=(findex+(Shift()?fcolors-1:1))%fcolors;
        goto op;
       case 'G':
        if(Shift()) {
            int *f=RShift()?&flags2:&flags1;
            int g=(((*f)>>24)+1)&3;
            *f=((*f)&~0x3000000)|(g<<24);
        }
        goto op;
       case 'M':
        if(Shift()) {if(RShift()) flags2^=0x200000; else flags1^=0x200000;}
        if(Ctrl()) {if(RCtrl()) flags2^=0x400000; else flags1^=0x400000;}
        goto op;
       case 'H':
	halftone=!halftone;
        UpdateHalftone();
        Redraw();
        break;
       case VK_F11:
       case 'Z':Full(!full);break;
       case 'A':
        pause=!pause;
        break;
       case VK_ESCAPE:
        if(full) {
          Full(FALSE);
          break;
        }
       case 'Q':q:
        PostQuitMessage(0);
       break;
      }
      break;
     case WM_SYSKEYUP:
     case WM_KEYUP:
      switch(Message.wParam) {
       case VK_SPACE:space=0;break;
       case VK_TAB:tab=0;break;
       case VK_CONTROL:
        ControlKey=0;
        if(ControlZoom>0) {
          zoom=ControlZoom;
          goto rezoom;
        }
        break;
      }
      break;
     case WM_LBUTTONDBLCLK:
      Full(!full);
      break;
     case WM_RBUTTONDOWN:
      ShowWindow(Message.hwnd,show=SW_HIDE); 
      //ShowWindow(Message.hwnd,SW_HIDE);
      break;
     case WM_LBUTTONDOWN:
      SetCapture(hWnd1);
      lmx=pmx=Message.pt.x;lmy=pmy=Message.pt.y;
      GetWindowRect(Message.hwnd,&r);
      lwx=r.left,lwy=r.top;
      break;
     case WM_LBUTTONUP:
      ReleaseCapture();
      mod_icon();
      break;
     case WM_MOUSEMOVE:
      while(PeekMessage(&Message,hWnd1,WM_MOUSEMOVE,WM_MOUSEMOVE,PM_REMOVE));
      nmx=Message.pt.x,nmy=Message.pt.y;
      if(Message.wParam&MK_LBUTTON&&(nmx!=lmx||nmy!=lmy)) {
        int nx,ny;
        int dx,dy;

        GetWindowRect(Message.hwnd,&r);
        nx=lwx+nmx-pmx,ny=lwy+nmy-pmy;
        if(nx<sticky) nx=0;if(ny<sticky) ny=0;
        dx=DesktopWidth-(r.right-r.left);
        dy=DesktopHeight-(r.bottom-r.top);
        if(nx>dx-sticky) nx=dx;if(ny>dy-sticky) ny=dy;
        
        SetWindowPos(Message.hwnd,HWND_TOPMOST,nx,ny,0,0
          ,SWP_DRAWFRAME|SWP_NOSIZE|SWP_NOZORDER
        );
      }
      lmx=nmx,lmy=nmy;
      break;
     case WM_MOUSEWHEEL:{
        short wh=Message.wParam>>16;
        if(tab) {
          ColorShift(wh<0?1:-1);
        } else if(Ctrl()) {
          if(wh>0&&scale>SCALE) scale--;
          else if(wh<0&&scale<16*SCALE) scale++;
        } else {
          if(wh>0&&zoom<32) zoom++;
          else if(wh<0&&zoom>1) zoom--;
        }
        UpdateHalftone();
        goto rezoom;
      } break;
     default:
      DispatchMessage(&Message);
    }
  }
 //quit:
  if(hDC2) DeleteDC(hDC2);
  KillTimer(hWnd1,TimerId);
  DeleteObject(SelectObject(hDC,GetStockObject(WHITE_PEN)));
  del_icon();
  DestroyWindow(hWnd2);
  ReleaseDC(GetDesktopWindow(),hDCDesktop);

  return Message.wParam;
}

