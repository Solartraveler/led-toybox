unit render;

{
  PixelAnimate

  SPDX-FileCopyrightText: 2026 Malte Marwedel
  SPDX-License-Identifier: GPL-2.0-or-later

}

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, DOM, StrUtils;

type
  TBuffer = record
  data: ^Byte;
  dataUsed: integer;
  dataMax: integer;
end;

type
  TPixel = record
  r: integer;
  g: integer;
  b: integer;
  format: integer;
end;


function renderChar(c, font, offset, sx, sy, lastFrameIdx: integer; fgcolor: TPixel; var animation: TBuffer): integer;
function render(rn: TDOMNode): TBuffer;

function DomstrToInt(str: DOMString): integer;
function DomstrToFloat(str: DOMString): double;

procedure addUint8(value: integer; var animation: TBuffer);
procedure setUint8(index, value: integer; var animation: TBuffer);
function getUint8(index: integer; animation: TBuffer): integer;
procedure addUint16(value: integer; var animation: TBuffer);
procedure setUint16(index, value: integer; var animation: TBuffer);
function getUint16(index: integer; animation: TBuffer): integer;
procedure addUint32(value: integer; var animation: TBuffer);
procedure addData(data: PByte; length: integer; var animation: TBuffer);
procedure addPixel(pixel: TPixel; var animation: TBuffer);
function  getPixel(x, y, frameIdx, format, sx: integer; animation: TBuffer): TPixel;
procedure setPixel(x, y, frameIdx, sx, sy: integer; pixel: TPixel; var animation: TBuffer);
function  string2Pixel(color: string; format: integer): TPixel;

function addFrame(ch: TDOMNode; sx, sy, format: integer; var animation: TBuffer): integer;
function addRoll(ch: TDOMNode; sx, sy, format: integer; lastFrameIdx: integer; var animation: TBuffer): integer;
function addShift(ch: TDOMNode; sx, sy, format: integer; lastFrameIdx: integer; var animation: TBuffer): integer;
function addText(ch: TDOMNode; sx, sy, format: integer; lastFrameIdx: integer; var animation: TBuffer): integer;


const charsPerColor = 12;
const version = 1;

const dataStop = 0;
const dataHeader = 1;
const dataFrame = 2;

//Array originated from the MenuDesigner project, converted from C by ChatGPT
const
  characters_5x5: array[0..(95 * 5 - 1)] of Byte = (
    $00,$00,$00,$00,$00, // space
    $00,$00,$17,$00,$00, // !
    $00,$03,$00,$03,$00, // "
    $0A,$1F,$0A,$1F,$0A, // #
    $02,$15,$1F,$15,$08, // $
    $13,$0B,$04,$1A,$19, // %
    $1A,$15,$09,$12,$00, // &
    $00,$00,$03,$00,$00, // '
    $00,$00,$0E,$11,$00, // (
    $00,$11,$0E,$00,$00, // )
    $11,$0A,$1F,$0A,$11, // *
    $04,$04,$1F,$04,$04, // +
    $00,$10,$08,$00,$00, // ,
    $04,$04,$04,$04,$04, // -
    $00,$00,$10,$00,$00, // .
    $00,$10,$0E,$01,$00, // /
    $1F,$11,$11,$1F,$00, // 0
    $00,$02,$1F,$00,$00, // 1
    $1D,$15,$15,$17,$00, // 2
    $15,$15,$15,$1F,$00, // 3
    $07,$04,$04,$1F,$00, // 4
    $17,$15,$15,$1D,$00, // 5
    $1F,$15,$15,$1D,$00, // 6
    $01,$01,$01,$1F,$00, // 7
    $1F,$15,$15,$1F,$00, // 8
    $17,$15,$15,$1F,$00, // 9
    $00,$00,$0A,$00,$00, // :
    $00,$10,$0A,$00,$00, // ;
    $00,$04,$0A,$11,$00, // <
    $0A,$0A,$0A,$0A,$0A, // =
    $00,$11,$0A,$04,$00, // >
    $00,$05,$15,$02,$00, // ?
    $09,$15,$1D,$11,$1E, // @
    $1E,$05,$05,$1E,$00, // A
    $1F,$15,$15,$0E,$00, // B
    $0E,$11,$11,$11,$00, // C
    $1F,$11,$11,$0E,$00, // D
    $00,$1F,$15,$15,$00, // E
    $00,$1F,$05,$05,$00, // F
    $0E,$11,$15,$0C,$00, // G
    $1F,$04,$04,$1F,$00, // H
    $00,$00,$1F,$00,$00, // I
    $00,$08,$10,$0F,$00, // J
    $1F,$04,$0A,$11,$00, // K
    $00,$1F,$10,$10,$00, // L
    $1F,$02,$04,$02,$1F, // M
    $1F,$02,$04,$08,$1F, // N
    $0E,$11,$11,$0E,$00, // O
    $1F,$05,$05,$02,$00, // P
    $0E,$11,$11,$19,$1E, // Q
    $1F,$05,$0D,$12,$00, // R
    $00,$12,$15,$09,$00, // S
    $01,$01,$1F,$01,$01, // T
    $0F,$10,$10,$0F,$00, // U
    $03,$0C,$10,$0C,$03, // V
    $07,$18,$06,$18,$07, // W
    $11,$0A,$04,$0A,$11, // X
    $03,$04,$18,$04,$03, // Y
    $11,$19,$15,$13,$11, // Z
    $00,$00,$1F,$11,$00, // [
    $00,$01,$0E,$10,$00, // \
    $00,$11,$1F,$00,$00, // ]
    $00,$02,$01,$02,$00, // ^
    $10,$10,$10,$10,$10, // _
    $00,$01,$02,$00,$00, // `
    $0C,$12,$12,$1E,$00, // a
    $1F,$12,$12,$0C,$00, // b
    $0C,$12,$12,$00,$00, // c
    $0C,$12,$12,$1F,$00, // d
    $0E,$15,$15,$16,$00, // e
    $00,$04,$1F,$05,$00, // f
    $00,$17,$15,$0F,$00, // g
    $1F,$04,$04,$18,$00, // h
    $00,$00,$1D,$00,$00, // i
    $00,$10,$1D,$00,$00, // j
    $00,$1F,$08,$14,$00, // k
    $00,$00,$1F,$00,$00, // l
    $1C,$02,$1C,$02,$1C, // m
    $1E,$04,$02,$1C,$00, // n
    $0C,$12,$12,$0C,$00, // o
    $1E,$0A,$0A,$04,$00, // p
    $04,$0A,$0A,$1E,$00, // q
    $00,$1E,$04,$02,$00, // r
    $02,$15,$15,$08,$00, // s
    $00,$02,$0F,$12,$00, // t
    $0E,$10,$10,$0E,$00, // u
    $06,$08,$10,$08,$06, // v
    $06,$18,$0C,$18,$06, // w
    $00,$12,$0C,$12,$00, // x
    $00,$16,$08,$06,$00, // y
    $12,$1A,$16,$12,$00, // z
    $00,$04,$0E,$11,$00, // {
    $00,$00,$1F,$00,$00, // |
    $00,$11,$0E,$04,$00, // }
    $04,$02,$04,$08,$04  // ~
  );


implementation


function DomstrToInt(str: DOMString): integer;
begin
  result := StrToInt(String(str));
end;

function DomstrToFloat(str: DOMString): double;
begin
  result := StrToFloat(String(str));
end;

procedure addUint8(value: integer; var animation: TBuffer);
begin
  if (animation.dataMax - animation.dataUsed) >= 1 then begin
    animation.data[animation.dataUsed] := value;
    inc(animation.dataUsed);
  end;
end;

procedure setUint8(index, value: integer; var animation: TBuffer);
begin
  if (index < animation.dataUsed) then begin
    animation.data[index] := value;
  end;
end;

function getUint8(index: integer; animation: TBuffer): integer;
begin
  result := 0;
  if (index < animation.dataUsed) then
    result := animation.data[index];
end;

procedure addUint16(value: integer; var animation: TBuffer);
begin
  addUint8(value shr 8, animation);
  addUint8(value and $FF, animation);
end;

procedure setUint16(index, value: integer; var animation: TBuffer);
begin
  setUint8(index, value shr 8, animation);
  setUint8(index + 1, value and $FF, animation);
end;

function getUint16(index: integer; animation: TBuffer): integer;
begin
  result := 0;
  if ((index + 1) < animation.dataUsed) then
    result := (animation.data[index] shl 8) or animation.data[index + 1];
end;

procedure addUint32(value: integer; var animation: TBuffer);
begin
  addUint16(value shr 16, animation);
  addUint16(value and $FFFF, animation);
end;

procedure addData(data: PByte; length: integer; var animation: TBuffer);
begin
  if (animation.dataUsed + length < animation.dataMax) then begin
    move(data^, animation.data[animation.dataUsed], length);
    inc(animation.dataUsed, length);
  end;
end;

procedure addPixel(pixel: TPixel; var animation: TBuffer);
begin
  if (pixel.format > 8) then begin
    addUint16(pixel.b, animation);
    addUint16(pixel.g, animation);
    addUint16(pixel.r, animation);
  end else begin
    addUint8(pixel.b, animation);
    addUint8(pixel.g, animation);
    addUint8(pixel.r, animation);
  end;
end;

function getPixel(x, y, frameIdx, format, sx: integer; animation: TBuffer): TPixel;
var offset, pixelsize: integer;
begin
  if (format > 8) then begin
    pixelsize := 6;
  end else begin
    pixelsize := 3;
  end;
  offset := frameIdx + 3 + (y * sx + x) * pixelsize; //3 = frame header size
  result.format := format;
  if ((offset + pixelsize) < animation.dataUsed) then begin
    if (format > 8) then begin
      result.b := getUint16(offset + 0, animation);
      result.g := getUint16(offset + 2, animation);
      result.r := getUint16(offset + 4, animation);
    end else begin
      result.b := getUint8(offset + 0, animation);
      result.g := getUint8(offset + 1, animation);
      result.r := getUint8(offset + 2, animation);
    end;
  end;
end;

//Only overwrites existing pixels, so they needed to be added by addPixel in the frame before
procedure setPixel(x, y, frameIdx, sx, sy: integer; pixel: TPixel; var animation: TBuffer);
var
  pixelsize, offset: integer;
begin
  if (x < sx) and (y < sy) and (x >= 0) and (y >= 0) then begin
    if (pixel.format > 8) then begin
      pixelsize := 6;
    end else begin
      pixelsize := 3;
    end;
    offset := frameIdx + 3 + (y * sx + x) * pixelsize; //3 = frame header size
    if (pixel.format > 8) then begin
      setUint16(offset + 0, pixel.b, animation);
      setUint16(offset + 2, pixel.g, animation);
      setUint16(offset + 4, pixel.r, animation);
    end else begin
      setUint8(offset + 0, pixel.b, animation);
      setUint8(offset + 1, pixel.g, animation);
      setUint8(offset + 2, pixel.r, animation);
    end;
  end;
end;

function string2Pixel(color: string; format: integer): TPixel;
var
  rs, gs, bs: string;
begin
  result.r := 0; //fallback, fill missing data with zeros
  result.g := 0;
  result.b := 0;
  result.format := format;
  if (color.length >= charsPerColor) then begin
    bs := Copy(color, 1, 4);
    gs := Copy(color, 5, 4);
    rs := Copy(color, 9, 4);
    result.b := Hex2Dec(bs) shr (16 - format);
    result.g := Hex2Dec(gs) shr (16 - format);
    result.r := Hex2Dec(rs) shr (16 - format);
  end;
end;

function addFrame(ch: TDOMNode; sx, sy, format: integer; var animation: TBuffer): integer;
var
  x, y: integer;
  ch2, ch3: TDOMNode;
  data, onePixel, fallback: string;
  pixel: TPixel;
begin
  result := animation.dataUsed;
  ch2 := ch.FirstChild;
  addUint8(dataFrame, animation);
  addUint16(DomstrToInt(ch.Attributes.GetNamedItem('duration').NodeValue), animation);
  pixel.format := format;
  fallback := StringOfChar('0', sx * charsPerColor); //fallback, fill missing data with zeros
  for y := 0 to sy - 1 do begin
    if (assigned(ch2)) then begin
      ch3 := ch2.FirstChild;
      if assigned(ch3) and (ch3.NodeType = TEXT_NODE) then begin
        data := String(ch3.NodeValue);
      end else
        data := fallback;
    end else
      data := fallback;
    for x := 0 to sx - 1 do begin
      //12 hex characters for each pixel
      onePixel := Copy(data, x * charsPerColor + 1, charsPerColor);
      pixel := string2Pixel(onePixel, format);
      addPixel(pixel, animation);
    end;
    if (assigned(ch2)) then
      ch2 := ch2.NextSibling;
  end;
end;

function addRoll(ch: TDOMNode; sx, sy, format, lastFrameIdx: integer; var animation: TBuffer): integer;
var
   repeats, duration, rollxo, rollyo, xsource, ysource, x, y, i, currentFrameIdx: integer;
   rollx, rollxa, rolly, rollya: double;
   pixel: TPixel;
begin
  result := lastFrameIdx; //default if repeats is zero
  repeats := DomstrToInt(ch.Attributes.GetNamedItem('repeats').NodeValue);
  duration := DomstrToInt(ch.Attributes.GetNamedItem('duration').NodeValue);
  rollx := DomstrToFloat(ch.Attributes.GetNamedItem('x').NodeValue);
  rolly := DomstrToFloat(ch.Attributes.GetNamedItem('y').NodeValue);
  rollxa := 0;
  rollya := 0;
  for i := 0 to repeats - 1 do begin
    rollxa := rollxa + rollx; //a = accumulator
    rollya := rollya + rolly;
    rollxo := trunc(rollxa); //o = overflow (rounded down to integer values)
    rollyo := trunc(rollya);
    rollxa := rollxa - rollxo; //so now the accumulator is in the range 0...< 1 again
    rollya := rollya - rollyo;
    currentFrameIdx := animation.dataUsed;
    addUint8(dataFrame, animation);
    addUint16(duration, animation);
    for y := 0 to sy - 1 do begin
      ysource := y - rollyo;
      if (ysource < 0) then ysource := ysource + sy;
      if (ysource >= sy) then ysource := ysource - sy;
      for x := 0 to sx - 1 do begin
        xsource := x - rollxo;
        if (xsource < 0) then xsource := xsource + sx;
        if (xsource >= sx) then xsource := xsource - sx;
        pixel := getPixel(xsource, ysource, lastFrameIdx, format, sx, animation);
        addPixel(pixel, animation);
      end;
    end;
    lastFrameIdx := currentFrameIdx;
  end;
end;

function addShift(ch: TDOMNode; sx, sy, format, lastFrameIdx: integer; var animation: TBuffer): integer;
var
   repeats, duration, rollxo, rollyo, xsource, ysource, x, y, i, currentFrameIdx: integer;
   rollx, rollxa, rolly, rollya: double;
   pixel, borderpixel: TPixel;
   bordercolor: boolean;
   bordercolorstr: string;
begin
  result := lastFrameIdx; //default if repeats is zero
  repeats := DomstrToInt(ch.Attributes.GetNamedItem('repeats').NodeValue);
  duration := DomstrToInt(ch.Attributes.GetNamedItem('duration').NodeValue);
  rollx := DomstrToFloat(ch.Attributes.GetNamedItem('x').NodeValue);
  rolly := DomstrToFloat(ch.Attributes.GetNamedItem('y').NodeValue);
  rollxa := 0;
  rollya := 0;
  bordercolorstr := String(ch.Attributes.GetNamedItem('color1').NodeValue);
  borderpixel := string2Pixel(bordercolorstr, format);
  for i := 0 to repeats - 1 do begin
    rollxa := rollxa + rollx; //a = accumulator
    rollya := rollya + rolly;
    rollxo := trunc(rollxa); //o = overflow (rounded down to integer values)
    rollyo := trunc(rollya);
    rollxa := rollxa - rollxo; //so now the accumulator is in the range 0...< 1 again
    rollya := rollya - rollyo;
    currentFrameIdx := animation.dataUsed;
    addUint8(dataFrame, animation);
    addUint16(duration, animation);
    for y := 0 to sy - 1 do begin
      ysource := y - rollyo;
      for x := 0 to sx - 1 do begin
        xsource := x - rollxo;
        bordercolor := false;
        if (ysource < 0) or (ysource >= sy) then bordercolor := true;
        if (xsource < 0) or (xsource >= sx) then bordercolor := true;
        if (bordercolor) then begin
          addPixel(borderpixel, animation);
        end else begin
          pixel := getPixel(xsource, ysource, lastFrameIdx, format, sx, animation);
          addPixel(pixel, animation);
        end;
      end;
    end;
    lastFrameIdx := currentFrameIdx;
  end;
end;

//font 1: 5x5 pixel fixed width
//font 2: 5x5 pixel variable width
//font 3: 5x5 pixel variable width, but all number digits have the same size
//returns the number of pixels used for the char in the width
//Function generated by ChatGPT
function renderChar(c, font, offset, sx, sy, lastFrameIdx: integer; fgcolor: TPixel; var animation: TBuffer): integer;
var
  glyphIndex: Integer;
  startCol, endCol: Integer;
  col, row: Integer;

  srcCols: array[0..4] of Byte;
  dstCols: array[0..4] of Byte;

  srcCount, dstCount: Integer;
  runStart, runLen, keepCount: Integer;
  columnData: Byte;
begin
  Result := 0;

  // supported fonts 1..3
  if (font < 1) or (font > 3) then Exit;

  // printable ASCII only
  if (c < 32) or (c > 126) then Exit;

  glyphIndex := (c - 32) * 5;

  startCol := 0;
  endCol := 4;

  // detect variable width bounds
  if (font = 2) or (font = 3) then
  begin
    if not ((font = 3) and (c >= Ord('0')) and (c <= Ord('9'))) then
    begin
      while (startCol <= 4) and (characters_5x5[glyphIndex + startCol] = 0) do
        Inc(startCol);

      while (endCol >= startCol) and (characters_5x5[glyphIndex + endCol] = 0) do
        Dec(endCol);

      if startCol > endCol then
      begin
        Result := 3;
        Exit;
      end;
    end;
  end;

  // copy source columns
  srcCount := 0;
  for col := startCol to endCol do
  begin
    srcCols[srcCount] := characters_5x5[glyphIndex + col];
    Inc(srcCount);
  end;

  // compress repeated columns (variable width fonts only)
  if (font = 2) or (font = 3) then
  begin
    dstCount := 0;
    runStart := 0;

    while runStart < srcCount do
    begin
      columnData := srcCols[runStart];
      runLen := 1;

      while (runStart + runLen < srcCount) and
            (srcCols[runStart + runLen] = columnData) do
        Inc(runLen);

      case runLen of
        5: keepCount := 3;
        4: keepCount := 3;
        3: keepCount := 2;
        else keepCount := runLen;
      end;

      for col := 0 to keepCount - 1 do
      begin
        dstCols[dstCount] := columnData;
        Inc(dstCount);
      end;

      Inc(runStart, runLen);
    end;
  end
  else
  begin
    // fixed width
    dstCount := srcCount;
    for col := 0 to srcCount - 1 do
      dstCols[col] := srcCols[col];
  end;

  // render
  for col := 0 to dstCount - 1 do
  begin
    columnData := dstCols[col];

    for row := 0 to 4 do
      if (columnData and (1 shl row)) <> 0 then
        setPixel(offset + col, row, lastFrameIdx, sx, sy, fgcolor, animation);
  end;

  Result := dstCount;
end;

function addText(ch: TDOMNode; sx, sy, format, lastFrameIdx: integer; var animation: TBuffer): integer;
var
  repeats, duration, font, i, j, x, y, offsetx, c: integer;
  fgcolor, bgcolor: TPixel;
  text: utf8string;
begin
  result := lastFrameIdx; //default if repeats is zero
  repeats := DomstrToInt(ch.Attributes.GetNamedItem('repeats').NodeValue);
  duration := DomstrToInt(ch.Attributes.GetNamedItem('duration').NodeValue);
  font := DomstrToInt(ch.Attributes.GetNamedItem('font').NodeValue);
  fgcolor := string2Pixel(String(ch.Attributes.GetNamedItem('color1').NodeValue), format);
  bgcolor := string2Pixel(String(ch.Attributes.GetNamedItem('color2').NodeValue), format);
  text := Utf8String(ch.Attributes.GetNamedItem('comment').NodeValue);
  for i := 0 to repeats - 1 do begin
    result := animation.dataUsed;
    addUint8(dataFrame, animation);
    addUint16(duration, animation);
    //add background color
    for y := 0 to sy - 1 do begin
      for x := 0 to sx - 1 do begin
        addPixel(bgcolor, animation);
      end;
    end;
    //add text
    offsetx := -i;
    //TODO: Add proper utf-8 handling here!
    for j := 1 to length(text) do begin
      c := Ord(text[j]);
      if (c = $10) or (c = $13) then //stop at first newline char
        break;
      offsetx := offsetx + renderChar(c, font, offsetx, sx, sy, result, fgcolor, animation);
      inc(offsetx); //space between characters
    end;
  end;
end;

function render(rn: TDOMNode): TBuffer;
var
 animation: TBuffer;
 ch: TDOMNode;
 lastFrameIdx, sx, sy, format: integer;
begin
  lastFrameIdx := 0;
  animation.dataUsed := 0;
  animation.dataMax := 16777216;
  GetMem(animation.data, animation.dataMax);
  sx := DomstrToInt(rn.Attributes.GetNamedItem('Xsize').NodeValue);
  sy := DomstrToInt(rn.Attributes.GetNamedItem('Ysize').NodeValue);
  format := DomstrToInt(rn.Attributes.GetNamedItem('colorFormat').NodeValue);
  //append header (12byte)
  addUint8(dataHeader, animation);
  addUint32($616e696d, animation); //ascii = anim
  addUint8(version, animation); //version information at idx 5
  addUint8(format, animation); //at idx 6
  addUint16(sx, animation); //at idx 7..8
  addUint16(sy, animation); //at idx 9..10
  addUint8(0, animation); //reserved
  //render animation data
  ch := rn.FirstChild;
  while (assigned(ch)) do begin
    if (ch.NodeName = 'frame') then begin
      lastFrameIdx := addFrame(ch, sx, sy, format, animation);
    end else if (ch.NodeName = 'roll') then begin
      lastFrameIdx := addRoll(ch, sx, sy, format, lastFrameIdx, animation);
    end else if (ch.NodeName = 'shift') then begin
      lastFrameIdx := addShift(ch, sx, sy, format, lastFrameIdx, animation);
    end else if (ch.NodeName = 'text') then begin
      lastFrameIdx := addText(ch, sx, sy, format, lastFrameIdx, animation);
    end;
    ch := ch.NextSibling;
  end;
  //append termination
  addUint8(dataStop, animation);
  ReAllocMem(animation.data, animation.dataUsed);
  animation.dataMax := animation.dataUsed;
  result := animation;
end;


end.

