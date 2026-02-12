unit main;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls, Spin,
  ExtCtrls, laz2_DOM, laz2_XMLWrite, laz2_XMLRead;

type

  { TForm1 }

  TForm1 = class(TForm)
    Button1: TButton;
    Button10: TButton;
    Button2: TButton;
    Button3: TButton;
    Button4: TButton;
    Button5: TButton;
    Button6: TButton;
    Button7: TButton;
    Button8: TButton;
    Button9: TButton;
    FloatSpinEdit1: TFloatSpinEdit;
    FloatSpinEdit2: TFloatSpinEdit;
    GroupBox1: TGroupBox;
    GroupBox2: TGroupBox;
    GroupBox3: TGroupBox;
    Image1: TImage;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label5: TLabel;
    ListBox1: TListBox;
    Memo1: TMemo;
    OpenDialog1: TOpenDialog;
    SaveDialog1: TSaveDialog;
    SaveDialog2: TSaveDialog;
    SpinEdit1: TSpinEdit;
    SpinEdit2: TSpinEdit;
    SpinEdit3: TSpinEdit;
    SpinEdit4: TSpinEdit;
    procedure Button10Click(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure Button3Click(Sender: TObject);
    procedure Button5Click(Sender: TObject);
    procedure Button6Click(Sender: TObject);
    procedure Button7Click(Sender: TObject);
    procedure Button8Click(Sender: TObject);
    procedure Button9Click(Sender: TObject);
    procedure FloatSpinEdit1Change(Sender: TObject);
    procedure FloatSpinEdit2Change(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure ListBox1SelectionChange(Sender: TObject; User: boolean);
    procedure Memo1Change(Sender: TObject);
    procedure SpinEdit3Change(Sender: TObject);
    procedure SpinEdit4Change(Sender: TObject);
  private
    doc: TXMLDocument;
    RootNode: TDOMNode;
    ascene: TDOMNode;    //selected active scene
    procedure GuiUpdate(Sender: TObject);
    procedure LoadFile(filename: utf8String);
    procedure Export(filename: utf8String);
    function FirstCommentLine(ch: TDomNode):utf8String;
  public

  end;

var
  Form1: TForm1;

const
  FILEFORMAT = 2;

implementation

{$R *.lfm}

{ TForm1 }

procedure TForm1.FormCreate(Sender: TObject);
  var img: TBitmap;
begin
  doc := TXMLDocument.create;
  //create a root node
  RootNode := doc.CreateElement('storybook');
  doc.Appendchild(RootNode);
  TDOMElement(RootNode).SetAttribute('Xsize', inttostr(spinedit1.value));
  TDOMElement(RootNode).SetAttribute('Ysize', inttostr(spinedit2.value));
  TDOMElement(RootNode).SetAttribute('fileformat', inttostr(FILEFORMAT));
  img := TBitmap.Create();
  img.SetSize(512, 512);
  img.canvas.FillRect(0, 0, 4096, 4096);
  image1.Picture.Graphic := img;
  img.free;
end;

procedure TForm1.ListBox1SelectionChange(Sender: TObject; User: boolean);
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
      memo1.text := ascene.Attributes.GetNamedItem('comment').NodeValue;
      if (ascene.NodeName = 'frame') then begin
        spinedit3.enabled := true;
        spinedit3.Value := strtoint(ascene.Attributes.GetNamedItem('duration').NodeValue);
      end;
      if (ascene.NodeName = 'roll') then begin
        spinedit3.enabled := true;
        spinedit3.Value := strtoint(ascene.Attributes.GetNamedItem('duration').NodeValue);
        floatspinedit1.enabled := true;
        floatspinedit1.Value := strtofloat(ascene.Attributes.GetNamedItem('x').NodeValue);
        floatspinedit2.enabled := true;
        floatspinedit2.Value := strtofloat(ascene.Attributes.GetNamedItem('y').NodeValue);
        spinedit4.enabled := true;
        spinedit4.Value := strtoint(ascene.Attributes.GetNamedItem('repeats').NodeValue);
      end;
    end;
  end;
end;

procedure TForm1.Memo1Change(Sender: TObject);
begin
    TDOMElement(ascene).SetAttribute('comment', memo1.Text);
end;

procedure TForm1.SpinEdit3Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('duration', inttostr(spinedit3.value));
end;

procedure TForm1.SpinEdit4Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('repeats', inttostr(spinedit4.value));
end;

procedure TForm1.Button2Click(Sender: TObject);
begin
  if (savedialog1.Execute) then begin
    WriteXMLFile(doc, savedialog1.filename);
    savedialog2.initialdir := extractfilepath(savedialog1.FileName);
  end;
end;

procedure TForm1.Button3Click(Sender: TObject);
begin

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
  TDOMElement(ch).SetAttribute('duration', inttostr(100));
  TDOMElement(ch).SetAttribute('comment', '');
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.Button9Click(Sender: TObject);
  var ch: TDomNode;
begin
  ch := doc.CreateElement('roll');
  TDOMElement(ch).SetAttribute('comment', '');
  TDOMElement(ch).SetAttribute('duration', inttostr(100));
  TDOMElement(ch).SetAttribute('x', inttostr(1));
  TDOMElement(ch).SetAttribute('y', inttostr(0));
  TDOMElement(ch).SetAttribute('repeats', inttostr(5));
  RootNode.AppendChild(ch);
  GuiUpdate(Sender);
end;

procedure TForm1.FloatSpinEdit1Change(Sender: TObject);
begin
  TDOMElement(ascene).SetAttribute('x', floattostr(floatspinedit1.value));
end;

procedure TForm1.FloatSpinEdit2Change(Sender: TObject);
begin
    TDOMElement(ascene).SetAttribute('y', floattostr(floatspinedit2.value));
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
begin
  if (listbox1.itemindex >= 0) then begin
    ch := ascene.CloneNode(true);
    RootNode.AppendChild(ch);
    GuiUpdate(Sender);
  end;
end;

procedure TForm1.GuiUpdate(Sender: TObject);
  var bases: TDOMNode;
  temp: utf8String;
begin
  //the update button
  if rootnode = nil then
    exit;
  spinedit1.Value := strtoint(RootNode.Attributes.GetNamedItem('Xsize').NodeValue);
  spinedit2.Value := strtoint(RootNode.Attributes.GetNamedItem('Ysize').NodeValue);
  bases := RootNode.FirstChild;
  listbox1.Clear;
  while Assigned(bases) do begin
    temp := bases.NodeName + ' ' + firstCommentLine(bases);
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
         fileversion := strtoint(RootNode.Attributes.GetNamedItem('fileformat').NodeValue);
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
     x := ch.Attributes.GetNamedItem('comment').NodeValue;
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


procedure TForm1.Export(filename: utf8String);
begin


end;

end.

