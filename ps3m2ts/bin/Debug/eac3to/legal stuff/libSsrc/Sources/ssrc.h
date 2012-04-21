
class Buffer
{
private:
  char * buffer;
  unsigned int buf_size,buf_data;
public:
  inline Buffer() {buf_size=0;buf_data=0;buffer=0;}
  inline ~Buffer() {if (buffer) free(buffer);}
  inline void* __fastcall GetBuffer(unsigned int * siz) {*siz=buf_data;return buffer;}
  inline unsigned int __fastcall Size() {return buf_data;}
  void __fastcall Read(unsigned int size);
  void __fastcall Write(void * ptr,unsigned int size);
  inline void __fastcall Flush(void) {buf_data = 0;}
};

typedef double REAL;

class Resampler_base
{
public:
  class CONFIG
  {
  public:
    int sfrq, dfrq, nch;
    double aa, df;
                int fftfirlen;
    _inline CONFIG(
          int _sfrq,
          int _dfrq,
          int _nch,
          double _aa,
                                        double _df,
                                        int _fftfirlen)
    {
      sfrq=_sfrq;
      dfrq=_dfrq;
      nch=_nch;
      aa=_aa;
      df=_df;
      fftfirlen=_fftfirlen;
    }
  };

private:
  Buffer in,out;
  void __fastcall bufloop(int finish);

protected:
  double AA, DF;
  int FFTFIRLEN;

  int nch, sfrq, dfrq;
  int delay;

  Resampler_base(CONFIG & c);

  void inline __fastcall __output(void * ptr, int size)
  {
    if(delay) {
      delay--;
    } else {
      out.Write(ptr, size);
    }
  }
  virtual unsigned int __fastcall Resample(unsigned char *input, unsigned int size, int ending)=0;
  void __fastcall make_outbuf(int nsmplwrt2, REAL* outbuf);
  void __fastcall make_inbuf(
              int nsmplread,
              int inbuflen,
              unsigned char* rawinbuf,
              REAL* inbuf,
              int toberead);

public:

  inline void __fastcall Write(void* input, unsigned int size) {in.Write(input,size); bufloop(0);}
  inline void __fastcall Finish() {bufloop(1);}

  inline void* __fastcall GetBuffer(unsigned int* s) {return out.GetBuffer(s);}
  inline void __fastcall Read(unsigned int s) {out.Read(s);}
  inline void __fastcall Flush(void) {in.Flush(); out.Flush();}

  unsigned int __fastcall GetLatency();//returns amount of audio data in in/out buffers in milliseconds

  inline unsigned int __fastcall GetDataInInbuf() {return in.Size();}
  inline unsigned int __fastcall GetDataInOutbuf() {return out.Size();}

  virtual ~Resampler_base() {}

  static Resampler_base* __fastcall Create(CONFIG& c);
};

#define  SSRC_create(sfrq, dfrq, nch, fast) \
      Resampler_base::Create(Resampler_base::CONFIG(\
      sfrq, dfrq, nch, aa, df, fftfirlen))

int __fastcall CanResample(int sfrq,int dfrq);

extern "C"
{
  __declspec(dllexport) HANDLE _stdcall ssrc_init(int sfrq, int dfrq, int nch, double aa, double df, int fftfirlen)
  {
    if (CanResample(sfrq, dfrq))
      return (HANDLE) Resampler_base::Create(Resampler_base::CONFIG(sfrq, dfrq, nch, aa, df, fftfirlen));
    else
      return NULL;
  }

  __declspec(dllexport) void _stdcall ssrc_write(HANDLE ssrc, void* inBuf, unsigned int inSize)
  {
    Resampler_base *ssrc_ = (Resampler_base*) ssrc;

    ssrc_->Write(inBuf, inSize);
  }

  __declspec(dllexport) unsigned int _stdcall ssrc_read(HANDLE ssrc, void* outBuf)
  {
    UINT result;
    VOID* buf;
    Resampler_base *ssrc_ = (Resampler_base*) ssrc;

    buf = (VOID*) ssrc_->GetBuffer(&result);
    if ((result > 0) && (outBuf)) {
      memmove(outBuf, buf, result);
      ssrc_->Read(result);
    }

    return result;
  }

  __declspec(dllexport) void _stdcall ssrc_flush(HANDLE ssrc)
  {
    Resampler_base *ssrc_ = (Resampler_base*) ssrc;

    ssrc_->Finish();
  }

  __declspec(dllexport) void _stdcall ssrc_close(HANDLE ssrc)
  {
    Resampler_base *ssrc_ = (Resampler_base*) ssrc;
    ssrc_->Finish();
    delete ssrc_;
  }

}

/*
USAGE:
Resampler_base::Create() / SSRC_create with your resampling params (see source for exact info what they do)

 loop:
Write() your PCM data to be resampled
GetBuffer() to get pointer to buffer with resampled data and amount of resampled data available (in bytes)
(note: SSRC operates on blocks, don't expect it to return any data right after your first Write() )

Read() to tell the resampler how much data you took from the buffer ( <= size returned by GetBuffer )

you can use GetLatency() to get amount of audio data (in ms) currently being kept in resampler's buffers
(quick hack for determining current output time without extra stupid counters)

Finish() to force-convert remaining PCM data after last Write() (warning: never Write() again after Flush() )

delete when done

*/
