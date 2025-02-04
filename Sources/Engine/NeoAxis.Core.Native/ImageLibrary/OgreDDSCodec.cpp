/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2016 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreStableHeaders.h"

#include "OgreRoot.h"
#include "OgreRenderSystem.h"
#include "OgreDDSCodec.h"
#include "OgreImage.h"
#include "OgreException.h"
#include "OgreLogManager.h"
#include "OgreStringConverter.h"
#include "OgreBitwise.h"

#ifdef PLATFORM_ANDROID
#	include <codecvt>
#endif
#ifdef PLATFORM_IOS
#	include <codecvt>
#endif

namespace Ogre {
	// Internal DDS structure definitions
#define FOURCC(c0, c1, c2, c3) (c0 | (c1 << 8) | (c2 << 16) | (c3 << 24))
	
#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (push, 1)
#else
#pragma pack (1)
#endif

	// Nested structure
	struct DDSPixelFormat
	{
		uint32 size;
		uint32 flags;
		uint32 fourCC;
		uint32 rgbBits;
		uint32 redMask;
		uint32 greenMask;
		uint32 blueMask;
		uint32 alphaMask;
	};
	
	// Nested structure
	struct DDSCaps
	{
		uint32 caps1;
		uint32 caps2;
		uint32 caps3;
		uint32 caps4;
	};
	// Main header, note preceded by 'DDS '
	struct DDSHeader
	{
		uint32 size;		
		uint32 flags;
		uint32 height;
		uint32 width;
		uint32 sizeOrPitch;
		uint32 depth;
		uint32 mipMapCount;
		uint32 reserved1[11];
		DDSPixelFormat pixelFormat;
		DDSCaps caps;
		uint32 reserved2;
	};

	// Extended header
	struct DDSExtendedHeader
	{
		uint32 dxgiFormat;
		uint32 resourceDimension;
		uint32 miscFlag; // see D3D11_RESOURCE_MISC_FLAG
		uint32 arraySize;
		uint32 reserved;
	};

	// An 8-byte DXT colour block, represents a 4x4 texel area. Used by all DXT formats
	struct DXTColourBlock
	{
		// 2 colour ranges
		uint16 colour_0;
		uint16 colour_1;
		// 16 2-bit indexes, each byte here is one row
		uint8 indexRow[4];
	};
	// An 8-byte DXT explicit alpha block, represents a 4x4 texel area. Used by DXT2/3
	struct DXTExplicitAlphaBlock
	{
		// 16 4-bit values, each 16-bit value is one row
		uint16 alphaRow[4];
	};
	// An 8-byte DXT interpolated alpha block, represents a 4x4 texel area. Used by DXT4/5
	struct DXTInterpolatedAlphaBlock
	{
		// 2 alpha ranges
		uint8 alpha_0;
		uint8 alpha_1;
		// 16 3-bit indexes. Unfortunately 3 bits doesn't map too well to row bytes
		// so just stored raw
		uint8 indexes[6];
	};
	
#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (pop)
#else
#pragma pack ()
#endif

	const uint32 DDS_MAGIC = FOURCC('D', 'D', 'S', ' ');
	const uint32 DDS_PIXELFORMAT_SIZE = 8 * sizeof(uint32);
	const uint32 DDS_CAPS_SIZE = 4 * sizeof(uint32);
	const uint32 DDS_HEADER_SIZE = 19 * sizeof(uint32) + DDS_PIXELFORMAT_SIZE + DDS_CAPS_SIZE;

	const uint32 DDSD_CAPS = 0x00000001;
	const uint32 DDSD_HEIGHT = 0x00000002;
	const uint32 DDSD_WIDTH = 0x00000004;
	const uint32 DDSD_PIXELFORMAT = 0x00001000;
	const uint32 DDSD_DEPTH = 0x00800000;
	const uint32 DDPF_ALPHAPIXELS = 0x00000001;
	const uint32 DDPF_FOURCC = 0x00000004;
	const uint32 DDPF_RGB = 0x00000040;
	const uint32 DDSCAPS_COMPLEX = 0x00000008;
	const uint32 DDSCAPS_TEXTURE = 0x00001000;
	const uint32 DDSCAPS_MIPMAP = 0x00400000;
	const uint32 DDSCAPS2_CUBEMAP = 0x00000200;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
	const uint32 DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
	const uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
	const uint32 DDSCAPS2_VOLUME = 0x00200000;

	// Currently unused
	//    const uint32 DDSD_PITCH = 0x00000008;
	//    const uint32 DDSD_MIPMAPCOUNT = 0x00020000;
	//    const uint32 DDSD_LINEARSIZE = 0x00080000;

	// Special FourCC codes
	const uint32 D3DFMT_R16F			= 111;
	const uint32 D3DFMT_G16R16F			= 112;
	const uint32 D3DFMT_A16B16G16R16F	= 113;
	const uint32 D3DFMT_R32F            = 114;
	const uint32 D3DFMT_G32R32F         = 115;
	const uint32 D3DFMT_A32B32G32R32F   = 116;


	//---------------------------------------------------------------------
	//DDSCodec* DDSCodec::msInstance = 0;
	//---------------------------------------------------------------------
	void DDSCodec::startup()
	{
		//if (!msInstance)
		//{

			root->mLogManager->logMessage( LML_NORMAL, "DDS codec registering");

			//msInstance = OGRE_NEW DDSCodec();
			//Codec::registerCodec(msInstance);

			root->ddsCodec = OGRE_NEW DDSCodec();
			root->registerCodec(root->ddsCodec);
		//}
	}
	//---------------------------------------------------------------------
	void DDSCodec::shutdown()
	{
		if(root->ddsCodec)
		{
			root->unRegisterCodec(root->ddsCodec);
			OGRE_DELETE root->ddsCodec;
			root->ddsCodec = 0;
		}

		//if(msInstance)
		//{
		//	Codec::unRegisterCodec(msInstance);
		//	OGRE_DELETE msInstance;
		//	msInstance = 0;
		//}

	}
	//---------------------------------------------------------------------
    DDSCodec::DDSCodec(): ImageCodec(),
        mType("dds")
    { 
    }
    //---------------------------------------------------------------------
    DataStreamPtr DDSCodec::encode(MemoryDataStreamPtr& input, Codec::CodecDataPtr& pData) const
    {        
		OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
			"DDS encoding not supported",
			"DDSCodec::code" ) ;
    }
    //---------------------------------------------------------------------
    void DDSCodec::encodeToFile(MemoryDataStreamPtr& input,
        const WString& outFileName, Codec::CodecDataPtr& pData) const
    {
		// Unwrap codecDataPtr - data is cleaned by calling function
		ImageData* imgData = static_cast<ImageData* >(pData.getPointer());


		// Check size for cube map faces
		bool isCubeMap = (imgData->size ==
			Image::calculateSize(imgData->num_mipmaps, 6, imgData->width,
				imgData->height, imgData->depth, imgData->format));

		// Establish texture attributes
		bool isVolume = (imgData->depth > 1);
		bool isFloat32r = (imgData->format == PF_FLOAT32_R);
		bool isFloat16 = (imgData->format == PF_FLOAT16_RGBA);
		bool isFloat32 = (imgData->format == PF_FLOAT32_RGBA);
		bool notImplemented = false;
		String notImplementedString = "";

		// Check for all the 'not implemented' conditions
		if ((isVolume == true) && (imgData->width != imgData->height))
		{
			// Square textures only
			notImplemented = true;
			notImplementedString += " non square textures";
		}

		uint32 size = 1;
		while (size < imgData->width)
		{
			size <<= 1;
		}
		if (size != imgData->width)
		{
			// Power two textures only
			notImplemented = true;
			notImplementedString += " non power two textures";
		}

		switch (imgData->format)
		{
		case PF_A8R8G8B8:
		case PF_X8R8G8B8:
		case PF_R8G8B8:
		case PF_A8B8G8R8:
		case PF_X8B8G8R8:
		case PF_B8G8R8:
		case PF_FLOAT32_R:
		case PF_FLOAT16_RGBA:
		case PF_FLOAT32_RGBA:
			break;
		default:
			// No crazy FOURCC or 565 et al. file formats at this stage
			notImplemented = true;
			notImplementedString = " unsupported pixel format";
			break;
		}



		// Except if any 'not implemented' conditions were met
		if (notImplemented)
		{
			OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
				"DDS encoding for" + notImplementedString + " not supported",
				"DDSCodec::encodeToFile");
		}
		else
		{
			// Build header and write to disk

			// Variables for some DDS header flags
			bool hasAlpha = false;
			uint32 ddsHeaderFlags = 0;
			uint32 ddsHeaderRgbBits = 0;
			uint32 ddsHeaderSizeOrPitch = 0;
			uint32 ddsHeaderCaps1 = 0;
			uint32 ddsHeaderCaps2 = 0;
			uint32 ddsMagic = DDS_MAGIC;

			// Initalise the header flags
			ddsHeaderFlags = (isVolume) ? DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_DEPTH | DDSD_PIXELFORMAT :
				DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;

			bool flipRgbMasks = false;

			// Initalise the rgbBits flags
			switch (imgData->format)
			{
			case PF_A8B8G8R8:
				flipRgbMasks = true;
			case PF_A8R8G8B8:
				ddsHeaderRgbBits = 8 * 4;
				hasAlpha = true;
				break;
			case PF_X8B8G8R8:
				flipRgbMasks = true;
			case PF_X8R8G8B8:
				ddsHeaderRgbBits = 8 * 4;
				break;
			case PF_B8G8R8:
			case PF_R8G8B8:
				ddsHeaderRgbBits = 8 * 3;
				break;
			case PF_FLOAT32_R:
				ddsHeaderRgbBits = 32;
				break;
			case PF_FLOAT16_RGBA:
				ddsHeaderRgbBits = 16 * 4;
				hasAlpha = true;
				break;
			case PF_FLOAT32_RGBA:
				ddsHeaderRgbBits = 32 * 4;
				hasAlpha = true;
				break;
			default:
				ddsHeaderRgbBits = 0;
				break;
			}

			// Initalise the SizeOrPitch flags (power two textures for now)
			ddsHeaderSizeOrPitch = static_cast<uint32>(ddsHeaderRgbBits * imgData->width);

			// Initalise the caps flags
			ddsHeaderCaps1 = (isVolume || isCubeMap) ? DDSCAPS_COMPLEX | DDSCAPS_TEXTURE : DDSCAPS_TEXTURE;
			if (isVolume)
			{
				ddsHeaderCaps2 = DDSCAPS2_VOLUME;
			}
			else if (isCubeMap)
			{
				ddsHeaderCaps2 = DDSCAPS2_CUBEMAP |
					DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |
					DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |
					DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ;
			}

			if (imgData->num_mipmaps > 0)
				ddsHeaderCaps1 |= DDSCAPS_MIPMAP;

			// Populate the DDS header information
			DDSHeader ddsHeader;
			ddsHeader.size = DDS_HEADER_SIZE;
			ddsHeader.flags = ddsHeaderFlags;
			ddsHeader.width = (uint32)imgData->width;
			ddsHeader.height = (uint32)imgData->height;
			ddsHeader.depth = (uint32)(isVolume ? imgData->depth : 0);
			ddsHeader.depth = (uint32)(isCubeMap ? 6 : ddsHeader.depth);
			ddsHeader.mipMapCount = imgData->num_mipmaps + 1;
			ddsHeader.sizeOrPitch = ddsHeaderSizeOrPitch;
			for (uint32 reserved1 = 0; reserved1<11; reserved1++) // XXX nasty constant 11
			{
				ddsHeader.reserved1[reserved1] = 0;
			}
			ddsHeader.reserved2 = 0;

			ddsHeader.pixelFormat.size = DDS_PIXELFORMAT_SIZE;
			ddsHeader.pixelFormat.flags = (hasAlpha) ? DDPF_RGB | DDPF_ALPHAPIXELS : DDPF_RGB;
			ddsHeader.pixelFormat.flags = (isFloat32r || isFloat16 || isFloat32) ? DDPF_FOURCC : ddsHeader.pixelFormat.flags;
			if (isFloat32r) {
				ddsHeader.pixelFormat.fourCC = D3DFMT_R32F;
			}
			else if (isFloat16) {
				ddsHeader.pixelFormat.fourCC = D3DFMT_A16B16G16R16F;
			}
			else if (isFloat32) {
				ddsHeader.pixelFormat.fourCC = D3DFMT_A32B32G32R32F;
			}
			else {
				ddsHeader.pixelFormat.fourCC = 0;
			}
			ddsHeader.pixelFormat.rgbBits = ddsHeaderRgbBits;

			ddsHeader.pixelFormat.alphaMask = (hasAlpha) ? 0xFF000000 : 0x00000000;
			ddsHeader.pixelFormat.alphaMask = (isFloat32r) ? 0x00000000 : ddsHeader.pixelFormat.alphaMask;
			ddsHeader.pixelFormat.redMask = (isFloat32r) ? 0xFFFFFFFF : 0x00FF0000;
			ddsHeader.pixelFormat.greenMask = (isFloat32r) ? 0x00000000 : 0x0000FF00;
			ddsHeader.pixelFormat.blueMask = (isFloat32r) ? 0x00000000 : 0x000000FF;

			if (flipRgbMasks)
				std::swap(ddsHeader.pixelFormat.redMask, ddsHeader.pixelFormat.blueMask);

			ddsHeader.caps.caps1 = ddsHeaderCaps1;
			ddsHeader.caps.caps2 = ddsHeaderCaps2;
			//          ddsHeader.caps.reserved[0] = 0;
			//          ddsHeader.caps.reserved[1] = 0;

			// Swap endian
			flipEndian(&ddsMagic, sizeof(uint32));
			flipEndian(&ddsHeader, 4, sizeof(DDSHeader) / 4);

			char *tmpData = 0;
			char const *dataPtr = (char const *)input->getPtr();

			if (imgData->format == PF_B8G8R8)
			{
				PixelBox src(imgData->size / 3, 1, 1, PF_B8G8R8, input->getPtr());
				tmpData = new char[imgData->size];
				PixelBox dst(imgData->size / 3, 1, 1, PF_R8G8B8, tmpData);

				PixelUtil::bulkPixelConversion(src, dst);

				dataPtr = tmpData;
			}

			try
			{
				// Write the file
				std::ofstream of;
#	if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
				using convert_type = std::codecvt_utf8<wchar_t>;
				std::wstring_convert<convert_type, wchar_t> converter;
				std::string fl = converter.to_bytes(outFileName);
				of.open(fl, std::ios_base::binary | std::ios_base::out);
#	else
				of.open(outFileName.c_str(), std::ios_base::binary | std::ios_base::out);
#	endif
				of.write((const char *)&ddsMagic, sizeof(uint32));
				of.write((const char *)&ddsHeader, DDS_HEADER_SIZE);
				// XXX flipEndian on each pixel chunk written unless isFloat32r ?
				of.write(dataPtr, (uint32)imgData->size);
				of.close();
			}
			catch (...)
			{
				delete[] tmpData;
			}
		}
	}
	//---------------------------------------------------------------------
	PixelFormat DDSCodec::convertDXToOgreFormat(uint32 dxfmt) const
	{
		switch (dxfmt) {
		case 80: // DXGI_FORMAT_BC4_UNORM
			return PF_BC4_UNORM;
		case 81: // DXGI_FORMAT_BC4_SNORM
			return PF_BC4_SNORM;
		case 83: // DXGI_FORMAT_BC5_UNORM
			return PF_BC5_UNORM;
		case 84: // DXGI_FORMAT_BC5_SNORM
			return PF_BC5_SNORM;
		case 95: // DXGI_FORMAT_BC6H_UF16
			return PF_BC6H_UF16;
		case 96: // DXGI_FORMAT_BC6H_SF16
			return PF_BC6H_SF16;
		case 98: // DXGI_FORMAT_BC7_UNORM
			return PF_BC7_UNORM;
		case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
			return PF_BC7_UNORM_SRGB;
		default:
			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
				"Unsupported DirectX format found in DDS file",
				"DDSCodec::convertDXToOgreFormat");
		}
	}
	//---------------------------------------------------------------------
	PixelFormat DDSCodec::convertFourCCFormat(uint32 fourcc) const
	{
		// convert dxt pixel format
		switch(fourcc)
		{
		case FOURCC('D','X','T','1'):
			return PF_DXT1;
		case FOURCC('D','X','T','2'):
			return PF_DXT2;
		case FOURCC('D','X','T','3'):
			return PF_DXT3;
		case FOURCC('D','X','T','4'):
			return PF_DXT4;
		case FOURCC('D','X','T','5'):
			return PF_DXT5;
		case FOURCC('A', 'T', 'I', '1'):
		case FOURCC('B', 'C', '4', 'U'):
			return PF_BC4_UNORM;
		case FOURCC('B', 'C', '4', 'S'):
			return PF_BC4_SNORM;
		//case FOURCC('A', 'T', 'I', '2'):  //!!!! use instead of PF_3DC ?
		case FOURCC('B', 'C', '5', 'U'):
			return PF_BC5_UNORM;
		case FOURCC('B', 'C', '5', 'S'):
			return PF_BC5_SNORM;
		case D3DFMT_R16F:
			return PF_FLOAT16_R;
		case D3DFMT_G16R16F:
			return PF_FLOAT16_GR;
		case D3DFMT_A16B16G16R16F:
			return PF_FLOAT16_RGBA;
		case D3DFMT_R32F:
			return PF_FLOAT32_R;
		case D3DFMT_G32R32F:
			return PF_FLOAT32_GR;
		case D3DFMT_A32B32G32R32F:
			return PF_FLOAT32_RGBA;
		case FOURCC('A','T','I','2'):
			return PF_3DC; // only ATI cards support it, not nVidia ?
		default:
			OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, 
				"Unsupported FourCC format found in DDS file", 
				"DDSCodec::decode");
		};

	}

	//---------------------------------------------------------------------
	PixelFormat DDSCodec::convertPixelFormat(uint32 rgbBits, uint32 rMask, 
		uint32 gMask, uint32 bMask, uint32 aMask) const
	{
		// General search through pixel formats
		for (int i = PF_UNKNOWN + 1; i < PF_COUNT; ++i)
		{
			PixelFormat pf = static_cast<PixelFormat>(i);
			if (PixelUtil::getNumElemBits(pf) == rgbBits)
			{
				uint64 testMasks[4];
				PixelUtil::getBitMasks(pf, testMasks);
				int testBits[4];
				PixelUtil::getBitDepths(pf, testBits);
				if (testMasks[0] == rMask && testMasks[1] == gMask &&
					testMasks[2] == bMask && 
					// for alpha, deal with 'X8' formats by checking bit counts
					(testMasks[3] == aMask || (aMask == 0 && testBits[3] == 0)))
				{
					return pf;
				}
			}

		}

		//!!!!  What is it ?:

		//PF_BYTE_LA
		if(rgbBits == 16 && rMask == 255 && gMask == 0 && bMask == 0 && aMask == 65280)
			return PF_BYTE_LA;
		//PF_BYTE_A
		if(rgbBits == 8 && rMask == 0 && gMask == 0 && bMask == 0 && aMask == 0)
			return PF_BYTE_A;

		//!!!!

		OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Cannot determine pixel format",
			"DDSCodec::convertPixelFormat");

	}
	//---------------------------------------------------------------------
	void DDSCodec::unpackDXTColour(PixelFormat pf, const DXTColourBlock& block, 
		ColourValue* pCol) const
	{
		// Note - we assume all values have already been endian swapped

		// Colour lookup table
		ColourValue derivedColours[4];

		if (pf == PF_DXT1 && block.colour_0 <= block.colour_1)
		{
			// 1-bit alpha
			PixelUtil::unpackColour(&(derivedColours[0]), PF_R5G6B5, &(block.colour_0));
			PixelUtil::unpackColour(&(derivedColours[1]), PF_R5G6B5, &(block.colour_1));
			// one intermediate colour, half way between the other two
			derivedColours[2] = (derivedColours[0] + derivedColours[1]) / 2;
			// transparent colour
			derivedColours[3] = ColourValue::ZERO;
		}
		else
		{
			PixelUtil::unpackColour(&(derivedColours[0]), PF_R5G6B5, &(block.colour_0));
			PixelUtil::unpackColour(&(derivedColours[1]), PF_R5G6B5, &(block.colour_1));
			// first interpolated colour, 1/3 of the way along
			derivedColours[2] = (2 * derivedColours[0] + derivedColours[1]) / 3;
			// second interpolated colour, 2/3 of the way along
			derivedColours[3] = (derivedColours[0] + 2 * derivedColours[1]) / 3;
		}

		// Process 4x4 block of texels
		for (size_t row = 0; row < 4; ++row)
		{
			for (size_t x = 0; x < 4; ++x)
			{
				// LSB come first
				uint8 colIdx = static_cast<uint8>(block.indexRow[row] >> (x * 2) & 0x3);
				if (pf == PF_DXT1)
				{
					// Overwrite entire colour
					pCol[(row * 4) + x] = derivedColours[colIdx];
				}
				else
				{
					// alpha has already been read (alpha precedes colour)
					ColourValue& col = pCol[(row * 4) + x];
					col.r = derivedColours[colIdx].r;
					col.g = derivedColours[colIdx].g;
					col.b = derivedColours[colIdx].b;
				}
			}
		}


	}
	//---------------------------------------------------------------------
	void DDSCodec::unpackBC5Colour(const DXTInterpolatedAlphaBlock& redColour, 
		const DXTInterpolatedAlphaBlock& greenColour, ColourValue* pCol) const
	{
		// Note - we assume all values have already been endian swapped
		float red[8], green[8];
		red[0] = redColour.alpha_0 / (Real)0xFF;
		red[1] = redColour.alpha_1 / (Real)0xFF;
		green[0] = greenColour.alpha_0 / (Real)0xFF;
		green[1] = greenColour.alpha_1 / (Real)0xFF;

		if(red[0] <= red[1])
		{
			for(int i=2; i<6; i++)
			{
				red[i] = ((6-i)*red[0] + (i-1)*red[1])/5.0f;
			}
			red[6] = 0.0f;
			red[7] = 1.0f;
		}
		else
		{
			for(int i=2; i<8; i++)
			{
				red[i] = ((8-i)*red[0] + (i-1)*red[1])/7.0f;
			}
		}
		if(green[0] <= green[1])
		{
			for(int i=2; i<8; i++)
			{
				green[i] = ((6-i)*green[0] + (i-1)*green[1])/5.0f;
			}
			green[6] = 0.0f;
			green[7] = 1.0f;
		}
		else
		{
			for(int i=2; i<8; i++)
			{
				green[i] = ((8-i)*green[0] + (i-1)*green[1])/7.0f;
			}
		}

		for (size_t i = 0; i < 16; ++i)
		{
			size_t baseByte = (i * 3) / 8;
			size_t baseBit = (i * 3) % 8;
			uint8 bitsRed = static_cast<uint8>(redColour.indexes[baseByte] >> baseBit & 0x7);
			uint8 bitsGreen = static_cast<uint8>(greenColour.indexes[baseByte] >> baseBit & 0x7);
			// do we need to stitch in next byte too?
			if (baseBit > 5)
			{
				uint8 extraBitsRed = static_cast<uint8>(
					(redColour.indexes[baseByte+1] << (8 - baseBit)) & 0xFF);
				bitsRed |= extraBitsRed & 0x7;
				uint8 extraBitsGreen = static_cast<uint8>(
					(greenColour.indexes[baseByte+1] << (8 - baseBit)) & 0xFF);
				bitsGreen |= extraBitsGreen & 0x7;
			}
			float r = red[bitsRed]*2.0f - 1.0f;
			float g = green[bitsGreen]*2.0f - 1.0f;
			float b = 1-r*r-g*g;
			if(b < 0)
				b = 0.0f;

			pCol[i].g = red[bitsRed];
			pCol[i].r = green[bitsGreen];
			pCol[i].b = (sqrt(b)+1.0f)/2.0f;
		}
	}
	//---------------------------------------------------------------------
	void DDSCodec::unpackDXTAlpha(
		const DXTExplicitAlphaBlock& block, ColourValue* pCol) const
	{
		// Note - we assume all values have already been endian swapped
		
		// This is an explicit alpha block, 4 bits per pixel, LSB first
		for (size_t row = 0; row < 4; ++row)
		{
			for (size_t x = 0; x < 4; ++x)
			{
				// Shift and mask off to 4 bits
				uint8 val = static_cast<uint8>(block.alphaRow[row] >> (x * 4) & 0xF);
				// Convert to [0,1]
				pCol->a = (Real)val / (Real)0xF;
				pCol++;
				
			}
			
		}

	}
	//---------------------------------------------------------------------
	void DDSCodec::unpackDXTAlpha(
		const DXTInterpolatedAlphaBlock& block, ColourValue* pCol) const
	{
		// Adaptive 3-bit alpha part
		float derivedAlphas[8];

		// Explicit extremes
		derivedAlphas[0] = ((float)block.alpha_0) * (1.0f / 255.0f);
		derivedAlphas[1] = ((float)block.alpha_1) * (1.0f / 255.0f);

		if (block.alpha_0 > block.alpha_1)
		{
			// 6 interpolated alpha values.
			// full range including extremes at [0] and [7]
			// we want to fill in [1] through [6] at weights ranging
			// from 1/7 to 6/7
			for (size_t i = 1; i < 7; ++i)
				derivedAlphas[i + 1] = (derivedAlphas[0] * (7 - i) + derivedAlphas[1] * i) * (1.0f / 7.0f);
		}
		else
		{
			// 4 interpolated alpha values.
			// full range including extremes at [0] and [5]
			// we want to fill in [1] through [4] at weights ranging
			// from 1/5 to 4/5
			for (size_t i = 1; i < 5; ++i)
				derivedAlphas[i + 1] = (derivedAlphas[0] * (5 - i) + derivedAlphas[1] * i) * (1.0f / 5.0f);

			derivedAlphas[6] = 0.0f;
			derivedAlphas[7] = 1.0f;
		}

		// Ok, now we've built the reference values, process the indexes
		uint32 dw = block.indexes[0] | (block.indexes[1] << 8) | (block.indexes[2] << 16);

		for (size_t i = 0; i < 8; ++i, dw >>= 3)
			pCol[i].a = derivedAlphas[dw & 0x7];

		dw = block.indexes[3] | (block.indexes[4] << 8) | (block.indexes[5] << 16);

		for (size_t i = 8; i < 16; ++i, dw >>= 3)
			pCol[i].a = derivedAlphas[dw & 0x7];
	}
    //---------------------------------------------------------------------
	Codec::DecodeResult DDSCodec::decode(DataStreamPtr& stream) const
	{
		// Read 4 character code
		uint32 fileType;
		stream->read(&fileType, sizeof(uint32));
		flipEndian(&fileType, sizeof(uint32));

		if (FOURCC('D', 'D', 'S', ' ') != fileType)
		{
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
				"Unable to load texture from \"" + StringUtil::toUTF8(stream->getName()) + "\". This is not a DDS file.", "DDSCodec::decode");
			//OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
			//	"This is not a DDS file!", "DDSCodec::decode");
		}

		// Read header in full
		DDSHeader header;
		stream->read(&header, sizeof(DDSHeader));

		// Endian flip if required, all 32-bit values
		flipEndian(&header, 4, sizeof(DDSHeader) / 4);

		// Check some sizes
		if (header.size != DDS_HEADER_SIZE)
		{
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
				"Unable to load texture from \"" + StringUtil::toUTF8(stream->getName()) + "\". DDS header size mismatch.", "DDSCodec::decode");
			//OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
			//	"DDS header size mismatch!", "DDSCodec::decode");
		}
		if (header.pixelFormat.size != DDS_PIXELFORMAT_SIZE)
		{
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
				"Unable to load texture from \"" + StringUtil::toUTF8(stream->getName()) + "\". DDS header size mismatch.", "DDSCodec::decode");
			//OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
			//	"DDS header size mismatch!", "DDSCodec::decode");
		}

		ImageData* imgData = OGRE_NEW ImageData();
		MemoryDataStreamPtr output;

		imgData->depth = 1; // (deal with volume later)
		imgData->width = header.width;
		imgData->height = header.height;
		size_t numFaces = 1; // assume one face until we know otherwise

		if (header.caps.caps1 & DDSCAPS_MIPMAP)
		{
			imgData->num_mipmaps = static_cast<uint8>(header.mipMapCount - 1);
		}
		else
		{
			imgData->num_mipmaps = 0;
		}
		imgData->flags = 0;

		bool decompressDXT = false;
		// Figure out basic image type
		if (header.caps.caps2 & DDSCAPS2_CUBEMAP)
		{
			imgData->flags |= IF_CUBEMAP;
			numFaces = 6;
		}
		else if (header.caps.caps2 & DDSCAPS2_VOLUME)
		{
			imgData->flags |= IF_3D_TEXTURE;
			imgData->depth = header.depth;
		}
		// Pixel format
		PixelFormat sourceFormat = PF_UNKNOWN;

		if (header.pixelFormat.flags & DDPF_FOURCC)
		{
			// Check if we have an DX10 style extended header and read it. This is necessary for B6H and B7 formats
			if (header.pixelFormat.fourCC == FOURCC('D', 'X', '1', '0'))
			{
				DDSExtendedHeader extHeader;
				stream->read(&extHeader, sizeof(DDSExtendedHeader));

				// Endian flip if required, all 32-bit values
				flipEndian(&header, sizeof(DDSExtendedHeader));
				sourceFormat = convertDXToOgreFormat(extHeader.dxgiFormat);
			}
			else
			{
				sourceFormat = convertFourCCFormat(header.pixelFormat.fourCC);
			}
		}
		else
		{
			sourceFormat = convertPixelFormat(header.pixelFormat.rgbBits,
				header.pixelFormat.redMask, header.pixelFormat.greenMask,
				header.pixelFormat.blueMask,
				header.pixelFormat.flags & DDPF_ALPHAPIXELS ?
				header.pixelFormat.alphaMask : 0);
		}

		if (PixelUtil::isCompressed(sourceFormat))
		{
			bool supported = true;
			//if (root->renderSystem == NULL ||
			//	!root->renderSystem->getCapabilities()->hasCapability(RSC_TEXTURE_COMPRESSION_DXT) ||
			//	(!root->renderSystem->getCapabilities()->hasCapability(RSC_AUTOMIPMAP) &&
			//		!imgData->num_mipmaps)) 
			//{
			//	supported = false;
			//}

			//if(sourceFormat == PF_3DC)
			//{
			//	if(!root->renderSystem->getCapabilities()->hasCapability(RSC_TEXTURE_COMPRESSION_3DC))
			//		supported = false;
			//}

			if (!supported)
			{
				// We'll need to decompress
				decompressDXT = true;
				// Convert format
				switch (sourceFormat)
				{
				case PF_DXT1:
					// source can be either 565 or 5551 depending on whether alpha present
					// unfortunately you have to read a block to figure out which
					// Note that we upgrade to 32-bit pixel formats here, even 
					// though the source is 16-bit; this is because the interpolated
					// values will benefit from the 32-bit results, and the source
					// from which the 16-bit samples are calculated may have been
					// 32-bit so can benefit from this.
					DXTColourBlock block;
					stream->read(&block, sizeof(DXTColourBlock));
					flipEndian(&(block.colour_0), sizeof(uint16));
					flipEndian(&(block.colour_1), sizeof(uint16));
					// skip back since we'll need to read this again
					stream->skip(0 - (long)sizeof(DXTColourBlock));
					// colour_0 <= colour_1 means transparency in DXT1
					if (block.colour_0 <= block.colour_1)
					{
						imgData->format = PF_BYTE_RGBA;
					}
					else
					{
						imgData->format = PF_BYTE_RGB;
					}
					break;
				case PF_DXT2:
				case PF_DXT3:
				case PF_DXT4:
				case PF_DXT5:
					// full alpha present, formats vary only in encoding 
					imgData->format = PF_BYTE_RGBA;
					break;
				case PF_3DC:
					imgData->format = PF_BYTE_RGB;
					break;
				default:
					// all other cases need no special format handling
					break;
				}
			}
			else
			{
				// Use original format
				imgData->format = sourceFormat;
				// Keep DXT data compressed
				imgData->flags |= IF_COMPRESSED;
			}
		}
		else // not compressed
		{
			// Don't test against DDPF_RGB since greyscale DDS doesn't set this
			// just derive any other kind of format
			imgData->format = sourceFormat;
		}

		// Calculate total size from number of mipmaps, faces and size
		imgData->size = Image::calculateSize(imgData->num_mipmaps, numFaces,
			imgData->width, imgData->height, imgData->depth, imgData->format);

		// Bind output buffer
		output.bind(OGRE_NEW MemoryDataStream(imgData->size));


		// Now deal with the data
		void* destPtr = output->getPtr();

		// all mips for a face, then each face
		for (size_t i = 0; i < numFaces; ++i)
		{
			uint32 width = imgData->width;
			uint32 height = imgData->height;
			uint32 depth = imgData->depth;

			for (size_t mip = 0; mip <= imgData->num_mipmaps; ++mip)
			{
				size_t dstPitch = width * PixelUtil::getNumElemBytes(imgData->format);

				if (PixelUtil::isCompressed(sourceFormat))
				{
					// Compressed data
					if (decompressDXT)
					{
						DXTColourBlock col;
						DXTInterpolatedAlphaBlock iAlpha;
						DXTExplicitAlphaBlock eAlpha;
						// 4x4 block of decompressed colour
						ColourValue tempColours[16];
						size_t destBpp = PixelUtil::getNumElemBytes(imgData->format);

						// slices are done individually
						for (size_t z = 0; z < depth; ++z)
						{
							size_t remainingHeight = height;

							// 4x4 blocks in x/y
							for (size_t y = 0; y < height; y += 4)
							{
								size_t sy = std::min<size_t>(remainingHeight, 4u);
								remainingHeight -= sy;

								size_t remainingWidth = width;

								for (size_t x = 0; x < width; x += 4)
								{
									size_t sx = std::min<size_t>(remainingWidth, 4u);
									size_t destPitchMinus4 = dstPitch - destBpp * sx;

									remainingWidth -= sx;

									if (sourceFormat == PF_DXT2 ||
										sourceFormat == PF_DXT3)
									{
										// explicit alpha
										stream->read(&eAlpha, sizeof(DXTExplicitAlphaBlock));
										flipEndian(eAlpha.alphaRow, sizeof(uint16), 4);
										unpackDXTAlpha(eAlpha, tempColours);
									}
									else if (sourceFormat == PF_DXT4 ||
										sourceFormat == PF_DXT5)
									{
										// interpolated alpha
										stream->read(&iAlpha, sizeof(DXTInterpolatedAlphaBlock));
										flipEndian(&(iAlpha.alpha_0), sizeof(uint16));
										flipEndian(&(iAlpha.alpha_1), sizeof(uint16));
										unpackDXTAlpha(iAlpha, tempColours);
									}

									//colour
									if(sourceFormat == PF_3DC)
									{
										DXTInterpolatedAlphaBlock red, green;
										// red color block
										stream->read(&red, sizeof(DXTInterpolatedAlphaBlock));
										flipEndian(&(red.alpha_0), sizeof(uint8), 1);
										flipEndian(&(red.alpha_1), sizeof(uint8), 1);
										// green color block
										stream->read(&green, sizeof(DXTInterpolatedAlphaBlock));
										flipEndian(&(green.alpha_0), sizeof(uint8), 1);
										flipEndian(&(green.alpha_1), sizeof(uint8), 1);
										unpackBC5Colour(red, green, tempColours);
									}
									else
									{
										// always read colour
										stream->read(&col, sizeof(DXTColourBlock));
										flipEndian(&(col.colour_0), sizeof(uint16));
										flipEndian(&(col.colour_1), sizeof(uint16));
										unpackDXTColour(sourceFormat, col, tempColours);
									}

									// write 4x4 block to uncompressed version
									for (size_t by = 0; by < sy; ++by)
									{
										for (size_t bx = 0; bx < sx; ++bx)
										{
											PixelUtil::packColour(tempColours[by * 4 + bx],
												imgData->format, destPtr);
											destPtr = static_cast<void*>(
												static_cast<uchar*>(destPtr) + destBpp);
										}
										// advance to next row
										destPtr = static_cast<void*>(
											static_cast<uchar*>(destPtr) + destPitchMinus4);
									}
									// next block. Our dest pointer is 4 lines down
									// from where it started
									if (x + 4 >= width)
									{
										// Jump back to the start of the line
										destPtr = static_cast<void*>(
											static_cast<uchar*>(destPtr) - destPitchMinus4);
									}
									else
									{
										// Jump back up 4 rows and 4 pixels to the
										// right to be at the next block to the right
										destPtr = static_cast<void*>(
											static_cast<uchar*>(destPtr) - dstPitch * sy + destBpp * sx);

									}

								}

							}
						}

					}
					else
					{
						// load directly
						// DDS format lies! sizeOrPitch is not always set for DXT!!
						size_t dxtSize = PixelUtil::getMemorySize(width, height, depth, imgData->format);
						stream->read(destPtr, dxtSize);
						destPtr = static_cast<void*>(static_cast<uchar*>(destPtr) + dxtSize);
					}

				}
				else
				{
					// Note: We assume the source and destination have the same pitch
					for (size_t z = 0; z < depth; ++z)
					{
						for (size_t y = 0; y < height; ++y)
						{
							stream->read(destPtr, dstPitch);
							destPtr = static_cast<void*>(static_cast<uchar*>(destPtr) + dstPitch);
						}
					}
				}

				/// Next mip
				if (width != 1) width /= 2;
				if (height != 1) height /= 2;
				if (depth != 1) depth /= 2;
			}

		}

		DecodeResult ret;
		ret.first = output;
		ret.second = CodecDataPtr(imgData);
		return ret;



	}
    //---------------------------------------------------------------------    
    String DDSCodec::getType() const 
    {
        return mType;
    }
    //---------------------------------------------------------------------    
	void DDSCodec::flipEndian(void * pData, size_t size, size_t count)
	{
#if OGRE_ENDIAN == OGRE_ENDIAN_BIG
		Bitwise::bswapChunks(pData, size, count);
#endif
	}
	//---------------------------------------------------------------------    
	void DDSCodec::flipEndian(void * pData, size_t size)
	{
#if OGRE_ENDIAN == OGRE_ENDIAN_BIG
		Bitwise::bswapBuffer(pData, size);
#endif
	}
	//---------------------------------------------------------------------
	String DDSCodec::magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const
	{
		if (maxbytes >= sizeof(uint32))
		{
			uint32 fileType;
			memcpy(&fileType, magicNumberPtr, sizeof(uint32));
			flipEndian(&fileType, sizeof(uint32));

			if (DDS_MAGIC == fileType)
			{
				return String("dds");
			}
		}

		return StringUtil::BLANK;

	}

	bool DDSCodec::is3Dc(MemoryDataStreamPtr& stream)
	{
		// Read 4 character code
		uint32 fileType;
		stream->read(&fileType, sizeof(uint32));
		flipEndian(&fileType, sizeof(uint32), 1);
		
		if (FOURCC('D', 'D', 'S', ' ') != fileType)
			return false;
		
		// Read header in full
		DDSHeader header;
		stream->read(&header, sizeof(DDSHeader));

		// Endian flip if required, all 32-bit values
		flipEndian(&header, 4, sizeof(DDSHeader) / 4);

		//// Pixel format
		//PixelFormat sourceFormat = PF_UNKNOWN;

		if(header.pixelFormat.flags & DDPF_FOURCC)
		{
			if(header.pixelFormat.fourCC == FOURCC('A','T','I','2'))
				return true;
		}

		return false;
	}

	uint DDSCodec::getImageFlags(DataStreamPtr& stream)
	{
		// Read 4 character code
		uint32 fileType;
		stream->read(&fileType, sizeof(uint32));
		flipEndian(&fileType, sizeof(uint32), 1);

		if (FOURCC('D', 'D', 'S', ' ') != fileType) {
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
				"This is not a DDS file!", "DDSCodec::decode");
		}

		// Read header in full
		DDSHeader header;
		stream->read(&header, sizeof(DDSHeader));

		// Endian flip if required, all 32-bit values
		flipEndian(&header, 4, sizeof(DDSHeader) / 4);

		// Check some sizes
		if (header.size != DDS_HEADER_SIZE)
		{
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
				"DDS header size mismatch!", "DDSCodec::decode");
		}
		if (header.pixelFormat.size != DDS_PIXELFORMAT_SIZE)
		{
			OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
				"DDS header size mismatch!", "DDSCodec::decode");
		}

		//TODO: IF_COMPRESSED ignored!

		uint flags = 0;
		if (header.caps.caps2 & DDSCAPS2_CUBEMAP)
			flags |= IF_CUBEMAP;
		else if (header.caps.caps2 & DDSCAPS2_VOLUME)
			flags |= IF_3D_TEXTURE;

		return flags;
	}

}

