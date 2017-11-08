/*
Jpeg2PDF.c
Created by HH.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Jpeg2PDF.h"

#define INDEX_USE_PPDF		(-1)

static void Jpeg2PDF_SetXREF(PJPEG2PDF pPDF, int index, int offset, char c) {
	if(INDEX_USE_PPDF == index) index = pPDF->pdfObj;

	if('f' == c) 
		sprintf(pPDF->pdfXREF[index], "%010d 65535 f \n", offset); //space on tail required
	else
		sprintf(pPDF->pdfXREF[index], "%010d 00000 %c \n", offset, c); //space on tail required

	if(JPEG2PDF_DEBUG) logMsg("pPDF->pdfXREF[%d] = %s", index, (int)pPDF->pdfXREF[index], 3,4,5,6);
}

PJPEG2PDF Jpeg2PDF_BeginDocument(double pdfW, double pdfH, double margin) { // width and height in default (portrait) orientation
	PJPEG2PDF pPDF;
	
	pPDF = (PJPEG2PDF)malloc(sizeof(JPEG2PDF));
	if(pPDF) {
		memset(pPDF, 0, sizeof(JPEG2PDF));
		if(JPEG2PDF_DEBUG) logMsg("PDF List Inited (pPDF = %p)\n", (int)pPDF, 2,3,4,5,6);
		pPDF->pageW = (UINT32)(pdfW * PDF_DOT_PER_INCH);
		pPDF->pageH = (UINT32)(pdfH * PDF_DOT_PER_INCH);
		//Maximum image size without margins
		pPDF->margin = margin;
		pPDF->maxImgW = (double) pPDF->pageW - (2 * margin * PDF_DOT_PER_INCH);
		pPDF->maxImgH = (double) pPDF->pageH - (2 * margin * PDF_DOT_PER_INCH);
		if(JPEG2PDF_DEBUG) logMsg("PDF Page Size (%d %d) Max Image Size (%f %f)\n", pPDF->pageW, pPDF->pageH, pPDF->maxImgW, pPDF->maxImgH,3,4,5,6);
		
		pPDF->currentOffSet = 0;
		Jpeg2PDF_SetXREF(pPDF, 0, pPDF->currentOffSet, 'f');
		pPDF->currentOffSet += sprintf(pPDF->pdfHeader, "%%PDF-1.4\n%%%cJpeg2PDF Engine By: HH [ihaohu@gmail.com]%c\n", 0xFF, 0xFF);
		if(JPEG2PDF_DEBUG) logMsg("[pPDF=%p], header: %s", (int)pPDF, (int)pPDF->pdfHeader,3,4,5,6);
		
		pPDF->imgObj = 0;
		pPDF->pdfObj = 2;		/* 0 & 1 was reserved for xref & document Root */
	}
	
	return pPDF;
}

STATUS Jpeg2PDF_AddJpeg(PJPEG2PDF pPDF, UINT32 imgW, UINT32 imgH, UINT32 fileSize, UINT8 *pJpeg, UINT8 isColor, PageOrientation pageOrientation,
						double dpiX, double dpiY, ScaleMethod scale, bool cropHeight, bool cropWidth) {
	STATUS result = ERROR;
	PJPEG2PDF_NODE pNode;
	double imgAspect, newImgW, newImgH;

	if(pPDF) {
		if(pPDF->nodeCount >= MAX_PDF_PAGES) {
			logMsg("Add JPEG into PDF Skipped. Reason: Reached Max Page Number (%d) in single PDF.\n", MAX_PDF_PAGES,2,3,4,5,6);
			return result;
		}

		pNode = (PJPEG2PDF_NODE)malloc(sizeof(JPEG2PDF_NODE));
		if(pNode) {
			UINT32 nChars, currentImageObject;
			UINT8 *pFormat, lenStr[256];
			pNode->JpegW = imgW;
			pNode->JpegH = imgH;
			pNode->JpegSize = fileSize;
			pNode->pJpeg = (UINT8 *)malloc(pNode->JpegSize);
			pNode->pNext = NULL;
			
			if(pNode->pJpeg != NULL) {
				bool jpegPortrait, pagePortrait;
				double pageWidth, pageHeight; // accounting for page orientation, in PDF units (pixels at PDF_DOT_PER_INCH dpi)
				double maxImgWidth, maxImgHeight; // actual values accounting for page orientation, in PDF units
				double jpegWidth, jpegHeight; // jpeg dimensions (accounting for dpiX, dpiY, PDF_DOT_PER_INCH), in PDF units
				Fit fit;

				memcpy(pNode->pJpeg, pJpeg, pNode->JpegSize);
				
				/* Image Object */
				Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
				currentImageObject = pPDF->pdfObj;


				pPDF->currentOffSet += sprintf(pNode->preFormat, "\n%d 0 obj\n<</Type/XObject/Subtype/Image/Filter/DCTDecode/BitsPerComponent 8/ColorSpace/%s/Width %d/Height %d/Length %d>>\nstream\n",
					pPDF->pdfObj, ((isColor)? "DeviceRGB" : "DeviceGray"), pNode->JpegW, pNode->JpegH, pNode->JpegSize);
				
				pPDF->currentOffSet += pNode->JpegSize;
				
				pFormat = pNode->pstFormat;
				nChars = sprintf(pFormat, "\nendstream\nendobj\n");
				pPDF->currentOffSet += nChars;	pFormat += nChars;
				pPDF->pdfObj++;

				/* determine scale of the image keeping aspect ratio */
				jpegWidth=((double)pNode->JpegW)*PDF_DOT_PER_INCH/dpiX;
				jpegHeight=((double)pNode->JpegH)*PDF_DOT_PER_INCH/dpiY;
				imgAspect=jpegWidth/jpegHeight;
				if(JPEG2PDF_DEBUG) logMsg("imgAspect = %f %d %d\n", imgAspect, pNode->JpegW, pNode->JpegH, 3,4,5,6);

				// Determine page orientation:
				jpegPortrait=(jpegWidth <= jpegHeight);
				if(pageOrientation==PageOrientationAuto){
					if(scale==ScaleNone && jpegWidth<=pPDF->maxImgW && jpegHeight<=pPDF->maxImgH){	// image already fits into available area, don't rotate the page
						pagePortrait=true;															// assuming portrait orientation the most convenient for most users
					}else{
						pagePortrait=jpegPortrait;
						if((pPDF->maxImgW < pPDF->maxImgH) ^ (pPDF->pageW < pPDF->pageH)){ // very rare case: page orientation is opposite to available area orientation (it's possible with differently sized margins)
							pagePortrait=!pagePortrait;
						}
					}
				}else{
					pagePortrait=(pageOrientation==Portrait) ? true : false;
				}
				maxImgWidth=pagePortrait ? pPDF->maxImgW : pPDF->maxImgH;
				maxImgHeight=pagePortrait ? pPDF->maxImgH : pPDF->maxImgW;

				// Determine scaling method:
				if(scale==ScaleFit || (scale==ScaleReduce && (jpegWidth>maxImgWidth || jpegHeight>maxImgHeight))){ // fit jpeg to available area
					if(maxImgWidth/maxImgHeight > imgAspect){ // available area aspect is wider than jpeg aspect
						fit=FitHeight;
					}else{ // jpeg aspect is wider than available area aspect
						fit=FitWidth;
					}
				}else if(scale==ScaleFitWidth || (scale==ScaleReduceWidth && jpegWidth>maxImgWidth)){
					fit=FitWidth;
				}else if(scale==ScaleFitHeight || (scale==ScaleReduceHeight && jpegHeight>maxImgHeight)){
					fit=FitHeight;
				}else{ // don't fit, keep original dpi
					fit=FitNone;
				}

				// Scale image:
				if(fit==FitWidth){
					newImgW=maxImgWidth;
					newImgH=maxImgWidth/imgAspect;
				}else if(fit==FitHeight){
					newImgW=maxImgHeight*imgAspect;
					newImgH=maxImgHeight;
				}else{//fit==FitNone
					newImgW=jpegWidth;
					newImgH=jpegHeight;
				}

				// Set paper size from image size (possibly fitted/reduced to specific paper size) or properly rotate the page:
				pageWidth=cropWidth ? (newImgW+pPDF->margin) : (pagePortrait ? pPDF->pageW : pPDF->pageH);
				pageHeight=cropHeight ? (newImgH+pPDF->margin) : (pagePortrait ? pPDF->pageH : pPDF->pageW);
				
				/* Page Object */
				Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
				pNode->PageObj = pPDF->pdfObj;
				nChars = sprintf(pFormat, "%d 0 obj\n<</Type/Page/Parent 1 0 R/MediaBox[0 0 %.2f %.2f]/Contents %d 0 R/Resources %d 0 R>>\nendobj\n",
						pPDF->pdfObj, pageWidth, pageHeight, pPDF->pdfObj+1, pPDF->pdfObj + 3);
				pPDF->currentOffSet += nChars;	pFormat += nChars;
				pPDF->pdfObj++;
				
				/* Contents Object in Page Object */
				Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
				sprintf(lenStr, "q\n1 0 0 1 %.2f %.2f cm\n%.2f 0 0 %.2f 0 0 cm\n/I%d Do\nQ\n",
						(pageWidth-newImgW)/2, (pageHeight-newImgH)/2, newImgW , newImgH  , pPDF->imgObj); // center image
				nChars = sprintf(pFormat, "%d 0 obj\n<</Length %d 0 R>>stream\n%sendstream\nendobj\n",
						pPDF->pdfObj, pPDF->pdfObj+1, lenStr);
				pPDF->currentOffSet += nChars;	pFormat += nChars;
				pPDF->pdfObj++;

				/* Length Object in Contents Object */
				Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
				nChars = sprintf(pFormat, "%d 0 obj\n%ld\nendobj\n", pPDF->pdfObj, strlen(lenStr));
				pPDF->currentOffSet += nChars;	pFormat += nChars;
				pPDF->pdfObj++;
				
				/* Resources Object in Page Object */
				Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
				nChars = sprintf(pFormat, "%d 0 obj\n<</ProcSet[/PDF/%s]/XObject<</I%d %d 0 R>>>>\nendobj\n",
								pPDF->pdfObj, ((isColor)? "ImageC" : "ImageB"), pPDF->imgObj, currentImageObject);
				pPDF->currentOffSet += nChars;	pFormat += nChars;
				pPDF->pdfObj++;
				
				pPDF->imgObj++;

				/* Update the Link List */
				pPDF->nodeCount++;
				if(1 == pPDF->nodeCount) {
					pPDF->pFirstNode = pNode;
				} else {
					pPDF->pLastNode->pNext = pNode;
				}

				pPDF->pLastNode = pNode;

				result = OK;
			} /* pNode->pJpeg allocated OK */
		} /* pNode is valid */
	} /* pPDF is valid */

	return result;
}

UINT32 Jpeg2PDF_EndDocument(PJPEG2PDF pPDF, char* timestamp, char* title, char* author, char* keywords, char* subject, char *creator) {
	UINT32 headerSize, tailerSize, pdfSize = 0;
	char *producer = "Jpeg2PDF Engine By: HH [ihaohu@gmail.com],  jpeg2pdf.sf.net";
	char *XMPmetadata;

	if(pPDF) {
		UINT8 strKids[MAX_PDF_PAGES * MAX_KIDS_STRLEN], *pTail = pPDF->pdfTailer;
		UINT32 i, nChars, xrefOffSet, metadataObj, infoObj;
		PJPEG2PDF_NODE pNode;

		XMPmetadata=(char *)malloc(2048+strlen(title)+strlen(author)+strlen(keywords)+strlen(subject)+strlen(creator));
		nChars = sprintf(XMPmetadata,"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n" \
			"<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n" \
			"<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n" \
			"<rdf:Description xmlns:dc=\"http://purl.org/dc/elements/1.1/\" rdf:about=\"\">\n" \
			"<dc:title>%s</dc:title>\n" \
			"<dc:subject>%s</dc:subject>\n" \
			"<dc:creator>%s</dc:creator>\n" \
			"<dc:date>%s</dc:date>\n" \
			"</rdf:Description>\n" \
			"<rdf:Description xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" rdf:about=\"\">\n" \
			"<xmp:CreateDate>%s</xmp:CreateDate>\n" \
			"<xmp:CreatorTool>%s</xmp:CreatorTool>\n" \
			"<xmp:MetadataDate>%s</xmp:MetadataDate>\n" \
			"</rdf:Description>\n" \
			"<rdf:Description xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\" rdf:about=\"\">\n" \
			"<pdf:Keywords>%s</pdf:Keywords>\n" \
			"<pdf:PDFVersion>1.4</pdf:PDFVersion>\n" \
			"<pdf:Producer>%s</pdf:Producer>\n" \
			"</rdf:Description>\n" \
			"</rdf:RDF>\n" \
			"</x:xmpmeta>\n" \
			"<?xpacket end=\"r\"?>\n", \
			title, subject, creator, timestamp, timestamp, creator, timestamp, keywords, producer);

		/* Metadata Object with XMP */
		metadataObj = pPDF->pdfObj;
		Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
		nChars = sprintf(pTail, "%d 0 obj\n<</Type/Metadata /Subtype/XML /Length %d>>\nstream\n%sendstream\nendobj\n", \
			pPDF->pdfObj, nChars, XMPmetadata);
		free(XMPmetadata);

		pPDF->currentOffSet += nChars;	pTail += nChars;
		pPDF->pdfObj++;

		/* convert ISO9601 to PDF Info format %Y-%m-%dT%H:%M:%S%z -> %Y%m%d%H%M%S%z' */
		timestamp[4]  = timestamp[5];
		timestamp[5]  = timestamp[6];
		timestamp[6]  = timestamp[8];
		timestamp[7]  = timestamp[9];
		timestamp[8]  = timestamp[11];
		timestamp[9]  = timestamp[12];
		timestamp[10] = timestamp[14];
		timestamp[11] = timestamp[15];
		timestamp[12] = timestamp[17];
		timestamp[13] = timestamp[18];
		timestamp[14] = timestamp[19];
		timestamp[15] = timestamp[20];
		timestamp[16] = timestamp[21];
		timestamp[17] = '\'';
		timestamp[18] = timestamp[23];
		timestamp[19] = timestamp[24];
		timestamp[20] = '\'';
		timestamp[21] = '\0';

		/* Info Object */
		infoObj = pPDF->pdfObj;
		Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
		nChars = sprintf(pTail, "%d 0 obj\n<<\n/Title (%s)\n/Author (%s)\n/Keywords (%s)\n/Subject (%s)\n/Producer (%s)\n/Creator (%s)\n/CreationDate (D:%s)\n/ModDate (D:%s)\n>>\nendobj\n", \
			pPDF->pdfObj, title, author, keywords, subject, producer, creator, timestamp, timestamp);
		pPDF->currentOffSet += nChars;	pTail += nChars;
		pPDF->pdfObj++;
		
		/* Catalog Object. This is the Last Object */
		Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
		nChars = sprintf(pTail, "%d 0 obj\n<</Type/Catalog /Pages 1 0 R /Metadata %d 0 R>>\nendobj\n", pPDF->pdfObj, metadataObj);
		pPDF->currentOffSet += nChars;	pTail += nChars;
		
		/* Pages Object. It's always the Object 1 */
		Jpeg2PDF_SetXREF(pPDF, 1, pPDF->currentOffSet, 'n');
		
		strKids[0] = 0;
		pNode = pPDF->pFirstNode;
		while(pNode != NULL) {
			UINT8 curStr[9];
			sprintf(curStr, "%d 0 R ", pNode->PageObj);
			strcat(strKids, curStr);
			pNode = pNode->pNext;
		}
		
		if(strlen(strKids) > 1 && strKids[strlen(strKids) - 1] == ' ') strKids[strlen(strKids) - 1] = 0;
		
		nChars = sprintf(pTail, "1 0 obj\n<</Type/Pages /Kids [%s] /Count %d>>\nendobj\n", strKids, pPDF->nodeCount);
		pPDF->currentOffSet += nChars;	pTail += nChars;
		
		
		/* The xref & the rest of the tail */
		xrefOffSet = pPDF->currentOffSet;
		nChars = sprintf(pTail, "xref\n0 %d\n", pPDF->pdfObj+1);
		pPDF->currentOffSet += nChars;	pTail += nChars;
		
		for(i=0; i<=pPDF->pdfObj; i++) {
			 nChars = sprintf(pTail, "%s", pPDF->pdfXREF[i]);
			 pPDF->currentOffSet += nChars;	pTail += nChars;
		}
		
		nChars = sprintf(pTail, "trailer\n<</Root %d 0 R /Info %d 0 R /Size %d>>\n", pPDF->pdfObj, infoObj, pPDF->pdfObj+1);
		pPDF->currentOffSet += nChars;	pTail += nChars;
		
		nChars = sprintf(pTail, "startxref\n%d\n%%%%EOF\n", xrefOffSet);
		pPDF->currentOffSet += nChars;	pTail += nChars;
	}
	
	headerSize = strlen(pPDF->pdfHeader);
	tailerSize = strlen(pPDF->pdfTailer);
	if( headerSize && tailerSize && ( pPDF->currentOffSet > headerSize + tailerSize ) ) {
		pdfSize = pPDF->currentOffSet;
	}
	
	return pdfSize;
}

STATUS Jpeg2PDF_GetFinalDocumentAndCleanup(PJPEG2PDF pPDF, UINT8 *outPDF, UINT32 *outPDFSize) {
	STATUS result = ERROR;
	
	if(pPDF) {
		PJPEG2PDF_NODE pNode, pFreeCurrent;
		
		if(outPDF && (*outPDFSize >= pPDF->currentOffSet)) {
			UINT32 nBytes, nBytesOut = 0;
			UINT8 *pOut = outPDF;
			
			nBytes = strlen(pPDF->pdfHeader);
			memcpy(pOut, pPDF->pdfHeader, nBytes);
			nBytesOut += nBytes; pOut += nBytes;
			
			pNode = pPDF->pFirstNode;
			while(pNode != NULL) {
				nBytes = strlen(pNode->preFormat);
				memcpy(pOut, pNode->preFormat, nBytes);
				nBytesOut += nBytes; pOut += nBytes;
				
				nBytes = pNode->JpegSize;
				memcpy(pOut, pNode->pJpeg, nBytes);
				nBytesOut += nBytes; pOut += nBytes;
				
				nBytes = strlen(pNode->pstFormat);
				memcpy(pOut, pNode->pstFormat, nBytes);
				nBytesOut += nBytes; pOut += nBytes;
				
				pNode = pNode->pNext;
			}
			
			nBytes = strlen(pPDF->pdfTailer);
			memcpy(pOut, pPDF->pdfTailer, nBytes);
			nBytesOut += nBytes; pOut += nBytes;
			
			*outPDFSize = nBytesOut;
			result = OK;
		}
		
		pNode = pPDF->pFirstNode;
		while(pNode != NULL) {
			if(pNode->pJpeg) free(pNode->pJpeg);
			pFreeCurrent = pNode;
			pNode = pNode->pNext;
			free(pFreeCurrent);
		}
		
		if(pPDF) {
			free(pPDF);
			pPDF = NULL;
		}
	}
	
	return result;
}
