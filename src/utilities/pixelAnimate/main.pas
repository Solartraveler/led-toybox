unit main;

{
  PixelAnimate

  SPDX-FileCopyrightText: 2026 Malte Marwedel
  SPDX-License-Identifier: GPL-2.0-or-later
}

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls, Spin,
  ExtCtrls, DOM, XMLWrite, XMLRead, Math, StrUtils, render;

type

  { TForm1 }

  TForm1 = class(TForm)
    Button1: TButton;
    Button10: TButton;
    Button11: TButton;
    Button12: TButton;
    Button13: TButton;
    Button14: TButton;
    Button15: TButton;
    Button16: TButton;
    Button2: TButton;
    Button3: TButton;
    Button4: TButton;
    Button5: TButton;
    Button6: TButton;
    Button7: TButton;
    Button8: TButton;
    Button9: TButton;
    CheckBox1: TCheckBox;
    ColorDialog1: TColorDialog;
    ColorDialog2: TColorDialog;
    FloatSpinEdit1: TFloatSpinEdit;
    FloatSpinEdit2: TFloatSpinEdit;
    FloatSpinEdit3: TFloatSpinEdit;
    GroupBox1: TGroupBox;
    GroupBox2: TGroupBox;
    GroupBox3: TGroupBox;
    Image1: TImage;
    Label1: TLabel;
    Label10: TLabel;
    Label11: TLabel;
    Label12: TLabel;
    Label13: TLabel;
    Label14: TLabel;
    Label15: TLabel;
    Label16: TLabel;
    Label17: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label5: TLabel;
    Label6: TLabel;
    Label7: TLabel;
    Label8: TLabel;
    Label9: TLabel;
    ListBox1: TListBox;
    Memo1: TMemo;
    OpenDialog1: TOpenDialog;
    Panel1: TPanel;
    Panel2: TPanel;
    SaveDialog1: TSaveDialog;
    SaveDialog2: TSaveDialog;
    SpinEdit1: TSpinEdit;
    SpinEdit10: TSpinEdit;
    SpinEdit11: TSpinEdit;
    SpinEdit12: TSpinEdit;
    SpinEdit2: TSpinEdit;
    SpinEdit3: TSpinEdit;
    SpinEdit4: TSpinEdit;
    SpinEdit5: TSpinEdit;
    SpinEdit6: TSpinEdit;
    SpinEdit7: TSpinEdit;
    SpinEdit8: TSpinEdit;
    SpinEdit9: TSpinEdit;
    Timer1: TTimer;
    procedure AnimationStart();
    procedure AnimationStop();
    procedure Button10Click(Sender: TObject);
    procedure Button11Click(Sender: TObject);
    procedure Button12Click(Sender: TObject);
    procedure Button13Click(Sender: TObject);
    procedure Button14Click(Sender: TObject);
    procedure Button15Click(Sender: TObject);
    procedure Button16Click(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure Button3Click(Sender: TObject);
    procedure Button4Click(Sender: TObject);
    procedure Button5Click(Sender: TObject);
    procedure Button6Click(Sender: TObject);
    procedure Button7Click(Sender: TObject);
    procedure Button8Click(Sender: TObject);
    procedure Button9Click(Sender: TObject);
    procedure CheckBox1Change(Sender: TObject);
    procedure FloatSpinEdit1Change(Sender: TObject);
    procedure FloatSpinEdit2Change(Sender: TObject);
    procedure FloatSpinEdit3Change(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    function ColorStr(r, g, b: integer): String;
    function Color1Str(): String;
    function Color2Str(): String;
    procedure Image1MouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure Image1MouseMove(Sender: TObject; Shift: TShiftState; X, Y: Integer
      );
    procedure Image1MouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure Image1Resize(Sender: TObject);
    procedure ListBox1SelectionChange(Sender: TObject; User: boolean);
    procedure Memo1Change(Sender: TObject);
    procedure Panel1Click(Sender: TObject);
    procedure Panel2Click(Sender: TObject);
    procedure SpinEdit12Change(Sender: TObject);
    procedure SpinEdit1Change(Sender: TObject);
    procedure SpinEdit2Change(Sender: TObject);
    procedure SpinEdit3Change(Sender: TObject);
    procedure SpinEdit4Change(Sender: TObject);
    procedure SpinEdit5Change(Sender: TObject);
    procedure SpinEdit6Change(Sender: TObject);
    procedure SpinEdit9Change(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
  private
    doc: TXMLDocument;
    RootNode: TDOMNode;
    ascene: TDOMNode;    //selected active scene
    g_draggedButton: TMouseButton;
    g_animation: TBuffer;
    g_animationFrame: integer;
    g_animationSx: integer;
    g_animationSy: integer;
    g_animationFormat: integer;

    procedure GuiUpdate(Sender: TObject);
    procedure LoadFile(filename: utf8String);
    function FirstCommentLine(ch: TDomNode):utf8String;
    procedure PreviewDrawPixel(r, g, b, x, y, sx, sy: integer; var img: TBitmap);
    procedure PreviewDraw(ch: TDomNode);
    function NormalizeColor(c, format: integer): integer;

    function IntToDomstr(val: integer): DOMString;
    function FloatToDomstr(val: double): DOMString;

  public

  end;

var
  Form1: TForm1;

const
  FILEFORMAT = 1;

implementation

{$R *.lfm}

{ TForm1 }

function TForm1.IntToDomstr(val: integer): DOMString;
begin
  result := DomString(IntToStr(val));
end;

function TForm1.FloatToDomstr(val: double): DOMString;
begin
  result := DomString(FloatToStr(val));
end;

procedure TForm1.FormCreate(Sender: TObject);
  var img: TBitmap;
begin
  doc := TXMLDocument.create;
  //create a root node
  RootNode := doc.CreateElement('storybook');
  doc.Appendchild(RootNode);
  TDOMElement(RootNode).SetAttribute('Xsize', IntToDomstr(spinedit1.value));
  TDOMElement(RootNode).SetAttribute('Ysize', IntToDomstr(spinedit2.value));
  TDOMElement(RootNode).SetAttribute('fileformat', IntToDomstr(FILEFORMAT));
  TDOMElement(RootNode).SetAttribute('colorFormat', IntToDomstr(spinedit5.value));
  img := TBitmap.Create();
  img.SetSize(image1.width, image1.Height);
  img.canvas.FillRect(0, 0, 4096, 4096);
  image1.Picture.Graphic := img;
  img.free;
  g_draggedButton := mbExtra1; //used extract replacement as there is no "none" state
  Spinedit6Change(Sender);
  Spinedit9Change(Sender);
end;


function TForm1.ColorStr(r, g, b: integer): String;
var
  bits: integer;
begin
  bits := spinedit5.value;
  if (bits < 16) and (bits >= 1) then begin
    r := (r * $FFFF) div ((1 shl bits) - 1);
    g := (g * $FFFF) div ((1 shl bits) - 1);
    b := (b * $FFFF) div ((1 shl bits) - 1);
  end;
  result := Dec2Numb(b, 4, 16) + Dec2Numb(g, 4, 16) + Dec2Numb(r, 4, 16);
end;

function TForm1.Color1Str(): String;
begin
  result := ColorStr(spinedit6.value, spinedit7.value, spinedit8.value);
end;

function TForm1.Color2Str(): String;
begin
  result := ColorStr(spinedit9.value, spinedit10.value, spinedit11.value);
end;

procedure TForm1.Image1MouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
  mx, my, px, py, i, pixelPos, xy: integer;
  ch2: TDomNode;
  data, dataOrigin, onePixel: DomString;
begin
  g_draggedButton := button;

  mx := spinedit1.value;
  my := spinedit2.value;
  xy := max(mx, my);
  px := x * xy div image1.width;
  py := y * xy div image1.Height;
  if (button = mbleft) then begin
    onePixel := DomString(Color1Str());
  end else if (button = mbright) then begin
    onePixel := DomString(Color2Str());
  end else
    exit;
  if not assigned(ascene) then
    exit;
  if (ascene.NodeName <> 'frame') then
    exit;
  //create and find data node
  if not Assigned(ascene) then begin
    exit;
  end;
  ch2 := ascene.FirstChild;
  for i := 0 to py do
   begin
     if not Assigned(ch2) then begin
       ch2 := doc.CreateElement('line');
       ascene.AppendChild(ch2);
       ch2.AppendChild(doc.CreateTextNode(DomString(StringOfChar('0', mx * render.charsPerColor))));
     end;
     if (i = py) then
       break;

     ch2 := ch2.NextSibling;
   end;
  //insert color at right position
  dataOrigin := ch2.FirstChild.NodeValue;
  data := dataOrigin;
  if Length(data) < px * render.charsPerColor then
    data := data + DomString(StringOfChar('0', mx * render.charsPerColor - Length(data)));
  pixelPos := px * render.charsPerColor + 1;
  Delete(data, pixelPos, render.charsPerColor);
  Insert(onePixel, data, pixelPos);
  //write data back to the node
  ch2.FirstChild.NodeValue := data;
  //update preview
  if (dataOrigin <> data) then //With large previews drawring at every 1 pixel mouse move really takes CPU time and lags
    PreviewDraw(ascene);
end;

procedure TForm1.Image1MouseMove(Sender: TObject; Shift: TShiftState; X,
  Y: Integer);
begin
  Image1MouseDown(sender, g_draggedButton, shift, x, y);
end;

procedure TForm1.Image1MouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  g_draggedButton := mbExtra1;
end;

procedure TForm1.Image1Resize(Sender: TObject);
begin
  image1.Height := image1.Width;
end;

procedure TForm1.ListBox1SelectionChange(Sender: TObject; User: boolean);
var
  colorst: string;
  pixel: TPixel;
begin
  if (listbox1.ItemIndex < 0) or (RootNode = nil) then begin
    groupbox3.Enabled := false;
    image1.enabled := false;
  end else begin
    //seek proper node
    ascene := RootNode.ChildNodes.Item[listbox1.itemindex];
    if (ascene <> nil) then begin
      groupbox3.Enabled := true;
      image1.enabled := true;
      spinedit3.Enabled := false;
      floatspinedit1.Enabled := false;
      floatspinedit2.Enabled := false;
      spinedit4.Enabled := false;
      spinedit12.Enabled := false;
      floatspinedit3.Enabled := false;
      memo1.text := String(ascene.Attributes.GetNamedItem('comment').NodeValue);
      if (ascene.NodeName = 'frame') then begin
        spinedit3.enabled := true;
        spinedit3.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('duration').NodeValue);
        PreviewDraw(ascene);
      end;
      if (ascene.NodeName = 'roll') or (ascene.NodeName = 'shift') then begin
        spinedit3.enabled := true;
        spinedit3.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('duration').NodeValue);
        floatspinedit1.enabled := true;
        floatspinedit1.Value := render.DomstrToFloat(ascene.Attributes.GetNamedItem('x').NodeValue);
        floatspinedit2.enabled := true;
        floatspinedit2.Value := render.DomstrToFloat(ascene.Attributes.GetNamedItem('y').NodeValue);
        spinedit4.enabled := true;
        spinedit4.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('repeats').NodeValue);
      end;
      if (ascene.NodeName = 'text') then begin
        spinedit3.enabled := true;
        spinedit3.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('duration').NodeValue);
        spinedit4.enabled := true;
        spinedit4.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('repeats').NodeValue);
        spinedit12.enabled := true;
        spinedit12.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('font').NodeValue);
      end;
      if (ascene.NodeName = 'pause') then begin
        spinedit3.enabled := true;
        spinedit3.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('duration').NodeValue);
      end;
      if (ascene.Attributes.GetNamedItem('color1') <> nil) then begin
        colorst := String(ascene.Attributes.GetNamedItem('color1').NodeValue);
        pixel := render.string2Pixel(colorst, spinedit5.value);
        spinedit6.Value := pixel.r;
        spinedit7.Value := pixel.g;
        spinedit8.Value := pixel.b;
        SpinEdit6Change(Sender);
      end;
      if (ascene.Attributes.GetNamedItem('color2') <> nil) then begin
        colorst := String(ascene.Attributes.GetNamedItem('color2').NodeValue);
        pixel := render.string2Pixel(colorst, spinedit5.value);
        spinedit9.Value := pixel.r;
        spinedit10.Value := pixel.g;
        spinedit11.Value := pixel.b;
        SpinEdit9Change(Sender);
      end;
      if (ascene.NodeName = 'fadein') or (ascene.NodeName = 'fadeout') then begin
        spinedit3.enabled := true;
        spinedit3.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('duration').NodeValue);
        spinedit4.enabled := true;
        spinedit4.Value := render.DomstrToInt(ascene.Attributes.GetNamedItem('repeats').NodeValue);
        floatspinedit3.Enabled := true;
        //floatspinedit3.Value := render.DomstrToFloat(ascene.Attributes.GetNamedItem('gamma').NodeValue);
      end;
    end;
  end;
end;

procedure TForm1.Memo1Change(Sender: TObject);
begin
    TDOMElement(ascene).SetAttribute('comment', DomString(memo1.Text));
end;

procedure TForm1.Panel1Click(Sender: TObject);
  var r, g, b, rm, gm, bm, bits: integer;
begin
  if (colordialog1.Execute) then begin
    b := colordialog1.Color shr 16;
    g := (colordialog1.Color shr 8) and $FF;
    r := colordialog1.Color and $FF;
    bits := spinedit5.Value;
    rm := (r shl 8) shr (16 - bits);
    gm := (g shl 8) shr (16 - bits);
    bm := (b shl 8) shr (16 - bits);
    SpinEdit6.value := rm;
    SpinEdit7.value := gm;
    SpinEdit8.value := bm;
    SpinEdit6Change(Sender);
  end;
end;

procedure TForm1.Panel2Click(Sender: TObject);
  var r, g, b, rm, gm, bm, bits: integer;
begin
  if (colordialog2.Execute) then begin
    b := colordialog2.Color shr 16;
    g := (colordialog2.Color shr 8) and $FF;
    r := colordialog2.Color and $FF;
    bits := spinedit5.Value;
    rm := (r shl 8) shr (16 - bits);
    gm := (g shl 8) shr (16 - bits);
    bm := (b shl 8) shr (16 - bits);
    SpinEdit9.value := rm;
    SpinEdit10.value := gm;
    SpinEdit11.value := bm;
    SpinEdit9Change(Sender);
  end;
end;

procedure TForm1.SpinEdit12Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('font', IntToDomstr(SpinEdit12.Value));
end;

procedure TForm1.SpinEdit1Change(Sender: TObject);
begin
  TDOMElement(RootNode).SetAttribute('Xsize', IntToDomstr(SpinEdit1.Value));
end;

procedure TForm1.SpinEdit2Change(Sender: TObject);
begin
  TDOMElement(RootNode).SetAttribute('Ysize', IntToDomstr(SpinEdit2.Value));
end;

procedure TForm1.SpinEdit3Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('duration', IntToDomstr(spinedit3.value));
end;

procedure TForm1.SpinEdit4Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('repeats', IntToDomstr(spinedit4.value));
end;

procedure TForm1.SpinEdit5Change(Sender: TObject);
  var iMax: integer;
begin
  TDOMElement(RootNode).SetAttribute('colorFormat', IntToDomstr(SpinEdit5.Value));
  iMax := (1 shl SpinEdit5.value) - 1;
  SpinEdit6.MaxValue := imax;
  SpinEdit7.MaxValue := imax;
  SpinEdit8.MaxValue := imax;
  SpinEdit6Change(Sender);
  SpinEdit9.MaxValue := imax;
  SpinEdit10.MaxValue := imax;
  SpinEdit11.MaxValue := imax;
  SpinEdit9Change(Sender);
end;

function TForm1.NormalizeColor(c, format: integer): integer;
begin
  if (format > 8) then begin
    result := c shr (format - 8);
  end else if (format > 0) then begin
    result := (c * 255) div ((1 shl format) - 1);
  end else
    result := 0; //error case
end;

procedure TForm1.SpinEdit6Change(Sender: TObject);
  var r, g, b, bits: integer;
begin
  bits := spinedit5.Value;
  r := NormalizeColor(spinedit6.Value, bits);
  g := NormalizeColor(spinedit7.Value, bits);
  b := NormalizeColor(spinedit8.Value, bits);
  panel1.Color := (b shl 16) or (g shl 8) or r;
  colordialog1.Color := panel1.Color;
  if r > 128 then begin r := 0; end else r := 255;
  if g > 128 then begin g := 0; end else g := 255;
  if b > 128 then begin b := 0; end else b := 255;
  panel1.font.Color := (b shl 16) or (g shl 8) or r;
  if (assigned(ascene)) then
    if (ascene.Attributes.GetNamedItem('color1') <> nil) then
      TDOMElement(ascene).SetAttribute('color1', DomString(Color1Str()));
end;

procedure TForm1.SpinEdit9Change(Sender: TObject);
  var r, g, b, bits: integer;
begin
  bits := spinedit5.Value;
  r := NormalizeColor(spinedit9.Value, bits);
  g := NormalizeColor(spinedit10.Value, bits);
  b := NormalizeColor(spinedit11.Value, bits);
  panel2.Color := (b shl 16) or (g shl 8) or r;
  colordialog2.Color := panel2.Color;
  if r > 128 then begin r := 0; end else r := 255;
  if g > 128 then begin g := 0; end else g := 255;
  if b > 128 then begin b := 0; end else b := 255;
  panel2.font.Color := (b shl 16) or (g shl 8) or r;
  if (assigned(ascene)) then
    if (ascene.Attributes.GetNamedItem('color2') <> nil) then
      TDOMElement(ascene).SetAttribute('color2', DomString(Color2Str()));
end;

procedure TForm1.Timer1Timer(Sender: TObject);
var
  x, y, r, g, b, bytesPerPixel: integer;
  pixel: TPixel;
  img: TBitmap;
begin
  if (g_animationFrame = 0) then begin
    g_animationFormat := getUint8(6, g_animation);
    g_animationSx := getUint16(7, g_animation);
    g_animationSy := getUint16(9, g_animation);
    inc(g_animationFrame, 12); //jump over header
  end;
  if (getUint8(g_animationFrame, g_animation) <> 2) then begin
    AnimationStop();
    exit;
  end;
  img := TBitmap.Create();
  img.SetSize(image1.Width, image1.height);
  img.canvas.Brush.Color := $808080;
  img.canvas.FillRect(0, 0, 4096, 4096);
  bytesPerPixel := render.bytesPerPixel(g_animationFormat);
  for y := 0 to g_animationSy - 1 do begin
    for x := 0 to g_animationSx - 1 do begin
      pixel := getPixel(x, y, g_animationFrame, g_animationFormat, g_animationSx, g_animation);
      r := NormalizeColor(pixel.r, pixel.format);
      g := NormalizeColor(pixel.g, pixel.format);
      b := NormalizeColor(pixel.b, pixel.format);
      PreviewDrawPixel(r, g, b, x, y, g_animationSx, g_animationSy, img);
    end;
  end;
  image1.Picture.Graphic := img;
  img.free;
  timer1.interval := getUint16(g_animationFrame + 1, g_animation);
  inc(g_animationFrame, 3 + g_animationSy * g_animationSx * bytesPerPixel);
end;

procedure TForm1.Button2Click(Sender: TObject);
begin
  if (savedialog1.Execute) then begin
    WriteXMLFile(doc, savedialog1.filename);
    savedialog2.initialdir := extractfilepath(savedialog1.FileName);
  end;
end;

procedure TForm1.Button3Click(Sender: TObject);
var
  animation: TBuffer;
  f: file;
begin
  if (SaveDialog2.Execute) then begin
   animation := render.render(RootNode);
   if (assigned(animation.data)) then begin
     AssignFile(f, Savedialog2.filename);
     ReWrite(f, 1);
     BlockWrite(f, animation.data^, animation.dataUsed);
     CloseFile(f);
     freeMem(animation.data);
   end else
     showmessage('Failed to render animation');
  end;
end;

procedure TForm1.AnimationStart();
begin
  g_animation := render.render(RootNode);
  if (assigned(g_animation.data)) then begin
    g_animationFrame := 0;
    timer1.interval := 1;
    timer1.enabled := true;
    button4.Caption := 'Stop';
    label16.Caption := 'Needs ' + inttostr(g_animation.dataUsed) + 'bytes';
  end;
end;

procedure TForm1.AnimationStop();
begin
  timer1.enabled := false;
  button4.Caption := 'Play';
  if assigned(g_animation.data) then
    freeMem(g_animation.data);
  g_animation.data := nil;
end;


procedure TForm1.Button4Click(Sender: TObject);
begin
  if (timer1.Enabled) then begin
    AnimationStop();
  end else begin
    AnimationStart();
  end;
end;

procedure TForm1.Button5Click(Sender: TObject);
var ch: TDomNode;
  ind : integer;
begin
  ind := listbox1.itemindex;
  if (ind > 0) then begin
    ch := ascene.PreviousSibling;
    if (assigned(ch)) then begin
      RootNode.InsertBefore(ascene, ch);
      GuiUpdate(Sender);
      dec(ind);
      listbox1.itemindex := ind;
      listbox1selectionchange(sender, false);
    end;
  end;

end;

procedure TForm1.Button6Click(Sender: TObject);
var ch, ch2: TDomNode;
  ind : integer;
begin
    ind := listbox1.itemindex;
  if (ind >= 0) and (ind < listbox1.Items.Count-1) then begin
    ch := ascene.NextSibling;
    if (assigned(ch)) then begin
      ch2 := ch.NextSibling;    //there is no insertAfter command
      if (assigned(ch2)) then begin
        RootNode.InsertBefore(ascene, ch2);
      end else
        RootNode.AppendChild(ascene);
      GuiUpdate(Sender);
      inc(ind);
      listbox1.itemindex := ind;
      listbox1selectionchange(sender, false);
    end;
  end;
end;

procedure TForm1.Button7Click(Sender: TObject);
begin
  if (listbox1.itemindex >= 0) then begin
    RootNode.RemoveChild(ascene);
    GuiUpdate(sender);
  end;
end;

procedure TForm1.Button8Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('frame');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('comment', '');
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button9Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('roll');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('x', IntToDomstr(1));
  TDOMElement(ch).SetAttribute('y', IntToDomstr(0));
  TDOMElement(ch).SetAttribute('repeats', IntToDomstr(5));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.CheckBox1Change(Sender: TObject);
begin
  PreviewDraw(ascene);
end;

procedure TForm1.FloatSpinEdit1Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('x', FloatToDomstr(floatspinedit1.value));
end;

procedure TForm1.FloatSpinEdit2Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('y', FloatToDomstr(floatspinedit2.value));
end;

procedure TForm1.FloatSpinEdit3Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('gamma', FloatToDomstr(floatspinedit3.value));
end;

procedure TForm1.Button1Click(Sender: TObject);
begin
  if (opendialog1.Execute) then begin
    opendialog1.initialdir := extractfilepath(opendialog1.FileName);
    savedialog1.initialdir := opendialog1.initialdir;
    savedialog2.initialdir := opendialog1.initialdir;
    Loadfile(opendialog1.filename);
  end;
end;

procedure TForm1.Button10Click(Sender: TObject);
  var ch: TDomNode;
  oldIndex: integer;
begin
  oldIndex := listbox1.itemindex;
  if (oldIndex >= 0) then begin
    ch := ascene.CloneNode(true);
    RootNode.AppendChild(ch);
    GuiUpdate(Sender);
    listbox1.itemindex := oldIndex;
  end;
end;

procedure TForm1.Button11Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('text');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('repeats', IntToDomstr(5));
  TDOMElement(ch).SetAttribute('font', IntToDomstr(1));
  TDOMElement(ch).SetAttribute('color1', DomString(Color1Str()));
  TDOMElement(ch).SetAttribute('color2', DomString(Color2Str()));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button12Click(Sender: TObject);
begin
  showmessage('Pixel Animate' + LineEnding +
    '(c) 2026 by Malte Marwedel' + LineEnding +
    'Version ' + render.progamVersion + LineEnding +
    'License: GPL version 2 or later');
end;

procedure TForm1.Button13Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('shift');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('x', IntToDomstr(1));
  TDOMElement(ch).SetAttribute('y', IntToDomstr(0));
  TDOMElement(ch).SetAttribute('repeats', IntToDomstr(5));
  TDOMElement(ch).SetAttribute('color1', DomString(Color1Str()));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button14Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('pause');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button15Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('fadein');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('repeats', IntToDomstr(5));
  TDOMElement(ch).SetAttribute('gamma', FloatToDomstr(0.5));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button16Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('fadeout');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', IntToDomstr(100));
  TDOMElement(ch).SetAttribute('repeats', IntToDomstr(5));
  TDOMElement(ch).SetAttribute('gamma', FloatToDomstr(0.5));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.GuiUpdate(Sender: TObject);
  var bases: TDOMNode;
  temp: utf8String;
begin
  //the update button
  if rootnode = nil then
    exit;
  spinedit1.Value := render.DomstrToInt(RootNode.Attributes.GetNamedItem('Xsize').NodeValue);
  spinedit2.Value := render.DomstrToInt(RootNode.Attributes.GetNamedItem('Ysize').NodeValue);
  spinedit5.Value := render.DomstrToInt(RootNode.Attributes.GetNamedItem('colorFormat').NodeValue);
  SpinEdit5Change(Sender);
  bases := RootNode.FirstChild;
  listbox1.Clear;
  while Assigned(bases) do begin
    temp := Utf8String(bases.NodeName) + ' ' + firstCommentLine(bases);
    listbox1.Items.Add(temp);
    bases := bases.NextSibling;
  end;
end;

procedure TForm1.LoadFile(filename: utf8String);
  var fileversion:integer;
begin
  if (fileexists(filename)) then begin
   ascene := nil;
   RootNode := nil;
   doc.free;
   ReadXMLFile(doc, FileName);
   savedialog1.FileName := FileName;
   savedialog2.InitialDir:= ExtractFilePath(filename);
   RootNode := doc.FirstChild;
   if (assigned(RootNode)) then begin
     if (RootNode.NodeName = 'storybook') then begin
       if (RootNode.Attributes.GetNamedItem('fileformat') <> nil) then begin
         fileversion := render.DomstrToInt(RootNode.Attributes.GetNamedItem('fileformat').NodeValue);
       end else begin
         fileversion := 1;
       end;
       if (fileversion > FILEFORMAT) then begin
         showmessage('Warning, this file has a newer format >'+inttostr(fileversion)+
         '< than supported by this version of PixelAnimate. Some functions might not work as expected.');
       end;
       GuiUpdate(nil);
     end else
       showmessage('Error: XML file does not contain a storybook');
   end else
     showmessage('Error: No root node');
 end else
   showmessage('Error: File does not exists');
end;

function TForm1.FirstCommentLine(ch: TDomNode):utf8String;
var
  x: utf8String;
  newline: integer;
begin
   result := '';
   newline := 1;
   if (ch.Attributes.GetNamedItem('comment') <> nil) then begin
     x := Utf8String(ch.Attributes.GetNamedItem('comment').NodeValue);
     while (newline <= length(x)) do begin
       if (x[newline] = char(10)) or (x[newline] = char(13)) then
         break;
       newline := newline + 1;
     end;
     if (newline > 1) then begin
       result := ' - ' + LeftStr(x, newline-1);
     end;
   end;
end;

procedure TForm1.PreviewDrawPixel(r, g, b, x, y, sx, sy: integer; var img: TBitmap);
var
  scaler, radius, offsetx, offsety, cx, cy: float;
begin
  scaler := single(image1.Width) / single(max(sx, sy));
  radius := scaler / 2;
  offsetx := scaler / 2;
  offsety := scaler / 2;
  img.canvas.Brush.Color := (b shl 16) or (g shl 8) or r;
  img.Canvas.Pen.Style := psClear;
  cx := offsetx + x * scaler;
  cy := offsety + y * scaler;
  if (checkbox1.Checked) then begin
    img.Canvas.Rectangle(round(cx - radius), round(cy - radius), round(cx + radius), round(cy + radius));
  end else
    img.Canvas.Ellipse(round(cx - radius), round(cy - radius), round(cx + radius), round(cy + radius));
end;

procedure TForm1.PreviewDraw(ch: TDomNode);
var
  x, y, r, g, b, sx, sy: integer;
  ch2, ch3: TDomNode;
  data, onePixel, rs, gs, bs: string;
  img: TBitmap;
begin
  sx := spinedit1.value;
  sy := spinedit2.value;
  if not assigned(ch) then
    exit;
  ch2 := ch.FirstChild;
  img := TBitmap.Create();
  img.SetSize(image1.Width, image1.height);
  img.canvas.Brush.Color := $808080;
  img.canvas.FillRect(0, 0, 4096, 4096);
  for y := 0 to sy - 1 do begin
    if (assigned(ch2)) then begin
      ch3 := ch2.FirstChild;
      if (assigned(ch3)) then begin
        data := String(ch3.NodeValue);
        for x := 0 to sx - 1 do begin
          //12 hex characters for each pixel
          onePixel := Copy(data, x * 12 + 1, 12);
          if (onePixel.length >= 12) then begin
            //showmessage(onePixel);
            bs := Copy(onePixel, 1, 4);
            gs := Copy(onePixel, 5, 4);
            rs := Copy(onePixel, 9, 4);
            b := Hex2Dec(bs) shr 8;
            g := Hex2Dec(gs) shr 8;
            r := Hex2Dec(rs) shr 8;
            PreviewDrawPixel(r, g, b, x, y, sx, sy, img);
          end;
        end;
      end;
      ch2 := ch2.NextSibling;
    end;
  end;
  image1.Picture.Graphic := img;
  img.free;
end;

end.

