MathMF
======

A *Mathematica* package for frame-by-frame import and export of videos using Windows Media Foundation.

This was developed for *Mathematica* users on Windows who do not have Quicktime installed, because the built-in video import is limited to uncompressed AVI in those circumstances. The frame-by-frame export functionality may be useful even for users who do have Quicktime, as it does not require the entire image list to be created before export.

For information on supported formats see the demo notebook.


Installation
------------

Place the MathMF folder in your Applications directory. To get the path to this directory you can evaluate `FileNameJoin[{$UserBaseDirectory, "Applications"}]` in *Mathematica*.


Loading
-------

To load the package use ``Needs["MathMF`"]``

Upon loading, the package will look for the LibraryLink DLL "MathMF.dll".
If the DLL is not found, the package will attempt to build it using `CreateLibrary`.
If the build fails you will need to use the pre-built DLL (see below).


Demo notebook
-------------

The "MathMF Demo.nb" notebook contains simple examples of using the import (Source Reader) and export (Sink Writer) functionality. Be sure to change the example file paths to something suitable for your system.


Pre-built DLL
-------------

The pre-built DLL is provided in case the library cannot be created locally (for example if a suitable C compiler is not present). The DLL should be placed in the directory obtained from `FileNameJoin[{$UserBaseDirectory, "SystemFiles\\LibraryResources\\Windows-x86-64"}]`

The pre-built DLL requires the Visual C++ Redistributable for Visual Studio 2012. If not already installed this can be obtained from Microsoft at http://www.microsoft.com/en-gb/download/details.aspx?id=30679


