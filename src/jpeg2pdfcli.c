
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

#ifdef WIN32
#define HAVE_FINDFIRST
#include <io.h>
#else
#define HAVE_GLOB
#include <glob.h>
#endif

#include "jpeg2pdf.h"

unsigned char *JPEG2PDF_VERSION = "1.2";

//Gets the JPEG size from the array of data passed to the function, file reference: http://www.obrador.com/essentialjpeg/headerinfo.htm
static int get_jpeg_size(unsigned char* data, unsigned int data_size, unsigned short *width, unsigned short *height, unsigned char *colors, double* dpiX, double* dpiY)
{
    //Check for valid JPEG image
    int i=0; // Keeps track of the position within the file
    if(data[i] == 0xFF && data[i+1] == 0xD8 && data[i+2] == 0xFF )
    {
        i += 4;
        // Check for valid JPEG header (null terminated JFIF)
//      if(data[i+2] == 'J' && data[i+3] == 'F' && data[i+4] == 'I' && data[i+5] == 'F' && data[i+6] == 0x00) {
        // Retrieve dpi:
        /* It is also possible to retrieve "rational" dpi from EXIF data -- in that case it'll be really double.
           Should we prefer EXIF data when present? */
        UINT8 units=data[i+9];
        if(units==1) // pixels per inch
        {
            *dpiX=data[i+10]*256+data[i+11]; // Xdensity
            *dpiY=data[i+12]*256+data[i+13]; // Ydensity
        }
        else if(units==2) // pixels per cm
        {
            *dpiX=(data[i+10]*256+data[i+11])*2.54; // Xdensity converted to dpi
            *dpiY=(data[i+12]*256+data[i+13])*2.54; // Ydensity --> dpi
        }
        else // units==0, fallback to 300dpi? Here EXIF data would be useful.
        {
            *dpiX=*dpiY=300;
        }
        //Retrieve the block length of the first block since the first block will not contain the size of file
        unsigned short block_length = data[i] * 256 + data[i+1];
        while(i<(int)data_size)
        {
            i+=block_length; //Increase the file index to get to the next block
            if(i >= (int)data_size) return 0; //Check to protect against segmentation faults
            if(data[i] != 0xFF) return 0; //Check that we are truly at the start of another block
            if(data[i+1] >= 0xC0 && data[i+1] <= 0xC2) //0xFFC0 is the "Start of frame" marker which contains the file size
            {
                //The structure of the 0xFFC0 block is quite simple [0xFFC0][ushort length][uchar precision][ushort x][ushort y]
                *height = data[i+5]*256 + data[i+6];
                *width = data[i+7]*256 + data[i+8];
                *colors = data[i+9];
                return 1;
            }
            else
            {
                i+=2; //Skip the block marker
                block_length = data[i] * 256 + data[i+1]; //Go to the next block
            }
        }
        return 0; //If this point is reached then no size was found
//      }else{ return 0; }                  //Not a valid JFIF string

    }
    else
    {
        return 0; //Not a valid SOI header
    }
}

void insertJPEGFile(const char *fileName, int fileSize, PJPEG2PDF pdfId, PageOrientation pageOrientation, ScaleMethod scale, int pageDpi, double pageLeft, double pageBottom, bool cropHeight, bool cropWidth)
{
    FILE  *fp;
    unsigned char *jpegBuf;
    int readInSize;
    unsigned short jpegImgW, jpegImgH;
    unsigned char colors;
    double dpiX, dpiY;

    jpegBuf = malloc(fileSize);
    if( jpegBuf==NULL )
    {
        fprintf(stderr,"Memory allocation error.\n");
        exit(EXIT_FAILURE);
    }

    fp = fopen(fileName, "rb");
    if(fp==NULL)
    {
        fprintf(stderr,"Can't open file '%s'. Aborted.\n", fileName);
        exit(EXIT_FAILURE);
    }
    readInSize = fread(jpegBuf, sizeof(UINT8), fileSize, fp);
    fclose(fp);

    if(readInSize != fileSize)
        fprintf(stderr,"Warning: File %s should be %d Bytes. But only read in %d Bytes.\n", fileName, fileSize, readInSize);

    if(1 == get_jpeg_size(jpegBuf, readInSize, &jpegImgW, &jpegImgH, &colors, &dpiX, &dpiY))
    {
        if (pageDpi > 0)
        {
            dpiX = pageDpi;
            dpiY = pageDpi;
        }
        printf("Adding %s (%dx%d, %.0fx%.0f dpi)\n", fileName, jpegImgW, jpegImgH, dpiX, dpiY);
        /* Add JPEG File into PDF */
        Jpeg2PDF_AddJpeg(pdfId, jpegImgW, jpegImgH, readInSize, jpegBuf, (3==colors), pageOrientation, dpiX, dpiY, scale, pageLeft, pageBottom, cropHeight, cropWidth);
    }
    else
    {
        fprintf(stderr,"Can't obtain image dimension from '%s'. Aborted.\n", fileName);
        exit(EXIT_FAILURE);
    }

    free(jpegBuf);
}

void getJpegFileImageDimensions(const char *fileName, int fileSize, int pageDpi, double *width, double *height)   //in inches; copy-pasted from insertJPEGFile(), Jpeg2PDF_AddJpeg() call is replaced with width/height calculation.
{
    FILE  *fp;
    unsigned char *jpegBuf;
    int readInSize;
    unsigned short jpegImgW, jpegImgH;
    unsigned char colors;
    double dpiX, dpiY;

    jpegBuf = malloc(fileSize);
    if( jpegBuf==NULL )
    {
        fprintf(stderr,"Memory allocation error.\n");
        exit(EXIT_FAILURE);
    }

    fp = fopen(fileName, "rb");
    if(fp==NULL)
    {
        fprintf(stderr,"Can't open file '%s'. Aborted.\n", fileName);
        exit(EXIT_FAILURE);
    }
    readInSize = fread(jpegBuf, sizeof(UINT8), fileSize, fp);
    fclose(fp);

    if(readInSize != fileSize)
        fprintf(stderr,"Warning: File %s should be %d Bytes. But only read in %d Bytes.\n", fileName, fileSize, readInSize);

    if(1 == get_jpeg_size(jpegBuf, readInSize, &jpegImgW, &jpegImgH, &colors, &dpiX, &dpiY))
    {
        //printf("Adding %s (%dx%d, %.0fx%.0f dpi)\n", fileName, jpegImgW, jpegImgH, dpiX, dpiY);
        ///* Add JPEG File into PDF */
        //Jpeg2PDF_AddJpeg(pdfId, jpegImgW, jpegImgH, readInSize, jpegBuf, (3==colors), pageOrientation, dpiX, dpiY, scale, cropPage);
        if (pageDpi > 0)
        {
            dpiX = pageDpi;
            dpiY = pageDpi;
        }
        *width=((double)jpegImgW)/dpiX;
        *height=((double)jpegImgH)/dpiY;
    }
    else
    {
        fprintf(stderr,"Can't obtain image dimension from '%s'. Aborted.\n", fileName);
        exit(EXIT_FAILURE);
    }

    free(jpegBuf);
}

inline double min(double a, double b)
{
    return (a < b) ? a : b;
}
inline double max(double a, double b)
{
    return (a > b) ? a : b;
}

#ifdef HAVE_GLOB
void findMaximumDimensions(glob_t globbuf, int globlen, int pageDpi, bool fixedOrientation, double *maxWidth, double *maxHeight)
{
    int i;
    double width, height;
    struct stat sb;

    *maxWidth=*maxHeight=0;
    for (i=0; i<globbuf.gl_pathc; i++)
    {
        if (stat(globbuf.gl_pathv[i], &sb) == -1)
        {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        getJpegFileImageDimensions(globbuf.gl_pathv[i], sb.st_size, pageDpi, &width, &height);
        if(fixedOrientation)
        {
            if(width>*maxWidth)
            {
                *maxWidth=width;
            }
            if(height>*maxHeight)
            {
                *maxHeight=height;
            }
        }
        else // orientation is unknown, so width is the smallest dimension, and height is the largest dimension (in default portrait orientation)
        {
            if(min(width,height)>*maxWidth)
            {
                *maxWidth=min(width,height);
            }
            if(max(width,height)>*maxHeight)
            {
                *maxHeight=max(width,height);
            }
        }
    }
}
#endif
#ifdef HAVE_FINDFIRST
void findMaximumDimensions(char **filesarray, int globlen, int pageDpi, bool fixedOrientation, double *maxWidth, double *maxHeight)
{
    int i;
    double width, height;
    struct stat sb;

    *maxWidth=*maxHeight=0;
    for(i=0; i<globlen; i++)
    {
        if (stat(filesarray[i], &sb) == -1)
        {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        getJpegFileImageDimensions(filesarray[i], sb.st_size, pageDpi, &width, &height);
        if(fixedOrientation)
        {
            if(width>*maxWidth)
            {
                *maxWidth=width;
            }
            if(height>*maxHeight)
            {
                *maxHeight=height;
            }
        }
        else // orientation is unknown, so width is the smallest dimension, and height is the largest dimension (in default portrait orientation)
        {
            if(min(width,height)>*maxWidth)
            {
                *maxWidth=min(width,height);
            }
            if(max(width,height)>*maxHeight)
            {
                *maxHeight=max(width,height);
            }
        }
    }
}
#endif

void printHelp( char *appname )
{
    fprintf(stderr, "Jpeg2PDF vesion %s\n", JPEG2PDF_VERSION);
    fprintf(stderr, "Usage: %s [options] filemask-1 ... [filemask-N]\n" \
            "  -o output        output PDF file name. required.\n" \
            "  [-d dpi]         global page dpi, default: get in image\n" \
            "  [-p papersize]   A0-A10,Letter,Legal,Junior,Ledger,Tabloid,auto default:auto\n" \
            "  [-n orientation] auto,portrait,landscape default:auto\n" \
            "  [-m marginsize]  margins size in inches (specify 'mm' for millimeters, 'p' for points) default:0\n" \
            "  [-x widthpage]   width page in inches (specify 'mm' for millimeters, 'p' for points) default:0\n" \
            "  [-y heightpage]  height page in inches (specify 'mm' for millimeters, 'p' for points) default:0\n" \
            "  [-l leftsize]    left size in inches (specify 'mm' for millimeters, 'p' for points) default:0\n" \
            "  [-b bottomsize]  bottom size in inches (specify 'mm' for millimeters, 'p' for points) default:0\n" \
            "  [-z scale]       fit,fw,fh,reduce,rw,rh,none default:fit\n" \
            "  [-r crop]        none,height,width,both default:none crop/expand page to image size\n" \
            "  [-t title]       default: none\n" \
            "  [-a author]      default: current user name\n" \
            "  [-k keywords]    default: input files list\n" \
            "  [-s subject]     default: 'Generated from JPEG images'\n" \
            "  [-c creator]     default: 'jpeg2pdf'\n" \
            "  [-h]             display this help\n" \
            "  filemask-...     input JPEG file name. requires at least one. wildcards '*' and '?' allowed.\n" \
            , appname);
    exit(EXIT_FAILURE);
}
int main(int argc, char *argv[])
{

    /* Delcare of the PDF Object */
    PJPEG2PDF pdfId;

    char *outputFilename=NULL;
    FILE *fp;
    char *title=NULL, *author=NULL, *keywords=NULL, *subject=NULL, *creator=NULL;
    int opt;
    double pageWidth=8.27, pageHeight=11.69, pageMargins=0, pageWidthDef=0, pageHeightDef=0, pageLeftDef=-1, pageBottomDef=-1;
    int globindex, globlen;
#ifdef HAVE_GLOB
    glob_t globbuf;
#endif
#ifdef HAVE_FINDFIRST
    struct _finddata_t jpeg_file;
    long hFile;
    char **filesarray = NULL;
#endif
    char *filename;
    struct stat sb;
    struct timeval tv1;
    struct tm *tmp;
    char timestamp[40];
    int keywordslen, pageDpi=-1;

    PageOrientation pageOrientation=PageOrientationAuto;
    ScaleMethod scale=ScaleFit;
    bool paperSizeAuto=true;
    bool cropWidth=false, cropHeight=false;

    /* no options, no files */
    if( argc < 2 )
    {
        printHelp( argv[0] );
    }

    while ((opt = getopt(argc, argv, "o:d:p:n:z:m:x:y:l:b:a:k:s:c:r:h")) != -1)
    {
        switch (opt)
        {
        case 'o':
            outputFilename = optarg;
            break;
        case 'd':
            pageDpi = atof(optarg);
            break;
        case 'p':
            if(strcasecmp("A0",optarg)==0)
            {
                pageWidth=33.11;
                pageHeight=46.81;
            }
            else if(strcasecmp("A1",optarg)==0)
            {
                pageWidth=23.39;
                pageHeight=33.11;
            }
            else if(strcasecmp("A2",optarg)==0)
            {
                pageWidth=16.54;
                pageHeight=23.39;
            }
            else if(strcasecmp("A3",optarg)==0)
            {
                pageWidth=11.69;
                pageHeight=16.54;
            }
            else if(strcasecmp("A4",optarg)==0)
            {
                pageWidth=8.27;
                pageHeight=11.69;
            }
            else if(strcasecmp("A5",optarg)==0)
            {
                pageWidth=5.83;
                pageHeight=8.27;
            }
            else if(strcasecmp("A6",optarg)==0)
            {
                pageWidth=4.13;
                pageHeight=5.83;
            }
            else if(strcasecmp("A7",optarg)==0)
            {
                pageWidth=2.91;
                pageHeight=4.13;
            }
            else if(strcasecmp("A8",optarg)==0)
            {
                pageWidth=2.05;
                pageHeight=2.91;
            }
            else if(strcasecmp("A9",optarg)==0)
            {
                pageWidth=1.46;
                pageHeight=2.05;
            }
            else if(strcasecmp("A10",optarg)==0)
            {
                pageWidth=1.02;
                pageHeight=1.46;
            }
            else if(strcasecmp("Letter",optarg)==0)
            {
                pageWidth=8.5;
                pageHeight=11.0;
            }
            else if(strcasecmp("Legal",optarg)==0)
            {
                pageWidth=8.5;
                pageHeight=14.0;
            }
            else if(strcasecmp("Junior",optarg)==0)
            {
                pageWidth=8.5;
                pageHeight=5.0;
            }
            else if(strcasecmp("Ledger",optarg)==0)
            {
                pageWidth=17.0;
                pageHeight=11.0;
            }
            else if(strcasecmp("Tabloid",optarg)==0)
            {
                pageWidth=11.0;
                pageHeight=17.0;
            }
            else if(strcmp("auto",optarg)==0)
            {
                paperSizeAuto=true;
            }
            else /* unknown */
            {
                printHelp( argv[0] );
            }
            break;
        case 'm':
            pageMargins = atof(optarg);
            /* mm */
            if( strchr(optarg,'m')!=NULL )
            {
                pageMargins /= 25.4;
            }
            else if( strchr(optarg,'p')!=NULL )
            {
				pageMargins /= ((pageDpi < 0) ? PDF_DEF_DENSITY : pageDpi);
            }
            break;
        case 'x':
            pageWidthDef = atof(optarg);
            /* mm */
            if( strchr(optarg,'m')!=NULL )
            {
                pageWidthDef /= 25.4;
            }
            else if( strchr(optarg,'p')!=NULL )
            {
				pageWidthDef /= ((pageDpi < 0) ? PDF_DEF_DENSITY : pageDpi);
			}
            break;
        case 'y':
            pageHeightDef = atof(optarg);
            /* mm */
            if( strchr(optarg,'m')!=NULL )
            {
                pageHeightDef /= 25.4;
            }
            else if( strchr(optarg,'p')!=NULL )
            {
				pageHeightDef /= ((pageDpi < 0) ? PDF_DEF_DENSITY : pageDpi);
            }
            break;
        case 'l':
            pageLeftDef = atof(optarg);
            /* mm */
            if( strchr(optarg,'m')!=NULL )
            {
                pageLeftDef /= 25.4;
            }
            else if( strchr(optarg,'p')!=NULL )
            {
				pageLeftDef /= ((pageDpi < 0) ? PDF_DEF_DENSITY : pageDpi);
            }
            break;
        case 'b':
            pageBottomDef = atof(optarg);
            /* mm */
            if( strchr(optarg,'m')!=NULL )
            {
                pageBottomDef /= 25.4;
            }
            else if( strchr(optarg,'p')!=NULL )
            {
				pageBottomDef /= ((pageDpi < 0) ? PDF_DEF_DENSITY : pageDpi);
            }
            break;
        case 't':
            title = optarg;
            break;
        case 'a':
            author = optarg;
            break;
        case 'k':
            keywords = optarg;
            break;
        case 's':
            subject = optarg;
            break;
        case 'c':
            creator = optarg;
            break;
        case 'n':
            if(strcmp("portrait", optarg)==0)
            {
                pageOrientation=Portrait;
            }
            else if(strcmp("landscape", optarg)==0)
            {
                pageOrientation=Landscape;
            }
            else if(strcmp("auto", optarg)==0)
            {
                pageOrientation=PageOrientationAuto;
            }
            else
            {
                printHelp(argv[0]);
            }
            break;
        case 'z':
            if(strcmp("fit", optarg)==0)
            {
                scale=ScaleFit;
            }
            else if(strcmp("reduce", optarg)==0)
            {
                scale=ScaleReduce;
            }
            else if(strcmp("none", optarg)==0)
            {
                scale=ScaleNone;
            }
            else if(strcmp("fw", optarg)==0)
            {
                scale=ScaleFitWidth;
            }
            else if(strcmp("fh", optarg)==0)
            {
                scale=ScaleFitHeight;
            }
            else if(strcmp("rw", optarg)==0)
            {
                scale=ScaleReduceWidth;
            }
            else if(strcmp("rh", optarg)==0)
            {
                scale=ScaleReduceHeight;
            }
            else
            {
                printHelp(argv[0]);
            }
            break;
        case 'r':
            if(strcmp("none", optarg)==0)
            {
                cropHeight=cropWidth=false;
            }
            else if(strcmp("height", optarg)==0)
            {
                cropHeight=true;
                cropWidth=false;
            }
            else if(strcmp("width", optarg)==0)
            {
                cropHeight=false;
                cropWidth=true;
            }
            else if(strcmp("both", optarg)==0)
            {
                cropHeight=cropWidth=true;
            }
            else
            {
                printHelp(argv[0]);
            }
            break;
        case 'h':
        default: /* '?' */
            printHelp( argv[0] );
        }
    }

    /* no output file */
    if( outputFilename==NULL )
    {
        printHelp( argv[0] );
    }

    /* no files */
    if ( optind >= argc )
    {
        printHelp( argv[0] );
    }

#ifdef HAVE_GLOB
    globbuf.gl_offs = 0;
    for( opt = optind; opt < argc; opt++ )
    {
        if( glob(argv[opt], (opt == optind) ? GLOB_DOOFFS : (GLOB_DOOFFS | GLOB_APPEND), NULL, &globbuf) == GLOB_NOMATCH )
        {
            fprintf(stderr,"File(s) not found '%s'.\n",argv[opt]);
            exit(EXIT_FAILURE);
        }
    }
#endif
#ifdef HAVE_FINDFIRST
    globlen = 0;
    for( opt = optind; opt < argc; opt++ )
    {
        if( (hFile = _findfirst( argv[opt], &jpeg_file )) == -1L )
        {
            fprintf(stderr,"File(s) not found '%s'.\n",argv[opt]);
            exit(EXIT_FAILURE);
        }
        do
        {
            filesarray = (char **)realloc(filesarray, (globlen + 1) * sizeof(char *));
            filesarray[globlen++] = strdup(jpeg_file.name);
        }
        while( _findnext( hFile, &jpeg_file ) == 0 );
        _findclose( hFile );
    }
#endif

    /* guess default values */
    if( title==NULL )
    {
        title="";
    }

    if( author==NULL && getenv("SUDO_USER")!=NULL )
    {
        author=getenv("SUDO_USER");
    }
    if( author==NULL && getenv("USER")!=NULL )
    {
        author=getenv("USER");
    }
    if( author==NULL && getenv("USERNAME")!=NULL )
    {
        author=getenv("USERNAME");
    }
    if( author==NULL && getenv("LOGNAME")!=NULL )
    {
        author=getenv("LOGNAME");
    }
    if( author==NULL )
    {
        author="anonymous";
    }

    if( keywords==NULL )
    {
        /* build files list */
#ifdef HAVE_GLOB
        keywordslen=0;
        for (globindex=0; globindex<globbuf.gl_pathc; globindex++)
        {
            keywordslen += strlen(globbuf.gl_pathv[globindex]);
        }
        keywords=(char*)malloc( keywordslen + globbuf.gl_pathc*2 );
        if( keywords==NULL )
        {
            fprintf(stderr,"Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }
        *keywords='\0';
        for (globindex=0; globindex<globbuf.gl_pathc; globindex++)
        {
            filename = strdup(globbuf.gl_pathv[globindex]);
            strcat(keywords,basename(filename));
            if (globindex!=globbuf.gl_pathc-1)
            {
                strcat(keywords,", ");
            }
        }
#endif
#ifdef HAVE_FINDFIRST
        keywordslen=0;
        for (globindex=0; globindex<globlen; globindex++)
        {
            keywordslen += strlen(filesarray[globindex]);
        }
        keywords=(char*)malloc( keywordslen + globlen*2 );
        if( keywords==NULL )
        {
            fprintf(stderr,"Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }
        *keywords='\0';
        for (globindex=0; globindex<globlen; globindex++)
        {
            filename = strdup(filesarray[globindex]);
            strcat(keywords,basename(filename));
            if (globindex!=globlen-1)
            {
                strcat(keywords,", ");
            }
        }
#endif
    }

    if( subject==NULL )
    {
        subject="Generated from JPEG images";
    }
    if( creator==NULL )
    {
        creator="jpeg2pdf";
    }
    if (pageWidthDef > 0 && pageHeightDef > 0)
    {
        pageWidth = pageWidthDef;
        pageHeight = pageHeightDef;
    } else {
        if(paperSizeAuto) // determine paper size from maximum jpeg dimensions
        {
#ifdef HAVE_GLOB
            findMaximumDimensions(globbuf, globlen, pageDpi, pageOrientation==Portrait || pageOrientation==Landscape, &pageWidth, &pageHeight);
#endif
#ifdef HAVE_FINDFIRST
            findMaximumDimensions(filesarray, globlen, pageDpi, pageOrientation==Portrait || pageOrientation==Landscape, &pageWidth, &pageHeight);
#endif
            pageWidth += (pageMargins * 2.0);
            pageHeight += (pageMargins * 2.0);
            printf("Selected paper size: %.2f x %.2f \" or %.2f x %.2f cm.\n", pageWidth, pageHeight, pageWidth*2.54, pageHeight*2.54);
        }
    }
    /* Initialize the PDF Object with Page Size Information */
    pdfId = Jpeg2PDF_BeginDocument(pageWidth, pageHeight, pageMargins); /* Letter is 8.5x11 inch */

    if(pdfId >= 0)
    {
        UINT32 pdfSize;
        UINT8  *pdfBuf;

#ifdef HAVE_GLOB
        for (globindex=0; globindex<globbuf.gl_pathc; globindex++)
        {
            if (stat(globbuf.gl_pathv[globindex], &sb) == -1)
            {
                perror("stat");
                exit(EXIT_FAILURE);
            }
            insertJPEGFile(globbuf.gl_pathv[globindex], sb.st_size, pdfId, pageOrientation, scale, pageDpi, pageLeftDef, pageBottomDef, cropHeight, cropWidth);
        }
        globfree(&globbuf);
#endif
#ifdef HAVE_FINDFIRST
        for (globindex=0; globindex<globlen; globindex++)
        {
            if (stat(filesarray[globindex], &sb) == -1)
            {
                perror("stat");
                exit(EXIT_FAILURE);
            }
            insertJPEGFile(filesarray[globindex], sb.st_size, pdfId, pageOrientation, scale, pageDpi, pageLeftDef, pageBottomDef, cropHeight, cropWidth);
        }
#endif

        gettimeofday(&tv1,NULL);
        tmp=localtime(&tv1.tv_sec);

        /* ISO8601 timestamp */
        if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", tmp) == 0)
        {
            putenv("TZ=UTC");
            tzset();
            if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S+0000", tmp) == 0)
            {
                fprintf(stderr, "strftime failed");
                exit(EXIT_FAILURE);
            }
        }
        /* timezone minutes colon hack */
        if (timestamp[19]=='+' || timestamp[19]=='-')
        {
            strcat(timestamp," ");
            timestamp[24]=timestamp[23];
            timestamp[23]=timestamp[22];
            timestamp[22]=':';
        }


        /* Finalize the PDF and get the PDF Size */
        pdfSize = Jpeg2PDF_EndDocument(pdfId, timestamp, title, author, keywords, subject, creator);
        /* Prepare the PDF Data Buffer based on the PDF Size */
        pdfBuf = malloc(pdfSize);
        if( pdfBuf==NULL )
        {
            fprintf(stderr,"Memory allocation error.\n");
            exit(EXIT_FAILURE);
        }

        printf("Generating PDF (%d bytes) ...\n", pdfSize);
        /* Get the PDF into the Data Buffer and do the cleanup */
        Jpeg2PDF_GetFinalDocumentAndCleanup(pdfId, pdfBuf, &pdfSize); /* pdfSize: In: bytes of pdfBuf; Out: bytes used in pdfBuf */

        printf("Writing file '%s' (%d bytes) ...\n", outputFilename, pdfSize);
        /* Output the PDF Data Buffer to file */
        fp = fopen(outputFilename, "wb");
        if( fp==NULL )
        {
            fprintf(stderr,"Can't open file '%s'. Aborted.\n", outputFilename);
            exit(EXIT_FAILURE);
        }
        if( fwrite(pdfBuf, sizeof(UINT8), pdfSize, fp) != pdfSize )
        {
            fprintf(stderr,"Write error. Aborted.\n", outputFilename);
            exit(EXIT_FAILURE);
        }
        fclose(fp);

        free(pdfBuf);
        printf("Done.\n");

    }
    else
    {
        fprintf(stderr,"Error Init.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
