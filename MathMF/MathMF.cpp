//////////////////////////////////////////////////////////////
//                         MathMF                           //
// Mathematica LibraryLink code for video frame import      //
// and export using Windows Media Foundation                //
//                                                          //
//                                   Simon Woods 2014       //
//                                                          //
//////////////////////////////////////////////////////////////


#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

// Wolfram Library
#include "WolframLibrary.h"

// Media Foundation 
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// Other bits
#include <assert.h>
#include <propvarutil.h>

// SafeRelease for COM Interface pointers
template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class VidReader
{
private:

	IMFSourceReader *m_pReader;
	IMFMediaBuffer	*m_pBuffer;
	
public:

    VidReader();
    ~VidReader();
	
	LONGLONG	m_timestamp;
	double		m_duration;
	double		m_framerate;
	UINT32		m_imageheight;
	UINT32		m_imagewidth;

    HRESULT     initSourceReader(WCHAR *filename);
	HRESULT     getReadBuffer(BYTE **ppData);
	HRESULT		releaseBuffer();

private:
    HRESULT     getDuration();
    HRESULT     selectVideoStream();
    HRESULT     getVideoFormat();
};

//-------------------------------------------------------------------
// VidReader constructor
//
VidReader::VidReader() 
    : m_pReader(NULL), m_pBuffer(NULL)
{
	m_timestamp = 0;
	m_imageheight = 0;
	m_imagewidth = 0;
	m_duration = 0;
	m_framerate = 0;
}

//-------------------------------------------------------------------
// VidReader destructor
//
VidReader::~VidReader()
{
    SafeRelease(&m_pBuffer);
    SafeRelease(&m_pReader);
}

//-------------------------------------------------------------------
// Initialise the source reader
//
HRESULT VidReader::initSourceReader(WCHAR *filename)
{
    HRESULT hr = S_OK;
    IMFAttributes *pAttributes = NULL;

    SafeRelease(&m_pReader);

	// Configure the source reader to perform video processing
    hr = MFCreateAttributes(&pAttributes, 1);
	if (FAILED(hr)) goto done;    
	hr = pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (FAILED(hr)) goto done;

    // Create the source reader from the URL
    hr = MFCreateSourceReaderFromURL(filename, pAttributes, &m_pReader);
    if (FAILED(hr)) goto done;

	// Attempt to find a video stream
    hr = selectVideoStream();
    if (FAILED(hr)) goto done;

	// Get the stream format
	hr = getVideoFormat();
    if (FAILED(hr)) goto done;

	// Get the duration
	hr = getDuration();

done:    
    return hr;
}

//-------------------------------------------------------------------
// Read a frame and provide access to the data
//
HRESULT VidReader::getReadBuffer(BYTE **ppData)
{
    HRESULT     hr = S_OK;
    DWORD       dwFlags = 0;
    DWORD       cbBitmapData = 0;       // Size of data, in bytes
	IMFSample	*pSample;

	if (!m_pReader) return E_ABORT; // if no source reader run away

    while (1)
    {
        hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 
            0, NULL, &dwFlags, &m_timestamp, &pSample );

        if (FAILED(hr)) goto done;

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            break;
        }

        if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
        {
            // Type change. Get the new format.
            hr = getVideoFormat();
            if (FAILED(hr)) goto done;
        }

        if (pSample == NULL)
        {
            continue;
        }

        // We got a sample.
        break;
	}

    if (pSample)
    {
        UINT32 pitch = 4 * m_imagewidth; 

        hr = pSample->ConvertToContiguousBuffer(&m_pBuffer);
        if (FAILED(hr)) goto done;

		hr = m_pBuffer->Lock(ppData, NULL, &cbBitmapData);
        if (FAILED(hr)) goto done;

        assert(cbBitmapData == (pitch * m_imageheight));
    }
    else
    {
        hr = MF_E_END_OF_STREAM;
    }

done:
	SafeRelease(&pSample);
	return hr;
}

//-------------------------------------------------------------------
// Release the buffer
//
HRESULT VidReader::releaseBuffer()
{
	HRESULT hr = S_OK;

    if (m_pBuffer) hr = m_pBuffer->Unlock();
	SafeRelease(&m_pBuffer);

	return hr;
}

//-------------------------------------------------------------------
// selectVideoStream:  Finds the first video stream and sets the format to RGB32.
//
HRESULT VidReader::selectVideoStream()
{
    HRESULT hr = S_OK;
    IMFMediaType *pType = NULL;

    // Configure the source reader to give us progressive RGB32 frames.
    // The source reader will load the decoder if needed.

    hr = MFCreateMediaType(&pType);
	if (FAILED(hr)) goto done;

    hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (FAILED(hr)) goto done;

    hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (FAILED(hr)) goto done;

	hr = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
	if (FAILED(hr)) goto done;

    // Ensure the stream is selected.
    hr = m_pReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
	if (FAILED(hr)) goto done;

done:

    SafeRelease(&pType);
    return hr;
}

//-------------------------------------------------------------------
// getVideoFormat:  Gets format information for the video stream.
//
HRESULT VidReader::getVideoFormat()
{
    HRESULT hr = S_OK;
    IMFMediaType *pType = NULL;
	GUID subtype = { 0 };

    // Get the media type from the stream.
    hr = m_pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (FAILED(hr)) goto done;

    // Make sure it is a video format.
    
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (subtype != MFVideoFormat_RGB32)
    {
        hr = E_UNEXPECTED;
        goto done;
    }

    // Get the width and height
    hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &m_imagewidth, &m_imageheight);
    if (FAILED(hr)) goto done;

	// Get the frame rate
	UINT32 frN, frD;
	hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &frN, &frD);   
	if (FAILED(hr)) goto done;
	m_framerate = (double)frN / (double)frD;

done:
    
	SafeRelease(&pType);
    return hr;
}

//-------------------------------------------------------------------
// getDuration: Finds the duration of the current video file
//
HRESULT VidReader::getDuration()
{
	HRESULT hr = S_OK;
	PROPVARIANT var;
	LONGLONG hnsDuration;

	if (m_pReader == NULL) return MF_E_NOT_INITIALIZED;

	PropVariantInit(&var);
	    
    hr = m_pReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);

    if (SUCCEEDED(hr))
    {
        assert(var.vt == VT_UI8);
        hnsDuration = var.hVal.QuadPart;
    }

    PropVariantClear(&var);

	// update member
	m_duration = (double)hnsDuration * 0.0000001;

    return hr; 
}

class VidWriter
{
private:

	IMFSinkWriter	*m_pWriter;
	IMFMediaBuffer  *m_pBuffer;
	DWORD			m_streamIndex;

	HRESULT		configureOutput(IMFMediaType *pMT);
	HRESULT		configureInput(IMFMediaType *pMT);

public:

    VidWriter();
    ~VidWriter();
	
	GUID		m_encodeformat;
	UINT32		m_bitrate;
	UINT64		m_frametime;
	UINT32		m_width, m_height;
	UINT64		m_rtStart; // start time of current frame

	HRESULT     initSinkWriter(WCHAR *filename);
	HRESULT		setParams(long encoderformat, long width, long height, double framerate, long kbps);
	HRESULT		getWriteBuffer(BYTE **ppData);
	HRESULT		writeFrame(BYTE *pData);
	HRESULT		finalise();

};

//-------------------------------------------------------------------
// VidWriter constructor
//
VidWriter::VidWriter() 
	: m_pWriter(NULL), m_pBuffer(NULL)
{
}

//-------------------------------------------------------------------
// VidWriter destructor
//
VidWriter::~VidWriter()
{
	SafeRelease(&m_pWriter);
    SafeRelease(&m_pBuffer);
}

//-------------------------------------------------------------------
// Open a file for writing and prepare the sink writer
//
HRESULT VidWriter::initSinkWriter(WCHAR *filename)
{
    HRESULT hr = S_OK;
	IMFMediaType *pMediaTypeOut = NULL;   
    IMFMediaType *pMediaTypeIn = NULL;   
   
	// Create the sink writer
	SafeRelease(&m_pWriter);
	hr = MFCreateSinkWriterFromURL(filename, NULL, NULL, &m_pWriter);
	if (FAILED(hr)) goto done;

	// Create the output media type
	hr = MFCreateMediaType(&pMediaTypeOut);   
	if (FAILED(hr)) goto done;

	// Configure it
	hr = configureOutput(pMediaTypeOut);
	if (FAILED(hr)) goto done;

	// Add it to the sink writer
	hr = m_pWriter->AddStream(pMediaTypeOut, &m_streamIndex);   
	if (FAILED(hr)) goto done;

	// Create the input media type
	hr = MFCreateMediaType(&pMediaTypeIn);   
	if (FAILED(hr)) goto done;
	
	// Configure it
	hr = configureInput(pMediaTypeIn);
	if (FAILED(hr)) goto done;

	// Add it to the sink writer
	hr = m_pWriter->SetInputMediaType(m_streamIndex, pMediaTypeIn, NULL);   
   	if (FAILED(hr)) goto done;  

    // Tell the sink writer to start accepting data
	hr = m_pWriter->BeginWriting();

	// Reset the frame timer
	m_rtStart = 0;

done:    
	SafeRelease(&pMediaTypeOut);
    SafeRelease(&pMediaTypeIn);
    return hr;
}

//-------------------------------------------------------------------
// Configure output & input media types
//
HRESULT VidWriter::configureOutput(IMFMediaType *pMT)
{
	HRESULT hr = S_OK;
	UINT32 frNumerator, frDenominator;

	hr = pMT->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);     
	if (FAILED(hr)) goto done;

	// Set format
	hr = pMT->SetGUID(MF_MT_SUBTYPE, m_encodeformat);   
	if (FAILED(hr)) goto done;

	// Set bit rate
	hr = pMT->SetUINT32(MF_MT_AVG_BITRATE, m_bitrate);   
	if (FAILED(hr)) goto done;

	// Set progressive frames
	hr = pMT->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
	if (FAILED(hr)) goto done;

	// Set frame size
	hr = MFSetAttributeSize(pMT, MF_MT_FRAME_SIZE, m_width, m_height);   
	if (FAILED(hr)) goto done;

	// Set frame rate
	hr = MFAverageTimePerFrameToFrameRate(m_frametime, &frNumerator, &frDenominator);
	if (FAILED(hr)) goto done;
	hr = MFSetAttributeRatio(pMT, MF_MT_FRAME_RATE, frNumerator, frDenominator);   
	if (FAILED(hr)) goto done;

	// Set PAR
	hr = MFSetAttributeRatio(pMT, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
	if (FAILED(hr)) goto done;

done:
	return hr;
}

HRESULT VidWriter::configureInput(IMFMediaType *pMT)
{
	HRESULT hr = S_OK;
	UINT32 frNumerator, frDenominator;

	hr = pMT->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);   
	if (FAILED(hr)) goto done;

	// Set format
	hr = pMT->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);     
	if (FAILED(hr)) goto done;

	// Set progressive frames
	hr = pMT->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);   
 	if (FAILED(hr)) goto done;

	// Set frame size
	hr = MFSetAttributeSize(pMT, MF_MT_FRAME_SIZE, m_width, m_height);   
 	if (FAILED(hr)) goto done;

	// Set frame rate (not sure why we need to set a frame rate on the input)
	hr = MFAverageTimePerFrameToFrameRate(m_frametime, &frNumerator, &frDenominator);
	if (FAILED(hr)) goto done;
	hr = MFSetAttributeRatio(pMT, MF_MT_FRAME_RATE, frNumerator, frDenominator);   
 	if (FAILED(hr)) goto done;

	// Set PAR
	hr = MFSetAttributeRatio(pMT, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);   
 	if (FAILED(hr)) goto done;

done:
	return hr;
}

//-------------------------------------------------------------------
// Set the output parameters
//
HRESULT VidWriter::setParams(long encoderformat, long width, long height, double framerate, long kbps)
{
	// Select encoding format
	if (encoderformat == 1)
	{
		m_encodeformat = MFVideoFormat_WMV3;
	}
	else
	{
		m_encodeformat = MFVideoFormat_H264;
	}

	//Set frame size
	m_width = (UINT32) width;
	m_height = (UINT32) height;

	// Set frame time
	m_frametime = (UINT64) (10000000 / framerate);

	//Set bitrate
	m_bitrate = (UINT32) (1000 * kbps);

	return S_OK;
}

//-------------------------------------------------------------------
// Get a buffer to write data into 
//
HRESULT VidWriter::getWriteBuffer(BYTE **ppData)
{
	HRESULT hr;
    const DWORD cbBuffer = 4 * m_width * m_height;

    // Create a new memory buffer
	SafeRelease(&m_pBuffer);
    hr = MFCreateMemoryBuffer(cbBuffer, &m_pBuffer);
	if (FAILED(hr)) goto done;
	
    // Lock the buffer
	hr = m_pBuffer->Lock(ppData, NULL, NULL);

done:
    return hr;
}

//-------------------------------------------------------------------
// Write the sample 
//
HRESULT VidWriter::writeFrame(BYTE *pData)
{
	HRESULT hr;
    IMFSample *pSample = NULL;
    const DWORD cbBuffer = 4 * m_width * m_height;

	// Unlock the buffer
    if (m_pBuffer) m_pBuffer->Unlock();	

	// Set the data length of the buffer
    hr = m_pBuffer->SetCurrentLength(cbBuffer);
	if (FAILED(hr)) goto done;	

    // Create a media sample and add the buffer to it
    hr = MFCreateSample(&pSample);
	if (FAILED(hr)) goto done;
	hr = pSample->AddBuffer(m_pBuffer);
	if (FAILED(hr)) goto done;

    // Set the time stamp and the duration
    hr = pSample->SetSampleTime(m_rtStart);
	if (FAILED(hr)) goto done;
	hr = pSample->SetSampleDuration(m_frametime);
	if (FAILED(hr)) goto done;

	// increment the time stamp
	m_rtStart += m_frametime;

    // Send the sample to the Sink Writer
    hr = m_pWriter->WriteSample(m_streamIndex, pSample);

done:
    SafeRelease(&pSample);
    return hr;
}

//-------------------------------------------------------------------
// Finalise the sink writer 
//
HRESULT VidWriter::finalise()
{
	if (!m_pWriter) return E_ABORT;

	HRESULT hr = m_pWriter->Finalize();
    SafeRelease(&m_pWriter);
	return hr;
}

////////////////////////////////
// Globals
////////////////////////////////

VidReader VR;
VidWriter VW;
static HRESULT hr;

////////////////////////////////
// LibraryLink stuff
////////////////////////////////

EXTERN_C DLLEXPORT mint WolframLibrary_getVersion( ) {
	return WolframLibraryVersion;
}

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
   
	// Initialize the COM library
	hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		libData->Message("cominitfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	// Initialize Media Foundation.
	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
    {
		libData->Message("mfinitfail");
		return LIBRARY_FUNCTION_ERROR;    
    }

	return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) {

	VR.~VidReader(); // destruct the class instances
	VW.~VidWriter(); 

	// Shut down media foundation and close the COM library
	hr = MFShutdown();
	if (FAILED(hr))
	{
		libData->Message("mfshutdownfail");
	} 
	CoUninitialize();
	
	return;
}


// Source Reader exports

EXTERN_C DLLEXPORT int InitSourceReader(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	// Get the arguments from MArgument
	char *videofile = MArgument_getUTF8String(Args[0]);

	// convert UTF8 filename to wide chars
	int size = MultiByteToWideChar(CP_ACP, 0, videofile, -1, NULL, 0);
	WCHAR* videofileW = new WCHAR[size];
	size = MultiByteToWideChar(CP_ACP, 0, videofile, -1, videofileW, size);
	
	// Open the file
	hr = VR.initSourceReader(videofileW);

	// finished with the filename now
	delete videofileW;	
	
	if (FAILED(hr))
	{
		libData->Message("initsourcefail");
		return LIBRARY_FUNCTION_ERROR;
	}

	// Return the duration and frame rate
	MTensor T0;
	mint dims[1] = {4};
	mint pos[1];
	
	libData->MTensor_new(MType_Real, 1, dims, &T0);

	pos[0] = 1;
	libData->MTensor_setReal(T0, pos, VR.m_duration);

	pos[0] = 2;
	libData->MTensor_setReal(T0, pos, VR.m_framerate);

	pos[0] = 3;
	libData->MTensor_setReal(T0, pos, VR.m_imagewidth);

	pos[0] = 4;
	libData->MTensor_setReal(T0, pos, VR.m_imageheight);

	MArgument_setMTensor(Res, T0);
	
	return LIBRARY_NO_ERROR;
} 

EXTERN_C DLLEXPORT int GrabFrame(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	int err = LIBRARY_NO_ERROR;
	BYTE *pData;

	// get a pointer to the byte buffer containing the frame:
	hr = VR.getReadBuffer(&pData);

	if (hr == MF_E_END_OF_STREAM)
	{
		libData->Message("endofstream");
		return LIBRARY_FUNCTION_ERROR;
	}

	if (FAILED(hr))
	{
		libData->Message("getdatafail");
		return LIBRARY_FUNCTION_ERROR;
	} 

	MTensor T0;
	mint dims[3];

	dims[0] = (mint) VR.m_imageheight;
	dims[1] = (mint) VR.m_imagewidth;
	dims[2] = 3;

	// create the rank 3 MTensor
	err = libData->MTensor_new(MType_Integer, 3, dims, &T0);

	mint index[3];
//	BYTE *pbbcopy = VR.m_pbytebuffer;
	BYTE *pbbcopy = pData;
	mint pixelvalue;

	// fill the MTensor from the byte buffer
	for (int i = 1; i <= dims[0]; i++) 
	{
		index[0] = i;
		for (int j = 1; j <= dims[1]; j++) 
		{
			index[1] = j;
			for (int k = 3; k >= 1 && !err; k--) // count backwards to convert BGR to RGB	
			{		
				index[2] = k;
				pixelvalue = (mint) *pbbcopy++;
				err = libData->MTensor_setInteger(T0, index, pixelvalue);
			}
			pbbcopy++; // skip the fourth byte (useless alpha channel)
		}
	}

	// unlock the buffer
	hr = VR.releaseBuffer();
	if (FAILED(hr))
	{
		libData->Message("releaseBufferfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	MArgument_setMTensor(Res, T0);
	return err; 

} 

EXTERN_C DLLEXPORT int SourceTime(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	// Get the timestamp
	mreal time;
	time = (mreal)VR.m_timestamp * 0.0000001;

	MArgument_setReal(Res, time);

	return LIBRARY_NO_ERROR;
} 

// Sink Writer exports

EXTERN_C DLLEXPORT int InitSinkWriter(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	// Get the arguments from MArgument
	char *videofile = MArgument_getUTF8String(Args[0]);

	// convert UTF8 filename to wide chars
	int size = MultiByteToWideChar(CP_ACP, 0, videofile, -1, NULL, 0);
	WCHAR* videofileW = new WCHAR[size];
	size = MultiByteToWideChar(CP_ACP, 0, videofile, -1, videofileW, size);
	
	// extract the sink parameters
	mint encoder =		MArgument_getInteger(Args[1]);
	mint width =		MArgument_getInteger(Args[2]);
	mint height =		MArgument_getInteger(Args[3]);
	mreal framerate =	MArgument_getReal(Args[4]);
	mint kbps =			MArgument_getInteger(Args[5]);

	hr = VW.setParams(encoder, width, height, framerate, kbps);
	if (FAILED(hr))
	{
		libData->Message("setsinkparamsfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	// Initialise the sink writer
	hr = VW.initSinkWriter(videofileW);

	// finished with the filename now
	delete videofileW;	
	
	if (FAILED(hr))
	{
		libData->Message("initsinkfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	return LIBRARY_NO_ERROR;
} 

EXTERN_C DLLEXPORT int SendFrame(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	MTensor T0;
	mint const *dims;
	BYTE *pData = NULL;
	
	T0 = MArgument_getMTensor(Args[0]);
	dims = libData->MTensor_getDimensions(T0);

	if(dims[0] != (mint)VW.m_height || dims[1] != (mint)VW.m_width || dims[2] != 3)
	{
		libData->Message("dimensionsfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	// get a buffer
	hr = VW.getWriteBuffer(&pData);
	if (FAILED(hr))
	{
		libData->Message("getwritebufferfail");
		return LIBRARY_FUNCTION_ERROR;
	}

	// fill the buffer

	mint index[3];
	BYTE *pbbcopy = pData;
	mint pixelvalue;
	int err = LIBRARY_NO_ERROR;
	
	for (int i = 1; i <= dims[0]; i++) 
	{
		index[0] = i;
		for (int j = 1; j <= dims[1]; j++) 
		{
			index[1] = j;
			for (int k = 3; k >= 1 && !err; k--) // count backwards to convert RGB to BGR	
			{		
				index[2] = k;
				err = libData->MTensor_getInteger(T0, index, &pixelvalue);
				*pbbcopy++ = (BYTE) pixelvalue;
			}
			*pbbcopy++ = 0; // zero the fourth byte (useless alpha channel)
		}
	}

	// write the sample
	hr = VW.writeFrame(pData);
	if (FAILED(hr))
	{
		libData->Message("writeFramefail");
		return LIBRARY_FUNCTION_ERROR;
	}

	MArgument_setInteger(Res, VW.m_rtStart);

	return LIBRARY_NO_ERROR;
} 

EXTERN_C DLLEXPORT int FinaliseSink(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {

	hr = VW.finalise();
	if (FAILED(hr))
	{
		libData->Message("finalisefail");
		return LIBRARY_FUNCTION_ERROR;
	}

	return LIBRARY_NO_ERROR;
} 


