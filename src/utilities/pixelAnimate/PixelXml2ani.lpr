program PixelXml2ani;

{
  PixelXml2ani

  SPDX-FileCopyrightText: 2026 Malte Marwedel
  SPDX-License-Identifier: GPL-2.0-or-later

}

uses
  SysUtils, DOM, XMLRead, render;

var
  filenameIn, filenameOut: string;
  animation: TBuffer;
  doc: TXMLDocument;
  rn: TDOMNode;
  f: file;

begin
  if ParamCount <> 2 then
  begin
    WriteLn('PixelXml2ani (c) 2026 by Malte Marwedel. Version ' + render.progamVersion);
    WriteLn('Usage: <input filename> <output filename>');
    Halt(1);
  end;
  filenameIn := ParamStr(1);
  filenameOut := ParamStr(2);
  try
    ReadXMLFile(doc, filenameIn);
    rn := doc.FirstChild;
    animation := render.render(rn);
    doc.free;
    if (assigned(animation.data)) then begin
     AssignFile(f, filenameOut);
     ReWrite(f, 1);
     BlockWrite(f, animation.data^, animation.dataUsed);
     CloseFile(f);
     freeMem(animation.data);
   end else
     WriteLn('Failed to render animation');
  except
    on E: Exception do
    begin
      WriteLn('Error: ', E.Message);
      Halt(1);
    end;
  end;
end.
