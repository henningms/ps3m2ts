
#define  STRICT

#include <windows.h>
#include <math.h>

#include "ssrc.h"
//#include "../WSL/StdProc.h"

#define  M      15

#ifndef M_PI
#define  M_PI    3.1415926535897932384626433832795028842
#endif

void __fastcall  rdft(int n, int isgn, REAL* a, int* ip, REAL* w);
double __fastcall  dbesi0(double x);

inline double __fastcall alpha(double a)
{
  if (a <= 21) return 0;
  if (a <= 50) return 0.5842*pow(a-21,0.4)+0.07886*(a-21);
  return 0.1102*(a-8.7);
}

inline double __fastcall win(double n,int len,double alp,double iza)
{
  return dbesi0(alp*sqrt(1-4*n*n/(((double)len-1)*((double)len-1))))/iza;
}

inline double __fastcall sinc(double x)
{
  return x == 0 ? 1 : sin(x)/x;
}

inline double __fastcall hn_lpf(int n,double lpf,double fs)
{
  double t = 1/fs;
  double omega = 2*M_PI*lpf;
  return 2*lpf*t*sinc(n*omega*t);
}

inline int __fastcall gcd(int x, int y)
{
    int t;

    while (y != 0) {
        t = x % y;  x = y;  y = t;
    }
    return x;
}

int __fastcall CanResample(int sfrq,int dfrq)
{
  if (sfrq==dfrq) return 1;
    int frqgcd = gcd(sfrq,dfrq);

  if (dfrq>sfrq)
  {
    int fs1 = sfrq / frqgcd * dfrq;

    if (fs1/dfrq == 1) return 1;
    else if (fs1/dfrq % 2 == 0) return 1;
    else if (fs1/dfrq % 3 == 0) return 1;
    else return 0;
  }
  else
  {
    if (dfrq/frqgcd == 1) return 1;
    else if (dfrq/frqgcd % 2 == 0) return 1;
    else if (dfrq/frqgcd % 3 == 0) return 1;
    else return 0;
  }
}

void __fastcall Buffer::Read(unsigned int size)
{
  if (size)
  {
    if (buf_data==size) buf_data=0;
    else
    {
      memmove(buffer,buffer+size,buf_data-size);
      buf_data-=size;
    }
  }
}

void __fastcall Buffer::Write(void * ptr,unsigned int size)
{
  if (!buffer)
  {
    buf_size=1024;
    while(buf_size<size) buf_size<<=1;
    buffer=(char*)malloc(buf_size);
    buf_data=0;
  }
  else if (buf_size<buf_data+size)
  {
    do
    {
      buf_size<<=1;
    } while(buf_size<buf_data+size);
    buffer=(char*)realloc(buffer,buf_size);
  }
  memcpy(buffer+buf_data,ptr,size);
  buf_data+=size;
}

void __fastcall Resampler_base::bufloop(int finish)
{
  unsigned int s;
  unsigned char * ptr=(unsigned char *)in.GetBuffer(&s);
  unsigned int done=0;
  while(done<s)
  {
    unsigned int d=Resample((unsigned char*)ptr,s-done,finish);
    if (d==0) break;
    done+=d;
    ptr+=d;
  }
  in.Read(done);
}

void __fastcall Resampler_base::make_inbuf(
                    int nsmplread,
                    int inbuflen,
                    unsigned char* rawinbuf,
                    REAL* inbuf,
                    int toberead)
{
  const int  MaxLoop = nsmplread * nch;
  const int  InbufBase = inbuflen * nch;

  memcpy(inbuf + InbufBase, rawinbuf, MaxLoop * sizeof(double));

  size_t  ClearSize = toberead - nsmplread;

  if(ClearSize) {
    memset(inbuf + InbufBase + MaxLoop, 0, ClearSize * nch * sizeof REAL);
  }
}

void __fastcall Resampler_base::make_outbuf(int nsmplwrt2, REAL* outbuf)
{
  unsigned char  rawoutbuf[8];
  const int  MaxLoop = nsmplwrt2 * nch;

  const int  CopySize = MaxLoop - delay;

  if(CopySize > 0) out.Write(outbuf + (MaxLoop - CopySize), CopySize * sizeof(double));

  delay = max(delay - MaxLoop, 0);
}

/*
#ifdef WIN32
extern "C" {_declspec(dllimport) int _stdcall MulDiv(int nNumber,int nNumerator,int nDenominator);}
#else
#define MulDiv(x,y,z) ((x)*(y)/(z))
#endif
*/

unsigned int __fastcall Resampler_base::GetLatency()
{
  return MulDiv(in.Size() / (8 * nch), 1000, sfrq) + MulDiv(out.Size() / (8 * nch), 1000, dfrq);
}

class Upsampler : public Resampler_base
{
  int frqgcd,osf,fs1,fs2;
  REAL **stage1,*stage2;
  int n1,n1x,n1y,n2,n2b;
  int filter2len;
  int *f1order,*f1inc;
  int *fft_ip;// = NULL;
  REAL *fft_w;// = NULL;
  //unsigned char *rawinbuf,*rawoutbuf;
  REAL *inbuf,*outbuf;
  REAL **buf1,**buf2;
  int spcount;
  int i,j;

    int n2b2;//=n2b/2;
    int rp;        // inbufのfs1での次に読むサンプルの場所を保持
    int ds;        // 次にdisposeするsfrqでのサンプル数
    int nsmplwrt1; // 実際にファイルからinbufに読み込まれた値から計算した
             // stage2 filterに渡されるサンプル数
    int nsmplwrt2; // 実際にファイルからinbufに読み込まれた値から計算した
             // stage2 filterに渡されるサンプル数
    int s1p;       // stage1 filterから出力されたサンプルの数をn1y*osfで割った余り
    int osc;
    REAL *ip,*ip_backup;
    int s1p_backup,osc_backup;
    int p;
    int inbuflen;

public:
  Upsampler(CONFIG& c) : Resampler_base(c)
  {
    fft_ip = NULL;
    fft_w = NULL;
    spcount = 0;

    filter2len = FFTFIRLEN; /* stage 2 filter length */

    /* Make stage 1 filter */

    {
    double aa = AA; /* stop band attenuation(dB) */
    double lpf,delta,d,df,alp,iza;
    double guard = 2;

    frqgcd = gcd(sfrq,dfrq);

    fs1 = sfrq / frqgcd * dfrq;

    if (fs1/dfrq == 1) osf = 1;
    else if (fs1/dfrq % 2 == 0) osf = 2;
    else if (fs1/dfrq % 3 == 0) osf = 3;
    else {
//      fprintf(stderr,"Resampling from %dHz to %dHz is not supported.\n",sfrq,dfrq);
//      fprintf(stderr,"%d/gcd(%d,%d)=%d must be divided by 2 or 3.\n",sfrq,sfrq,dfrq,fs1/dfrq);
//      exit(-1);
      return;
    }

    df = (dfrq*osf/2 - sfrq/2) * 2 / guard;
    lpf = sfrq/2 + (dfrq*osf/2 - sfrq/2)/guard;

    delta = pow(10.,-aa/20);
    if (aa <= 21) d = 0.9222; else d = (aa-7.95)/14.36;

    n1 = static_cast<int>(fs1/df*d+1);
    if (n1 % 2 == 0) n1++;

    alp = alpha(aa);
    iza = dbesi0(alp);
    //printf("iza = %g\n",iza);

    n1y = fs1/sfrq;
    n1x = n1/n1y+1;

    f1order = (int*)malloc(sizeof(int)*n1y*osf);
    for(i=0;i<n1y*osf;i++) {
      f1order[i] = fs1/sfrq-(i*(fs1/(dfrq*osf)))%(fs1/sfrq);
      if (f1order[i] == fs1/sfrq) f1order[i] = 0;
    }

    f1inc = (int*)malloc(sizeof(int)*n1y*osf);
    for(i=0;i<n1y*osf;i++) {
      f1inc[i] = f1order[i] < fs1/(dfrq*osf) ? nch : 0;
      if (f1order[i] == fs1/sfrq) f1order[i] = 0;
    }

    stage1 = (REAL**)malloc(sizeof(REAL *)*n1y);
    stage1[0] = (REAL*)malloc(sizeof(REAL)*n1x*n1y);

    for(i=1;i<n1y;i++) {
      stage1[i] = &(stage1[0][n1x*i]);
      for(j=0;j<n1x;j++) stage1[i][j] = 0;
    }

    for(i=-(n1/2);i<=n1/2;i++)
      {
    stage1[(i+n1/2)%n1y][(i+n1/2)/n1y] = win(i,n1,alp,iza)*hn_lpf(i,lpf,fs1)*fs1/sfrq;
      }
    }

    /* Make stage 2 filter */

    {
    double aa = AA; /* stop band attenuation(dB) */
    double lpf,delta,d,df,alp,iza;
    int ipsize,wsize;

    delta = pow(10.,-aa/20);
    if (aa <= 21) d = 0.9222; else d = (aa-7.95)/14.36;

    fs2 = dfrq*osf;

    for(i=1;;i = i * 2)
      {
    n2 = filter2len * i;
    if (n2 % 2 == 0) n2--;
    df = (fs2*d)/(n2-1);
//    lpf = sfrq/2;
    if (df < DF) break;
      }

    lpf = sfrq/2;

    alp = alpha(aa);

    iza = dbesi0(alp);

    for(n2b=1;n2b<n2;n2b*=2);
    n2b *= 2;

    stage2 = (REAL*)malloc(sizeof(REAL)*n2b);

    for(i=0;i<n2b;i++) stage2[i] = 0;

    for(i=-(n2/2);i<=n2/2;i++) {
      stage2[i+n2/2] = win(i,n2,alp,iza)*hn_lpf(i,lpf,fs2)/n2b*2;
    }

    ipsize    = static_cast<int>(2+sqrt((double)n2b));
    fft_ip    = (int*)malloc(sizeof(int)*ipsize);
    fft_ip[0] = 0;
    wsize     = n2b/2;
    fft_w     = (REAL*)malloc(sizeof(REAL)*wsize);

    rdft(n2b,1,stage2,fft_ip,fft_w);
    }

    n2b2=n2b/2;

    buf1 = (REAL**)malloc(sizeof(REAL *)*nch);
    for(i=0;i<nch;i++)
      {
    buf1[i] = (REAL*)malloc(sizeof(REAL)*(n2b2/osf+1));
    for(j=0;j<(n2b2/osf+1);j++) buf1[i][j] = 0;
      }

    buf2 = (REAL**)malloc(sizeof(REAL *)*nch);
    for(i=0;i<nch;i++) buf2[i] = (REAL*)malloc(sizeof(REAL)*n2b);

    //rawinbuf  = (unsigned char*)calloc(nch*(n2b2+n1x),8);
    //rawoutbuf = (unsigned char*)malloc(8*nch*(n2b2/osf+1));

    inbuf  = (REAL*)calloc(nch*(n2b2+n1x),sizeof(REAL));
    outbuf = (REAL*)malloc(sizeof(REAL)*nch*(n2b2/osf+1));

    s1p = 0;
    rp  = 0;
    ds  = 0;
    osc = 0;

    inbuflen = n1/2/(fs1/sfrq)+1;
    delay = static_cast<int>((double)n2/2/((double)fs2/dfrq)) * nch;
  }

  ~Upsampler(void)
  {
    free(f1order);
    free(f1inc);
    free(stage1[0]);
    free(stage1);
    free(stage2);
    free(fft_ip);
    free(fft_w);
    for(i=0;i<nch;i++) free(buf1[i]);
    free(buf1);
    for(i=0;i<nch;i++) free(buf2[i]);
    free(buf2);
    free(inbuf);
    free(outbuf);
  }

  unsigned int __fastcall Resample(unsigned char * rawinbuf,unsigned int in_size,int ending)
  {
    /* Apply filters */

    int nsmplread,toberead,toberead2;
    unsigned int rv=0;
    int ch;


    toberead2 = toberead = static_cast<int>(floor((double)n2b2*sfrq/(dfrq*osf))+1+n1x)-inbuflen;

    if (!ending)
    {
      rv=8*nch*toberead;
      if (in_size<rv) return 0;
      nsmplread=toberead;
    }
    else
    {
      nsmplread=in_size/(8*nch);
      rv=nsmplread*(8*nch);
    }

    make_inbuf(nsmplread,inbuflen,rawinbuf,inbuf,toberead);

    inbuflen += toberead2;

    //nsmplwrt1 = ((rp-1)*sfrq/fs1+inbuflen-n1x)*dfrq*osf/sfrq;
    //if (nsmplwrt1 > n2b2) nsmplwrt1 = n2b2;
    nsmplwrt1 = n2b2;


    // apply stage 1 filter

    ip = &inbuf[((sfrq*(rp-1)+fs1)/fs1)*nch];

    s1p_backup = s1p;
    ip_backup  = ip;
    osc_backup = osc;

    for(ch=0;ch<nch;ch++)
      {
      REAL *op = &outbuf[ch];
      int fdo = fs1/(dfrq*osf),no = n1y*osf;

      s1p = s1p_backup; ip = ip_backup+ch;

      switch(n1x) {
      case 2:
        for(p = 0; p < nsmplwrt1; p++) {
          const int  s1o = f1order[s1p];

          buf2[ch][p] =
                stage1[s1o][0] * *(ip + 0 * nch) +
                stage1[s1o][1] * *(ip + 1 * nch);

          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      case 5:
        for(p = 0; p < nsmplwrt1; p++) {
          const int  s1o = f1order[s1p];

          buf2[ch][p] =
                stage1[s1o][0] * *(ip + 0 * nch) +
                stage1[s1o][1] * *(ip + 1 * nch) +
                stage1[s1o][2] * *(ip + 2 * nch) +
                stage1[s1o][3] * *(ip + 3 * nch) +
                stage1[s1o][4] * *(ip + 4 * nch);

          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      case 8:
        for(p = 0; p < nsmplwrt1; p++) {
          const int  s1o = f1order[s1p];

          buf2[ch][p] =
                stage1[s1o][0] * *(ip + 0 * nch) +
                stage1[s1o][1] * *(ip + 1 * nch) +
                stage1[s1o][2] * *(ip + 2 * nch) +
                stage1[s1o][3] * *(ip + 3 * nch) +
                stage1[s1o][4] * *(ip + 4 * nch) +
                stage1[s1o][5] * *(ip + 5 * nch) +
                stage1[s1o][6] * *(ip + 6 * nch) +
                stage1[s1o][7] * *(ip + 7 * nch);

          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      case 10:
        for(p = 0; p < nsmplwrt1; p++) {
          const int  s1o = f1order[s1p];

          buf2[ch][p] =
                stage1[s1o][0] * *(ip + 0 * nch) +
                stage1[s1o][1] * *(ip + 1 * nch) +
                stage1[s1o][2] * *(ip + 2 * nch) +
                stage1[s1o][3] * *(ip + 3 * nch) +
                stage1[s1o][4] * *(ip + 4 * nch) +
                stage1[s1o][5] * *(ip + 5 * nch) +
                stage1[s1o][6] * *(ip + 6 * nch) +
                stage1[s1o][7] * *(ip + 7 * nch) +
                stage1[s1o][8] * *(ip + 8 * nch) +
                stage1[s1o][9] * *(ip + 9 * nch);

          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      case 24:
        for(p = 0; p < nsmplwrt1; p++) {
          const int  s1o = f1order[s1p];

          buf2[ch][p] =
                stage1[s1o][0] * *(ip + 0 * nch) +
                stage1[s1o][1] * *(ip + 1 * nch) +
                stage1[s1o][2] * *(ip + 2 * nch) +
                stage1[s1o][3] * *(ip + 3 * nch) +
                stage1[s1o][4] * *(ip + 4 * nch) +
                stage1[s1o][5] * *(ip + 5 * nch) +
                stage1[s1o][6] * *(ip + 6 * nch) +
                stage1[s1o][7] * *(ip + 7 * nch) +
                stage1[s1o][8] * *(ip + 8 * nch) +
                stage1[s1o][9] * *(ip + 9 * nch) +
                stage1[s1o][10] * *(ip + 10 * nch) +
                stage1[s1o][11] * *(ip + 11 * nch) +
                stage1[s1o][12] * *(ip + 12 * nch) +
                stage1[s1o][13] * *(ip + 13 * nch) +
                stage1[s1o][14] * *(ip + 14 * nch) +
                stage1[s1o][15] * *(ip + 15 * nch) +
                stage1[s1o][16] * *(ip + 16 * nch) +
                stage1[s1o][17] * *(ip + 17 * nch) +
                stage1[s1o][18] * *(ip + 18 * nch) +
                stage1[s1o][19] * *(ip + 19 * nch) +
                stage1[s1o][20] * *(ip + 20 * nch) +
                stage1[s1o][21] * *(ip + 21 * nch) +
                stage1[s1o][22] * *(ip + 22 * nch) +
                stage1[s1o][23] * *(ip + 23 * nch);

          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      default:
        for(p = 0; p < nsmplwrt1; p++) {
          REAL  tmp = 0;
          REAL*  ip2 = ip;

          const int  s1o = f1order[s1p];

          for(i = 0; i < n1x; i++) {
            tmp += stage1[s1o][i] * *ip2;
            ip2 += nch;
          }

          buf2[ch][p] = tmp;
          ip += f1inc[s1p];
          s1p++;
          if(s1p == no) s1p = 0;
        }
        break;
      }

      osc = osc_backup;

      // apply stage 2 filter

      for(p=nsmplwrt1;p<n2b;p++) buf2[ch][p] = 0;

      //for(i=0;i<n2b2;i++) printf("%d:%g ",i,buf2[ch][i]);

      rdft(n2b,1,buf2[ch],fft_ip,fft_w);

      buf2[ch][0] = stage2[0]*buf2[ch][0];
      buf2[ch][1] = stage2[1]*buf2[ch][1]; 

      for(i=1;i<n2b/2;i++)
        {
      REAL re,im;

      re = stage2[i*2  ]*buf2[ch][i*2] - stage2[i*2+1]*buf2[ch][i*2+1];
      im = stage2[i*2+1]*buf2[ch][i*2] + stage2[i*2  ]*buf2[ch][i*2+1];

      //printf("%d : %g %g %g %g %g %g\n",i,stage2[i*2],stage2[i*2+1],buf2[ch][i*2],buf2[ch][i*2+1],re,im);

      buf2[ch][i*2  ] = re;
      buf2[ch][i*2+1] = im;
        }

      rdft(n2b,-1,buf2[ch],fft_ip,fft_w);

      for(i=osc,j=0;i<n2b2;i+=osf,j++) {
        op[j*nch] = buf1[ch][j] + buf2[ch][i];
      }

      nsmplwrt2 = j;

      osc = i - n2b2;

      for(j=0;i<n2b;i+=osf,j++)
        buf1[ch][j] = buf2[ch][i];
      }

    rp += nsmplwrt1 * (sfrq / frqgcd) / osf;

    make_outbuf(nsmplwrt2,outbuf);

    {
      int ds = (rp-1)/(fs1/sfrq);

//      assert(inbuflen >= ds);

      memmove(inbuf,inbuf+nch*ds,sizeof(REAL)*nch*(inbuflen-ds));
      inbuflen -= ds;
      rp -= ds*(fs1/sfrq);
    }

    return rv;
  }
};

class Downsampler : public Resampler_base
{
private:
  int frqgcd,osf,fs1,fs2;
  REAL *stage1,**stage2;
  int n2,n2x,n2y,n1,n1b;
  int filter1len;
  int *f2order,*f2inc;
  int *fft_ip;// = NULL;
  REAL *fft_w;// = NULL;
  //unsigned char *rawinbuf,*rawoutbuf;
  REAL *inbuf,*outbuf;
  REAL **buf1,**buf2;
  int i,j;
  int spcount;// = 0;

    int n1b2;// = n1b/2;
    int rp;        // inbufのfs1での次に読むサンプルの場所を保持
    int rps;       // rpを(fs1/sfrq=osf)で割った余り
    int rp2;       // buf2のfs2での次に読むサンプルの場所を保持
    int ds;        // 次にdisposeするsfrqでのサンプル数
    int nsmplwrt2; // 実際にファイルからinbufに読み込まれた値から計算した
                   // stage2 filterに渡されるサンプル数
    int s2p;       // stage1 filterから出力されたサンプルの数をn1y*osfで割った余り
//    int init; //,ending;
    int osc;
    REAL *bp; // rp2から計算される．buf2の次に読むサンプルの位置
    int rps_backup,s2p_backup;
    int k,ch,p;
    int inbuflen;//=0;
    REAL *op;

public:
  Downsampler(CONFIG& c) : Resampler_base(c)
  {
    spcount=0;
    fft_ip=0;
    fft_w=0;

  filter1len = FFTFIRLEN; /* stage 1 filter length */

  /* Make stage 1 filter */

  {
    double aa = AA; /* stop band attenuation(dB) */
    double lpf,delta,d,df,alp,iza;
    int ipsize,wsize;

    frqgcd = gcd(sfrq,dfrq);

    if (dfrq/frqgcd == 1) osf = 1;
    else if (dfrq/frqgcd % 2 == 0) osf = 2;
    else if (dfrq/frqgcd % 3 == 0) osf = 3;
    else {
//      fprintf(stderr,"Resampling from %dHz to %dHz is not supported.\n",sfrq,dfrq);
//      fprintf(stderr,"%d/gcd(%d,%d)=%d must be divided by 2 or 3.\n",dfrq,sfrq,dfrq,dfrq/frqgcd);
//      exit(-1);
    return;
    }

    fs1 = sfrq*osf;

    delta = pow(10.,-aa/20);
    if (aa <= 21) d = 0.9222; else d = (aa-7.95)/14.36;

    n1 = filter1len;
    for(i=1;;i = i * 2)
      {
  n1 = filter1len * i;
  if (n1 % 2 == 0) n1--;
  df = (fs1*d)/(n1-1);
  lpf = (dfrq-df)/2;
  if (df < DF) break;
      }

    alp = alpha(aa);

    iza = dbesi0(alp);

    for(n1b=1;n1b<n1;n1b*=2);
    n1b *= 2;

    stage1 = (REAL*)malloc(sizeof(REAL)*n1b);

    for(i=0;i<n1b;i++) stage1[i] = 0;

    for(i=-(n1/2);i<=n1/2;i++) {
      stage1[i+n1/2] = win(i,n1,alp,iza)*hn_lpf(i,lpf,fs1)*fs1/sfrq/n1b*2;
    }

    ipsize    = static_cast<int>(2+sqrt((double)n1b));
    fft_ip    = (int*)malloc(sizeof(int)*ipsize);
    fft_ip[0] = 0;
    wsize     = n1b/2;
    fft_w     = (REAL*)malloc(sizeof(REAL)*wsize);

    rdft(n1b,1,stage1,fft_ip,fft_w);
  }

  /* Make stage 2 filter */

  if (osf == 1) {
    fs2 = sfrq/frqgcd*dfrq;
    n2 = 1;
    n2y = n2x = 1;
    f2order = (int*)malloc(sizeof(int)*n2y);
    f2order[0] = 0;
    f2inc = (int*)malloc(sizeof(int)*n2y);
    f2inc[0] = sfrq/dfrq;
    stage2 = (REAL**)malloc(sizeof(REAL *)*n2y);
    stage2[0] = (REAL*)malloc(sizeof(REAL)*n2x*n2y);
    stage2[0][0] = 1;
  } else {
    double aa = AA; /* stop band attenuation(dB) */
    double lpf,delta,d,df,alp,iza;
    double guard = 2;

    fs2 = sfrq / frqgcd * dfrq ;

    df = (fs1/2 - sfrq/2) * 2 / guard;
    lpf = sfrq/2 + (fs1/2 - sfrq/2)/guard;

    delta = pow(10.,-aa/20);
    if (aa <= 21) d = 0.9222; else d = (aa-7.95)/14.36;

    n2 = static_cast<int>(fs2/df*d+1);
    if (n2 % 2 == 0) n2++;

    alp = alpha(aa);
    iza = dbesi0(alp);

    n2y = fs2/fs1; // 0でないサンプルがfs2で何サンプルおきにあるか？
    n2x = n2/n2y+1;

    f2order = (int*)malloc(sizeof(int)*n2y);
    for(i=0;i<n2y;i++) {
      f2order[i] = fs2/fs1-(i*(fs2/dfrq))%(fs2/fs1);
      if (f2order[i] == fs2/fs1) f2order[i] = 0;
    }

    f2inc = (int*)malloc(sizeof(int)*n2y);
    for(i=0;i<n2y;i++) {
      f2inc[i] = (fs2/dfrq-f2order[i])/(fs2/fs1)+1;
      if (f2order[i+1==n2y ? 0 : i+1] == 0) f2inc[i]--;
    }

    stage2 = (REAL**)malloc(sizeof(REAL *)*n2y);
    stage2[0] = (REAL*)malloc(sizeof(REAL)*n2x*n2y);

    for(i=1;i<n2y;i++) {
      stage2[i] = &(stage2[0][n2x*i]);
      for(j=0;j<n2x;j++) stage2[i][j] = 0;
    }

    for(i=-(n2/2);i<=n2/2;i++)
      {
  stage2[(i+n2/2)%n2y][(i+n2/2)/n2y] = win(i,n2,alp,iza)*hn_lpf(i,lpf,fs2)*fs2/fs1;
      }
    }

  /* Apply filters */

    n1b2 = n1b/2;
    inbuflen=0;
//    delay = 0;

    buf1 = (REAL**)malloc(sizeof(REAL *)*nch);
    for(i=0;i<nch;i++)
      buf1[i] = (REAL*)malloc(n1b*sizeof(REAL));

    buf2 = (REAL**)malloc(sizeof(REAL *)*nch);
    for(i=0;i<nch;i++) {
      buf2[i] = (REAL*)malloc(sizeof(REAL)*(n2x+1+n1b2));
      for(j=0;j<n2x+1+n1b2;j++) buf2[i][j] = 0;
    }

    //rawoutbuf = (unsigned char*)malloc(8*nch*((double)n1b2*sfrq/dfrq+1));
    inbuf = (REAL*)calloc(nch*(n1b2/osf+osf+1),sizeof(REAL));
    outbuf = (REAL*)malloc(static_cast<size_t>(sizeof(REAL)*nch*((double)n1b2*sfrq/dfrq+1)));

    op = outbuf;

    s2p = 0;
    rp  = 0;
    rps = 0;
    ds  = 0;
    osc = 0;
    rp2 = 0;

    delay = static_cast<int>((double)n1/2/((double)fs1/dfrq)+(double)n2/2/((double)fs2/dfrq)) * nch;
  };

  ~Downsampler(void)
  {
  free(stage1);
  free(fft_ip);
  free(fft_w);
  free(f2order);
  free(f2inc);
  free(stage2[0]);
  free(stage2);
  for(i=0;i<nch;i++) free(buf1[i]);
  free(buf1);
  for(i=0;i<nch;i++) free(buf2[i]);
  free(buf2);
  free(inbuf);
  free(outbuf);
  }

  unsigned int __fastcall Resample(unsigned char * rawinbuf,unsigned int in_size,int ending)
  {
    unsigned int rv;
    int nsmplread;
    int toberead;

    toberead = (n1b2-rps-1)/osf+1;

    if (!ending)
    {
      rv=8*nch*toberead;
      if (in_size<rv) return 0;
      nsmplread=toberead;
    }
    else
    {
      nsmplread=in_size/(8*nch);
      rv=nsmplread*(8*nch);
    }

    make_inbuf(nsmplread,inbuflen,rawinbuf,inbuf,toberead);

    rps_backup = rps;
    s2p_backup = s2p;

    for(ch=0;ch<nch;ch++)
    {
      rps = rps_backup;

      for(k=0;k<rps;k++) buf1[ch][k] = 0;

      for(i=rps,j=0;i<n1b2;i+=osf,j++)
      {
  //      assert(j < ((n1b2-rps-1)/osf+1));

        buf1[ch][i] = inbuf[j*nch+ch];

        for(k=i+1;k<i+osf;k++) buf1[ch][k] = 0;
      }

  //    assert(j == ((n1b2-rps-1)/osf+1));

      for(k=n1b2;k<n1b;k++) buf1[ch][k] = 0;

      rps = i - n1b2;
      rp += j;

      rdft(n1b,1,buf1[ch],fft_ip,fft_w);

      buf1[ch][0] = stage1[0]*buf1[ch][0];
      buf1[ch][1] = stage1[1]*buf1[ch][1]; 

      for(i=1;i<n1b2;i++)
      {
        REAL re,im;

        re = stage1[i*2  ]*buf1[ch][i*2] - stage1[i*2+1]*buf1[ch][i*2+1];
        im = stage1[i*2+1]*buf1[ch][i*2] + stage1[i*2  ]*buf1[ch][i*2+1];

        buf1[ch][i*2  ] = re;
        buf1[ch][i*2+1] = im;
      }

      rdft(n1b,-1,buf1[ch],fft_ip,fft_w);

      for(i=0;i<n1b2;i++) {
        buf2[ch][n2x+1+i] += buf1[ch][i];
      }

      {
        int t1 = rp2/(fs2/fs1);
        if (rp2%(fs2/fs1) != 0) t1++;

        bp = &(buf2[ch][t1]);
      }

      s2p = s2p_backup;

      for(p=0;bp-buf2[ch]<n1b2+1;p++)
      {
        REAL tmp = 0;
        REAL *bp2;
        int s2o;

        bp2 = bp;
        s2o = f2order[s2p];
        bp += f2inc[s2p];
        s2p++;

        if (s2p == n2y) s2p = 0;

  //      assert((bp2-&(buf2[ch][0]))*(fs2/fs1)-(rp2+p*(fs2/dfrq)) == s2o);

        for(i=0;i<n2x;i++)
          tmp += stage2[s2o][i] * *bp2++;

        op[p*nch+ch] = tmp;
      }

      nsmplwrt2 = p;
    }

    rp2 += nsmplwrt2 * (fs2 / dfrq);

    make_outbuf(nsmplwrt2,outbuf);

    {
      int ds = (rp2-1)/(fs2/fs1);

      if (ds > n1b2) ds = n1b2;

      for(ch=0;ch<nch;ch++)
        memmove(buf2[ch],buf2[ch]+ds,sizeof(REAL)*(n2x+1+n1b2-ds));

      rp2 -= ds*(fs2/fs1);
    }

    for(ch=0;ch<nch;ch++)
      memcpy(buf2[ch]+n2x+1,buf1[ch]+n1b2,sizeof(REAL)*n1b2);

    return rv;
  }
};

Resampler_base::Resampler_base(CONFIG & c)
{
  AA = c.aa;
  DF = c.df;
  FFTFIRLEN = c.fftfirlen;
  nch = c.nch;
  sfrq = c.sfrq;
  dfrq = c.dfrq;
}

Resampler_base* __fastcall Resampler_base::Create(CONFIG& c)
{
  if(!CanResample(c.sfrq,c.dfrq)) return 0;

  if(c.sfrq < c.dfrq)
    return new Upsampler(c);
  else
    if(c.sfrq > c.dfrq)
      return new Downsampler(c);

  return NULL;
}
