Here's how the libav dlls which are shipping with eac3to were compiled:

(1)
svn checkout svn://svn.mplayerhq.hu/ffmpeg/trunk .
svn checkout svn://svn.mplayerhq.hu/soc/eac3 .

(2)
copy "eac3\*.c" "ffmpeg\libavcodec\*.*"
copy "eac3\*.h" "ffmpeg\libavcodec\*.*"
copy "eac3\ffmpeg.patch" "ffmpeg\*.*"

(3)
patch -p0 < ffmpeg.patch
patch -p0 < mlpdec.patch
patch -p0 < dca.patch
patch -p0 < ac3dec.patch

(4)
copy "mlpdec.c" "ffmpeg\libavcodec\*.*"

(5)
./configure --enable-shared --disable-static --enable-memalign-hack

(6)
add to "config.h":
#define USE_HIGHPRECISION 1
#define CONFIG_AUDIO_NONSHORT 1
#define CONFIG_AUDIO_NODRC 1

(7)
make
make install
