//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2011 Marti Maria Saguer
//
// Permission is hereby granted, free of charge, to any person obtaining 
// a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software 
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//---------------------------------------------------------------------------------
//

#include "lcms2_internal.h"

// Tag Serialization  ----------------------------------------------------------------------------- 
// This file implements every single tag and tag type as described in the ICC spec. Some types
// have been deprecated, like ncl and Data. There is no implementation for those types as there
// are no profiles holding them. The programmer can also extend this list by defining his own types
// by using the appropiate plug-in. There are three types of plug ins regarding that. First type
// allows to define new tags using any existing type. Next plug-in type allows to define new types
// and the third one is very specific: allows to extend the number of elements in the multiprofile
// elements special type. 
//--------------------------------------------------------------------------------------------------

// Some broken types
#define cmsCorbisBrokenXYZtype    ((cmsTagTypeSignature) 0x17A505B8)
#define cmsMonacoBrokenCurveType  ((cmsTagTypeSignature) 0x9478ee00)

// This is the linked list that keeps track of the defined types
typedef struct _cmsTagTypeLinkedList_st {

    cmsTagTypeHandler Handler;
    struct _cmsTagTypeLinkedList_st* Next;

} _cmsTagTypeLinkedList;

// Some macros to define callbacks.
#define READ_FN(x)  Type_##x##_Read
#define WRITE_FN(x) Type_##x##_Write
#define FREE_FN(x)  Type_##x##_Free
#define DUP_FN(x)   Type_##x##_Dup

// Helper macro to define a handler. Callbacks do have a fixed naming convention.
#define TYPE_HANDLER(t, x)  { (t), READ_FN(x), WRITE_FN(x), DUP_FN(x), FREE_FN(x) }

// Helper macro to define a MPE handler. Callbacks do have a fixed naming convention
#define TYPE_MPE_HANDLER(t, x)  { (t), READ_FN(x), WRITE_FN(x), GenericMPEdup, GenericMPEfree }

// Register a new type handler. This routine is shared between normal types and MPE
static
cmsBool RegisterTypesPlugin(cmsPluginBase* Data, _cmsTagTypeLinkedList* LinkedList, cmsUInt32Number DefaultListCount)
{
    cmsPluginTagType* Plugin = (cmsPluginTagType*) Data;
    _cmsTagTypeLinkedList *pt, *Anterior = NULL;

    // Calling the function with NULL as plug-in would unregister the plug in.
    if (Data == NULL) {

        LinkedList[DefaultListCount-1].Next = NULL;
        return TRUE;
    }

    pt = Anterior = LinkedList; 
    while (pt != NULL) {

        if (Plugin->Handler.Signature == pt -> Handler.Signature) {
            pt ->Handler = Plugin ->Handler;    // Replace old behaviour. 
            // Note that since no memory is allocated, unregister does not
            // reset this action. 
            return TRUE;
        }   

        Anterior = pt;          
        pt = pt ->Next;
    }

    // Registering happens in plug-in memory pool
    pt = (_cmsTagTypeLinkedList*) _cmsPluginMalloc(sizeof(_cmsTagTypeLinkedList));
    if (pt == NULL) return FALSE;

    pt ->Handler   = Plugin ->Handler;  
    pt ->Next      = NULL;

    if (Anterior) 
        Anterior -> Next = pt;

    return TRUE;
}

// Return handler for a given type or NULL if not found. Shared between normal types and MPE
static
cmsTagTypeHandler* GetHandler(cmsTagTypeSignature sig, _cmsTagTypeLinkedList* LinkedList)
{
    _cmsTagTypeLinkedList* pt;

    for (pt = LinkedList; 
         pt != NULL;
         pt = pt ->Next) {

            if (sig == pt -> Handler.Signature) return &pt ->Handler;
    }

    return NULL;
}


// Auxiliar to convert UTF-32 to UTF-16 in some cases
static
cmsBool _cmsWriteWCharArray(cmsIOHANDLER* io, cmsUInt32Number n, const wchar_t* Array)
{
    cmsUInt32Number i;

    _cmsAssert(io != NULL);
    _cmsAssert(Array != NULL);

    for (i=0; i < n; i++) {
        if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) Array[i])) return FALSE;
    }

    return TRUE;
}

static
cmsBool _cmsReadWCharArray(cmsIOHANDLER* io, cmsUInt32Number n, wchar_t* Array)
{
    cmsUInt32Number i;
    cmsUInt16Number tmp;

    _cmsAssert(io != NULL);

    for (i=0; i < n; i++) {

        if (Array != NULL) {

            if (!_cmsReadUInt16Number(io, &tmp)) return FALSE;
            Array[i] = (wchar_t) tmp;
        }
        else {
            if (!_cmsReadUInt16Number(io, NULL)) return FALSE;
        }

    }
    return TRUE;
}

// To deal with position tables
typedef cmsBool (* PositionTableEntryFn)(struct _cms_typehandler_struct* self, 
                                             cmsIOHANDLER* io,
                                             void* Cargo,
                                             cmsUInt32Number n, 
                                             cmsUInt32Number SizeOfTag);

// Helper function to deal with position tables as decribed in several addendums to ICC spec 4.2
// A table of n elements is written, where first comes n records containing offsets and sizes and
// then a block containing the data itself. This allows to reuse same data in more than one entry
static
cmsBool ReadPositionTable(struct _cms_typehandler_struct* self, 
                              cmsIOHANDLER* io, 
                              cmsUInt32Number Count, 
                              cmsUInt32Number BaseOffset, 
                              void *Cargo,
                              PositionTableEntryFn ElementFn)
{
    cmsUInt32Number i;
    cmsUInt32Number *ElementOffsets = NULL, *ElementSizes = NULL;

    // Let's take the offsets to each element
    ElementOffsets = (cmsUInt32Number *) _cmsCalloc(io ->ContextID, Count, sizeof(cmsUInt32Number *));
    if (ElementOffsets == NULL) goto Error;

    ElementSizes = (cmsUInt32Number *) _cmsCalloc(io ->ContextID, Count, sizeof(cmsUInt32Number *));
    if (ElementSizes == NULL) goto Error;

    for (i=0; i < Count; i++) {

        if (!_cmsReadUInt32Number(io, &ElementOffsets[i])) goto Error;
        if (!_cmsReadUInt32Number(io, &ElementSizes[i])) goto Error;

        ElementOffsets[i] += BaseOffset;
    }

    // Seek to each element and read it
    for (i=0; i < Count; i++) {

        if (!io -> Seek(io, ElementOffsets[i])) goto Error;

        // This is the reader callback
        if (!ElementFn(self, io, Cargo, i, ElementSizes[i])) goto Error;
    }

    // Success
    if (ElementOffsets != NULL) _cmsFree(io ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(io ->ContextID, ElementSizes);    
    return TRUE;

Error:    
    if (ElementOffsets != NULL) _cmsFree(io ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(io ->ContextID, ElementSizes);
    return FALSE;
}

// Same as anterior, but for write position tables
static
cmsBool WritePositionTable(struct _cms_typehandler_struct* self, 
                               cmsIOHANDLER* io,
                               cmsUInt32Number SizeOfTag,
                               cmsUInt32Number Count, 
                               cmsUInt32Number BaseOffset, 
                               void *Cargo,
                               PositionTableEntryFn ElementFn)
{
    cmsUInt32Number i;
    cmsUInt32Number DirectoryPos, CurrentPos, Before;
    cmsUInt32Number *ElementOffsets = NULL, *ElementSizes = NULL;

     // Create table
    ElementOffsets = (cmsUInt32Number *) _cmsCalloc(io ->ContextID, Count, sizeof(cmsUInt32Number *));
    if (ElementOffsets == NULL) goto Error;

    ElementSizes = (cmsUInt32Number *) _cmsCalloc(io ->ContextID, Count, sizeof(cmsUInt32Number *));
    if (ElementSizes == NULL) goto Error;
  
    // Keep starting position of curve offsets
    DirectoryPos = io ->Tell(io);

    // Write a fake directory to be filled latter on
    for (i=0; i < Count; i++) {

        if (!_cmsWriteUInt32Number(io, 0)) goto Error;  // Offset 
        if (!_cmsWriteUInt32Number(io, 0)) goto Error;  // size
    }

    // Write each element. Keep track of the size as well.
    for (i=0; i < Count; i++) {

        Before = io ->Tell(io);
        ElementOffsets[i] = Before - BaseOffset;                

        // Callback to write...
        if (!ElementFn(self, io, Cargo, i, SizeOfTag)) goto Error;

        // Now the size
        ElementSizes[i] = io ->Tell(io) - Before;       
    }

    // Write the directory
    CurrentPos = io ->Tell(io);
    if (!io ->Seek(io, DirectoryPos)) goto Error;

    for (i=0; i <  Count; i++) {
        if (!_cmsWriteUInt32Number(io, ElementOffsets[i])) goto Error;  
        if (!_cmsWriteUInt32Number(io, ElementSizes[i])) goto Error;  
    }

    if (!io ->Seek(io, CurrentPos)) goto Error;

    if (ElementOffsets != NULL) _cmsFree(io ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(io ->ContextID, ElementSizes);
    return TRUE;

Error:
    if (ElementOffsets != NULL) _cmsFree(io ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(io ->ContextID, ElementSizes);
    return FALSE;
}


// ********************************************************************************
// Type XYZ. Only one value is allowed
// ********************************************************************************

//The XYZType contains an array of three encoded values for the XYZ tristimulus
//values. Tristimulus values must be non-negative. The signed encoding allows for 
//implementation optimizations by minimizing the number of fixed formats.


static
void *Type_XYZ_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsCIEXYZ* xyz;

    *nItems = 0;
    xyz = (cmsCIEXYZ*) _cmsMallocZero(self ->ContextID, sizeof(cmsCIEXYZ));
    if (xyz == NULL) return NULL;

    if (!_cmsReadXYZNumber(io, xyz)) {
        _cmsFree(self ->ContextID, xyz);
        return NULL;
    }

    *nItems = 1;
    return (void*) xyz;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool  Type_XYZ_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    return _cmsWriteXYZNumber(io, (cmsCIEXYZ*) Ptr);

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_XYZ_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsCIEXYZ));

    cmsUNUSED_PARAMETER(n);
}

static
void Type_XYZ_Free(struct _cms_typehandler_struct* self, void *Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}


static
cmsTagTypeSignature DecideXYZtype(cmsFloat64Number ICCVersion, const void *Data)
{
    return cmsSigXYZType;

    cmsUNUSED_PARAMETER(ICCVersion);
    cmsUNUSED_PARAMETER(Data);
}


// ********************************************************************************
// Type chromaticity. Only one value is allowed
// ********************************************************************************
// The chromaticity tag type provides basic chromaticity data and type of 
// phosphors or colorants of a monitor to applications and utilities.

static
void *Type_Chromaticity_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsCIExyYTRIPLE* chrm;
    cmsUInt16Number nChans, Table;

    *nItems = 0;
    chrm =  (cmsCIExyYTRIPLE*) _cmsMallocZero(self ->ContextID, sizeof(cmsCIExyYTRIPLE));
    if (chrm == NULL) return NULL;

    if (!_cmsReadUInt16Number(io, &nChans)) goto Error;

    // Let's recover from a bug introduced in early versions of lcms1
    if (nChans == 0 && SizeOfTag == 32) {

        if (!_cmsReadUInt16Number(io, NULL)) goto Error;
        if (!_cmsReadUInt16Number(io, &nChans)) goto Error;
    }

    if (nChans != 3) goto Error;

    if (!_cmsReadUInt16Number(io, &Table)) goto Error;

    if (!_cmsRead15Fixed16Number(io, &chrm ->Red.x)) goto Error;
    if (!_cmsRead15Fixed16Number(io, &chrm ->Red.y)) goto Error;

    chrm ->Red.Y = 1.0;

    if (!_cmsRead15Fixed16Number(io, &chrm ->Green.x)) goto Error;
    if (!_cmsRead15Fixed16Number(io, &chrm ->Green.y)) goto Error;

    chrm ->Green.Y = 1.0;

    if (!_cmsRead15Fixed16Number(io, &chrm ->Blue.x)) goto Error;
    if (!_cmsRead15Fixed16Number(io, &chrm ->Blue.y)) goto Error;

    chrm ->Blue.Y = 1.0;

    *nItems = 1;
    return (void*) chrm;

Error:
    _cmsFree(self ->ContextID, (void*) chrm);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool  SaveOneChromaticity(cmsFloat64Number x, cmsFloat64Number y, cmsIOHANDLER* io)
{
    if (!_cmsWriteUInt32Number(io, _cmsDoubleTo15Fixed16(x))) return FALSE;     
    if (!_cmsWriteUInt32Number(io, _cmsDoubleTo15Fixed16(y))) return FALSE;     

    return TRUE;
}

static
cmsBool  Type_Chromaticity_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsCIExyYTRIPLE* chrm = (cmsCIExyYTRIPLE*) Ptr;

    if (!_cmsWriteUInt16Number(io, 3)) return FALSE;        // nChannels
    if (!_cmsWriteUInt16Number(io, 0)) return FALSE;        // Table

    if (!SaveOneChromaticity(chrm -> Red.x,   chrm -> Red.y, io)) return FALSE;
    if (!SaveOneChromaticity(chrm -> Green.x, chrm -> Green.y, io)) return FALSE;
    if (!SaveOneChromaticity(chrm -> Blue.x,  chrm -> Blue.y, io)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);  
}

static
void* Type_Chromaticity_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsCIExyYTRIPLE));
    cmsUNUSED_PARAMETER(n);
}

static
void Type_Chromaticity_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}


// ********************************************************************************
// Type cmsSigColorantOrderType
// ********************************************************************************

// This is an optional tag which specifies the laydown order in which colorants will 
// be printed on an n-colorant device. The laydown order may be the same as the 
// channel generation order listed in the colorantTableTag or the channel order of a 
// colour space such as CMYK, in which case this tag is not needed. When this is not 
// the case (for example, ink-towers sometimes use the order KCMY), this tag may be 
// used to specify the laydown order of the colorants. 


static
void *Type_ColorantOrderType_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt8Number* ColorantOrder;
    cmsUInt32Number Count;

    *nItems = 0;
    if (!_cmsReadUInt32Number(io, &Count)) return NULL;
    if (Count > cmsMAXCHANNELS) return NULL;

    ColorantOrder = (cmsUInt8Number*) _cmsCalloc(self ->ContextID, cmsMAXCHANNELS, sizeof(cmsUInt8Number));
    if (ColorantOrder == NULL) return NULL;

    // We use FF as end marker
    memset(ColorantOrder, 0xFF, cmsMAXCHANNELS * sizeof(cmsUInt8Number));

    if (io ->Read(io, ColorantOrder, sizeof(cmsUInt8Number), Count) != Count) {

        _cmsFree(self ->ContextID, (void*) ColorantOrder);
        return NULL;
    }

    *nItems = 1;
    return (void*) ColorantOrder;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool Type_ColorantOrderType_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsUInt8Number*  ColorantOrder = (cmsUInt8Number*) Ptr; 
    cmsUInt32Number i, sz, Count;

    // Get the length
    for (Count=i=0; i < cmsMAXCHANNELS; i++) {
        if (ColorantOrder[i] != 0xFF) Count++;
    }

    if (!_cmsWriteUInt32Number(io, Count)) return FALSE;    

    sz = Count * sizeof(cmsUInt8Number);
    if (!io -> Write(io, sz, ColorantOrder)) return FALSE;          

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_ColorantOrderType_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, cmsMAXCHANNELS * sizeof(cmsUInt8Number));

    cmsUNUSED_PARAMETER(n);
}


static
void Type_ColorantOrderType_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigS15Fixed16ArrayType
// ********************************************************************************
// This type represents an array of generic 4-byte/32-bit fixed point quantity. 
// The number of values is determined from the size of the tag.

static
void *Type_S15Fixed16_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsFloat64Number*  array_double;
    cmsUInt32Number i, n;

    *nItems = 0;
    n = SizeOfTag / sizeof(cmsUInt32Number);
    array_double = (cmsFloat64Number*) _cmsCalloc(self ->ContextID, n, sizeof(cmsFloat64Number));
    if (array_double == NULL) return NULL;

    for (i=0; i < n; i++) {

        if (!_cmsRead15Fixed16Number(io, &array_double[i])) {

            _cmsFree(self ->ContextID, array_double);
            return NULL;
        }       
    }

    *nItems = n;
    return (void*) array_double;
}

static
cmsBool Type_S15Fixed16_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsFloat64Number* Value = (cmsFloat64Number*) Ptr;
    cmsUInt32Number i;

    for (i=0; i < nItems; i++) {

        if (!_cmsWrite15Fixed16Number(io, Value[i])) return FALSE;  
    }

    return TRUE;

    cmsUNUSED_PARAMETER(self);
}

static
void* Type_S15Fixed16_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, n * sizeof(cmsFloat64Number));
}


static
void Type_S15Fixed16_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigU16Fixed16ArrayType
// ********************************************************************************
// This type represents an array of generic 4-byte/32-bit quantity. 
// The number of values is determined from the size of the tag.


static
void *Type_U16Fixed16_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsFloat64Number*  array_double;
    cmsUInt32Number v;
    cmsUInt32Number i, n;

    *nItems = 0;
    n = SizeOfTag / sizeof(cmsUInt32Number);
    array_double = (cmsFloat64Number*) _cmsCalloc(self ->ContextID, n, sizeof(cmsFloat64Number));
    if (array_double == NULL) return NULL;

    for (i=0; i < n; i++) {

        if (!_cmsReadUInt32Number(io, &v)) {
            _cmsFree(self ->ContextID, (void*) array_double);
            return NULL;
        }

        // Convert to cmsFloat64Number
        array_double[i] =  (cmsFloat64Number) (v / 65536.0);
    }

    *nItems = n;
    return (void*) array_double;
}

static
cmsBool Type_U16Fixed16_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsFloat64Number* Value = (cmsFloat64Number*) Ptr;
    cmsUInt32Number i;

    for (i=0; i < nItems; i++) {

        cmsUInt32Number v = (cmsUInt32Number) floor(Value[i]*65536.0 + 0.5);

        if (!_cmsWriteUInt32Number(io, v)) return FALSE;    
    }

    return TRUE;

    cmsUNUSED_PARAMETER(self);
}


static
void* Type_U16Fixed16_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, n * sizeof(cmsFloat64Number));
}

static
void Type_U16Fixed16_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigSignatureType
// ********************************************************************************
//
// The signatureType contains a four-byte sequence, Sequences of less than four 
// characters are padded at the end with spaces, 20h. 
// Typically this type is used for registered tags that can be displayed on many 
// development systems as a sequence of four characters.

static
void *Type_Signature_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsSignature* SigPtr = (cmsSignature*) _cmsMalloc(self ->ContextID, sizeof(cmsSignature));
    if (SigPtr == NULL) return NULL;

     if (!_cmsReadUInt32Number(io, SigPtr)) return NULL;
     *nItems = 1;

     return SigPtr;

     cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool  Type_Signature_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsSignature* SigPtr = (cmsSignature*) Ptr; 
    
    return _cmsWriteUInt32Number(io, *SigPtr);

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_Signature_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, n * sizeof(cmsSignature));
}

static
void Type_Signature_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}


// ********************************************************************************
// Type cmsSigTextType
// ********************************************************************************
//
// The textType is a simple text structure that contains a 7-bit ASCII text string. 
// The length of the string is obtained by subtracting 8 from the element size portion 
// of the tag itself. This string must be terminated with a 00h byte.

static
void *Type_Text_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    char* Text = NULL;
    cmsMLU* mlu = NULL;

    // Create a container
    mlu = cmsMLUalloc(self ->ContextID, 1);
    if (mlu == NULL) return NULL;

    *nItems = 0;

    // We need to store the "\0" at the end, so +1
    if (SizeOfTag == UINT_MAX) goto Error;

    Text = (char*) _cmsMalloc(self ->ContextID, SizeOfTag + 1);
    if (Text == NULL) goto Error;

    if (io -> Read(io, Text, sizeof(char), SizeOfTag) != SizeOfTag) goto Error;        

    // Make sure text is properly ended
    Text[SizeOfTag] = 0;
    *nItems = 1;
    
    // Keep the result
    if (!cmsMLUsetASCII(mlu, cmsNoLanguage, cmsNoCountry, Text)) goto Error;

    _cmsFree(self ->ContextID, Text);
    return (void*) mlu;

Error:
    if (mlu != NULL) 
        cmsMLUfree(mlu);
    if (Text != NULL)
        _cmsFree(self ->ContextID, Text);

    return NULL;
}

// The conversion implies to choose a language. So, we choose the actual language.
static
cmsBool Type_Text_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsMLU* mlu = (cmsMLU*) Ptr; 
    cmsUInt32Number size;
    cmsBool  rc;
    char* Text;
   
    // Get the size of the string. Note there is an extra "\0" at the end
    size = cmsMLUgetASCII(mlu, cmsNoLanguage, cmsNoCountry, NULL, 0);
    if (size == 0) return FALSE;       // Cannot be zero!

    // Create memory
    Text = (char*) _cmsMalloc(self ->ContextID, size);
    cmsMLUgetASCII(mlu, cmsNoLanguage, cmsNoCountry, Text, size);

    // Write it, including separator
    rc = io ->Write(io, size, Text);

    _cmsFree(self ->ContextID, Text);
    return rc;

    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_Text_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsMLUdup((cmsMLU*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}


static
void Type_Text_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    cmsMLU* mlu = (cmsMLU*) Ptr;
    cmsMLUfree(mlu);
    return;

    cmsUNUSED_PARAMETER(self);
}

static
cmsTagTypeSignature DecideTextType(cmsFloat64Number ICCVersion, const void *Data)
{
    if (ICCVersion >= 4.0) 
        return cmsSigMultiLocalizedUnicodeType;
        
    return cmsSigTextType;
    
    cmsUNUSED_PARAMETER(Data);
}


// ********************************************************************************
// Type cmsSigDataType
// ********************************************************************************

// General purpose data type
static
void *Type_Data_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsICCData* BinData;
    cmsUInt32Number LenOfData;

    *nItems = 0;
    
    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;

    LenOfData = SizeOfTag - sizeof(cmsUInt32Number);
    if (LenOfData > INT_MAX) return NULL;

    BinData = (cmsICCData*) _cmsMalloc(self ->ContextID, sizeof(cmsICCData) + LenOfData - 1);
    if (BinData == NULL) return NULL;

    BinData ->len = LenOfData;
    if (!_cmsReadUInt32Number(io, &BinData->flag)) {
        _cmsFree(self ->ContextID, BinData);
        return NULL;
    }

    if (io -> Read(io, BinData ->data, sizeof(cmsUInt8Number), LenOfData) != LenOfData) {
       
        _cmsFree(self ->ContextID, BinData);
        return NULL;
    }

    *nItems = 1;
  
    return (void*) BinData;
}


static
cmsBool Type_Data_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
   cmsICCData* BinData = (cmsICCData*) Ptr;
    
   if (!_cmsWriteUInt32Number(io, BinData ->flag)) return FALSE;

   return io ->Write(io, BinData ->len, BinData ->data);

   cmsUNUSED_PARAMETER(nItems);
   cmsUNUSED_PARAMETER(self);
}


static
void* Type_Data_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    cmsICCData* BinData = (cmsICCData*) Ptr;

    return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsICCData) + BinData ->len - 1);

    cmsUNUSED_PARAMETER(n);
}

static
void Type_Data_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigTextDescriptionType
// ********************************************************************************

static
void *Type_Text_Description_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    char* Text = NULL;
    cmsMLU* mlu = NULL;
    cmsUInt32Number  AsciiCount;
    cmsUInt32Number  i, UnicodeCode, UnicodeCount;
    cmsUInt16Number  ScriptCodeCode, Dummy;
    cmsUInt8Number   ScriptCodeCount;

    *nItems = 0;

    //  One dword should be there
    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;

    // Read len of ASCII
    if (!_cmsReadUInt32Number(io, &AsciiCount)) return NULL;
    SizeOfTag -= sizeof(cmsUInt32Number);

    // Check for size
    if (SizeOfTag < AsciiCount) return NULL; 

    // All seems Ok, allocate the container
    mlu = cmsMLUalloc(self ->ContextID, 1);
    if (mlu == NULL) return NULL;

    // As many memory as size of tag
    Text = (char*) _cmsMalloc(self ->ContextID, AsciiCount + 1);
    if (Text == NULL) goto Error;

    // Read it
    if (io ->Read(io, Text, sizeof(char), AsciiCount) != AsciiCount) goto Error;
    SizeOfTag -= AsciiCount;

    // Make sure there is a terminator
    Text[AsciiCount] = 0;

    // Set the MLU entry. From here we can be tolerant to wrong types
    if (!cmsMLUsetASCII(mlu, cmsNoLanguage, cmsNoCountry, Text)) goto Error;
    _cmsFree(self ->ContextID, (void*) Text);
    Text = NULL;
    
    // Skip Unicode code
    if (SizeOfTag < 2* sizeof(cmsUInt32Number)) goto Done;
    if (!_cmsReadUInt32Number(io, &UnicodeCode)) goto Done;                      
    if (!_cmsReadUInt32Number(io, &UnicodeCount)) goto Done;    
    SizeOfTag -= 2* sizeof(cmsUInt32Number);

    if (SizeOfTag < UnicodeCount*sizeof(cmsUInt16Number)) goto Done;

    for (i=0; i < UnicodeCount; i++) {
        if (!io ->Read(io, &Dummy, sizeof(cmsUInt16Number), 1)) goto Done;
    }
    SizeOfTag -= UnicodeCount*sizeof(cmsUInt16Number);

    // Skip ScriptCode code if present. Some buggy profiles does have less
    // data that stricttly required. We need to skip it as this type may come 
    // embedded in other types.
    
    if (SizeOfTag >= sizeof(cmsUInt16Number) + sizeof(cmsUInt8Number) + 67) {

        if (!_cmsReadUInt16Number(io, &ScriptCodeCode)) goto Done;
        if (!_cmsReadUInt8Number(io,  &ScriptCodeCount)) goto Done;                      

        // Skip rest of tag
        for (i=0; i < 67; i++) {
            if (!io ->Read(io, &Dummy, sizeof(cmsUInt8Number), 1)) goto Error;
        }
    }

Done:

    *nItems = 1;
    return mlu;

Error:
    if (Text) _cmsFree(self ->ContextID, (void*) Text);
    if (mlu) cmsMLUfree(mlu);
    return NULL;
}


// This tag can come IN UNALIGNED SIZE. In order to prevent issues, we force zeros on description to align it
static
cmsBool  Type_Text_Description_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsMLU* mlu = (cmsMLU*) Ptr;
    char *Text = NULL;
    wchar_t *Wide = NULL;
    cmsUInt32Number len, len_aligned, len_filler_alignment;
    cmsBool  rc = FALSE;
    char Filler[68];

    // Used below for writting zeroes
    memset(Filler, 0, sizeof(Filler));
    
    // Get the len of string
    len = cmsMLUgetASCII(mlu, cmsNoLanguage, cmsNoCountry, NULL, 0);
    
    // From ICC3.4: It has been found that textDescriptionType can contain misaligned data
    //(see clause 4.1 for the definition of �aligned�). Because the Unicode language
    // code and Unicode count immediately follow the ASCII description, their
    // alignment is not correct if the ASCII count is not a multiple of four. The
    // ScriptCode code is misaligned when the ASCII count is odd. Profile reading and
    // writing software must be written carefully in order to handle these alignment
    // problems. 

    // Compute an aligned size
    len_aligned = _cmsALIGNLONG(len);
    len_filler_alignment = len_aligned - len;

    // Null strings
    if (len <= 0) {

        Text = (char*)    _cmsDupMem(self ->ContextID, "", sizeof(char));
        Wide = (wchar_t*) _cmsDupMem(self ->ContextID, L"", sizeof(wchar_t));
    }
    else {
        // Create independent buffers
        Text = (char*) _cmsCalloc(self ->ContextID, len, sizeof(char));
        if (Text == NULL) goto Error;

        Wide = (wchar_t*) _cmsCalloc(self ->ContextID, len, sizeof(wchar_t));
        if (Wide == NULL) goto Error;

        // Get both representations. 
        cmsMLUgetASCII(mlu, cmsNoLanguage, cmsNoCountry,  Text, len * sizeof(char));
        cmsMLUgetWide(mlu,  cmsNoLanguage, cmsNoCountry,  Wide, len * sizeof(wchar_t));
    }
          
  // * cmsUInt32Number       count;          * Description length
  // * cmsInt8Number         desc[count]     * NULL terminated ascii string
  // * cmsUInt32Number       ucLangCode;     * UniCode language code
  // * cmsUInt32Number       ucCount;        * UniCode description length
  // * cmsInt16Number        ucDesc[ucCount];* The UniCode description
  // * cmsUInt16Number       scCode;         * ScriptCode code
  // * cmsUInt8Number        scCount;        * ScriptCode count
  // * cmsInt8Number         scDesc[67];     * ScriptCode Description
    
    if (!_cmsWriteUInt32Number(io, len_aligned)) goto Error;        
    if (!io ->Write(io, len, Text)) goto Error;    
    if (!io ->Write(io, len_filler_alignment, Filler)) goto Error;    

    if (!_cmsWriteUInt32Number(io, 0)) goto Error;  // ucLanguageCode
    
    // This part is tricky: we need an aligned tag size, and the ScriptCode part
    // takes 70 bytes, so we need 2 extra bytes to do the alignment

    if (!_cmsWriteUInt32Number(io, len_aligned+1)) goto Error;  

    // Note that in some compilers sizeof(cmsUInt16Number) != sizeof(wchar_t)
    if (!_cmsWriteWCharArray(io, len, Wide)) goto Error;            
    if (!_cmsWriteUInt16Array(io, len_filler_alignment+1, (cmsUInt16Number*) Filler)) goto Error;   

    // ScriptCode Code & count (unused)
    if (!_cmsWriteUInt16Number(io, 0)) goto Error;  
    if (!_cmsWriteUInt8Number(io, 0)) goto Error;   
        
    if (!io ->Write(io, 67, Filler)) goto Error;

    rc = TRUE;

Error:
    if (Text) _cmsFree(self ->ContextID, Text);
    if (Wide) _cmsFree(self ->ContextID, Wide);

    return rc;

    cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_Text_Description_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsMLUdup((cmsMLU*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_Text_Description_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    cmsMLU* mlu = (cmsMLU*) Ptr;

    cmsMLUfree(mlu);
    return;

    cmsUNUSED_PARAMETER(self);
}


static
cmsTagTypeSignature DecideTextDescType(cmsFloat64Number ICCVersion, const void *Data)
{
    if (ICCVersion >= 4.0) 
        return cmsSigMultiLocalizedUnicodeType;
        
    return cmsSigTextDescriptionType;
    
    cmsUNUSED_PARAMETER(Data);
}


// ********************************************************************************
// Type cmsSigCurveType
// ********************************************************************************

static
void *Type_Curve_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt32Number Count;
    cmsToneCurve* NewGamma;
    cmsUInt16Number Linear[2] = { 0, 0xffff };


    *nItems = 0;
    if (!_cmsReadUInt32Number(io, &Count)) return NULL;

    switch (Count) {

           case 0:   // Linear.

               NewGamma = cmsBuildTabulatedToneCurve16(self ->ContextID, 2, Linear);               
               if (!NewGamma) return NULL;
               *nItems = 1;
               return NewGamma;

           case 1:  // Specified as the exponent of gamma function
               {
                   cmsUInt16Number SingleGammaFixed;
                   cmsFloat64Number SingleGamma;

                   if (!_cmsReadUInt16Number(io, &SingleGammaFixed)) return NULL;
                   SingleGamma = _cms8Fixed8toDouble(SingleGammaFixed);

                   *nItems = 1;
                   return cmsBuildParametricToneCurve(self ->ContextID, 1, &SingleGamma);
               }

           default:  // Curve

               NewGamma = cmsBuildTabulatedToneCurve16(self ->ContextID, Count, NULL);
               if (!NewGamma) return NULL;

               if (!_cmsReadUInt16Array(io, Count, NewGamma -> Table16)) return NULL;         

               *nItems = 1;
               return NewGamma;
    }

    cmsUNUSED_PARAMETER(SizeOfTag);
}


static
cmsBool  Type_Curve_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsToneCurve* Curve = (cmsToneCurve*) Ptr;

    if (Curve ->nSegments == 1 && Curve ->Segments[0].Type == 1) {

            // Single gamma, preserve number
            cmsUInt16Number SingleGammaFixed = _cmsDoubleTo8Fixed8(Curve ->Segments[0].Params[0]);

            if (!_cmsWriteUInt32Number(io, 1)) return FALSE;    
            if (!_cmsWriteUInt16Number(io, SingleGammaFixed)) return FALSE;
            return TRUE;
        
    }

    if (!_cmsWriteUInt32Number(io, Curve ->nEntries)) return FALSE; 
    return _cmsWriteUInt16Array(io, Curve ->nEntries, Curve ->Table16);

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}


static
void* Type_Curve_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsDupToneCurve((cmsToneCurve*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_Curve_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    cmsToneCurve* gamma = (cmsToneCurve*) Ptr;

    cmsFreeToneCurve(gamma);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigParametricCurveType
// ********************************************************************************


// Decide which curve type to use on writting
static
cmsTagTypeSignature DecideCurveType(cmsFloat64Number ICCVersion, const void *Data)
{
    cmsToneCurve* Curve = (cmsToneCurve*) Data;

    if (ICCVersion < 4.0) return cmsSigCurveType;
    if (Curve ->nSegments != 1) return cmsSigCurveType;          // Only 1-segment curves can be saved as parametric
    if (Curve ->Segments[0].Type < 0) return cmsSigCurveType;    // Only non-inverted curves

    return cmsSigParametricCurveType;
}

static
void *Type_ParametricCurve_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    static const int ParamsByType[] = { 1, 3, 4, 5, 7 };
    cmsFloat64Number Params[10];
    cmsUInt16Number Type;
    int i, n;
    cmsToneCurve* NewGamma;

    if (!_cmsReadUInt16Number(io, &Type)) return NULL;
    if (!_cmsReadUInt16Number(io, NULL)) return NULL;   // Reserved

    if (Type > 4) {

        cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown parametric curve type '%d'", Type);
        return NULL;
    }

    memset(Params, 0, sizeof(Params));
    n = ParamsByType[Type];

    for (i=0; i < n; i++) {

        if (!_cmsRead15Fixed16Number(io, &Params[i])) return NULL;      
    }

    NewGamma = cmsBuildParametricToneCurve(self ->ContextID, Type+1, Params);

    *nItems = 1;
    return NewGamma;

    cmsUNUSED_PARAMETER(SizeOfTag);
}
    

static
cmsBool  Type_ParametricCurve_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsToneCurve* Curve = (cmsToneCurve*) Ptr;
    int i, nParams;
    static const int ParamsByType[] = { 0, 1, 3, 4, 5, 7 };
      

    if (Curve ->nSegments > 1 || Curve -> Segments[0].Type < 1) {

        cmsSignalError(self->ContextID, 0, "Multisegment or Inverted parametric curves cannot be written");          
        return FALSE;
    }

    nParams = ParamsByType[Curve ->Segments[0].Type];
    
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) (Curve ->Segments[0].Type - 1))) return FALSE;
    if (!_cmsWriteUInt16Number(io, 0)) return FALSE;        // Reserved

    for (i=0; i < nParams; i++) {

        if (!_cmsWrite15Fixed16Number(io, Curve -> Segments[0].Params[i])) return FALSE;        
    }
    
    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_ParametricCurve_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsDupToneCurve((cmsToneCurve*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_ParametricCurve_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    cmsToneCurve* gamma = (cmsToneCurve*) Ptr;

    cmsFreeToneCurve(gamma);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigDateTimeType
// ********************************************************************************

// A 12-byte value representation of the time and date, where the byte usage is assigned 
// as specified in table 1. The actual values are encoded as 16-bit unsigned integers 
// (uInt16Number - see 5.1.6).
//
// All the dateTimeNumber values in a profile shall be in Coordinated Universal Time 
// (UTC, also known as GMT or ZULU Time). Profile writers are required to convert local
// time to UTC when setting these values. Programmes that display these values may show 
// the dateTimeNumber as UTC, show the equivalent local time (at current locale), or 
// display both UTC and local versions of the dateTimeNumber.

static
void *Type_DateTime_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsDateTimeNumber timestamp;
    struct tm * NewDateTime;

    *nItems = 0;
    NewDateTime = (struct tm*) _cmsMalloc(self ->ContextID, sizeof(struct tm));
    if (NewDateTime == NULL) return NULL;

    if (io->Read(io, &timestamp, sizeof(cmsDateTimeNumber), 1) != 1) return NULL;
            
     _cmsDecodeDateTimeNumber(&timestamp, NewDateTime);

     *nItems = 1;
     return NewDateTime;

     cmsUNUSED_PARAMETER(SizeOfTag);
}


static
cmsBool  Type_DateTime_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    struct tm * DateTime = (struct tm*) Ptr;
    cmsDateTimeNumber timestamp;

    _cmsEncodeDateTimeNumber(&timestamp, DateTime);
    if (!io ->Write(io, sizeof(cmsDateTimeNumber), &timestamp)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_DateTime_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(self ->ContextID, Ptr, sizeof(struct tm));

    cmsUNUSED_PARAMETER(n);
}

static
void Type_DateTime_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    _cmsFree(self ->ContextID, Ptr);
}



// ********************************************************************************
// Type icMeasurementType
// ********************************************************************************

/*
The measurementType information refers only to the internal profile data and is
meant to provide profile makers an alternative to the default measurement 
specifications.
*/

static
void *Type_Measurement_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsICCMeasurementConditions mc;

    if (!_cmsReadUInt32Number(io, &mc.Observer)) return NULL;
    if (!_cmsReadXYZNumber(io,    &mc.Backing)) return NULL;
    if (!_cmsReadUInt32Number(io, &mc.Geometry)) return NULL;
    if (!_cmsRead15Fixed16Number(io, &mc.Flare)) return NULL;
    if (!_cmsReadUInt32Number(io, &mc.IlluminantType)) return NULL;

    *nItems = 1;
    return _cmsDupMem(self ->ContextID, &mc, sizeof(cmsICCMeasurementConditions));

    cmsUNUSED_PARAMETER(SizeOfTag);
}


static
cmsBool  Type_Measurement_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsICCMeasurementConditions* mc =(cmsICCMeasurementConditions*) Ptr;
    
    if (!_cmsWriteUInt32Number(io, mc->Observer)) return FALSE;
    if (!_cmsWriteXYZNumber(io,    &mc->Backing)) return FALSE;
    if (!_cmsWriteUInt32Number(io, mc->Geometry)) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, mc->Flare)) return FALSE;
    if (!_cmsWriteUInt32Number(io, mc->IlluminantType)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_Measurement_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
     return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsICCMeasurementConditions));

     cmsUNUSED_PARAMETER(n);
}

static
void Type_Measurement_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
   _cmsFree(self ->ContextID, Ptr);
}


// ********************************************************************************
// Type cmsSigMultiLocalizedUnicodeType
// ********************************************************************************
//
//   Do NOT trust SizeOfTag as there is an issue on the definition of profileSequenceDescTag. See the TechNote from 
//   Max Derhak and Rohit Patil about this: basically the size of the string table should be guessed and cannot be
//   taken from the size of tag if this tag is embedded as part of bigger structures (profileSequenceDescTag, for instance)
//

static
void *Type_MLU_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsMLU* mlu;
    cmsUInt32Number Count, RecLen, NumOfWchar;       
    cmsUInt32Number SizeOfHeader;
    cmsUInt32Number  Len, Offset;
    cmsUInt32Number  i;
    wchar_t*         Block;
    cmsUInt32Number  BeginOfThisString, EndOfThisString, LargestPosition;

    *nItems = 0;
    if (!_cmsReadUInt32Number(io, &Count)) return NULL;           
    if (!_cmsReadUInt32Number(io, &RecLen)) return NULL;           

    if (RecLen != 12) {

        cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "multiLocalizedUnicodeType of len != 12 is not supported.");
        return NULL;
    }

    mlu = cmsMLUalloc(self ->ContextID, Count);
    if (mlu == NULL) return NULL;

    mlu ->UsedEntries = Count;

    SizeOfHeader = 12 * Count + sizeof(_cmsTagBase);
    LargestPosition = 0;

    for (i=0; i < Count; i++) {

        if (!_cmsReadUInt16Number(io, &mlu ->Entries[i].Language)) goto Error;           
        if (!_cmsReadUInt16Number(io, &mlu ->Entries[i].Country))  goto Error;           

        // Now deal with Len and offset.
        if (!_cmsReadUInt32Number(io, &Len)) goto Error;      
        if (!_cmsReadUInt32Number(io, &Offset)) goto Error;

        // Check for overflow
        if (Offset < (SizeOfHeader + 8)) goto Error;

        // True begin of the string
        BeginOfThisString = Offset - SizeOfHeader - 8;
        
        // Ajust to wchar_t elements
        mlu ->Entries[i].Len = (Len * sizeof(wchar_t)) / sizeof(cmsUInt16Number);        
        mlu ->Entries[i].StrW = (BeginOfThisString * sizeof(wchar_t)) / sizeof(cmsUInt16Number);

        // To guess maximum size, add offset + len
        EndOfThisString = BeginOfThisString + Len;
        if (EndOfThisString > LargestPosition)
            LargestPosition = EndOfThisString;
    }

    // Now read the remaining of tag and fill all strings. Substract the directory
    SizeOfTag   = (LargestPosition * sizeof(wchar_t)) / sizeof(cmsUInt16Number);

    Block = (wchar_t*) _cmsMalloc(self ->ContextID, SizeOfTag);
    if (Block == NULL) goto Error;

    NumOfWchar = SizeOfTag / sizeof(wchar_t);

    if (!_cmsReadWCharArray(io, NumOfWchar, Block)) goto Error;

    mlu ->MemPool  = Block;
    mlu ->PoolSize = SizeOfTag;
    mlu ->PoolUsed = SizeOfTag;

    *nItems = 1;
    return (void*) mlu;

Error:          
    if (mlu) cmsMLUfree(mlu);
    return NULL;                                                
}

static
cmsBool  Type_MLU_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsMLU* mlu =(cmsMLU*) Ptr;
    cmsUInt32Number HeaderSize;
    cmsUInt32Number  Len, Offset;
    int i;

    if (Ptr == NULL) {

          // Empty placeholder
          if (!_cmsWriteUInt32Number(io, 0)) return FALSE;           
          if (!_cmsWriteUInt32Number(io, 12)) return FALSE;           
          return TRUE;
    }
    
    if (!_cmsWriteUInt32Number(io, mlu ->UsedEntries)) return FALSE;           
    if (!_cmsWriteUInt32Number(io, 12)) return FALSE;           
          
    HeaderSize = 12 * mlu ->UsedEntries + sizeof(_cmsTagBase);

    for (i=0; i < mlu ->UsedEntries; i++) {

        Len    =  mlu ->Entries[i].Len;
        Offset =  mlu ->Entries[i].StrW;
        
        Len    = (Len * sizeof(cmsUInt16Number)) / sizeof(wchar_t);
        Offset = (Offset * sizeof(cmsUInt16Number)) / sizeof(wchar_t) + HeaderSize + 8;

        if (!_cmsWriteUInt16Number(io, mlu ->Entries[i].Language)) return FALSE;           
        if (!_cmsWriteUInt16Number(io, mlu ->Entries[i].Country))  return FALSE;  
        if (!_cmsWriteUInt32Number(io, Len)) return FALSE;
        if (!_cmsWriteUInt32Number(io, Offset)) return FALSE;               
    }

    if (!_cmsWriteWCharArray(io, mlu ->PoolUsed / sizeof(wchar_t), (wchar_t*)  mlu ->MemPool)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}


static
void* Type_MLU_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsMLUdup((cmsMLU*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_MLU_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsMLUfree((cmsMLU*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigLut8Type
// ********************************************************************************

// Decide which LUT type to use on writting
static
cmsTagTypeSignature DecideLUTtypeA2B(cmsFloat64Number ICCVersion, const void *Data)
{
    cmsPipeline* Lut = (cmsPipeline*) Data;

    if (ICCVersion < 4.0) {
        if (Lut ->SaveAs8Bits) return cmsSigLut8Type;
        return cmsSigLut16Type;
    }
    else {
         return cmsSigLutAtoBType;
    }
}

static
cmsTagTypeSignature DecideLUTtypeB2A(cmsFloat64Number ICCVersion, const void *Data)
{
    cmsPipeline* Lut = (cmsPipeline*) Data;

    if (ICCVersion < 4.0) {
        if (Lut ->SaveAs8Bits) return cmsSigLut8Type;
        return cmsSigLut16Type;
    }
    else {
         return cmsSigLutBtoAType;
    }
}

/*
This structure represents a colour transform using tables of 8-bit precision. 
This type contains four processing elements: a 3 by 3 matrix (which shall be 
the identity matrix unless the input colour space is XYZ), a set of one dimensional 
input tables, a multidimensional lookup table, and a set of one dimensional output 
tables. Data is processed using these elements via the following sequence:
(matrix) -> (1d input tables)  -> (multidimensional lookup table - CLUT) -> (1d output tables)

Byte Position   Field Length (bytes)  Content Encoded as...
8                  1          Number of Input Channels (i)    uInt8Number
9                  1          Number of Output Channels (o)   uInt8Number
10                 1          Number of CLUT grid points (identical for each side) (g) uInt8Number
11                 1          Reserved for padding (fill with 00h)

12..15             4          Encoded e00 parameter   s15Fixed16Number
*/


// Read 8 bit tables as gamma functions
static 
cmsBool  Read8bitTables(cmsContext ContextID, cmsIOHANDLER* io, cmsPipeline* lut, int nChannels)
{
    cmsStage* mpe;
    cmsUInt8Number* Temp = NULL;
    int i, j;
    cmsToneCurve* Tables[cmsMAXCHANNELS];

    if (nChannels > cmsMAXCHANNELS) return FALSE;
    if (nChannels <= 0) return FALSE;
        
    memset(Tables, 0, sizeof(Tables));

    Temp = (cmsUInt8Number*) _cmsMalloc(ContextID, 256);
    if (Temp == NULL) return FALSE;

    for (i=0; i < nChannels; i++) {
        Tables[i] = cmsBuildTabulatedToneCurve16(ContextID, 256, NULL);
        if (Tables[i] == NULL) goto Error;
    }

    for (i=0; i < nChannels; i++) {

        if (io ->Read(io, Temp, 256, 1) != 1) goto Error;

        for (j=0; j < 256; j++)
            Tables[i]->Table16[j] = (cmsUInt16Number) FROM_8_TO_16(Temp[j]);
    }

    _cmsFree(ContextID, Temp); 
    Temp = NULL;


    mpe = cmsStageAllocToneCurves(ContextID, nChannels, Tables);
    if (mpe == NULL) goto Error;

    cmsPipelineInsertStage(lut, cmsAT_END, mpe);

    for (i=0; i < nChannels; i++) 
        cmsFreeToneCurve(Tables[i]);

    return TRUE;

Error:
    for (i=0; i < nChannels; i++) {
        if (Tables[i]) cmsFreeToneCurve(Tables[i]);
    }

    if (Temp) _cmsFree(ContextID, Temp);
    return FALSE;       
}


static
cmsBool Write8bitTables(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt32Number n, _cmsStageToneCurvesData* Tables)
{
    int j;
    cmsUInt32Number i;
    cmsUInt8Number val;

    for (i=0; i < n; i++) {

        if (Tables) {

            if (Tables ->TheCurves[i]->nEntries != 256) {
                cmsSignalError(ContextID, cmsERROR_RANGE, "LUT8 needs 256 entries on prelinearization");
                return FALSE;
            }

        }

        for (j=0; j < 256; j++) {

            if (Tables != NULL)
                val = (cmsUInt8Number) FROM_16_TO_8(Tables->TheCurves[i]->Table16[j]);
            else
                val = (cmsUInt8Number) j;

            if (!_cmsWriteUInt8Number(io, val)) return FALSE;
        }
    }
    return TRUE;
}


// Check overflow
static
unsigned int uipow(cmsUInt32Number n, cmsUInt32Number a, cmsUInt32Number b) 
{
    cmsUInt32Number rv = 1, rc;

    if (a == 0) return 0;
    if (n == 0) return 0;

    for (; b > 0; b--) {
        
        rv *= a;

        // Check for overflow
        if (rv > UINT_MAX / a) return 0;

    }
    
    rc = rv * n;

    if (rv != rc / n) return 0;
    return rc;
}


// That will create a MPE LUT with Matrix, pre tables, CLUT and post tables. 
// 8 bit lut may be scaled easely to v4 PCS, but we need also to properly adjust
// PCS on BToAxx tags and AtoB if abstract. We need to fix input direction.

static
void *Type_LUT8_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt8Number InputChannels, OutputChannels, CLUTpoints;
    cmsUInt8Number* Temp = NULL;
    cmsPipeline* NewLUT = NULL;
    cmsStage *mpemat, *mpeclut;
    cmsUInt32Number nTabSize, i;    
    cmsFloat64Number Matrix[3*3];

    *nItems = 0;
 
    if (!_cmsReadUInt8Number(io, &InputChannels)) goto Error;
    if (!_cmsReadUInt8Number(io, &OutputChannels)) goto Error;
    if (!_cmsReadUInt8Number(io, &CLUTpoints)) goto Error;

    // Padding
    if (!_cmsReadUInt8Number(io, NULL)) goto Error;

    // Do some checking
   
    if (InputChannels > cmsMAXCHANNELS)  goto Error;
    if (OutputChannels > cmsMAXCHANNELS) goto Error;

   // Allocates an empty Pipeline
    NewLUT = cmsPipelineAlloc(self ->ContextID, InputChannels, OutputChannels);
    if (NewLUT == NULL) goto Error;

    // Read the Matrix
    if (!_cmsRead15Fixed16Number(io,  &Matrix[0])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[1])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[2])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[3])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[4])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[5])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[6])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[7])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[8])) goto Error;

  
    // Only operates if not identity...
    if ((InputChannels == 3) && !_cmsMAT3isIdentity((cmsMAT3*) Matrix)) {

        mpemat = cmsStageAllocMatrix(self ->ContextID, 3, 3, Matrix, NULL);
        if (mpemat == NULL) goto Error;
        cmsPipelineInsertStage(NewLUT, cmsAT_BEGIN, mpemat);
    }
    
    // Get input tables
    if (!Read8bitTables(self ->ContextID, io,  NewLUT, InputChannels)) goto Error;
    
    // Get 3D CLUT. Check the overflow....
    nTabSize = uipow(OutputChannels, CLUTpoints, InputChannels);
    if (nTabSize > 0) {

        cmsUInt16Number *PtrW, *T;
        cmsUInt32Number Tsize;

        Tsize = (cmsUInt32Number) nTabSize * sizeof(cmsUInt16Number);

        PtrW = T  = (cmsUInt16Number*) _cmsCalloc(self ->ContextID, nTabSize, sizeof(cmsUInt16Number));
        if (T  == NULL) goto Error;

        Temp = (cmsUInt8Number*) _cmsMalloc(self ->ContextID, nTabSize);             
        if (Temp == NULL) goto Error;

        if (io ->Read(io, Temp, nTabSize, 1) != 1) goto Error;

        for (i = 0; i < nTabSize; i++) {

            *PtrW++ = FROM_8_TO_16(Temp[i]);
        }
        _cmsFree(self ->ContextID, Temp);
        Temp = NULL;

        
        mpeclut = cmsStageAllocCLut16bit(self ->ContextID, CLUTpoints, InputChannels, OutputChannels, T);
        if (mpeclut == NULL) goto Error;
        cmsPipelineInsertStage(NewLUT, cmsAT_END, mpeclut);
        _cmsFree(self ->ContextID, T);
    }


    // Get output tables
    if (!Read8bitTables(self ->ContextID, io,  NewLUT, OutputChannels)) goto Error;
    
    *nItems = 1;
    return NewLUT;

Error:
    if (NewLUT != NULL) cmsPipelineFree(NewLUT);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

// We only allow a specific MPE structure: Matrix plus prelin, plus clut, plus post-lin.
static
cmsBool  Type_LUT8_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsUInt32Number j, nTabSize;
    cmsUInt8Number  val;
    cmsPipeline* NewLUT = (cmsPipeline*) Ptr;
    cmsStage* mpe;
    _cmsStageToneCurvesData* PreMPE = NULL, *PostMPE = NULL;
    _cmsStageMatrixData* MatMPE = NULL;
    _cmsStageCLutData* clut = NULL;
    int clutPoints;

    // Disassemble the LUT into components.
    mpe = NewLUT -> Elements;
    if (mpe ->Type == cmsSigMatrixElemType) {

        MatMPE = (_cmsStageMatrixData*) mpe ->Data;
        mpe = mpe -> Next;
    }

    if (mpe != NULL && mpe ->Type == cmsSigCurveSetElemType) {
        PreMPE = (_cmsStageToneCurvesData*) mpe ->Data;
        mpe = mpe -> Next;
    }

    if (mpe != NULL && mpe ->Type == cmsSigCLutElemType) {
        clut  = (_cmsStageCLutData*) mpe -> Data;
        mpe = mpe ->Next;
    }

    if (mpe != NULL && mpe ->Type == cmsSigCurveSetElemType) {
        PostMPE = (_cmsStageToneCurvesData*) mpe ->Data;
        mpe = mpe -> Next;
    }

    // That should be all
    if (mpe != NULL) {
        cmsSignalError(mpe->ContextID, cmsERROR_UNKNOWN_EXTENSION, "LUT is not suitable to be saved as LUT8");
        return FALSE;
    }


    if (clut == NULL)
        clutPoints = 0;
    else
        clutPoints    = clut->Params->nSamples[0];

    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) NewLUT ->InputChannels)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) NewLUT ->OutputChannels)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) clutPoints)) return FALSE;
    if (!_cmsWriteUInt8Number(io, 0)) return FALSE; // Padding


    if (MatMPE != NULL) {

        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[0])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[1])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[2])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[3])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[4])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[5])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[6])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[7])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[8])) return FALSE;

    }
    else {

        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
    }

    // The prelinearization table
    if (!Write8bitTables(self ->ContextID, io, NewLUT ->InputChannels, PreMPE)) return FALSE;

    nTabSize = uipow(NewLUT->OutputChannels, clutPoints, NewLUT ->InputChannels);
    if (nTabSize > 0) {

        // The 3D CLUT.
        if (clut != NULL) {

            for (j=0; j < nTabSize; j++) {

                val = (cmsUInt8Number) FROM_16_TO_8(clut ->Tab.T[j]);
                if (!_cmsWriteUInt8Number(io, val)) return FALSE;
            }
        }
    }

    // The postlinearization table
    if (!Write8bitTables(self ->ContextID, io, NewLUT ->OutputChannels, PostMPE)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_LUT8_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsPipelineDup((cmsPipeline*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_LUT8_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsPipelineFree((cmsPipeline*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}

// ********************************************************************************
// Type cmsSigLut16Type
// ********************************************************************************

// Read 16 bit tables as gamma functions
static 
cmsBool  Read16bitTables(cmsContext ContextID, cmsIOHANDLER* io, cmsPipeline* lut, int nChannels, int nEntries)
{
    cmsStage* mpe;
    int i;
    cmsToneCurve* Tables[cmsMAXCHANNELS];

    // Maybe an empty table? (this is a lcms extension)
    if (nEntries <= 0) return TRUE;

    // Check for malicious profiles
    if (nChannels > cmsMAXCHANNELS) return FALSE;
    
    // Init table to zero
    memset(Tables, 0, sizeof(Tables));

    for (i=0; i < nChannels; i++) {

        Tables[i] = cmsBuildTabulatedToneCurve16(ContextID, nEntries, NULL);
        if (Tables[i] == NULL) goto Error;

        if (!_cmsReadUInt16Array(io, nEntries, Tables[i]->Table16)) goto Error;
    }


    // Add the table (which may certainly be an identity, but this is up to the optimizer, not the reading code)
    mpe = cmsStageAllocToneCurves(ContextID, nChannels, Tables);
    if (mpe == NULL) goto Error;

    cmsPipelineInsertStage(lut, cmsAT_END, mpe);

    for (i=0; i < nChannels; i++) 
        cmsFreeToneCurve(Tables[i]);

    return TRUE;

Error:
    for (i=0; i < nChannels; i++) {
        if (Tables[i]) cmsFreeToneCurve(Tables[i]);
    }

    return FALSE;       
}

static
cmsBool Write16bitTables(cmsContext ContextID, cmsIOHANDLER* io, _cmsStageToneCurvesData* Tables)
{
    int j;
    cmsUInt32Number i;
    cmsUInt16Number val;
    int nEntries = 256;

    nEntries = Tables->TheCurves[0]->nEntries;
    
    for (i=0; i < Tables ->nCurves; i++) {

        for (j=0; j < nEntries; j++) {

            if (Tables != NULL)
                val = Tables->TheCurves[i]->Table16[j];
            else
                val = _cmsQuantizeVal(j, nEntries);

            if (!_cmsWriteUInt16Number(io, val)) return FALSE;
        }
    }
    return TRUE;

    cmsUNUSED_PARAMETER(ContextID);
}

static
void *Type_LUT16_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt8Number InputChannels, OutputChannels, CLUTpoints;
    cmsPipeline* NewLUT = NULL;
    cmsStage *mpemat,  *mpeclut;
    cmsUInt32Number nTabSize;
    cmsFloat64Number Matrix[3*3];
    cmsUInt16Number InputEntries, OutputEntries;
    
    *nItems = 0;

    if (!_cmsReadUInt8Number(io, &InputChannels)) return NULL;
    if (!_cmsReadUInt8Number(io, &OutputChannels)) return NULL;
    if (!_cmsReadUInt8Number(io, &CLUTpoints)) return NULL;   // 255 maximum
    
    // Padding
    if (!_cmsReadUInt8Number(io, NULL)) return NULL;

    // Do some checking
    if (InputChannels > cmsMAXCHANNELS)  goto Error;
    if (OutputChannels > cmsMAXCHANNELS) goto Error;

    // Allocates an empty LUT
    NewLUT = cmsPipelineAlloc(self ->ContextID, InputChannels, OutputChannels);
    if (NewLUT == NULL) goto Error;

    // Read the Matrix
    if (!_cmsRead15Fixed16Number(io,  &Matrix[0])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[1])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[2])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[3])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[4])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[5])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[6])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[7])) goto Error;
    if (!_cmsRead15Fixed16Number(io,  &Matrix[8])) goto Error;


    // Only operates on 3 channels

    if ((InputChannels == 3) && !_cmsMAT3isIdentity((cmsMAT3*) Matrix)) {

        mpemat = cmsStageAllocMatrix(self ->ContextID, 3, 3, Matrix, NULL);
        if (mpemat == NULL) goto Error;
        cmsPipelineInsertStage(NewLUT, cmsAT_END, mpemat);
    }

    if (!_cmsReadUInt16Number(io, &InputEntries)) return NULL;
    if (!_cmsReadUInt16Number(io, &OutputEntries)) return NULL; 

    
    // Get input tables
    if (!Read16bitTables(self ->ContextID, io,  NewLUT, InputChannels, InputEntries)) goto Error;

    // Get 3D CLUT
    nTabSize = uipow(OutputChannels, CLUTpoints, InputChannels);
    if (nTabSize > 0) {

        cmsUInt16Number *T;

        T  = (cmsUInt16Number*) _cmsCalloc(self ->ContextID, nTabSize, sizeof(cmsUInt16Number));
        if (T  == NULL) goto Error;

        if (!_cmsReadUInt16Array(io, nTabSize, T)) {
            _cmsFree(self ->ContextID, T);
            goto Error;      
        }
        
        mpeclut = cmsStageAllocCLut16bit(self ->ContextID, CLUTpoints, InputChannels, OutputChannels, T);
        if (mpeclut == NULL) {
             _cmsFree(self ->ContextID, T);
            goto Error;
        }

        cmsPipelineInsertStage(NewLUT, cmsAT_END, mpeclut);
        _cmsFree(self ->ContextID, T);
    }


    // Get output tables
    if (!Read16bitTables(self ->ContextID, io,  NewLUT, OutputChannels, OutputEntries)) goto Error;

    *nItems = 1;
    return NewLUT;

Error:
    if (NewLUT != NULL) cmsPipelineFree(NewLUT);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

// We only allow some specific MPE structures: Matrix plus prelin, plus clut, plus post-lin. 
// Some empty defaults are created for missing parts

static
cmsBool  Type_LUT16_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsUInt32Number nTabSize;
    cmsPipeline* NewLUT = (cmsPipeline*) Ptr;
    cmsStage* mpe;
    _cmsStageToneCurvesData* PreMPE = NULL, *PostMPE = NULL;
    _cmsStageMatrixData* MatMPE = NULL;
    _cmsStageCLutData* clut = NULL;
    int InputChannels, OutputChannels, clutPoints;

    // Disassemble the LUT into components.
    mpe = NewLUT -> Elements;
    if (mpe != NULL && mpe ->Type == cmsSigMatrixElemType) {

        MatMPE = (_cmsStageMatrixData*) mpe ->Data;
        mpe = mpe -> Next;
    }
    

    if (mpe != NULL && mpe ->Type == cmsSigCurveSetElemType) {
        PreMPE = (_cmsStageToneCurvesData*) mpe ->Data;
        mpe = mpe -> Next;
    }
    
    if (mpe != NULL && mpe ->Type == cmsSigCLutElemType) {
        clut  = (_cmsStageCLutData*) mpe -> Data;
        mpe = mpe ->Next;
    }
    
    if (mpe != NULL && mpe ->Type == cmsSigCurveSetElemType) {
        PostMPE = (_cmsStageToneCurvesData*) mpe ->Data;
        mpe = mpe -> Next;
    }

    // That should be all
    if (mpe != NULL) {
        cmsSignalError(mpe->ContextID, cmsERROR_UNKNOWN_EXTENSION, "LUT is not suitable to be saved as LUT16");
        return FALSE;
    }

    InputChannels  = cmsPipelineInputChannels(NewLUT);
    OutputChannels = cmsPipelineOutputChannels(NewLUT);
    
    if (clut == NULL)
        clutPoints = 0;
    else
        clutPoints    = clut->Params->nSamples[0];

    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) InputChannels)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) OutputChannels)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) clutPoints)) return FALSE;
    if (!_cmsWriteUInt8Number(io, 0)) return FALSE; // Padding


    if (MatMPE != NULL) {
    
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[0])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[1])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[2])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[3])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[4])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[5])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[6])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[7])) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, MatMPE -> Double[8])) return FALSE;
    }
    else {

        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 1)) return FALSE;
    }


    if (PreMPE != NULL) {
        if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) PreMPE ->TheCurves[0]->nEntries)) return FALSE;
    } else {
            if (!_cmsWriteUInt16Number(io, 0)) return FALSE;
    }

    if (PostMPE != NULL) {
        if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) PostMPE ->TheCurves[0]->nEntries)) return FALSE;
    } else {
        if (!_cmsWriteUInt16Number(io, 0)) return FALSE;
    
    }

    // The prelinearization table

    if (PreMPE != NULL) {
        if (!Write16bitTables(self ->ContextID, io, PreMPE)) return FALSE;
    }

    nTabSize = uipow(OutputChannels, clutPoints, InputChannels);

    if (nTabSize > 0) {
        // The 3D CLUT.
        if (clut != NULL) {
            if (!_cmsWriteUInt16Array(io, nTabSize, clut->Tab.T)) return FALSE;
        }
    }

    // The postlinearization table
    if (PostMPE != NULL) {
        if (!Write16bitTables(self ->ContextID, io, PostMPE)) return FALSE;
    }


    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_LUT16_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsPipelineDup((cmsPipeline*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_LUT16_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsPipelineFree((cmsPipeline*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigLutAToBType
// ********************************************************************************


// V4 stuff. Read matrix for LutAtoB and LutBtoA

static
cmsStage* ReadMatrix(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number Offset)
{
    cmsFloat64Number dMat[3*3];
    cmsFloat64Number dOff[3];
    cmsStage* Mat;

    // Go to address 
    if (!io -> Seek(io, Offset)) return NULL;

    // Read the Matrix
    if (!_cmsRead15Fixed16Number(io, &dMat[0])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[1])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[2])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[3])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[4])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[5])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[6])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[7])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dMat[8])) return NULL;

    if (!_cmsRead15Fixed16Number(io, &dOff[0])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dOff[1])) return NULL;
    if (!_cmsRead15Fixed16Number(io, &dOff[2])) return NULL;
    
    Mat = cmsStageAllocMatrix(self ->ContextID, 3, 3, dMat, dOff);

     return Mat;
}




//  V4 stuff. Read CLUT part for LutAtoB and LutBtoA

static
cmsStage* ReadCLUT(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number Offset, int InputChannels, int OutputChannels)
{
    cmsUInt8Number  gridPoints8[cmsMAXCHANNELS]; // Number of grid points in each dimension.  
    cmsUInt32Number GridPoints[cmsMAXCHANNELS], i;
    cmsUInt8Number  Precision;
    cmsStage* CLUT;
    _cmsStageCLutData* Data;

    if (!io -> Seek(io, Offset)) return NULL;
    if (io -> Read(io, gridPoints8, cmsMAXCHANNELS, 1) != 1) return NULL;

    for (i=0; i < cmsMAXCHANNELS; i++)
        GridPoints[i] = gridPoints8[i];

    if (!_cmsReadUInt8Number(io, &Precision)) return NULL;

    if (!_cmsReadUInt8Number(io, NULL)) return NULL;
    if (!_cmsReadUInt8Number(io, NULL)) return NULL;
    if (!_cmsReadUInt8Number(io, NULL)) return NULL;

    CLUT = cmsStageAllocCLut16bitGranular(self ->ContextID, GridPoints, InputChannels, OutputChannels, NULL);
    if (CLUT == NULL) return NULL;

    Data = (_cmsStageCLutData*) CLUT ->Data;

    // Precision can be 1 or 2 bytes
    if (Precision == 1) {

       cmsUInt8Number  v;
  
        for (i=0; i < Data ->nEntries; i++) {

                if (io ->Read(io, &v, sizeof(cmsUInt8Number), 1) != 1) return NULL;
                Data ->Tab.T[i] = FROM_8_TO_16(v);
        }
                
    }
    else  
        if (Precision == 2) {
        
            if (!_cmsReadUInt16Array(io, Data->nEntries, Data ->Tab.T)) return NULL;
    }
    else {
        cmsSignalError(self ->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown precision of '%d'", Precision); 
        return NULL;
    }

    
    return CLUT;
}

static
cmsToneCurve* ReadEmbeddedCurve(struct _cms_typehandler_struct* self, cmsIOHANDLER* io)
{
    cmsTagTypeSignature  BaseType;
    cmsUInt32Number nItems;

    BaseType = _cmsReadTypeBase(io);       
    switch (BaseType) {

            case cmsSigCurveType:
                return (cmsToneCurve*) Type_Curve_Read(self, io, &nItems, 0);

            case cmsSigParametricCurveType:
                return (cmsToneCurve*) Type_ParametricCurve_Read(self, io, &nItems, 0);

            default: 
                {
                    char String[5];

                    _cmsTagSignature2String(String, (cmsTagSignature) BaseType);
                    cmsSignalError(self ->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown curve type '%s'", String);
                }
                return NULL;
    }
}


// Read a set of curves from specific offset
static
cmsStage* ReadSetOfCurves(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number Offset, cmsUInt32Number nCurves)
{     
    cmsToneCurve* Curves[cmsMAXCHANNELS];
    cmsUInt32Number i;
    cmsStage* Lin = NULL;

    if (nCurves > cmsMAXCHANNELS) return FALSE;

    if (!io -> Seek(io, Offset)) return FALSE;

    for (i=0; i < nCurves; i++) 
        Curves[i] = NULL;

    for (i=0; i < nCurves; i++) {

        Curves[i] = ReadEmbeddedCurve(self, io);                     
        if (Curves[i] == NULL) goto Error;
        if (!_cmsReadAlignment(io)) goto Error;   
    }

    Lin = cmsStageAllocToneCurves(self ->ContextID, nCurves, Curves);

Error:
    for (i=0; i < nCurves; i++) 
        cmsFreeToneCurve(Curves[i]);

    return Lin;
}


// LutAtoB type 

// This structure represents a colour transform. The type contains up to five processing 
// elements which are stored in the AtoBTag tag in the following order: a set of one 
// dimensional curves, a 3 by 3 matrix with offset terms, a set of one dimensional curves, 
// a multidimensional lookup table, and a set of one dimensional output curves.
// Data are processed using these elements via the following sequence:
//
//("A" curves) -> (multidimensional lookup table - CLUT) -> ("M" curves) -> (matrix) -> ("B" curves).
//
/*
It is possible to use any or all of these processing elements. At least one processing element 
must be included.Only the following combinations are allowed:

B
M - Matrix - B
A - CLUT - B
A - CLUT - M - Matrix - B

*/

static
void* Type_LUTA2B_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt32Number      BaseOffset;
    cmsUInt8Number       inputChan;      // Number of input channels     
    cmsUInt8Number       outputChan;     // Number of output channels     
    cmsUInt32Number      offsetB;        // Offset to first "B" curve    
    cmsUInt32Number      offsetMat;      // Offset to matrix             
    cmsUInt32Number      offsetM;        // Offset to first "M" curve    
    cmsUInt32Number      offsetC;        // Offset to CLUT               
    cmsUInt32Number      offsetA;        // Offset to first "A" curve
    cmsStage* mpe;
    cmsPipeline* NewLUT = NULL;


    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    if (!_cmsReadUInt8Number(io, &inputChan)) return NULL;
    if (!_cmsReadUInt8Number(io, &outputChan)) return NULL;
    
    if (!_cmsReadUInt16Number(io, NULL)) return NULL;
    
    if (!_cmsReadUInt32Number(io, &offsetB)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetMat)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetM)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetC)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetA)) return NULL;

   // Allocates an empty LUT
    NewLUT = cmsPipelineAlloc(self ->ContextID, inputChan, outputChan);
    if (NewLUT == NULL) return NULL;

    if (offsetA!= 0) {
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetA, inputChan);
        cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetC != 0) {
        mpe = ReadCLUT(self, io, BaseOffset + offsetC, inputChan, outputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetM != 0) {
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetM, outputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetMat != 0) {           
        mpe = ReadMatrix(self, io, BaseOffset + offsetMat);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetB != 0) {                                        
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetB, outputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }
      
    *nItems = 1;
    return NewLUT;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

// Write a set of curves
static
cmsBool  WriteMatrix(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsStage* mpe)
{   
    _cmsStageMatrixData* m = (_cmsStageMatrixData*) mpe -> Data;

    // Write the Matrix
    if (!_cmsWrite15Fixed16Number(io, m -> Double[0])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[1])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[2])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[3])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[4])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[5])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[6])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[7])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Double[8])) return FALSE;

    if (m ->Offset != NULL) {

    if (!_cmsWrite15Fixed16Number(io, m -> Offset[0])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Offset[1])) return FALSE;
    if (!_cmsWrite15Fixed16Number(io, m -> Offset[2])) return FALSE;
    }
    else {
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, 0)) return FALSE;

    }


    return TRUE;

    cmsUNUSED_PARAMETER(self);
}


// Write a set of curves
static
cmsBool WriteSetOfCurves(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsTagTypeSignature Type, cmsStage* mpe)
{   
    cmsUInt32Number i, n;
    cmsTagTypeSignature CurrentType;
    cmsToneCurve** Curves;


    n      = cmsStageOutputChannels(mpe);
    Curves = _cmsStageGetPtrToCurveSet(mpe);

    for (i=0; i < n; i++) {

        // If this is a table-based curve, use curve type even on V4
        CurrentType = Type;

        if (Curves[i] ->nSegments == 0)
            CurrentType = cmsSigCurveType;
        else
        if (Curves[i] ->Segments[0].Type < 0)
            CurrentType = cmsSigCurveType;

        if (!_cmsWriteTypeBase(io, CurrentType)) return FALSE;

        switch (CurrentType) {

            case cmsSigCurveType:
                if (!Type_Curve_Write(self, io, Curves[i], 1)) return FALSE;
                break;

            case cmsSigParametricCurveType:
                if (!Type_ParametricCurve_Write(self, io, Curves[i], 1)) return FALSE;
                break;

            default: 
                {
                    char String[5];

                    _cmsTagSignature2String(String, (cmsTagSignature) Type);
                    cmsSignalError(self ->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown curve type '%s'", String);
                }               
                return FALSE;
        }

        if (!_cmsWriteAlignment(io)) return FALSE;
    }


    return TRUE;
}


static
cmsBool WriteCLUT(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt8Number  Precision, cmsStage* mpe)
{
    cmsUInt8Number  gridPoints[cmsMAXCHANNELS]; // Number of grid points in each dimension.  
    cmsUInt32Number i;    
    _cmsStageCLutData* CLUT = ( _cmsStageCLutData*) mpe -> Data;

    if (CLUT ->HasFloatValues) {
         cmsSignalError(self ->ContextID, cmsERROR_NOT_SUITABLE, "Cannot save floating point data, CLUT are 8 or 16 bit only"); 
         return FALSE;
    }

    memset(gridPoints, 0, sizeof(gridPoints));
    for (i=0; i < (cmsUInt32Number) CLUT ->Params ->nInputs; i++) 
        gridPoints[i] = (cmsUInt8Number) CLUT ->Params ->nSamples[i];

    if (!io -> Write(io, cmsMAXCHANNELS*sizeof(cmsUInt8Number), gridPoints)) return FALSE;

    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) Precision)) return FALSE;
    if (!_cmsWriteUInt8Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt8Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt8Number(io, 0)) return FALSE;

    // Precision can be 1 or 2 bytes
    if (Precision == 1) {

        for (i=0; i < CLUT->nEntries; i++) {

            if (!_cmsWriteUInt8Number(io, FROM_16_TO_8(CLUT->Tab.T[i]))) return FALSE;                
        }
    }
    else  
        if (Precision == 2) {

            if (!_cmsWriteUInt16Array(io, CLUT->nEntries, CLUT ->Tab.T)) return FALSE;
        }
        else {
             cmsSignalError(self ->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown precision of '%d'", Precision); 
            return FALSE;
        }

        if (!_cmsWriteAlignment(io)) return FALSE;

        return TRUE;
}




static
cmsBool Type_LUTA2B_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsPipeline* Lut = (cmsPipeline*) Ptr;
    int inputChan, outputChan;
    cmsStage *A = NULL, *B = NULL, *M = NULL;
    cmsStage * Matrix = NULL;
    cmsStage * CLUT = NULL;
    cmsUInt32Number offsetB = 0, offsetMat = 0, offsetM = 0, offsetC = 0, offsetA = 0;
    cmsUInt32Number BaseOffset, DirectoryPos, CurrentPos;

    // Get the base for all offsets
    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    if (Lut ->Elements != NULL)
        if (!cmsPipelineCheckAndRetreiveStages(Lut, 1, cmsSigCurveSetElemType, &B))
            if (!cmsPipelineCheckAndRetreiveStages(Lut, 3, cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType, &M, &Matrix, &B))          
                if (!cmsPipelineCheckAndRetreiveStages(Lut, 3, cmsSigCurveSetElemType, cmsSigCLutElemType, cmsSigCurveSetElemType, &A, &CLUT, &B))              
                    if (!cmsPipelineCheckAndRetreiveStages(Lut, 5, cmsSigCurveSetElemType, cmsSigCLutElemType, cmsSigCurveSetElemType, 
                        cmsSigMatrixElemType, cmsSigCurveSetElemType, &A, &CLUT, &M, &Matrix, &B)) {    

                            cmsSignalError(self->ContextID, cmsERROR_NOT_SUITABLE, "LUT is not suitable to be saved as LutAToB");
                            return FALSE;
                    }

    // Get input, output channels
    inputChan  = cmsPipelineInputChannels(Lut);
    outputChan = cmsPipelineOutputChannels(Lut);

    // Write channel count
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) inputChan)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) outputChan)) return FALSE;
    if (!_cmsWriteUInt16Number(io, 0)) return FALSE;

    // Keep directory to be filled latter
    DirectoryPos = io ->Tell(io);

    // Write the directory
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;

    if (A != NULL) {

        offsetA = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, A)) return FALSE;
    }
    
    if (CLUT != NULL) {
        offsetC = io ->Tell(io) - BaseOffset;
        if (!WriteCLUT(self, io, Lut ->SaveAs8Bits ? 1 : 2, CLUT)) return FALSE;

    }
    if (M != NULL) {

        offsetM = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, M)) return FALSE;
    }
    
    if (Matrix != NULL) {
        offsetMat = io ->Tell(io) - BaseOffset;
        if (!WriteMatrix(self, io, Matrix)) return FALSE;
    }

    if (B != NULL) {

        offsetB = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, B)) return FALSE;
    }
    
    CurrentPos = io ->Tell(io);

    if (!io ->Seek(io, DirectoryPos)) return FALSE;

    if (!_cmsWriteUInt32Number(io, offsetB)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetMat)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetM)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetC)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetA)) return FALSE;

    if (!io ->Seek(io, CurrentPos)) return FALSE;
    
    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_LUTA2B_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsPipelineDup((cmsPipeline*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_LUTA2B_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsPipelineFree((cmsPipeline*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// LutBToA type 

static
void* Type_LUTB2A_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt8Number       inputChan;      // Number of input channels     
    cmsUInt8Number       outputChan;     // Number of output channels     
    cmsUInt32Number      BaseOffset;     // Actual position in file
    cmsUInt32Number      offsetB;        // Offset to first "B" curve    
    cmsUInt32Number      offsetMat;      // Offset to matrix             
    cmsUInt32Number      offsetM;        // Offset to first "M" curve    
    cmsUInt32Number      offsetC;        // Offset to CLUT               
    cmsUInt32Number      offsetA;        // Offset to first "A" curve
    cmsStage* mpe;
    cmsPipeline* NewLUT = NULL;


    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    if (!_cmsReadUInt8Number(io, &inputChan)) return NULL;
    if (!_cmsReadUInt8Number(io, &outputChan)) return NULL;

    // Padding
    if (!_cmsReadUInt16Number(io, NULL)) return NULL;
    
    if (!_cmsReadUInt32Number(io, &offsetB)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetMat)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetM)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetC)) return NULL;
    if (!_cmsReadUInt32Number(io, &offsetA)) return NULL;

    // Allocates an empty LUT
    NewLUT = cmsPipelineAlloc(self ->ContextID, inputChan, outputChan);
    if (NewLUT == NULL) return NULL;
    
    if (offsetB != 0) {                                        
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetB, inputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetMat != 0) {           
        mpe = ReadMatrix(self, io, BaseOffset + offsetMat);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetM != 0) {
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetM, inputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetC != 0) {
        mpe = ReadCLUT(self, io, BaseOffset + offsetC, inputChan, outputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    if (offsetA!= 0) {
        mpe = ReadSetOfCurves(self, io, BaseOffset + offsetA, outputChan);
        if (mpe != NULL) cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    *nItems = 1;
    return NewLUT;

    cmsUNUSED_PARAMETER(SizeOfTag);
}


/*
B
B - Matrix - M
B - CLUT - A
B - Matrix - M - CLUT - A
*/

static
cmsBool  Type_LUTB2A_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsPipeline* Lut = (cmsPipeline*) Ptr;
    int inputChan, outputChan;
    cmsStage *A = NULL, *B = NULL, *M = NULL;
    cmsStage *Matrix = NULL;
    cmsStage *CLUT = NULL;
    cmsUInt32Number offsetB = 0, offsetMat = 0, offsetM = 0, offsetC = 0, offsetA = 0;
    cmsUInt32Number BaseOffset, DirectoryPos, CurrentPos;


    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    if (!cmsPipelineCheckAndRetreiveStages(Lut, 1, cmsSigCurveSetElemType, &B))
        if (!cmsPipelineCheckAndRetreiveStages(Lut, 3, cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType, &B, &Matrix, &M))          
            if (!cmsPipelineCheckAndRetreiveStages(Lut, 3, cmsSigCurveSetElemType, cmsSigCLutElemType, cmsSigCurveSetElemType, &B, &CLUT, &A))              
                if (!cmsPipelineCheckAndRetreiveStages(Lut, 5, cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType, 
                    cmsSigCLutElemType, cmsSigCurveSetElemType, &B, &Matrix, &M, &CLUT, &A)) {                 
                        cmsSignalError(self->ContextID, cmsERROR_NOT_SUITABLE, "LUT is not suitable to be saved as LutBToA");
                        return FALSE;
                }

    inputChan  = cmsPipelineInputChannels(Lut);
    outputChan = cmsPipelineOutputChannels(Lut);
    
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) inputChan)) return FALSE;
    if (!_cmsWriteUInt8Number(io, (cmsUInt8Number) outputChan)) return FALSE;
    if (!_cmsWriteUInt16Number(io, 0)) return FALSE;

    DirectoryPos = io ->Tell(io);

    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;  
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;  
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;

    if (A != NULL) {

        offsetA = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, A)) return FALSE;
    }
    
    if (CLUT != NULL) {
        offsetC = io ->Tell(io) - BaseOffset;
        if (!WriteCLUT(self, io, Lut ->SaveAs8Bits ? 1 : 2, CLUT)) return FALSE;

    }
    if (M != NULL) {

        offsetM = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, M)) return FALSE;
    }
    
    if (Matrix != NULL) {
        offsetMat = io ->Tell(io) - BaseOffset;
        if (!WriteMatrix(self, io, Matrix)) return FALSE;    
    }

    if (B != NULL) {

        offsetB = io ->Tell(io) - BaseOffset;
        if (!WriteSetOfCurves(self, io, cmsSigParametricCurveType, B)) return FALSE;
    }
    
    CurrentPos = io ->Tell(io);

    if (!io ->Seek(io, DirectoryPos)) return FALSE;

    if (!_cmsWriteUInt32Number(io, offsetB)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetMat)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetM)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetC)) return FALSE;
    if (!_cmsWriteUInt32Number(io, offsetA)) return FALSE;

    if (!io ->Seek(io, CurrentPos)) return FALSE;
    
    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}



static
void* Type_LUTB2A_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsPipelineDup((cmsPipeline*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_LUTB2A_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsPipelineFree((cmsPipeline*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}



// ********************************************************************************
// Type cmsSigColorantTableType
// ********************************************************************************
/*
The purpose of this tag is to identify the colorants used in the profile by a 
unique name and set of XYZ or L*a*b* values to give the colorant an unambiguous 
value. The first colorant listed is the colorant of the first device channel of
a lut tag. The second colorant listed is the colorant of the second device channel
of a lut tag, and so on.
*/

static
void *Type_ColorantTable_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUInt32Number i, Count;
    cmsNAMEDCOLORLIST* List;
    char Name[34];
    cmsUInt16Number PCS[3]; 


    if (!_cmsReadUInt32Number(io, &Count)) return NULL;

    if (Count > cmsMAXCHANNELS) {
        cmsSignalError(self->ContextID, cmsERROR_RANGE, "Too many colorants '%d'", Count);
        return NULL;
    }

    List = cmsAllocNamedColorList(self ->ContextID, Count, 0, "", "");
    for (i=0; i < Count; i++) {

        if (io ->Read(io, Name, 32, 1) != 1) goto Error;
        Name[33] = 0;

        if (!_cmsReadUInt16Array(io, 3, PCS)) goto Error;

        if (!cmsAppendNamedColor(List, Name, PCS, NULL)) goto Error; 
                                                            
    }

    *nItems = 1;
    return List;

Error:
    *nItems = 0;
    cmsFreeNamedColorList(List);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}



// Saves a colorant table. It is using the named color structure for simplicity sake
static
cmsBool  Type_ColorantTable_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsNAMEDCOLORLIST* NamedColorList = (cmsNAMEDCOLORLIST*) Ptr; 
    int i, nColors;

    nColors = cmsNamedColorCount(NamedColorList);

    if (!_cmsWriteUInt32Number(io, nColors)) return FALSE;

    for (i=0; i < nColors; i++) {

        char root[33];
        cmsUInt16Number PCS[3]; 

        if (!cmsNamedColorInfo(NamedColorList, i, root, NULL, NULL, PCS, NULL)) return 0;
        root[32] = 0;

        if (!io ->Write(io, 32, root)) return FALSE;
        if (!_cmsWriteUInt16Array(io, 3, PCS)) return FALSE;
    }

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}


static
void* Type_ColorantTable_Dup(struct _cms_typehandler_struct* self, const void* Ptr, cmsUInt32Number n)
{
    cmsNAMEDCOLORLIST* nc = (cmsNAMEDCOLORLIST*) Ptr;
    return (void*) cmsDupNamedColorList(nc); 

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}


static
void Type_ColorantTable_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsFreeNamedColorList((cmsNAMEDCOLORLIST*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigNamedColor2Type
// ********************************************************************************
//
//The namedColor2Type is a count value and array of structures that provide color 
//coordinates for 7-bit ASCII color names. For each named color, a PCS and optional 
//device representation of the color are given. Both representations are 16-bit values. 
//The device representation corresponds to the header�s �color space of data� field. 
//This representation should be consistent with the �number of device components�
//field in the namedColor2Type. If this field is 0, device coordinates are not provided.
//The PCS representation corresponds to the header�s PCS field. The PCS representation 
//is always provided. Color names are fixed-length, 32-byte fields including null 
//termination. In order to maintain maximum portability, it is strongly recommended 
//that special characters of the 7-bit ASCII set not be used.

static
void *Type_NamedColor_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{

    cmsUInt32Number      vendorFlag;     // Bottom 16 bits for ICC use 
    cmsUInt32Number      count;          // Count of named colors 
    cmsUInt32Number      nDeviceCoords;  // Num of device coordinates 
    char                 prefix[32];     // Prefix for each color name 
    char                 suffix[32];     // Suffix for each color name 
    cmsNAMEDCOLORLIST*  v;
    cmsUInt32Number i;


    *nItems = 0;
    if (!_cmsReadUInt32Number(io, &vendorFlag)) return NULL;
    if (!_cmsReadUInt32Number(io, &count)) return NULL;
    if (!_cmsReadUInt32Number(io, &nDeviceCoords)) return NULL;

    if (io -> Read(io, prefix, 32, 1) != 1) return NULL;
    if (io -> Read(io, suffix, 32, 1) != 1) return NULL;

    prefix[31] = suffix[31] = 0;

    v = cmsAllocNamedColorList(self ->ContextID, count, nDeviceCoords, prefix, suffix);
    if (v == NULL) {
        cmsSignalError(self->ContextID, cmsERROR_RANGE, "Too many named colors '%d'", count);
        return NULL;
    }

    if (nDeviceCoords > cmsMAXCHANNELS) {
        cmsSignalError(self->ContextID, cmsERROR_RANGE, "Too many device coordinates '%d'", nDeviceCoords);
        return 0;
    }
    for (i=0; i < count; i++) {

        cmsUInt16Number PCS[3];
        cmsUInt16Number Colorant[cmsMAXCHANNELS];
        char Root[33];

        memset(Colorant, 0, sizeof(Colorant));
        if (io -> Read(io, Root, 32, 1) != 1) return NULL;
        if (!_cmsReadUInt16Array(io, 3, PCS)) goto Error;
        if (!_cmsReadUInt16Array(io, nDeviceCoords, Colorant)) goto Error;

        if (!cmsAppendNamedColor(v, Root, PCS, Colorant)) goto Error;
    }

    *nItems = 1;
    return (void*) v ;

Error:
    cmsFreeNamedColorList(v);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}


// Saves a named color list into a named color profile
static
cmsBool Type_NamedColor_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsNAMEDCOLORLIST* NamedColorList = (cmsNAMEDCOLORLIST*) Ptr;
    char                prefix[32];     // Prefix for each color name 
    char                suffix[32];     // Suffix for each color name 
    int i, nColors;

    nColors = cmsNamedColorCount(NamedColorList);

    if (!_cmsWriteUInt32Number(io, 0)) return FALSE;
    if (!_cmsWriteUInt32Number(io, nColors)) return FALSE;
    if (!_cmsWriteUInt32Number(io, NamedColorList ->ColorantCount)) return FALSE;

    strncpy(prefix, (const char*) NamedColorList->Prefix, 32);
    strncpy(suffix, (const char*) NamedColorList->Suffix, 32);

    suffix[31] = prefix[31] = 0;

    if (!io ->Write(io, 32, prefix)) return FALSE;
    if (!io ->Write(io, 32, suffix)) return FALSE;

    for (i=0; i < nColors; i++) {

       cmsUInt16Number PCS[3];
       cmsUInt16Number Colorant[cmsMAXCHANNELS];
       char Root[33];

        if (!cmsNamedColorInfo(NamedColorList, i, Root, NULL, NULL, PCS, Colorant)) return 0;
        if (!io ->Write(io, 32 , Root)) return FALSE;
        if (!_cmsWriteUInt16Array(io, 3, PCS)) return FALSE;   
        if (!_cmsWriteUInt16Array(io, NamedColorList ->ColorantCount, Colorant)) return FALSE;
    }

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}

static
void* Type_NamedColor_Dup(struct _cms_typehandler_struct* self, const void* Ptr, cmsUInt32Number n)
{
    cmsNAMEDCOLORLIST* nc = (cmsNAMEDCOLORLIST*) Ptr;

    return (void*) cmsDupNamedColorList(nc);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}


static
void Type_NamedColor_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsFreeNamedColorList((cmsNAMEDCOLORLIST*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigProfileSequenceDescType
// ********************************************************************************

// This type is an array of structures, each of which contains information from the 
// header fields and tags from the original profiles which were combined to create 
// the final profile. The order of the structures is the order in which the profiles 
// were combined and includes a structure for the final profile. This provides a 
// description of the profile sequence from source to destination, 
// typically used with the DeviceLink profile.

static
cmsBool ReadEmbeddedText(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsMLU** mlu, cmsUInt32Number SizeOfTag)
{
    cmsTagTypeSignature  BaseType;
    cmsUInt32Number nItems;

    BaseType = _cmsReadTypeBase(io); 

    switch (BaseType) {

       case cmsSigTextType:
           if (*mlu) cmsMLUfree(*mlu);
           *mlu = (cmsMLU*)Type_Text_Read(self, io, &nItems, SizeOfTag);
           return (*mlu != NULL); 

       case cmsSigTextDescriptionType:
           if (*mlu) cmsMLUfree(*mlu);
           *mlu =  (cmsMLU*) Type_Text_Description_Read(self, io, &nItems, SizeOfTag);
           return (*mlu != NULL); 

           /*
           TBD: Size is needed for MLU, and we have no idea on which is the available size
           */

       case cmsSigMultiLocalizedUnicodeType:
           if (*mlu) cmsMLUfree(*mlu);
           *mlu =  (cmsMLU*) Type_MLU_Read(self, io, &nItems, SizeOfTag);
           return (*mlu != NULL);

       default: return FALSE;
    }
}


static
void *Type_ProfileSequenceDesc_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsSEQ* OutSeq;
    cmsUInt32Number i, Count;
    
    *nItems = 0;
    
    if (!_cmsReadUInt32Number(io, &Count)) return NULL;  

    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
    SizeOfTag -= sizeof(cmsUInt32Number);

   
    OutSeq = cmsAllocProfileSequenceDescription(self ->ContextID, Count);
    if (OutSeq == NULL) return NULL;

    OutSeq ->n = Count;
    
    // Get structures as well

    for (i=0; i < Count; i++) {
        
        cmsPSEQDESC* sec = &OutSeq -> seq[i];

        if (!_cmsReadUInt32Number(io, &sec ->deviceMfg)) return NULL;
        if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
        SizeOfTag -= sizeof(cmsUInt32Number);

        if (!_cmsReadUInt32Number(io, &sec ->deviceModel)) return NULL;
        if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
        SizeOfTag -= sizeof(cmsUInt32Number);

        if (!_cmsReadUInt64Number(io, &sec ->attributes)) return NULL;        
        if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
        SizeOfTag -= sizeof(cmsUInt64Number);

        if (!_cmsReadUInt32Number(io, (cmsUInt32Number *)&sec ->technology)) return NULL;
        if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
        SizeOfTag -= sizeof(cmsUInt32Number);

        if (!ReadEmbeddedText(self, io, &sec ->Manufacturer, SizeOfTag)) return NULL;
        if (!ReadEmbeddedText(self, io, &sec ->Model, SizeOfTag)) return NULL;
    }

    *nItems = 1;
    return OutSeq;
}


// Aux--Embed a text description type. It can be of type text description or multilocalized unicode
// and it depends of the version number passed on cmsTagDescriptor structure instead of stack
static
cmsBool  SaveDescription(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsMLU* Text)
{
    if (self ->ICCVersion < 0x4000000) { 

        if (!_cmsWriteTypeBase(io, cmsSigTextDescriptionType)) return FALSE;
        return Type_Text_Description_Write(self, io, Text, 1);
    }
    else {
        if (!_cmsWriteTypeBase(io, cmsSigMultiLocalizedUnicodeType)) return FALSE;
        return Type_MLU_Write(self, io, Text, 1);
    }
}


static
cmsBool  Type_ProfileSequenceDesc_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsSEQ* Seq = (cmsSEQ*) Ptr;
    cmsUInt32Number i;

    if (!_cmsWriteUInt32Number(io, Seq->n)) return FALSE;
    
    for (i=0; i < Seq ->n; i++) {

        cmsPSEQDESC* sec = &Seq -> seq[i];

        if (!_cmsWriteUInt32Number(io, sec ->deviceMfg)) return FALSE;
        if (!_cmsWriteUInt32Number(io, sec ->deviceModel)) return FALSE;
        if (!_cmsWriteUInt64Number(io, sec ->attributes)) return FALSE;        
        if (!_cmsWriteUInt32Number(io, sec ->technology)) return FALSE;
        
        if (!SaveDescription(self, io, sec ->Manufacturer)) return FALSE;       
        if (!SaveDescription(self, io, sec ->Model)) return FALSE;          
    }
    
     return TRUE;  

     cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_ProfileSequenceDesc_Dup(struct _cms_typehandler_struct* self, const void* Ptr, cmsUInt32Number n)
{   
    return (void*) cmsDupProfileSequenceDescription((cmsSEQ*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_ProfileSequenceDesc_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsFreeProfileSequenceDescription((cmsSEQ*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigProfileSequenceIdType
// ********************************************************************************
/*
In certain workflows using ICC Device Link Profiles, it is necessary to identify the 
original profiles that were combined to create the Device Link Profile.
This type is an array of structures, each of which contains information for 
identification of a profile used in a sequence
*/


static
cmsBool ReadSeqID(struct _cms_typehandler_struct* self, 
                                             cmsIOHANDLER* io,
                                             void* Cargo,
                                             cmsUInt32Number n, 
                                             cmsUInt32Number SizeOfTag)
{
    cmsSEQ* OutSeq = (cmsSEQ*) Cargo;
    cmsPSEQDESC* seq = &OutSeq ->seq[n];
    
    if (io -> Read(io, seq ->ProfileID.ID8, 16, 1) != 1) return FALSE;
    if (!ReadEmbeddedText(self, io, &seq ->Description, SizeOfTag)) return FALSE;

    return TRUE;
}



static
void *Type_ProfileSequenceId_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsSEQ* OutSeq;
    cmsUInt32Number Count;
    cmsUInt32Number BaseOffset;

    *nItems = 0;

    // Get actual position as a basis for element offsets
    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    // Get table count
    if (!_cmsReadUInt32Number(io, &Count)) return NULL;  
    SizeOfTag -= sizeof(cmsUInt32Number);

    // Allocate an empty structure
    OutSeq = cmsAllocProfileSequenceDescription(self ->ContextID, Count);
    if (OutSeq == NULL) return NULL;


    // Read the position table
    if (!ReadPositionTable(self, io, Count, BaseOffset, OutSeq, ReadSeqID)) {

        cmsFreeProfileSequenceDescription(OutSeq);
        return NULL;
    }

    // Success
    *nItems = 1;
    return OutSeq;

}


static
cmsBool WriteSeqID(struct _cms_typehandler_struct* self, 
                                             cmsIOHANDLER* io,
                                             void* Cargo,
                                             cmsUInt32Number n, 
                                             cmsUInt32Number SizeOfTag)
{
    cmsSEQ* Seq = (cmsSEQ*) Cargo;
    
    if (!io ->Write(io, 16, Seq ->seq[n].ProfileID.ID8)) return FALSE;

    // Store here the MLU
    if (!SaveDescription(self, io, Seq ->seq[n].Description)) return FALSE;  

    return TRUE;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool  Type_ProfileSequenceId_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsSEQ* Seq = (cmsSEQ*) Ptr;
    cmsUInt32Number BaseOffset;

    // Keep the base offset
    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);
    
    // This is the table count
    if (!_cmsWriteUInt32Number(io, Seq ->n)) return FALSE;

    // This is the position table and content
    if (!WritePositionTable(self, io, 0, Seq ->n, BaseOffset, Seq, WriteSeqID)) return FALSE;
          
    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_ProfileSequenceId_Dup(struct _cms_typehandler_struct* self, const void* Ptr, cmsUInt32Number n)
{   
    return (void*) cmsDupProfileSequenceDescription((cmsSEQ*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_ProfileSequenceId_Free(struct _cms_typehandler_struct* self, void* Ptr)
{    
    cmsFreeProfileSequenceDescription((cmsSEQ*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigUcrBgType
// ********************************************************************************
/*
This type contains curves representing the under color removal and black
generation and a text string which is a general description of the method used
for the ucr/bg.
*/

static
void *Type_UcrBg_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsUcrBg* n = (cmsUcrBg*) _cmsMallocZero(self ->ContextID, sizeof(cmsUcrBg));
    cmsUInt32Number CountUcr, CountBg;
    char* ASCIIString;

    *nItems = 0;
    if (n == NULL) return NULL;

    // First curve is Under color removal
    if (!_cmsReadUInt32Number(io, &CountUcr)) return NULL;
    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
    SizeOfTag -= sizeof(cmsUInt32Number);

    n ->Ucr = cmsBuildTabulatedToneCurve16(self ->ContextID, CountUcr, NULL);
    if (n ->Ucr == NULL) return NULL;

    if (!_cmsReadUInt16Array(io, CountUcr, n ->Ucr->Table16)) return NULL;
    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
    SizeOfTag -= CountUcr * sizeof(cmsUInt16Number);

    // Second curve is Black generation
    if (!_cmsReadUInt32Number(io, &CountBg)) return NULL;
    if (SizeOfTag < sizeof(cmsUInt32Number)) return NULL;
    SizeOfTag -= sizeof(cmsUInt32Number);

    n ->Bg = cmsBuildTabulatedToneCurve16(self ->ContextID, CountBg, NULL);
    if (n ->Bg == NULL) return NULL;
    if (!_cmsReadUInt16Array(io, CountBg, n ->Bg->Table16)) return NULL;
    if (SizeOfTag < CountBg * sizeof(cmsUInt16Number)) return NULL;
    SizeOfTag -= CountBg * sizeof(cmsUInt16Number);
    if (SizeOfTag == UINT_MAX) return NULL;

    // Now comes the text. The length is specified by the tag size
    n ->Desc = cmsMLUalloc(self ->ContextID, 1);
    if (n ->Desc == NULL) return NULL;

    ASCIIString = (char*) _cmsMalloc(self ->ContextID, SizeOfTag + 1);
    if (io ->Read(io, ASCIIString, sizeof(char), SizeOfTag) != SizeOfTag) return NULL;
    ASCIIString[SizeOfTag] = 0;
    cmsMLUsetASCII(n ->Desc, cmsNoLanguage, cmsNoCountry, ASCIIString);
    _cmsFree(self ->ContextID, ASCIIString);

    *nItems = 1;
    return (void*) n;
}

static
cmsBool  Type_UcrBg_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsUcrBg* Value = (cmsUcrBg*) Ptr;
    cmsUInt32Number TextSize;
    char* Text;

    // First curve is Under color removal
    if (!_cmsWriteUInt32Number(io, Value ->Ucr ->nEntries)) return FALSE;
    if (!_cmsWriteUInt16Array(io, Value ->Ucr ->nEntries, Value ->Ucr ->Table16)) return FALSE;

    // Then black generation    
    if (!_cmsWriteUInt32Number(io, Value ->Bg ->nEntries)) return FALSE;
    if (!_cmsWriteUInt16Array(io, Value ->Bg ->nEntries, Value ->Bg ->Table16)) return FALSE;

    // Now comes the text. The length is specified by the tag size
    TextSize = cmsMLUgetASCII(Value ->Desc, cmsNoLanguage, cmsNoCountry, NULL, 0);
    Text     = (char*) _cmsMalloc(self ->ContextID, TextSize);
    if (cmsMLUgetASCII(Value ->Desc, cmsNoLanguage, cmsNoCountry, Text, TextSize) != TextSize) return FALSE;

    if (!io ->Write(io, TextSize, Text)) return FALSE;      
    _cmsFree(self ->ContextID, Text);

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_UcrBg_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    cmsUcrBg* Src = (cmsUcrBg*) Ptr;
    cmsUcrBg* NewUcrBg = (cmsUcrBg*) _cmsMallocZero(self ->ContextID, sizeof(cmsUcrBg));

    if (NewUcrBg == NULL) return NULL;

    NewUcrBg ->Bg   = cmsDupToneCurve(Src ->Bg);
    NewUcrBg ->Ucr  = cmsDupToneCurve(Src ->Ucr);
    NewUcrBg ->Desc = cmsMLUdup(Src ->Desc);

    return (void*) NewUcrBg;

    cmsUNUSED_PARAMETER(n);
}

static
void Type_UcrBg_Free(struct _cms_typehandler_struct* self, void *Ptr)
{
   cmsUcrBg* Src = (cmsUcrBg*) Ptr;
   
   if (Src ->Ucr) cmsFreeToneCurve(Src ->Ucr);
   if (Src ->Bg)  cmsFreeToneCurve(Src ->Bg);
   if (Src ->Desc) cmsMLUfree(Src ->Desc);
   
   _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigCrdInfoType
// ********************************************************************************

/*
This type contains the PostScript product name to which this profile corresponds
and the names of the companion CRDs. Recall that a single profile can generate
multiple CRDs. It is implemented as a MLU being the language code "PS" and then
country varies for each element:

                nm: PostScript product name
                #0: Rendering intent 0 CRD name
                #1: Rendering intent 1 CRD name
                #2: Rendering intent 2 CRD name
                #3: Rendering intent 3 CRD name
*/



// Auxiliar, read an string specified as count + string
static
cmsBool  ReadCountAndSting(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsMLU* mlu, cmsUInt32Number* SizeOfTag, const char* Section)
{
    cmsUInt32Number Count;
    char* Text;

    if (*SizeOfTag < sizeof(cmsUInt32Number)) return FALSE; 
    
    if (!_cmsReadUInt32Number(io, &Count)) return FALSE;

    if (Count > UINT_MAX - sizeof(cmsUInt32Number)) return FALSE;
    if (*SizeOfTag < Count + sizeof(cmsUInt32Number)) return FALSE; 

    Text     = (char*) _cmsMalloc(self ->ContextID, Count+1);
    if (Text == NULL) return FALSE;

    if (io ->Read(io, Text, sizeof(cmsUInt8Number), Count) != Count) {
        _cmsFree(self ->ContextID, Text);
        return FALSE;
    }

    Text[Count] = 0;
   
    cmsMLUsetASCII(mlu, "PS", Section, Text);
    _cmsFree(self ->ContextID, Text);

    *SizeOfTag -= (Count + sizeof(cmsUInt32Number));
    return TRUE;    
}

static
cmsBool  WriteCountAndSting(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsMLU* mlu, const char* Section)
{
 cmsUInt32Number TextSize;
 char* Text;    
    
    TextSize = cmsMLUgetASCII(mlu, "PS", Section, NULL, 0);
    Text     = (char*) _cmsMalloc(self ->ContextID, TextSize);

    if (!_cmsWriteUInt32Number(io, TextSize)) return FALSE; 

    if (cmsMLUgetASCII(mlu, "PS", Section, Text, TextSize) == 0) return FALSE;

    if (!io ->Write(io, TextSize, Text)) return FALSE;      
    _cmsFree(self ->ContextID, Text);

    return TRUE;
}

static
void *Type_CrdInfo_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsMLU* mlu = cmsMLUalloc(self ->ContextID, 5);

    *nItems = 0;
    if (!ReadCountAndSting(self, io, mlu, &SizeOfTag, "nm")) goto Error;
    if (!ReadCountAndSting(self, io, mlu, &SizeOfTag, "#0")) goto Error;
    if (!ReadCountAndSting(self, io, mlu, &SizeOfTag, "#1")) goto Error;
    if (!ReadCountAndSting(self, io, mlu, &SizeOfTag, "#2")) goto Error;
    if (!ReadCountAndSting(self, io, mlu, &SizeOfTag, "#3")) goto Error;
  
    *nItems = 1;
    return (void*) mlu;

Error:
    cmsMLUfree(mlu);
    return NULL;

}

static
cmsBool  Type_CrdInfo_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{

    cmsMLU* mlu = (cmsMLU*) Ptr;

    if (!WriteCountAndSting(self, io, mlu, "nm")) goto Error;
    if (!WriteCountAndSting(self, io, mlu, "#0")) goto Error;
    if (!WriteCountAndSting(self, io, mlu, "#1")) goto Error;
    if (!WriteCountAndSting(self, io, mlu, "#2")) goto Error;
    if (!WriteCountAndSting(self, io, mlu, "#3")) goto Error;

    return TRUE;

Error:
    return FALSE;

    cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_CrdInfo_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsMLUdup((cmsMLU*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_CrdInfo_Free(struct _cms_typehandler_struct* self, void *Ptr)
{    
    cmsMLUfree((cmsMLU*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}

// ********************************************************************************
// Type cmsSigScreeningType
// ********************************************************************************
//
//The screeningType describes various screening parameters including screen
//frequency, screening angle, and spot shape.

static
void *Type_Screening_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsScreening* sc = NULL;
    cmsUInt32Number i;
    
    sc = (cmsScreening*) _cmsMallocZero(self ->ContextID, sizeof(cmsScreening));
    if (sc == NULL) return NULL;

    *nItems = 0;
    
    if (!_cmsReadUInt32Number(io, &sc ->Flag)) goto Error;
    if (!_cmsReadUInt32Number(io, &sc ->nChannels)) goto Error;

    if (sc ->nChannels > cmsMAXCHANNELS - 1)
        sc ->nChannels = cmsMAXCHANNELS - 1;

    for (i=0; i < sc ->nChannels; i++) {

        if (!_cmsRead15Fixed16Number(io, &sc ->Channels[i].Frequency)) goto Error;      
        if (!_cmsRead15Fixed16Number(io, &sc ->Channels[i].ScreenAngle)) goto Error;        
        if (!_cmsReadUInt32Number(io, &sc ->Channels[i].SpotShape)) goto Error;
    }


    *nItems = 1;

    return (void*) sc;

Error:
    if (sc != NULL) 
        _cmsFree(self ->ContextID, sc);

    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}


static
cmsBool Type_Screening_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsScreening* sc = (cmsScreening* ) Ptr; 
    cmsUInt32Number i;
        
    if (!_cmsWriteUInt32Number(io, sc ->Flag)) return FALSE;
    if (!_cmsWriteUInt32Number(io, sc ->nChannels)) return FALSE;

    for (i=0; i < sc ->nChannels; i++) {

        if (!_cmsWrite15Fixed16Number(io, sc ->Channels[i].Frequency)) return FALSE;
        if (!_cmsWrite15Fixed16Number(io, sc ->Channels[i].ScreenAngle)) return FALSE;
        if (!_cmsWriteUInt32Number(io, sc ->Channels[i].SpotShape)) return FALSE;
    }

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}


static
void* Type_Screening_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
   return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsScreening));

   cmsUNUSED_PARAMETER(n);
}


static
void Type_Screening_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
   _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type cmsSigViewingConditionsType
// ********************************************************************************
//
//This type represents a set of viewing condition parameters including: 
//CIE �absolute� illuminant white point tristimulus values and CIE �absolute� 
//surround tristimulus values.

static
void *Type_ViewingConditions_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsICCViewingConditions* vc = NULL;
    
    vc = (cmsICCViewingConditions*) _cmsMallocZero(self ->ContextID, sizeof(cmsICCViewingConditions));
    if (vc == NULL) return NULL;

    *nItems = 0;
        
    if (!_cmsReadXYZNumber(io, &vc ->IlluminantXYZ)) goto Error;
    if (!_cmsReadXYZNumber(io, &vc ->SurroundXYZ)) goto Error;
    if (!_cmsReadUInt32Number(io, &vc ->IlluminantType)) goto Error;

    *nItems = 1;

    return (void*) vc;

Error:
    if (vc != NULL) 
        _cmsFree(self ->ContextID, vc);

    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}


static
cmsBool Type_ViewingConditions_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsICCViewingConditions* sc = (cmsICCViewingConditions* ) Ptr; 
            
    if (!_cmsWriteXYZNumber(io, &sc ->IlluminantXYZ)) return FALSE;
    if (!_cmsWriteXYZNumber(io, &sc ->SurroundXYZ)) return FALSE;
    if (!_cmsWriteUInt32Number(io, sc ->IlluminantType)) return FALSE;

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}


static
void* Type_ViewingConditions_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
   return _cmsDupMem(self ->ContextID, Ptr, sizeof(cmsScreening));

   cmsUNUSED_PARAMETER(n);
}


static
void Type_ViewingConditions_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
   _cmsFree(self ->ContextID, Ptr);
}


// ********************************************************************************
// Type cmsSigMultiProcessElementType
// ********************************************************************************


static
void* GenericMPEdup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsStageDup((cmsStage*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void GenericMPEfree(struct _cms_typehandler_struct* self, void *Ptr)
{    
    cmsStageFree((cmsStage*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}

// Each curve is stored in one or more curve segments, with break-points specified between curve segments.
// The first curve segment always starts at �Infinity, and the last curve segment always ends at +Infinity. The
// first and last curve segments shall be specified in terms of a formula, whereas the other segments shall be
// specified either in terms of a formula, or by a sampled curve.


// Read an embedded segmented curve
static
cmsToneCurve* ReadSegmentedCurve(struct _cms_typehandler_struct* self, cmsIOHANDLER* io)
{   
    cmsCurveSegSignature ElementSig;
    cmsUInt32Number i, j;
    cmsUInt16Number nSegments;
    cmsCurveSegment*  Segments;
    cmsToneCurve* Curve;
    cmsFloat32Number PrevBreak = -1E22F;    // - infinite
 
    // Take signature and channels for each element.
     if (!_cmsReadUInt32Number(io, (cmsUInt32Number*) &ElementSig)) return NULL;    

     // That should be a segmented curve
     if (ElementSig != cmsSigSegmentedCurve) return NULL;

     if (!_cmsReadUInt32Number(io, NULL)) return NULL;
     if (!_cmsReadUInt16Number(io, &nSegments)) return NULL;
     if (!_cmsReadUInt16Number(io, NULL)) return NULL;

     if (nSegments < 1) return NULL;
     Segments = (cmsCurveSegment*) _cmsCalloc(self ->ContextID, nSegments, sizeof(cmsCurveSegment));
     if (Segments == NULL) return NULL;

     // Read breakpoints
     for (i=0; i < (cmsUInt32Number) nSegments - 1; i++) {

         Segments[i].x0 = PrevBreak;
         if (!_cmsReadFloat32Number(io, &Segments[i].x1)) goto Error;
         PrevBreak = Segments[i].x1;
     }

     Segments[nSegments-1].x0 = PrevBreak;
     Segments[nSegments-1].x1 = 1E22F;     // A big cmsFloat32Number number

     // Read segments
     for (i=0; i < nSegments; i++) {

          if (!_cmsReadUInt32Number(io, (cmsUInt32Number*) &ElementSig)) goto Error;
          if (!_cmsReadUInt32Number(io, NULL)) goto Error;
         
           switch (ElementSig) {

            case cmsSigFormulaCurveSeg: {
                                       
                cmsUInt16Number Type;
                cmsUInt32Number ParamsByType[] = {4, 5, 5 };

                if (!_cmsReadUInt16Number(io, &Type)) goto Error;
                if (!_cmsReadUInt16Number(io, NULL)) goto Error;

                Segments[i].Type = Type + 6;
                if (Type > 2) goto Error;

                for (j=0; j < ParamsByType[Type]; j++) {

                    cmsFloat32Number f;
                    if (!_cmsReadFloat32Number(io, &f)) goto Error;
                    Segments[i].Params[j] = f;
                }
                }
                break;


            case cmsSigSampledCurveSeg: {
                cmsUInt32Number Count;

                if (!_cmsReadUInt32Number(io, &Count)) return NULL;

                Segments[i].nGridPoints = Count;
                Segments[i].SampledPoints = (cmsFloat32Number*) _cmsCalloc(self ->ContextID, Count, sizeof(cmsFloat32Number));
                if (Segments[i].SampledPoints == NULL) goto Error;

                for (j=0; j < Count; j++) {
                    if (!_cmsReadFloat32Number(io, &Segments[i].SampledPoints[j])) goto Error;
                }
                }
                break;

            default:
                {
                char String[5];

                _cmsTagSignature2String(String, (cmsTagSignature) ElementSig);
                cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown curve element type '%s' found.", String);
                }
                return NULL;                

         }
     }

     Curve = cmsBuildSegmentedToneCurve(self ->ContextID, nSegments, Segments);

     for (i=0; i < nSegments; i++) {
         if (Segments[i].SampledPoints) _cmsFree(self ->ContextID, Segments[i].SampledPoints);
     }
     _cmsFree(self ->ContextID, Segments);
     return Curve;

Error:
     if (Segments) _cmsFree(self ->ContextID, Segments);
     return NULL;
}


static
cmsBool ReadMPECurve(struct _cms_typehandler_struct* self, 
                     cmsIOHANDLER* io,
                     void* Cargo,
                     cmsUInt32Number n, 
                     cmsUInt32Number SizeOfTag)
{
      cmsToneCurve** GammaTables = ( cmsToneCurve**) Cargo;

      GammaTables[n] = ReadSegmentedCurve(self, io);
      return (GammaTables[n] != NULL);

      cmsUNUSED_PARAMETER(SizeOfTag);
}

static
void *Type_MPEcurve_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsStage* mpe = NULL;
    cmsUInt16Number InputChans, OutputChans;
    cmsUInt32Number i, BaseOffset;
    cmsToneCurve** GammaTables;

    *nItems = 0;

    // Get actual position as a basis for element offsets
    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    if (!_cmsReadUInt16Number(io, &InputChans)) return NULL;
    if (!_cmsReadUInt16Number(io, &OutputChans)) return NULL;

    if (InputChans != OutputChans) return NULL;

    GammaTables = (cmsToneCurve**) _cmsCalloc(self ->ContextID, InputChans, sizeof(cmsToneCurve*));
    if (GammaTables == NULL) return NULL;

    if (ReadPositionTable(self, io, InputChans, BaseOffset, GammaTables, ReadMPECurve)) {

        mpe = cmsStageAllocToneCurves(self ->ContextID, InputChans, GammaTables);
    }
    else {
        mpe = NULL;
    }

    for (i=0; i < InputChans; i++) {
        if (GammaTables[i]) cmsFreeToneCurve(GammaTables[i]);
    }

    _cmsFree(self ->ContextID, GammaTables);
    *nItems = (mpe != NULL) ? 1 : 0;
    return mpe;

    cmsUNUSED_PARAMETER(SizeOfTag);
}


// Write a single segmented curve. NO CHECK IS PERFORMED ON VALIDITY
static
cmsBool WriteSegmentedCurve(cmsIOHANDLER* io, cmsToneCurve* g)
{
    cmsUInt32Number i, j;
    cmsCurveSegment* Segments = g ->Segments;
    cmsUInt32Number nSegments = g ->nSegments;

    if (!_cmsWriteUInt32Number(io, cmsSigSegmentedCurve)) goto Error;    
    if (!_cmsWriteUInt32Number(io, 0)) goto Error;  
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) nSegments)) goto Error;    
    if (!_cmsWriteUInt16Number(io, 0)) goto Error;  

    // Write the break-points
    for (i=0; i < nSegments - 1; i++) {
        if (!_cmsWriteFloat32Number(io, Segments[i].x1)) goto Error;
    }

    // Write the segments
    for (i=0; i < g ->nSegments; i++) {

        cmsCurveSegment* ActualSeg = Segments + i;

        if (ActualSeg -> Type == 0) {

            // This is a sampled curve
            if (!_cmsWriteUInt32Number(io, (cmsUInt32Number) cmsSigSampledCurveSeg)) goto Error;
            if (!_cmsWriteUInt32Number(io, 0)) goto Error;
            if (!_cmsWriteUInt32Number(io, ActualSeg -> nGridPoints)) goto Error;

            for (j=0; j < g ->Segments[i].nGridPoints; j++) {
                if (!_cmsWriteFloat32Number(io, ActualSeg -> SampledPoints[j])) goto Error;
            }

        }
        else {
            int Type;
            cmsUInt32Number ParamsByType[] = { 4, 5, 5 };

            // This is a formula-based
            if (!_cmsWriteUInt32Number(io, (cmsUInt32Number) cmsSigFormulaCurveSeg)) goto Error;
            if (!_cmsWriteUInt32Number(io, 0)) goto Error;

            // We only allow 1, 2 and 3 as types
            Type = ActualSeg ->Type - 6;
            if (Type > 2 || Type < 0) goto Error;

            if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) Type)) goto Error;
            if (!_cmsWriteUInt16Number(io, 0)) goto Error;

            for (j=0; j < ParamsByType[Type]; j++) {
                if (!_cmsWriteFloat32Number(io, (cmsFloat32Number) ActualSeg ->Params[j])) goto Error;
            }
        }         

        // It seems there is no need to align. Code is here, and for safety commented out
        // if (!_cmsWriteAlignment(io)) goto Error;
    }

    return TRUE;

Error:
    return FALSE;
}


static
cmsBool WriteMPECurve(struct _cms_typehandler_struct* self, 
                      cmsIOHANDLER* io,
                      void* Cargo,
                      cmsUInt32Number n, 
                      cmsUInt32Number SizeOfTag)
{
    _cmsStageToneCurvesData* Curves  = (_cmsStageToneCurvesData*) Cargo;

    return WriteSegmentedCurve(io, Curves ->TheCurves[n]);

    cmsUNUSED_PARAMETER(SizeOfTag);
    cmsUNUSED_PARAMETER(self);
}

// Write a curve, checking first for validity
static
cmsBool  Type_MPEcurve_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{   
    cmsUInt32Number BaseOffset;
    cmsStage* mpe = (cmsStage*) Ptr;
    _cmsStageToneCurvesData* Curves = (_cmsStageToneCurvesData*) mpe ->Data;

    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    // Write the header. Since those are curves, input and output channels are same
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->InputChannels)) return FALSE;
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->InputChannels)) return FALSE;
    
    if (!WritePositionTable(self, io, 0, 
                                mpe ->InputChannels, BaseOffset, Curves, WriteMPECurve)) return FALSE;


    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
}



// The matrix is organized as an array of PxQ+Q elements, where P is the number of input channels to the
// matrix, and Q is the number of output channels. The matrix elements are each float32Numbers. The array
// is organized as follows:
// array = [e11, e12, �, e1P, e21, e22, �, e2P, �, eQ1, eQ2, �, eQP, e1, e2, �, eQ]

static
void *Type_MPEmatrix_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsStage* mpe;
    cmsUInt16Number   InputChans, OutputChans;
    cmsUInt32Number   nElems, i;  
    cmsFloat64Number* Matrix;
    cmsFloat64Number* Offsets;

    if (!_cmsReadUInt16Number(io, &InputChans)) return NULL;
    if (!_cmsReadUInt16Number(io, &OutputChans)) return NULL;

    
    nElems = InputChans * OutputChans;

    // Input and output chans may be ANY (up to 0xffff)
    Matrix = (cmsFloat64Number*) _cmsCalloc(self ->ContextID, nElems, sizeof(cmsFloat64Number));
    if (Matrix == NULL) return NULL;

    Offsets = (cmsFloat64Number*) _cmsCalloc(self ->ContextID, OutputChans, sizeof(cmsFloat64Number));
    if (Offsets == NULL) {
    
        _cmsFree(self ->ContextID, Matrix);
        return NULL;
    }

    for (i=0; i < nElems; i++) {

        cmsFloat32Number v;

        if (!_cmsReadFloat32Number(io, &v)) return NULL;        
        Matrix[i] = v;
    }


    for (i=0; i < OutputChans; i++) {

        cmsFloat32Number v;

        if (!_cmsReadFloat32Number(io, &v)) return NULL;        
        Offsets[i] = v;
    }


    mpe = cmsStageAllocMatrix(self ->ContextID, OutputChans, InputChans, Matrix, Offsets);
    _cmsFree(self ->ContextID, Matrix);
    _cmsFree(self ->ContextID, Offsets);

    *nItems = 1;

    return mpe;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

static
cmsBool  Type_MPEmatrix_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsUInt32Number i, nElems;
    cmsStage* mpe = (cmsStage*) Ptr;
    _cmsStageMatrixData* Matrix = (_cmsStageMatrixData*) mpe ->Data;

    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->InputChannels)) return FALSE;    
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->OutputChannels)) return FALSE;

    nElems = mpe ->InputChannels * mpe ->OutputChannels;

    for (i=0; i < nElems; i++) {
        if (!_cmsWriteFloat32Number(io, (cmsFloat32Number) Matrix->Double[i])) return FALSE;        
    }


    for (i=0; i < mpe ->OutputChannels; i++) {

        if (Matrix ->Offset == NULL) {

               if (!_cmsWriteFloat32Number(io, 0)) return FALSE;        
        }
        else {
               if (!_cmsWriteFloat32Number(io, (cmsFloat32Number) Matrix->Offset[i])) return FALSE;        
        }
    }

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}



static
void *Type_MPEclut_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{
    cmsStage* mpe = NULL;
    cmsUInt16Number InputChans, OutputChans;
    cmsUInt8Number Dimensions8[16];
    cmsUInt32Number i, nMaxGrids, GridPoints[MAX_INPUT_DIMENSIONS];
    _cmsStageCLutData* clut;
    
    if (!_cmsReadUInt16Number(io, &InputChans)) return NULL;
    if (!_cmsReadUInt16Number(io, &OutputChans)) return NULL;
    
    if (io ->Read(io, Dimensions8, sizeof(cmsUInt8Number), 16) != 16)
        goto Error;
    
    // Copy MAX_INPUT_DIMENSIONS at most. Expand to cmsUInt32Number
    nMaxGrids = InputChans > MAX_INPUT_DIMENSIONS ? MAX_INPUT_DIMENSIONS : InputChans;
    for (i=0; i < nMaxGrids; i++) GridPoints[i] = (cmsUInt32Number) Dimensions8[i];

    // Allocate the true CLUT
    mpe = cmsStageAllocCLutFloatGranular(self ->ContextID, GridPoints, InputChans, OutputChans, NULL);
    if (mpe == NULL) goto Error;

    // Read the data
    clut = (_cmsStageCLutData*) mpe ->Data;
    for (i=0; i < clut ->nEntries; i++) {
        
        if (!_cmsReadFloat32Number(io, &clut ->Tab.TFloat[i])) goto Error;
    }

    *nItems = 1;    
    return mpe;

Error:
    *nItems = 0;
    if (mpe != NULL) cmsStageFree(mpe);
    return NULL;

    cmsUNUSED_PARAMETER(SizeOfTag);
}

// Write a CLUT in floating point
static
cmsBool  Type_MPEclut_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{   
    cmsUInt8Number Dimensions8[16];
    cmsUInt32Number i;
    cmsStage* mpe = (cmsStage*) Ptr;
    _cmsStageCLutData* clut = (_cmsStageCLutData*) mpe ->Data;

    // Check for maximum number of channels
    if (mpe -> InputChannels > 15) return FALSE;
    
    // Only floats are supported in MPE
    if (clut ->HasFloatValues == FALSE) return FALSE;

    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->InputChannels)) return FALSE;
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) mpe ->OutputChannels)) return FALSE;

    memset(Dimensions8, 0, sizeof(Dimensions8));

    for (i=0; i < mpe ->InputChannels; i++) 
        Dimensions8[i] = (cmsUInt8Number) clut ->Params ->nSamples[i];

    if (!io ->Write(io, 16, Dimensions8)) return FALSE;

    for (i=0; i < clut ->nEntries; i++) {
        
        if (!_cmsWriteFloat32Number(io, clut ->Tab.TFloat[i])) return FALSE;
    }

    return TRUE;

    cmsUNUSED_PARAMETER(nItems);
    cmsUNUSED_PARAMETER(self);
}



// This is the list of built-in MPE types
static _cmsTagTypeLinkedList SupportedMPEtypes[] = {

{{ (cmsTagTypeSignature) cmsSigBAcsElemType, NULL, NULL, NULL, NULL }, &SupportedMPEtypes[1] },   // Ignore those elements for now
{{ (cmsTagTypeSignature) cmsSigEAcsElemType, NULL, NULL, NULL, NULL }, &SupportedMPEtypes[2] },   // (That's what the spec says)

{TYPE_MPE_HANDLER((cmsTagTypeSignature) cmsSigCurveSetElemType,     MPEcurve),      &SupportedMPEtypes[3] },
{TYPE_MPE_HANDLER((cmsTagTypeSignature) cmsSigMatrixElemType,       MPEmatrix),     &SupportedMPEtypes[4] },
{TYPE_MPE_HANDLER((cmsTagTypeSignature) cmsSigCLutElemType,         MPEclut),        NULL },
};

#define DEFAULT_MPE_TYPE_COUNT  (sizeof(SupportedMPEtypes) / sizeof(_cmsTagTypeLinkedList))

static
cmsBool ReadMPEElem(struct _cms_typehandler_struct* self, 
                    cmsIOHANDLER* io,
                    void* Cargo,
                    cmsUInt32Number n, 
                    cmsUInt32Number SizeOfTag)
{
    cmsStageSignature ElementSig;
    cmsTagTypeHandler* TypeHandler;
    cmsStage *mpe = NULL;
    cmsUInt32Number nItems;
    cmsPipeline *NewLUT = (cmsPipeline *) Cargo;

    // Take signature and channels for each element.
    if (!_cmsReadUInt32Number(io, (cmsUInt32Number*) &ElementSig)) return FALSE;

    // The reserved placeholder
    if (!_cmsReadUInt32Number(io, NULL)) return FALSE;

    // Read diverse MPE types
    TypeHandler = GetHandler((cmsTagTypeSignature) ElementSig, SupportedMPEtypes);
    if (TypeHandler == NULL)  {

        char String[5];

        _cmsTagSignature2String(String, (cmsTagSignature) ElementSig);

        // An unknown element was found. 
        cmsSignalError(self ->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unknown MPE type '%s' found.", String);
        return FALSE;
    }

    // If no read method, just ignore the element (valid for cmsSigBAcsElemType and cmsSigEAcsElemType)
    // Read the MPE. No size is given
    if (TypeHandler ->ReadPtr != NULL) {

        // This is a real element which should be read and processed
        mpe = (cmsStage*) TypeHandler ->ReadPtr(self, io, &nItems, SizeOfTag);
        if (mpe == NULL) return FALSE;

        // All seems ok, insert element
        cmsPipelineInsertStage(NewLUT, cmsAT_END, mpe);
    }

    return TRUE;

    cmsUNUSED_PARAMETER(SizeOfTag);
    cmsUNUSED_PARAMETER(n);
}


// This is the main dispatcher for MPE
static
void *Type_MPE_Read(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, cmsUInt32Number* nItems, cmsUInt32Number SizeOfTag)
{   
    cmsUInt16Number InputChans, OutputChans;
    cmsUInt32Number ElementCount;
    cmsPipeline *NewLUT = NULL;
    cmsUInt32Number BaseOffset;

    // Get actual position as a basis for element offsets
    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    // Read channels and element count
    if (!_cmsReadUInt16Number(io, &InputChans)) return NULL;
    if (!_cmsReadUInt16Number(io, &OutputChans)) return NULL; 
    
    // Allocates an empty LUT
    NewLUT = cmsPipelineAlloc(self ->ContextID, InputChans, OutputChans);
    if (NewLUT == NULL) return NULL;

    if (!_cmsReadUInt32Number(io, &ElementCount)) return NULL;

    if (!ReadPositionTable(self, io, ElementCount, BaseOffset, NewLUT, ReadMPEElem)) {
        if (NewLUT != NULL) cmsPipelineFree(NewLUT);
        *nItems = 0;
        return NULL;
    }

    // Success
    *nItems = 1;
    return NewLUT;

    cmsUNUSED_PARAMETER(SizeOfTag);
}



// This one is a liitle bit more complex, so we don't use position tables this time.
static
cmsBool Type_MPE_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{   
    cmsUInt32Number i, BaseOffset, DirectoryPos, CurrentPos;
    int inputChan, outputChan;
    cmsUInt32Number ElemCount;
    cmsUInt32Number *ElementOffsets = NULL, *ElementSizes = NULL, Before;
    cmsStageSignature ElementSig;
    cmsPipeline* Lut = (cmsPipeline*) Ptr;
    cmsStage* Elem = Lut ->Elements;
    cmsTagTypeHandler* TypeHandler;

    BaseOffset = io ->Tell(io) - sizeof(_cmsTagBase);

    inputChan  = cmsPipelineInputChannels(Lut);
    outputChan = cmsPipelineOutputChannels(Lut);
    ElemCount  = cmsPipelineStageCount(Lut);

    ElementOffsets = (cmsUInt32Number *) _cmsCalloc(self ->ContextID, ElemCount, sizeof(cmsUInt32Number *));
    if (ElementOffsets == NULL) goto Error;

    ElementSizes = (cmsUInt32Number *) _cmsCalloc(self ->ContextID, ElemCount, sizeof(cmsUInt32Number *));
    if (ElementSizes == NULL) goto Error;

    // Write the head
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) inputChan)) goto Error;
    if (!_cmsWriteUInt16Number(io, (cmsUInt16Number) outputChan)) goto Error;
    if (!_cmsWriteUInt32Number(io, (cmsUInt16Number) ElemCount)) goto Error;

    DirectoryPos = io ->Tell(io);

    // Write a fake directory to be filled latter on
    for (i=0; i < ElemCount; i++) {
        if (!_cmsWriteUInt32Number(io, 0)) goto Error;  // Offset 
        if (!_cmsWriteUInt32Number(io, 0)) goto Error;  // size
    }

    // Write each single tag. Keep track of the size as well.
    for (i=0; i < ElemCount; i++) {

        ElementOffsets[i] = io ->Tell(io) - BaseOffset;

        ElementSig = Elem ->Type;

        TypeHandler = GetHandler((cmsTagTypeSignature) ElementSig, SupportedMPEtypes);
        if (TypeHandler == NULL)  {

                char String[5];

                _cmsTagSignature2String(String, (cmsTagSignature) ElementSig);

                 // An unknow element was found. 
                 cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Found unknown MPE type '%s'", String);
                 goto Error;
        }

        if (!_cmsWriteUInt32Number(io, ElementSig)) goto Error;  
        if (!_cmsWriteUInt32Number(io, 0)) goto Error;  
        Before = io ->Tell(io);
        if (!TypeHandler ->WritePtr(self, io, Elem, 1)) goto Error;
        if (!_cmsWriteAlignment(io)) goto Error;

        ElementSizes[i] = io ->Tell(io) - Before;

        Elem = Elem ->Next;
    }

    // Write the directory
    CurrentPos = io ->Tell(io);

    if (!io ->Seek(io, DirectoryPos)) goto Error;

    for (i=0; i < ElemCount; i++) {
        if (!_cmsWriteUInt32Number(io, ElementOffsets[i])) goto Error;  
        if (!_cmsWriteUInt32Number(io, ElementSizes[i])) goto Error;  
    }

    if (!io ->Seek(io, CurrentPos)) goto Error;

    if (ElementOffsets != NULL) _cmsFree(self ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(self ->ContextID, ElementSizes);
    return TRUE;

Error:
    if (ElementOffsets != NULL) _cmsFree(self ->ContextID, ElementOffsets);
    if (ElementSizes != NULL) _cmsFree(self ->ContextID, ElementSizes);
    return FALSE;

    cmsUNUSED_PARAMETER(nItems);
}


static
void* Type_MPE_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    return (void*) cmsPipelineDup((cmsPipeline*) Ptr);

    cmsUNUSED_PARAMETER(n);
    cmsUNUSED_PARAMETER(self);
}

static
void Type_MPE_Free(struct _cms_typehandler_struct* self, void *Ptr)
{    
    cmsPipelineFree((cmsPipeline*) Ptr);
    return;

    cmsUNUSED_PARAMETER(self);
}


// ********************************************************************************
// Type cmsSigVcgtType
// ********************************************************************************


#define cmsVideoCardGammaTableType    0
#define cmsVideoCardGammaFormulaType  1

// Used internally
typedef struct {
    double Gamma;
    double Min;
    double Max;
} _cmsVCGTGAMMA;


static
void *Type_vcgt_Read(struct _cms_typehandler_struct* self, 
                     cmsIOHANDLER* io, 
                     cmsUInt32Number* nItems, 
                     cmsUInt32Number SizeOfTag)
{
    cmsUInt32Number TagType, n, i;
    cmsToneCurve** Curves;

    *nItems = 0;

    // Read tag type
    if (!_cmsReadUInt32Number(io, &TagType)) return NULL;
 
    // Allocate space for the array
    Curves = ( cmsToneCurve**) _cmsCalloc(self ->ContextID, 3, sizeof(cmsToneCurve*));
    if (Curves == NULL) return NULL;

    // There are two possible flavors
    switch (TagType) {

    // Gamma is stored as a table
    case cmsVideoCardGammaTableType: 
    {
       cmsUInt16Number nChannels, nElems, nBytes;

       // Check channel count, which should be 3 (we don't support monochrome this time)
       if (!_cmsReadUInt16Number(io, &nChannels)) goto Error;

       if (nChannels != 3) {
           cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unsupported number of channels for VCGT '%d'", nChannels);
           goto Error;                               
       }

       // Get Table element count and bytes per element
       if (!_cmsReadUInt16Number(io, &nElems)) goto Error;
       if (!_cmsReadUInt16Number(io, &nBytes)) goto Error;
       
       // Adobe's quirk fixup. Fixing broken profiles...
       if (nElems == 256 && nBytes == 1 && SizeOfTag == 1576)
           nBytes = 2;


       // Populate tone curves
       for (n=0; n < 3; n++) {

           Curves[n] = cmsBuildTabulatedToneCurve16(self ->ContextID, nElems, NULL);
           if (Curves[n] == NULL) goto Error;

           // On depending on byte depth
           switch (nBytes) {

           // One byte, 0..255
           case 1:
               for (i=0; i < nElems; i++) {

                   cmsUInt8Number v;

                      if (!_cmsReadUInt8Number(io, &v)) goto Error;
                      Curves[n] ->Table16[i] = FROM_8_TO_16(v);
               }
               break;

           // One word 0..65535
           case 2:
              if (!_cmsReadUInt16Array(io, nElems, Curves[n]->Table16)) goto Error;
              break;

          // Unsupported
           default:
              cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unsupported bit depth for VCGT '%d'", nBytes * 8);
              goto Error;       
           }
       } // For all 3 channels
    }
    break;

   // In this case, gamma is stored as a formula
   case cmsVideoCardGammaFormulaType: 
   {
       _cmsVCGTGAMMA Colorant[3];
       
        // Populate tone curves
       for (n=0; n < 3; n++) {

           double Params[10];

           if (!_cmsRead15Fixed16Number(io, &Colorant[n].Gamma)) goto Error;
           if (!_cmsRead15Fixed16Number(io, &Colorant[n].Min)) goto Error;
           if (!_cmsRead15Fixed16Number(io, &Colorant[n].Max)) goto Error;
           
            // Parametric curve type 5 is:
            // Y = (aX + b)^Gamma + e | X >= d
            // Y = cX + f             | X < d

            // vcgt formula is:
            // Y = (Max � Min) * (X ^ Gamma) + Min
            
            // So, the translation is
            // a = (Max � Min) ^ ( 1 / Gamma) 
            // e = Min
            // b=c=d=f=0

           Params[0] = Colorant[n].Gamma;
           Params[1] = pow((Colorant[n].Max - Colorant[n].Min), (1.0 / Colorant[n].Gamma));
           Params[2] = 0;
           Params[3] = 0;
           Params[4] = 0;
           Params[5] = Colorant[n].Min;
           Params[6] = 0;
    
           Curves[n] = cmsBuildParametricToneCurve(self ->ContextID, 5, Params);
           if (Curves[n] == NULL) goto Error;
       }
   }
   break;

   // Unsupported
   default: 
      cmsSignalError(self->ContextID, cmsERROR_UNKNOWN_EXTENSION, "Unsupported tag type for VCGT '%d'", TagType);
      goto Error;
   }

   *nItems = 1;
   return (void*) Curves;

// Regret,  free all resources
Error:

    cmsFreeToneCurveTriple(Curves);
    _cmsFree(self ->ContextID, Curves);
    return NULL;

     cmsUNUSED_PARAMETER(SizeOfTag);
}


// We don't support all flavors, only 16bits tables and formula
static
cmsBool Type_vcgt_Write(struct _cms_typehandler_struct* self, cmsIOHANDLER* io, void* Ptr, cmsUInt32Number nItems)
{
    cmsToneCurve** Curves =  (cmsToneCurve**) Ptr;
    cmsUInt32Number i, j;

    if (cmsGetToneCurveParametricType(Curves[0]) == 5 &&
        cmsGetToneCurveParametricType(Curves[1]) == 5 &&
        cmsGetToneCurveParametricType(Curves[2]) == 5) {

            if (!_cmsWriteUInt32Number(io, cmsVideoCardGammaFormulaType)) return FALSE;  

            // Save parameters
            for (i=0; i < 3; i++) {

                _cmsVCGTGAMMA v;

                v.Gamma = Curves[i] ->Segments[0].Params[0];
                v.Min   = Curves[i] ->Segments[0].Params[5];
                v.Max   = pow(Curves[i] ->Segments[0].Params[1], v.Gamma) + v.Min;

                if (!_cmsWrite15Fixed16Number(io, v.Gamma)) return FALSE;
                if (!_cmsWrite15Fixed16Number(io, v.Min)) return FALSE;
                if (!_cmsWrite15Fixed16Number(io, v.Max)) return FALSE;
            }
    }

    else {

        // Always store as a table of 256 words
        if (!_cmsWriteUInt32Number(io, cmsVideoCardGammaTableType)) return FALSE;    
        if (!_cmsWriteUInt16Number(io, 3)) return FALSE;    
        if (!_cmsWriteUInt16Number(io, 256)) return FALSE;    
        if (!_cmsWriteUInt16Number(io, 2)) return FALSE;    

        for (i=0; i < 3; i++) {
            for (j=0; j < 256; j++) {

                cmsFloat32Number v = cmsEvalToneCurveFloat(Curves[i], (cmsFloat32Number) (j / 255.0));
                cmsUInt16Number  n = _cmsQuickSaturateWord(v * 65535.0);

                if (!_cmsWriteUInt16Number(io, n)) return FALSE;    
            }
        }
    }

    return TRUE;

    cmsUNUSED_PARAMETER(self);
    cmsUNUSED_PARAMETER(nItems);
}

static
void* Type_vcgt_Dup(struct _cms_typehandler_struct* self, const void *Ptr, cmsUInt32Number n)
{
    cmsToneCurve** OldCurves =  (cmsToneCurve**) Ptr;
    cmsToneCurve** NewCurves;

    NewCurves = ( cmsToneCurve**) _cmsCalloc(self ->ContextID, 3, sizeof(cmsToneCurve*));
    if (NewCurves == NULL) return NULL;

    NewCurves[0] = cmsDupToneCurve(OldCurves[0]);
    NewCurves[1] = cmsDupToneCurve(OldCurves[1]);
    NewCurves[2] = cmsDupToneCurve(OldCurves[2]);

    return (void*) NewCurves;

    cmsUNUSED_PARAMETER(n);
}


static
void Type_vcgt_Free(struct _cms_typehandler_struct* self, void* Ptr)
{
    cmsFreeToneCurveTriple((cmsToneCurve**) Ptr);
    _cmsFree(self ->ContextID, Ptr);
}

// ********************************************************************************
// Type support main routines
// ********************************************************************************


// This is the list of built-in types
static _cmsTagTypeLinkedList SupportedTagTypes[] = {

{TYPE_HANDLER(cmsSigChromaticityType,          Chromaticity),        &SupportedTagTypes[1] },
{TYPE_HANDLER(cmsSigColorantOrderType,         ColorantOrderType),   &SupportedTagTypes[2] },
{TYPE_HANDLER(cmsSigS15Fixed16ArrayType,       S15Fixed16),          &SupportedTagTypes[3] },
{TYPE_HANDLER(cmsSigU16Fixed16ArrayType,       U16Fixed16),          &SupportedTagTypes[4] },
{TYPE_HANDLER(cmsSigTextType,                  Text),                &SupportedTagTypes[5] },
{TYPE_HANDLER(cmsSigTextDescriptionType,       Text_Description),    &SupportedTagTypes[6] },
{TYPE_HANDLER(cmsSigCurveType,                 Curve),               &SupportedTagTypes[7] },
{TYPE_HANDLER(cmsSigParametricCurveType,       ParametricCurve),     &SupportedTagTypes[8] },
{TYPE_HANDLER(cmsSigDateTimeType,              DateTime),            &SupportedTagTypes[9] },
{TYPE_HANDLER(cmsSigLut8Type,                  LUT8),                &SupportedTagTypes[10] },
{TYPE_HANDLER(cmsSigLut16Type,                 LUT16),               &SupportedTagTypes[11] },
{TYPE_HANDLER(cmsSigColorantTableType,         ColorantTable),       &SupportedTagTypes[12] },
{TYPE_HANDLER(cmsSigNamedColor2Type,           NamedColor),          &SupportedTagTypes[13] },
{TYPE_HANDLER(cmsSigMultiLocalizedUnicodeType, MLU),                 &SupportedTagTypes[14] },
{TYPE_HANDLER(cmsSigProfileSequenceDescType,   ProfileSequenceDesc), &SupportedTagTypes[15] },
{TYPE_HANDLER(cmsSigSignatureType,             Signature),           &SupportedTagTypes[16] },
{TYPE_HANDLER(cmsSigMeasurementType,           Measurement),         &SupportedTagTypes[17] },
{TYPE_HANDLER(cmsSigDataType,                  Data),                &SupportedTagTypes[18] },
{TYPE_HANDLER(cmsSigLutAtoBType,               LUTA2B),              &SupportedTagTypes[19] },
{TYPE_HANDLER(cmsSigLutBtoAType,               LUTB2A),              &SupportedTagTypes[20] },
{TYPE_HANDLER(cmsSigUcrBgType,                 UcrBg),               &SupportedTagTypes[21] },
{TYPE_HANDLER(cmsSigCrdInfoType,               CrdInfo),             &SupportedTagTypes[22] },
{TYPE_HANDLER(cmsSigMultiProcessElementType,   MPE),                 &SupportedTagTypes[23] },
{TYPE_HANDLER(cmsSigScreeningType,             Screening),           &SupportedTagTypes[24] },
{TYPE_HANDLER(cmsSigViewingConditionsType,     ViewingConditions),   &SupportedTagTypes[25] },
{TYPE_HANDLER(cmsSigXYZType,                   XYZ),                 &SupportedTagTypes[26] },
{TYPE_HANDLER(cmsCorbisBrokenXYZtype,          XYZ),                 &SupportedTagTypes[27] },
{TYPE_HANDLER(cmsMonacoBrokenCurveType,        Curve),               &SupportedTagTypes[28] },
{TYPE_HANDLER(cmsSigProfileSequenceIdType,     ProfileSequenceId),   &SupportedTagTypes[29] },
{TYPE_HANDLER(cmsSigVcgtType,                  vcgt),                NULL }
};

#define DEFAULT_TAG_TYPE_COUNT  (sizeof(SupportedTagTypes) / sizeof(_cmsTagTypeLinkedList))
                                
// Both kind of plug-ins share same structure
cmsBool  _cmsRegisterTagTypePlugin(cmsPluginBase* Data)
{
    return RegisterTypesPlugin(Data, SupportedTagTypes, DEFAULT_TAG_TYPE_COUNT);
}

cmsBool  _cmsRegisterMultiProcessElementPlugin(cmsPluginBase* Data)
{
    return RegisterTypesPlugin(Data, SupportedMPEtypes, DEFAULT_MPE_TYPE_COUNT);
}


// Wrapper for tag types
cmsTagTypeHandler* _cmsGetTagTypeHandler(cmsTagTypeSignature sig)
{
    return GetHandler(sig, SupportedTagTypes);
}
    
// ********************************************************************************
// Tag support main routines
// ********************************************************************************

typedef struct _cmsTagLinkedList_st {

            cmsTagSignature Signature;
            cmsTagDescriptor Descriptor;
            struct _cmsTagLinkedList_st* Next;

} _cmsTagLinkedList;

// This is the list of built-in tags
static _cmsTagLinkedList SupportedTags[] = {

    { cmsSigAToB0Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutAtoBType, cmsSigLut8Type}, DecideLUTtypeA2B}, &SupportedTags[1]},   
    { cmsSigAToB1Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutAtoBType, cmsSigLut8Type}, DecideLUTtypeA2B}, &SupportedTags[2]},
    { cmsSigAToB2Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutAtoBType, cmsSigLut8Type}, DecideLUTtypeA2B}, &SupportedTags[3]},
    { cmsSigBToA0Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutBtoAType, cmsSigLut8Type}, DecideLUTtypeB2A}, &SupportedTags[4]},
    { cmsSigBToA1Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutBtoAType, cmsSigLut8Type}, DecideLUTtypeB2A}, &SupportedTags[5]},
    { cmsSigBToA2Tag,               { 1, 3,  { cmsSigLut16Type,  cmsSigLutBtoAType, cmsSigLut8Type}, DecideLUTtypeB2A}, &SupportedTags[6]},
     
    // Allow corbis  and its broken XYZ type
    { cmsSigRedColorantTag,         { 1, 2, { cmsSigXYZType, cmsCorbisBrokenXYZtype }, DecideXYZtype}, &SupportedTags[7]},
    { cmsSigGreenColorantTag,       { 1, 2, { cmsSigXYZType, cmsCorbisBrokenXYZtype }, DecideXYZtype}, &SupportedTags[8]},
    { cmsSigBlueColorantTag,        { 1, 2, { cmsSigXYZType, cmsCorbisBrokenXYZtype }, DecideXYZtype}, &SupportedTags[9]},
                                   
    { cmsSigRedTRCTag,              { 1, 3, { cmsSigCurveType, cmsSigParametricCurveType, cmsMonacoBrokenCurveType }, DecideCurveType}, &SupportedTags[10]},
    { cmsSigGreenTRCTag,            { 1, 3, { cmsSigCurveType, cmsSigParametricCurveType, cmsMonacoBrokenCurveType }, DecideCurveType}, &SupportedTags[11]},
    { cmsSigBlueTRCTag,             { 1, 3, { cmsSigCurveType, cmsSigParametricCurveType, cmsMonacoBrokenCurveType }, DecideCurveType}, &SupportedTags[12]}, 

    { cmsSigCalibrationDateTimeTag, { 1, 1, { cmsSigDateTimeType }, NULL}, &SupportedTags[13]},
    { cmsSigCharTargetTag,          { 1, 1, { cmsSigTextType },     NULL}, &SupportedTags[14]},

    { cmsSigChromaticAdaptationTag, { 9, 1, { cmsSigS15Fixed16ArrayType }, NULL}, &SupportedTags[15]},
    { cmsSigChromaticityTag,        { 1, 1, { cmsSigChromaticityType    }, NULL}, &SupportedTags[16]},
    { cmsSigColorantOrderTag,       { 1, 1, { cmsSigColorantOrderType   }, NULL}, &SupportedTags[17]},
    { cmsSigColorantTableTag,       { 1, 1, { cmsSigColorantTableType   }, NULL}, &SupportedTags[18]},
    { cmsSigColorantTableOutTag,    { 1, 1, { cmsSigColorantTableType   }, NULL}, &SupportedTags[19]},

    { cmsSigCopyrightTag,           { 1, 3, { cmsSigTextType,  cmsSigMultiLocalizedUnicodeType, cmsSigTextDescriptionType}, DecideTextType}, &SupportedTags[20]},
    { cmsSigDateTimeTag,            { 1, 1, { cmsSigDateTimeType }, NULL}, &SupportedTags[21]},

    { cmsSigDeviceMfgDescTag,       { 1, 3, { cmsSigTextDescriptionType, cmsSigMultiLocalizedUnicodeType, cmsSigTextType}, DecideTextDescType}, &SupportedTags[22]},
    { cmsSigDeviceModelDescTag,     { 1, 3, { cmsSigTextDescriptionType, cmsSigMultiLocalizedUnicodeType, cmsSigTextType}, DecideTextDescType}, &SupportedTags[23]},

    { cmsSigGamutTag,               { 1, 3, { cmsSigLut16Type, cmsSigLutBtoAType, cmsSigLut8Type }, DecideLUTtypeB2A}, &SupportedTags[24]},

    { cmsSigGrayTRCTag,             { 1, 2, { cmsSigCurveType, cmsSigParametricCurveType }, DecideCurveType}, &SupportedTags[25]},
    { cmsSigLuminanceTag,           { 1, 1, { cmsSigXYZType }, NULL}, &SupportedTags[26]},

    { cmsSigMediaBlackPointTag,     { 1, 2, { cmsSigXYZType, cmsCorbisBrokenXYZtype }, NULL}, &SupportedTags[27]},
    { cmsSigMediaWhitePointTag,     { 1, 2, { cmsSigXYZType, cmsCorbisBrokenXYZtype }, NULL}, &SupportedTags[28]},

    { cmsSigNamedColor2Tag,         { 1, 1, { cmsSigNamedColor2Type }, NULL}, &SupportedTags[29]},

    { cmsSigPreview0Tag,            { 1, 3,  { cmsSigLut16Type, cmsSigLutBtoAType, cmsSigLut8Type }, DecideLUTtypeB2A}, &SupportedTags[30]},
    { cmsSigPreview1Tag,            { 1, 3,  { cmsSigLut16Type, cmsSigLutBtoAType, cmsSigLut8Type }, DecideLUTtypeB2A}, &SupportedTags[31]},
    { cmsSigPreview2Tag,            { 1, 3,  { cmsSigLut16Type, cmsSigLutBtoAType, cmsSigLut8Type }, DecideLUTtypeB2A}, &SupportedTags[32]},

    { cmsSigProfileDescriptionTag,  { 1, 3, { cmsSigTextDescriptionType, cmsSigMultiLocalizedUnicodeType, cmsSigTextType}, DecideTextDescType}, &SupportedTags[33]},
    { cmsSigProfileSequenceDescTag, { 1, 1, { cmsSigProfileSequenceDescType }, NULL}, &SupportedTags[34]},
    { cmsSigTechnologyTag,          { 1, 1, { cmsSigSignatureType }, NULL},  &SupportedTags[35]},

    { cmsSigColorimetricIntentImageStateTag,   { 1, 1, { cmsSigSignatureType }, NULL}, &SupportedTags[36]},
    { cmsSigPerceptualRenderingIntentGamutTag, { 1, 1, { cmsSigSignatureType }, NULL}, &SupportedTags[37]},
    { cmsSigSaturationRenderingIntentGamutTag, { 1, 1, { cmsSigSignatureType }, NULL}, &SupportedTags[38]},

    { cmsSigMeasurementTag,         { 1, 1, { cmsSigMeasurementType }, NULL}, &SupportedTags[39]},

    { cmsSigPs2CRD0Tag,             { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[40]},          
    { cmsSigPs2CRD1Tag,             { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[41]},                     
    { cmsSigPs2CRD2Tag,             { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[42]},                    
    { cmsSigPs2CRD3Tag,             { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[43]},                    
    { cmsSigPs2CSATag,              { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[44]},                    
    { cmsSigPs2RenderingIntentTag,  { 1, 1, { cmsSigDataType }, NULL}, &SupportedTags[45]},     

    { cmsSigViewingCondDescTag,     { 1, 3, { cmsSigTextDescriptionType, cmsSigMultiLocalizedUnicodeType, cmsSigTextType}, DecideTextDescType}, &SupportedTags[46]},

    { cmsSigUcrBgTag,               { 1, 1, { cmsSigUcrBgType}, NULL},    &SupportedTags[47]}, 
    { cmsSigCrdInfoTag,             { 1, 1, { cmsSigCrdInfoType}, NULL},  &SupportedTags[48]}, 

    { cmsSigDToB0Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[49]}, 
    { cmsSigDToB1Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[50]}, 
    { cmsSigDToB2Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[51]}, 
    { cmsSigDToB3Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[52]}, 
    { cmsSigBToD0Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[53]}, 
    { cmsSigBToD1Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[54]}, 
    { cmsSigBToD2Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[55]}, 
    { cmsSigBToD3Tag,               { 1, 1, { cmsSigMultiProcessElementType}, NULL}, &SupportedTags[56]}, 
        
    { cmsSigScreeningDescTag,       { 1, 1, { cmsSigTextDescriptionType },    NULL}, &SupportedTags[57]}, 
    { cmsSigViewingConditionsTag,   { 1, 1, { cmsSigViewingConditionsType },  NULL}, &SupportedTags[58]}, 

    { cmsSigScreeningTag,           { 1, 1, { cmsSigScreeningType},          NULL }, &SupportedTags[59]},  
    { cmsSigVcgtTag,                { 1, 1, { cmsSigVcgtType},               NULL }, &SupportedTags[60]}, 
    { cmsSigProfileSequenceIdTag,   { 1, 1, { cmsSigProfileSequenceIdType},  NULL},  NULL}  

};

/*
    Not supported                 Why
    =======================       =========================================                
    cmsSigOutputResponseTag   ==> WARNING, POSSIBLE PATENT ON THIS SUBJECT!                
    cmsSigNamedColorTag       ==> Deprecated                                 
    cmsSigDataTag             ==> Ancient, unused             
    cmsSigDeviceSettingsTag   ==> Deprecated, useless     
*/               

#define DEFAULT_TAG_COUNT  (sizeof(SupportedTags) / sizeof(_cmsTagLinkedList))

cmsBool  _cmsRegisterTagPlugin(cmsPluginBase* Data)
{
    cmsPluginTag* Plugin = (cmsPluginTag*) Data;
    _cmsTagLinkedList *pt, *Anterior;


    if (Data == NULL) {
    
        SupportedTags[DEFAULT_TAG_COUNT-1].Next = NULL;
        return TRUE;
    }

    pt = Anterior = SupportedTags; 
    while (pt != NULL) {

        if (Plugin->Signature == pt -> Signature) {
            pt ->Descriptor = Plugin ->Descriptor;  // Replace old behaviour
            return TRUE;
        }   

        Anterior = pt;          
        pt = pt ->Next;
    }

    pt = (_cmsTagLinkedList*) _cmsPluginMalloc(sizeof(_cmsTagLinkedList));
    if (pt == NULL) return FALSE;

    pt ->Signature  = Plugin ->Signature;
    pt ->Descriptor = Plugin ->Descriptor;  
    pt ->Next       = NULL;

    if (Anterior != NULL) Anterior -> Next = pt;

    return TRUE;
}

// Return a descriptor for a given tag or NULL
cmsTagDescriptor* _cmsGetTagDescriptor(cmsTagSignature sig)
{
    _cmsTagLinkedList* pt;

    for (pt = SupportedTags; 
            pt != NULL;
            pt = pt ->Next) {

                if (sig == pt -> Signature) return &pt ->Descriptor;
    }

    return NULL;
}

