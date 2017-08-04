#include "quakedef.h"

char loadfilename[MAX_OSPATH]; //file scope so that error messages can use it

/*
============
Image_LoadImage

returns a pointer to hunk allocated RGBA data

TODO: search order: tga png jpg pcx lmp
============
*/
byte* Image_LoadImage(char* name, int* width, int* height)
{
	FILE* f;

	sprintf(loadfilename, "%s.tga", name);
	COM_FOpenFile(loadfilename, &f);
	if (f)
		return Image_LoadTGA(f, width, height);

	sprintf(loadfilename, "%s.pcx", name);
	COM_FOpenFile(loadfilename, &f);
	if (f)
		return Image_LoadPCX(f, width, height);

	return nullptr;
}

//==============================================================================
//
//  TGA
//
//==============================================================================

struct targaheader_t
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
};

#define TARGAHEADERSIZE 18 //size on disk

targaheader_t targa_header;

int fgetLittleShort(FILE* f)
{
	byte b1 = fgetc(f);
	byte b2 = fgetc(f);

	return static_cast<short>(b1 + b2 * 256);
}

int fgetLittleLong(FILE* f)
{
	byte b1 = fgetc(f);
	byte b2 = fgetc(f);
	byte b3 = fgetc(f);
	byte b4 = fgetc(f);

	return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}

/*
============
Image_WriteTGA -- writes RGB or RGBA data to a TGA file

returns true if successful

TODO: support BGRA and BGR formats (since opengl can return them, and we don't have to swap)
============
*/
bool Image_WriteTGA(char* name, byte* data, int width, int height, int bpp, bool upsidedown)
{
	char pathname[MAX_OSPATH];
	byte header[TARGAHEADERSIZE];

	Sys_mkdir(com_gamedir); //if we've switched to a nonexistant gamedir, create it now so we don't crash
	sprintf(pathname, "%s/%s", com_gamedir, name);
	int handle = Sys_FileOpenWrite(pathname);
	if (handle == -1)
		return false;

	Q_memset(&header, 0, TARGAHEADERSIZE);
	header[2] = 2; // uncompressed type
	header[12] = width & 255;
	header[13] = width >> 8;
	header[14] = height & 255;
	header[15] = height >> 8;
	header[16] = bpp; // pixel size
	if (upsidedown)
		header[17] = 0x20; //upside-down attribute

	// swap red and blue bytes
	auto bytes = bpp / 8;
	auto size = width * height * bytes;
	for (auto i = 0; i < size; i += bytes)
	{
		int temp = data[i];
		data[i] = data[i + 2];
		data[i + 2] = temp;
	}

	Sys_FileWrite(handle, &header, TARGAHEADERSIZE);
	Sys_FileWrite(handle, data, size);
	Sys_FileClose(handle);

	return true;
}

/*
=============
Image_LoadTGA
=============
*/
byte* Image_LoadTGA(FILE* fin, int* width, int* height)
{
	byte* pixbuf;
	int row, column;
	int realrow; //johnfitz -- fix for upside-down targas

	targa_header.id_length = fgetc(fin);
	targa_header.colormap_type = fgetc(fin);
	targa_header.image_type = fgetc(fin);

	targa_header.colormap_index = fgetLittleShort(fin);
	targa_header.colormap_length = fgetLittleShort(fin);
	targa_header.colormap_size = fgetc(fin);
	targa_header.x_origin = fgetLittleShort(fin);
	targa_header.y_origin = fgetLittleShort(fin);
	targa_header.width = fgetLittleShort(fin);
	targa_header.height = fgetLittleShort(fin);
	targa_header.pixel_size = fgetc(fin);
	targa_header.attributes = fgetc(fin);

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
		Sys_Error("Image_LoadTGA: %s is not a type 2 or type 10 targa\n", loadfilename);

	if (targa_header.colormap_type != 0 || targa_header.pixel_size != 32 && targa_header.pixel_size != 24)
		Sys_Error("Image_LoadTGA: %s is not a 24bit or 32bit targa\n", loadfilename);

	int columns = targa_header.width;
	int rows = targa_header.height;
	auto numPixels = columns * rows;
	auto upside_down = !(targa_header.attributes & 0x20); //johnfitz -- fix for upside-down targas

	auto targa_rgba = reinterpret_cast<byte*>(Hunk_Alloc(numPixels * 4));

	if (targa_header.id_length != 0)
		fseek(fin, targa_header.id_length, SEEK_CUR); // skip TARGA image comment

	if (targa_header.image_type == 2) // Uncompressed, RGB images
	{
		for (row = rows - 1; row >= 0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow * columns * 4;
			//johnfitz
			for (column = 0; column < columns; column++)
			{
				unsigned char red, green, blue;
				switch (targa_header.pixel_size)
				{
				case 24:
					blue = getc(fin);
					green = getc(fin);
					red = getc(fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = getc(fin);
					green = getc(fin);
					red = getc(fin);
					unsigned char alphabyte = getc(fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default: break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10) // Runlength encoded RGB images
	{
		unsigned char red = 0, green = 0, blue = 0, alphabyte = 0, j;
		for (row = rows - 1; row >= 0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow * columns * 4;
			//johnfitz
			for (column = 0; column < columns;)
			{
				unsigned char packetHeader = getc(fin);
				unsigned char packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) // run-length packet
				{
					switch (targa_header.pixel_size)
					{
					case 24:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						alphabyte = 255;
						break;
					case 32:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						alphabyte = getc(fin);
						break;
					default: break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) // run spans across rows
						{
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow * columns * 4;
							//johnfitz
						}
					}
				}
				else // non run-length packet
				{
					for (j = 0; j < packetSize; j++)
					{
						switch (targa_header.pixel_size)
						{
						case 24:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							alphabyte = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						default: break;
						}
						column++;
						if (column == columns) // pixel packet run spans across rows
						{
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow * columns * 4;
							//johnfitz
						}
					}
				}
			}
		breakOut:;
		}
	}

	fclose(fin);

	*width = static_cast<int>(targa_header.width);
	*height = static_cast<int>(targa_header.height);
	return targa_rgba;
}

//==============================================================================
//
//  PCX
//
//==============================================================================

struct pcxheader_t
{
	char signature;
	char version;
	char encoding;
	char bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hdpi, vdpi;
	byte colortable[48];
	char reserved;
	char color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char filler[58];
};

/*
============
Image_LoadPCX
============
*/
byte* Image_LoadPCX(FILE* f, int* width, int* height)
{
	pcxheader_t pcx;
	int runlength;
	byte palette[768];

	int start = ftell(f); //save start of file (since we might be inside a pak file, SEEK_SET might not be the start of the pcx)

	fread(&pcx, sizeof pcx, 1, f);
	pcx.xmin = static_cast<unsigned short>(LittleShort(pcx.xmin));
	pcx.ymin = static_cast<unsigned short>(LittleShort(pcx.ymin));
	pcx.xmax = static_cast<unsigned short>(LittleShort(pcx.xmax));
	pcx.ymax = static_cast<unsigned short>(LittleShort(pcx.ymax));
	pcx.bytes_per_line = static_cast<unsigned short>(LittleShort(pcx.bytes_per_line));

	if (pcx.signature != 0x0A)
		Sys_Error("'%s' is not a valid PCX file", loadfilename);

	if (pcx.version != 5)
		Sys_Error("'%s' is version %i, should be 5", loadfilename, pcx.version);

	if (pcx.encoding != 1 || pcx.bits_per_pixel != 8 || pcx.color_planes != 1)
		Sys_Error("'%s' has wrong encoding or bit depth", loadfilename);

	auto w = pcx.xmax - pcx.xmin + 1;
	auto h = pcx.ymax - pcx.ymin + 1;

	auto data = reinterpret_cast<byte*>(Hunk_Alloc((w * h + 1) * 4)); //+1 to allow reading padding byte on last line

	//load palette
	fseek(f, start + com_filesize - 768, SEEK_SET);
	fread(palette, 1, 768, f);

	//back to start of image data
	fseek(f, start + sizeof pcx, SEEK_SET);

	for (auto y = 0; y < h; y++)
	{
		auto p = data + y * w * 4;

		for (auto x = 0; x < pcx.bytes_per_line;) //read the extra padding byte if necessary
		{
			auto readbyte = fgetc(f);

			if (readbyte >= 0xC0)
			{
				runlength = readbyte & 0x3F;
				readbyte = fgetc(f);
			}
			else
				runlength = 1;

			while (runlength--)
			{
				p[0] = palette[readbyte * 3];
				p[1] = palette[readbyte * 3 + 1];
				p[2] = palette[readbyte * 3 + 2];
				p[3] = 255;
				p += 4;
				x++;
			}
		}
	}

	fclose(f);

	*width = w;
	*height = h;
	return data;
}
