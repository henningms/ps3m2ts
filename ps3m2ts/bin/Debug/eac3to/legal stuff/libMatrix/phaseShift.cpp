// Copyright (C) 2007 Christian Kothe

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <windows.h>
#include <complex>
#include "filter.h"

typedef std::complex<double> cdouble;

const unsigned fltlen = 2048;
const unsigned N = fltlen*2;
const unsigned block_size = fltlen*6;

void __fastcall rdft(int n, int isgn, double* a, int* ip, double* w);

class PhaseShift90
{
  // temp buffer for processing
  double *temp;
  // the filter kernel in the frequency domain
  double *kernel;
  // output buffer for processing
  double *output;

  // processing buffers for FFT
  double *fft_w;
  int *fft_ip;

public:

  PhaseShift90()
  {
    temp = (double*) malloc(sizeof(double) * N * 2);
    kernel = (double*) filter;
    output = (double*) malloc(sizeof(double) * N);
    fft_w = (double*) malloc(sizeof(double) * fltlen);
    fft_ip = (int*) malloc(sizeof(int) * static_cast<int>(2 + sqrt((double) N)));
    fft_ip[0] = 0;
  }

  ~PhaseShift90()
  {
    free(temp);
    free(output);
    free(fft_w);
    free(fft_ip);
  }

  // handle a fixed-size block
  void process_block(double *data)
  {
    // clear the second half of the input buffers
    for (unsigned i = fltlen; i < N; i++)
      temp[i] = 0;

    // fill the data into (the end of) the buffers
    for (unsigned i = 0; i < fltlen; i++)
      temp[i] = data[i];

    // move to the frequency domain
    rdft(N, 1, temp, fft_ip, fft_w);

    // swap sign (ooura code produces inverted sign)
    for (unsigned i = 0; i < fltlen; i++)
      temp[i * 2 + 1] = -temp[i * 2 + 1];

    // multiply by the kernel's fourier transform
    for (unsigned i = 0; i < fltlen; i++)
    {
      cdouble tmp(cdouble(temp[i * 2 + 0], temp[i * 2 + 1]) * cdouble(kernel[i * 2 + 0], kernel[i * 2 + 1]));
      temp[i * 2 + 0] = tmp.real();
      temp[i * 2 + 1] = tmp.imag();
    }

    // swap sign (ooura code expects inverted sign)
    for (unsigned i = 0; i < fltlen; i++)
      temp[i * 2 + 1] = -temp[i * 2 + 1];

    // backtransform
    rdft(N, -1, temp, fft_ip, fft_w);

    // correct scale (ooura backtransform results are scaled by 0.5)
    for (unsigned i = 0; i < N; i++)
      temp[i] = temp[i] * 2.0;

    // add the result to the output buffer
    for (unsigned i = 0; i < N; i++)
      output[i] += temp[i] / N;

    // emit the output buffers into ls/rs (inverted because Hilbert does -90°)
    for (unsigned i = 0; i < fltlen; i++)
      data[i] = -output[i];

    // shift & clear the output
    for (unsigned i = 0; i < fltlen; i++)
    {
      output[i] = output[i + fltlen];
      output[i + fltlen] = 0;
    }
  }
};

extern "C"
{
  __declspec(dllexport) HANDLE _stdcall PhaseShift90_init(unsigned int *blockSize)
  {
    *blockSize = fltlen;
    return (HANDLE) new PhaseShift90();
  }

  __declspec(dllexport) void _stdcall PhaseShift90_process(HANDLE phaseShift, void* buf)
  {
    PhaseShift90 *phaseShift_ = (PhaseShift90*) phaseShift;

    phaseShift_->process_block((double *) buf);
  }

  __declspec(dllexport) void _stdcall PhaseShift90_close(HANDLE phaseShift)
  {
    PhaseShift90 *phaseShift_ = (PhaseShift90*) phaseShift;

    delete phaseShift_;
  }
}
