(* ::Package:: *)

BeginPackage["MathMF`",{"CCompilerDriver`"}]


MFInitSourceReader::usage="MFInitSourceReader[filename] initialises the source reader. 
It returns {duration, framerate, width, height}"


MFGrabFrame::usage="MFGrabFrame[type] grabs a frame from the input stream. 
Possible values for type are \"Byte\", \"Real\", \"ByteImage\" or \"RealImage\""


MFSourceTime::usage="MFSourceTime[] returns the timestamp of the last grabbed frame"


MFInitSinkWriter::usage="MFInitSinkWriter[filename, width, height] initialises the sink writer"


MFSendFrame::usage="MFSendFrame[data] sends data (an Image or image data array) to the output stream"


MFFinaliseSink::usage="MFFinaliseSink[] is used to finish writing a video stream and close the output file.";


MFUnload::usage="MFUnload[] unloads the MathMF library. This is primarily for use when developing the library code and should not be needed in normal usage."


MFVideo::nofile="The input file does not exist";


MFVideo::fileexists="The output file already exists";


MFVideo::badtype="The output file must have a .wmv or .mp4 extension";


MFVideo::odddims="The frame width and height must be even numbers";


MFVideo::baddata="The data must be an Image or a 2D or 3D numerical array";


MFVideo::baddims="The data dimensions are incorrect, or there is no output stream";


LibraryFunction::initsourcefail="Unable to initialise source reader. Possible reasons include:\nThe file is not a valid media file.\nThe file does not contain any video streams.\nThe Media Foundation platform cannot decode the stream.\n";


LibraryFunction::finalisefail="The sink writer could not be finalised. Possible reasons include:\nThe sink writer was not initialised.\nNo frames were sent to the sink writer.\nThe bit rate was too low and no samples were added to the stream.";


Begin["Private`"]


(* ::Section:: *)
(*Find or create the DLL*)


MathMFlib=FindLibrary["MathMF"];


If[MathMFlib===$Failed,
Print["No MathMF.DLL found.\nMathMF will attempt to build the library from source."];
Module[{codefile,code},
codefile=FileNameJoin[ReplacePart[FileNameSplit[$InputFileName],-1->"MathMF.cpp"]];
If[!FileExistsQ[codefile],
Print["No source file found at "<>codefile<>"\nAborting MFMath."];Abort[]];
code=Import[codefile,"Text"];
Needs["CCompilerDriver`"];
MathMFlib=CreateLibrary[code,"MathMF",
"Language"->"C++",
"CleanIntermediate"->True,
"Libraries"->{"ole32.lib","mfreadwrite.lib","mfplat.lib","mfuuid.lib"}];
If[!FileExistsQ[MathMFlib],
Print["Library creation failed.\nAborting MFMath."];Abort[],
Print["MathMF.DLL successfully created at "<>MathMFlib]]]]


(* ::Section:: *)
(*Load the library functions*)


(* ::Subsection:: *)
(*Underlying LibraryFunction[] 's*)


MFISR=LibraryFunctionLoad[MathMFlib,"InitSourceReader",{"UTF8String"},{Real,1}];


MFGF=LibraryFunctionLoad[MathMFlib,"GrabFrame",{},{Integer,3}];


MFSourceTime=LibraryFunctionLoad[MathMFlib,"SourceTime",{},Real];


MFISW=LibraryFunctionLoad[MathMFlib,"InitSinkWriter",{"UTF8String",Integer,Integer,Integer,Real,Integer},"Void"];


MFSF=LibraryFunctionLoad[MathMFlib,"SendFrame",{{Integer,3,"Constant"}},Integer];


MFFS=LibraryFunctionLoad[MathMFlib,"FinaliseSink",{},"Void"];


(* ::Subsection:: *)
(*Create wrapper functions to check arguments and handle errors*)


convertFromByteData[x_,"Byte"]:=x;
convertFromByteData[x_,"Real"]:=x/255.;
convertFromByteData[x_,"ByteImage"]:=Image[x,"Byte"];
convertFromByteData[x_,"RealImage"]:=Image[x/255.];


convertToByteData[x_Image]:=ImageData[x~ColorConvert~"RGB","Byte"];
convertToByteData[x_List/;ArrayQ[x,2|3,IntegerQ]]:=convertToByteData[Image[x,"Byte"]];
convertToByteData[x_List/;ArrayQ[x,2|3,NumericQ]]:=convertToByteData[Image[x]];
convertToByteData[__]=$Failed;


MFInitSourceReader[fn_String]:=Catch[
If[!FileExistsQ[fn],Message[MFVideo::nofile];Throw[{}]];
MapAt[Round,Check[MFISR[fn],Throw[$Failed]],{{3},{4}}]]


MFGrabFrame[s:("Byte"|"Real"|"ByteImage"|"RealImage")]:=
With[{m=LibraryFunction::endofstream},
Catch[convertFromByteData[Quiet[Check[MFGF[],Throw[EndOfFile],{m}],{m}],s]]]


MFGrabFrame[]:=MFGrabFrame["Real"]


Options[MFInitSinkWriter]={"FrameRate"->29.97,"CompressionRatio"->100,"BitRate"->Automatic};


MFInitSinkWriter[fn_String,w_Integer,h_Integer,OptionsPattern[]]:=Catch[
If[FileExistsQ[fn],Message[MFVideo::fileexists];Throw[$Failed]];
If[OddQ[w]||OddQ[h],Message[MFVideo::odddims];Throw[$Failed]];
$mfswd={h,w,3};
Module[{FR,CR,BR},
FR=OptionValue["FrameRate"];
CR=OptionValue["CompressionRatio"];
BR=OptionValue["BitRate"]/.Automatic->Round[24FR w h /(1000CR)];
Which[
StringMatchQ[fn,__~~".wmv"],
$mfswf=Identity;MFISW[fn,1,w,h,FR,BR],
StringMatchQ[fn,__~~".mp4"],
(* MP4 writing requires the data upside down *)
$mfswf=Reverse;MFISW[fn,2,w,h,FR,BR],
True,Message[MFVideo::badtype];$Failed]]]


MFSendFrame[data_]:=Catch[
Module[{d=convertToByteData[data]},
If[d===$Failed,Message[MFVideo::baddata];Throw[$Failed]];
If[Dimensions[d]=!=$mfswd,Message[MFVideo::baddims];Throw[$Failed]];
MFSF[$mfswf@d]]]


MFFinaliseSink[]:=(MFFS[];$mfswd={0,0,0};)


MFUnload[]:=LibraryUnload@MathMFlib


(* ::Section:: *)
(*Finish*)


End[]


EndPackage[]
