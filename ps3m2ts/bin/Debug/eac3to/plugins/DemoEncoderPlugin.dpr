library DemoEncoderPlugin;

uses Windows, madStrings;

type
  TLogType = (ltInfo, ltWarning, ltError);
  TLogFunc = procedure (logType: TLogType; text: wideString);

var
  eac3toLogFunc : TLogFunc = nil;

procedure Log(logType: TLogType; text: wideString);
begin
  if @eac3toLogFunc <> nil then
    eac3toLogFunc(logType, text);
end;

const
  FORMAT_SIGNED_PCM_8  = $01;
  FORMAT_SIGNED_PCM_16 = $02;
  FORMAT_SIGNED_PCM_24 = $04;
  FORMAT_SIGNED_PCM_32 = $08;
  FORMAT_IEEE_32       = $10;
  FORMAT_IEEE_64       = $20;

const
  CDemoEncoderSamplesPerFrame = 48000;

function GetEncoderInformation(out encoderName           : pchar;  // 'Lame MP3 Encoder' (max 30 chars)
                               out activationOption      : pchar;  // '-lame' (max 20 chars)
                               out supportedInputFormats : dword;  // see above
                               out outputFormat          : pchar;  // 'mp3' (max 10 chars)
                               out samplesPerFrame       : dword;  // 48000 / 100
                               out threadSafe            : bool    // true
                               ) : bool; stdcall;
// fetch information about the encoder plugin
// all parameters must be set by the plugin
// an encoder should support multiple encoding operations at the same time
// if that's technically not possible, "threadSafe" must be set to "false"
// "samplesPerFrame" set to e.g. 48000 signals that the encoder wants to
// receive a full second worth of audio data with every "Encode" call
begin
  encoderName := 'non-functional demo encoder';
  activationOption := '-demoEncoder';
  supportedInputFormats := FORMAT_SIGNED_PCM_16 or FORMAT_SIGNED_PCM_24;
  outputFormat := '.demo';
  samplesPerFrame := CDemoEncoderSamplesPerFrame;
  threadSafe := true;
  result := true;
end;

type
  TDemoEncoder = record
    bitrate     : dword;
    format      : dword;
    channelno   : dword;
    channelmask : dword;
    bitdepth    : dword;
    green       : boolean;
    buf         : pointer;
    error       : string;
  end;
  PDemoEncoder = ^TDemoEncoder;

function OpenEncoder : dword; stdcall;
// get an instance of the encoder
var encoder : PDemoEncoder;
begin
  New(encoder);
  encoder.bitrate := 1536;
  encoder.green := false;
  encoder.buf := nil;
  encoder.error := '';
  result := dword(encoder);
end;

function CloseEncoder(encoderHandle: dword) : bool; stdcall;
// close the encoder and free all related resources
var encoder : PDemoEncoder;
begin
  result := false;
  encoder := PDemoEncoder(encoderHandle);
  try
    if encoder.buf <> nil then
      VirtualFree(encoder.buf, 0, MEM_RELEASE);
    Dispose(encoder);
    result := true;
  except end;
end;

function SetInputFormat(encoderHandle: dword; format, channelNo, channelMask: dword) : bool; stdcall;
// informs the encoder about the format of the incoming audio data
var encoder : PDemoEncoder;
begin
  result := false;
  encoder := PDemoEncoder(encoderHandle);
  try
    if (format = FORMAT_SIGNED_PCM_16) or (format = FORMAT_SIGNED_PCM_24) then begin
      if channelNo in [1, 2, 6] then begin
        encoder.format := format;
        encoder.channelNo := channelNo;
        encoder.channelMask := channelMask;
        if format = FORMAT_SIGNED_PCM_16 then
          encoder.bitdepth := 2
        else
          encoder.bitdepth := 3;
        result := true;
      end else
        encoder.error := 'This demo encoder doesn''t support this channel configuration.';
    end else
      encoder.error := 'This demo encoder doesn''t support this input format.';
  except end;
end;

function EncodeFrame(encoderHandle: dword; inBuf: pointer; out outBuf: pointer; out outSize: dword) : bool; stdcall;
// is called by eac3to to encode one audio frame
// so the number of samples in "inBuf" is defined by "samplesPerFrame"
// the output buffer must be allocated by the encoder
// the output buffer is not freed by eac3to
// at the end of encoding "EncodeFrame" is repeatedly called with "inBuf = nil"
// this is meant to flush any encoding buffers
// the encoder can signal final completion by setting "outSize := 0"
// the encoder must always return true, or else eac3to will abort processing
var encoder : PDemoEncoder;
begin
  result := false;
  encoder := PDemoEncoder(encoderHandle);
  try
    if encoder.buf = nil then
      // allocate internal buffer
      encoder.buf := VirtualAlloc(nil, CDemoEncoderSamplesPerFrame * encoder.channelNo * encoder.bitdepth, MEM_COMMIT, PAGE_READWRITE);
    if inBuf <> nil then begin
      // normal encoding
      Move(inBuf^, encoder.buf^, CDemoEncoderSamplesPerFrame * encoder.channelNo * encoder.bitdepth);
      outBuf := encoder.buf;
      outSize := CDemoEncoderSamplesPerFrame * encoder.channelNo * encoder.bitdepth;
    end else begin
      // eac3to asks us to flush our buffers
      outBuf := nil;
      outSize := 0;
    end;
    result := true;
  except end;
end;

procedure SetLogFunction(logFunc: TLogFunc); stdcall;
// eac3to wants to share its log function with the encoder
// in order to do so eac3to reports its log function address
// if the encoder wants to output information to the screen
// it should do so by using the official log function
begin
  eac3toLogFunc := logFunc;
end;

function ParseOption(encoderHandle: dword; option: pchar) : bool; stdcall;
// unknown options are passed through to the encoder
// if the encoder supports the option, it must return true
// otherwise eac3to will abort processing
var encoder : PDemoEncoder;
begin
  result := false;
  encoder := PDemoEncoder(encoderHandle);
  try
    if IsTextEqual(option, 'green') then begin
      encoder.green := true;
      result := true;
    end else
      encoder.error := 'The demo encoder doesn''t understand the option "' + option + '".';
  except end;
end;

function SetBitrate(encoderHandle: dword; bitrate: dword) : bool; stdcall;
// if the encoder supports the specified bitrate, it must return true
var encoder : PDemoEncoder;
begin
  result := false;
  encoder := PDemoEncoder(encoderHandle);
  try
    if (bitrate = 768) or (bitrate = 1536) then begin
      encoder.bitrate := bitrate;
      result := true;
    end else
      encoder.error := 'The demo encoder doesn''t support the bitrate ' + IntToStrEx(bitrate) + '.';
  except end;
end;

function GetErrorInformation(encoderHandle: dword; out errorText: pchar) : bool; stdcall;
// returns an error description, in case one of the APIs failed
var encoder : PDemoEncoder;
begin
  result := false;
  errorText := nil;
  encoder := PDemoEncoder(encoderHandle);
  try
    if encoder.error <> '' then begin
      errorText := pchar(encoder.error);
      result := true;
    end;
  except end;
end;

exports
  GetEncoderInformation,
  OpenEncoder,
  CloseEncoder,
  SetInputFormat,
  EncodeFrame,
  SetLogFunction,
  ParseOption,
  SetBitrate,
  GetErrorInformation;

end.
