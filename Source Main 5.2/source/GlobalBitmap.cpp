// GlobalBitmap.cpp: implementation of the CGlobalBitmap class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "turbojpeg.h"
#include "GlobalBitmap.h"
#include "./Utilities/Log/ErrorReport.h"


CBitmapCache::CBitmapCache()
{
    m_ppBitmapCache = nullptr;
    m_pNullBitmap = nullptr;
}
CBitmapCache::~CBitmapCache() { Release(); }

bool CBitmapCache::Create()
{
    Release();

    m_ppBitmapCache = new BITMAP_t*[CACHE_SIZE];
    memset(m_ppBitmapCache, 0, CACHE_SIZE * sizeof(BITMAP_t*));

    m_pNullBitmap = new BITMAP_t;
    memset(m_pNullBitmap, 0, sizeof(BITMAP_t));

    m_ManageTimer.SetTimer(1500);

    return true;
}
void CBitmapCache::Release()
{
    SAFE_DELETE(m_pNullBitmap);
    SAFE_DELETE_ARRAY(m_ppBitmapCache);
}

void CBitmapCache::Add(GLuint uiBitmapIndex, BITMAP_t* pBitmap)
{
    if (uiBitmapIndex < CACHE_SIZE)
    {
        m_ppBitmapCache[uiBitmapIndex] = pBitmap ? pBitmap : m_pNullBitmap;
    }
}
void CBitmapCache::Remove(GLuint uiBitmapIndex)
{
    if (uiBitmapIndex < CACHE_SIZE)
    {
        m_ppBitmapCache[uiBitmapIndex] = nullptr;
    }
}
void CBitmapCache::RemoveAll()
{
    if (m_ppBitmapCache)
    {
        memset(m_ppBitmapCache, 0, CACHE_SIZE * sizeof(BITMAP_t*));
    }
}

size_t CBitmapCache::GetCacheSize()
{
    size_t total = 0;
    if (m_ppBitmapCache)
    {
        for (size_t i = 0; i < CACHE_SIZE; i++)
        {
            if (m_ppBitmapCache[i] && m_ppBitmapCache[i] != m_pNullBitmap)
            {
                total++;
            }
        }
    }
    return total;
}

void CBitmapCache::Update()
{
    m_ManageTimer.UpdateTime();
    if (m_ManageTimer.IsTime())
    {
        for (size_t i = 0; i < CACHE_SIZE; i++)
        {
            if (m_ppBitmapCache[i] && m_ppBitmapCache[i] != m_pNullBitmap)
            {
                if (m_ppBitmapCache[i]->dwCallCount > 0)
                {
                    m_ppBitmapCache[i]->dwCallCount = 0;
                }
                else
                {
                    m_ppBitmapCache[i] = nullptr;
                }
            }
        }
    }
}

bool CBitmapCache::Find(GLuint uiBitmapIndex, BITMAP_t** ppBitmap)
{
    if (uiBitmapIndex < CACHE_SIZE && m_ppBitmapCache[uiBitmapIndex])
    {
        if (m_ppBitmapCache[uiBitmapIndex] == m_pNullBitmap)
        {
            *ppBitmap = nullptr;
        }
        else
        {
            *ppBitmap = m_ppBitmapCache[uiBitmapIndex];
            (*ppBitmap)->dwCallCount++;
        }
        return true;
    }
    return false;
}

CGlobalBitmap::CGlobalBitmap()
{
    Init();
    m_BitmapCache.Create();

#ifdef DEBUG_BITMAP_CACHE
    m_DebugOutputTimer.SetTimer(5000);
#endif // DEBUG_BITMAP_CACHE
}
CGlobalBitmap::~CGlobalBitmap()
{
    UnloadAllImages();
}
void CGlobalBitmap::Init()
{
    m_uiAlternate = 0;
    m_uiTextureIndexStream = BITMAP_NONAMED_TEXTURES_BEGIN;
    m_dwUsedTextureMemory = 0;
}

GLuint CGlobalBitmap::LoadImage(const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode)
{
    BITMAP_t* pBitmap = FindTexture(filename);
    if (pBitmap)
    {
        if (pBitmap->Ref > 0)
        {
            if (0 == _wcsicmp(pBitmap->FileName, filename.c_str()))
            {
                pBitmap->Ref++;

                return pBitmap->BitmapIndex;
            }
        }
    }
    else
    {
        GLuint uiNewTextureIndex = GenerateTextureIndex();
        if (true == LoadImage(uiNewTextureIndex, filename, uiFilter, uiWrapMode))
        {
            m_listNonamedIndex.push_back(uiNewTextureIndex);

            return uiNewTextureIndex;
        }
    }
    return BITMAP_UNKNOWN;
}
bool CGlobalBitmap::LoadImage(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode)
{
    unsigned int UICLAMP = GL_CLAMP_TO_EDGE;
    unsigned int UIREPEAT = GL_REPEAT;

    if (uiWrapMode != UICLAMP && uiWrapMode != UIREPEAT)
    {
#ifdef _DEBUG
        static unsigned int	uiCnt2 = 0;
        int			iBuff;	iBuff = 0;

        wchar_t		szDebugOutput[256];

        iBuff = iBuff + swprintf(iBuff + szDebugOutput, L"%d. Call No CLAMP & No REPEAT. \n", uiCnt2++);
        OutputDebugString(szDebugOutput);
#endif
    }

    auto mi = m_mapBitmap.find(uiBitmapIndex);
    if (mi != m_mapBitmap.end())
    {
        BITMAP_t* pBitmap = (*mi).second;
        if (pBitmap->Ref > 0)
        {
            if (0 == _wcsicmp(pBitmap->FileName, filename.c_str()))
            {
                pBitmap->Ref++;
                return true;
            }
            else
            {
                g_ErrorReport.Write(L"File not found %s (%d)->%s\r\n", pBitmap->FileName, uiBitmapIndex, filename.c_str());
                UnloadImage(uiBitmapIndex, true);
            }
        }
    }

    std::wstring ext;
    SplitExt(filename, ext, false);

    if (0 == _wcsicmp(ext.c_str(), L"jpg"))
        return OpenJpegTurbo(uiBitmapIndex, filename, uiFilter, uiWrapMode);
    else if (0 == _wcsicmp(ext.c_str(), L"tga"))
        return OpenTga(uiBitmapIndex, filename, uiFilter, uiWrapMode);

    return false;
}
void CGlobalBitmap::UnloadImage(GLuint uiBitmapIndex, bool bForce)
{
    auto mi = m_mapBitmap.find(uiBitmapIndex);
    if (mi != m_mapBitmap.end())
    {
        BITMAP_t* pBitmap = (*mi).second;

        if (--pBitmap->Ref == 0 || bForce)
        {
            glDeleteTextures(1, &(pBitmap->TextureNumber));

            m_dwUsedTextureMemory -= (DWORD)(pBitmap->Width * pBitmap->Height * pBitmap->Components);

            delete[] pBitmap->Buffer;
            delete pBitmap;
            m_mapBitmap.erase(mi);

            if (uiBitmapIndex >= BITMAP_NONAMED_TEXTURES_BEGIN && uiBitmapIndex <= BITMAP_NONAMED_TEXTURES_END)
            {
                m_listNonamedIndex.remove(uiBitmapIndex);
            }
            m_BitmapCache.Remove(uiBitmapIndex);
        }
    }
}
void CGlobalBitmap::UnloadAllImages()
{
#ifdef _DEBUG
    if (!m_mapBitmap.empty())
        g_ErrorReport.Write(L"Unload Images\r\n");
#endif // _DEBUG

    auto mi = m_mapBitmap.begin();
    for (; mi != m_mapBitmap.end(); mi++)
    {
        BITMAP_t* pBitmap = (*mi).second;

#ifdef _DEBUG
        if (pBitmap->Ref > 1)
        {
            g_ErrorReport.Write(L"Bitmap %s(RefCount= %d)\r\n", pBitmap->FileName, pBitmap->Ref);
        }
#endif // _DEBUG
        delete[] pBitmap->Buffer;
        delete pBitmap;
    }

    m_mapBitmap.clear();
    m_listNonamedIndex.clear();
    m_BitmapCache.RemoveAll();

    Init();
}

// Gets a texture bitmap by its index
// First checks the bitmap cache for fast lookup
// If not in cache, looks up in main bitmap map and adds to cache
// If texture not found, returns an error bitmap
// Returns: BITMAP_t pointer to the texture or error bitmap
BITMAP_t* CGlobalBitmap::GetTexture(GLuint uiBitmapIndex)
{
    BITMAP_t* pBitmap = NULL;
    if (false == m_BitmapCache.Find(uiBitmapIndex, &pBitmap))
    {
        auto mi = m_mapBitmap.find(uiBitmapIndex);
        if (mi != m_mapBitmap.end())
            pBitmap = (*mi).second;
        m_BitmapCache.Add(uiBitmapIndex, pBitmap);
    }
    if (NULL == pBitmap)
    {
        static BITMAP_t s_Error;
        memset(&s_Error, 0, sizeof(BITMAP_t));
        wcscpy(s_Error.FileName, L"CGlobalBitmap::GetTexture Error!!!");
        pBitmap = &s_Error;
    }
    return pBitmap;
}
BITMAP_t* CGlobalBitmap::FindTexture(GLuint uiBitmapIndex)
{
    BITMAP_t* pBitmap = NULL;
    if (false == m_BitmapCache.Find(uiBitmapIndex, &pBitmap))
    {
        auto mi = m_mapBitmap.find(uiBitmapIndex);
        if (mi != m_mapBitmap.end())
            pBitmap = (*mi).second;
        if (pBitmap != NULL)
            m_BitmapCache.Add(uiBitmapIndex, pBitmap);
    }
    return pBitmap;
}

BITMAP_t* CGlobalBitmap::FindTexture(const std::wstring& filename)
{
    auto mi = m_mapBitmap.begin();
    for (; mi != m_mapBitmap.end(); mi++)
    {
        BITMAP_t* pBitmap = (*mi).second;
        if (0 == wcsicmp(filename.c_str(), pBitmap->FileName))
            return pBitmap;
    }
    return NULL;
}

BITMAP_t* CGlobalBitmap::FindTextureByName(const std::wstring& name)
{
    auto mi = m_mapBitmap.begin();
    for (; mi != m_mapBitmap.end(); mi++)
    {
        BITMAP_t* pBitmap = (*mi).second;
        std::wstring texname;
        SplitFileName(pBitmap->FileName, texname, true);
        if (0 == wcsicmp(texname.c_str(), name.c_str()))
            return pBitmap;
    }
    return NULL;
}

DWORD CGlobalBitmap::GetUsedTextureMemory() const
{
    return m_dwUsedTextureMemory;
}
size_t CGlobalBitmap::GetNumberOfTexture() const
{
    return m_mapBitmap.size();
}

void CGlobalBitmap::Manage()
{
#ifdef DEBUG_BITMAP_CACHE
    m_DebugOutputTimer.UpdateTime();
    if (m_DebugOutputTimer.IsTime())
    {
        g_ConsoleDebug->Write(MCD_NORMAL, L"CacheSize=%d(NumberOfTexture=%d)", m_BitmapCache.GetCacheSize(), GetNumberOfTexture());
    }
#endif // DEBUG_BITMAP_CACHE
    m_BitmapCache.Update();
}

GLuint CGlobalBitmap::GenerateTextureIndex()
{
    GLuint uiAvailableTextureIndex = FindAvailableTextureIndex(m_uiTextureIndexStream);
    if (uiAvailableTextureIndex >= BITMAP_NONAMED_TEXTURES_END)
    {
        m_uiAlternate++;
        m_uiTextureIndexStream = BITMAP_NONAMED_TEXTURES_BEGIN;
        uiAvailableTextureIndex = FindAvailableTextureIndex(m_uiTextureIndexStream);
    }
    return m_uiTextureIndexStream = uiAvailableTextureIndex;
}
GLuint CGlobalBitmap::FindAvailableTextureIndex(GLuint uiSeed)
{
    if (m_uiAlternate > 0)
    {
        auto li = std::find(m_listNonamedIndex.begin(), m_listNonamedIndex.end(), uiSeed + 1);
        if (li != m_listNonamedIndex.end())
            return FindAvailableTextureIndex(uiSeed + 1);
    }
    return uiSeed + 1;
}

bool CGlobalBitmap::OpenJpegTurbo(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode)
{
    // create a reuseable buffer with the maximum size of the uncompressed image
    static unsigned char buffer[MAX_WIDTH * MAX_HEIGHT * 3];

    std::wstring filename_ozj;
    ExchangeExt(filename, L"OZJ", filename_ozj);

    auto* compressedFile = _wfopen(filename_ozj.c_str(), L"rb");
    if (compressedFile == nullptr)
    {
        return false;
    }

    fseek(compressedFile, 0, SEEK_END);
    const auto fileSize = ftell(compressedFile);
    assert(fileSize > 24);

    // Skip first 24 bytes, because these are added by the OZJ format
    fseek(compressedFile, 24, SEEK_SET);

    const auto jpegSize = fileSize - 24;
    int jpegWidth = 0, jpegHeight = 0;
    int jpegSubsamp = TJSAMP_444;
    int jpegColorspace = TJCS_RGB;

    auto tjhandle = tjInitDecompress();

    auto* jpegBuf = new unsigned char[jpegSize];
    fread(jpegBuf, 1, jpegSize, compressedFile);
    fclose(compressedFile);

    // First reading the header with the size information
    auto result = tjDecompressHeader3(tjhandle, jpegBuf, jpegSize, &jpegWidth, &jpegHeight, &jpegSubsamp, &jpegColorspace);
    if (result != 0)
    {
        auto errorstr = tjGetErrorStr();
        auto errorCode = tjGetErrorCode(compressedFile);
        assert(false);
    }
    //assert(jpegColorspace == TJCS_RGB);
    assert(jpegWidth <= MAX_WIDTH);
    assert(jpegHeight <= MAX_HEIGHT);

    // decompress into the buffer
    result = tjDecompress2(tjhandle, jpegBuf, jpegSize, buffer, jpegWidth, 0, jpegHeight, TJPF_RGB, TJFLAG_FASTDCT);
    if (result != 0)
    {
        auto errorstr = tjGetErrorStr();
        auto errorCode = tjGetErrorCode(compressedFile);
        assert(false);
    }
    delete[] jpegBuf;
    jpegBuf = nullptr;

    // now we can cleanup already
    tjDestroy(tjhandle);


    if (jpegWidth <= MAX_WIDTH && jpegHeight <= MAX_HEIGHT)
    {
        // rounds up to the next n^2 value, because that's what OpenGL supports
        int textureWidth = 0, textureHeight = 0;
        for (int i = 1; i <= MAX_WIDTH; i <<= 1)
        {
            textureWidth = i;
            if (i >= jpegWidth) break;
        }
        for (int i = 1; i <= MAX_HEIGHT; i <<= 1)
        {
            textureHeight = i;
            if (i >= jpegHeight) break;
        }

        auto* pNewBitmap = new BITMAP_t;
        memset(pNewBitmap, 0, sizeof(BITMAP_t));

        pNewBitmap->BitmapIndex = uiBitmapIndex;

        filename._Copy_s(pNewBitmap->FileName, MAX_BITMAP_FILE_NAME, MAX_BITMAP_FILE_NAME);

        pNewBitmap->Width = static_cast<float>(textureWidth);
        pNewBitmap->Height = static_cast<float>(textureHeight);
        pNewBitmap->Components = 3;
        pNewBitmap->Ref = 1;

        const auto textureBufferSize = textureWidth * textureHeight * pNewBitmap->Components;
        pNewBitmap->Buffer = new BYTE[textureBufferSize];
        m_dwUsedTextureMemory += textureBufferSize;

        int offset = 0;
        int jpeg_row_size = jpegWidth * 3;
        int texture_row_size = textureWidth * 3;
        int rows = min(jpegHeight, textureHeight);
        if (jpegWidth != textureWidth)
        {
            // we need copy it line by line
            for (int row = 0; row < rows; row++)
            {
                memcpy(&pNewBitmap->Buffer[offset], &buffer[row * jpeg_row_size], jpeg_row_size);
                offset += texture_row_size;
            }
        }
        else
        {
            // we can copy it 1:1
            memcpy(pNewBitmap->Buffer, buffer, jpegHeight * jpegWidth * 3);
        }

        m_mapBitmap.insert(type_bitmap_map::value_type(uiBitmapIndex, pNewBitmap));

        glGenTextures(1, &(pNewBitmap->TextureNumber));

        glBindTexture(GL_TEXTURE_2D, pNewBitmap->TextureNumber);

        glTexImage2D(GL_TEXTURE_2D, 0, 3, textureWidth, textureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pNewBitmap->Buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uiFilter);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uiFilter);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, uiWrapMode);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, uiWrapMode);
    }

    return true;
}

bool CGlobalBitmap::OpenTga(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode)
{
    std::wstring filename_ozt;
    ExchangeExt(filename, L"OZT", filename_ozt);

    FILE* fp = _wfopen(filename_ozt.c_str(), L"rb");
    if (fp == NULL)
    {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    int Size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    auto* PakBuffer = new unsigned char[Size];
    fread(PakBuffer, 1, Size, fp);
    fclose(fp);

    int index = 12;
    index += 4;
    short nx = *((short*)(PakBuffer + index)); index += 2;
    short ny = *((short*)(PakBuffer + index)); index += 2;
    char bit = *((char*)(PakBuffer + index)); index += 1;
    index += 1;

    if (bit != 32 || nx > MAX_WIDTH || ny > MAX_HEIGHT)
    {
        SAFE_DELETE_ARRAY(PakBuffer);
        return false;
    }

    int Width = 0, Height = 0;
    for (int i = 1; i <= MAX_WIDTH; i <<= 1)
    {
        Width = i;
        if (i >= nx) break;
    }
    for (int i = 1; i <= MAX_HEIGHT; i <<= 1)
    {
        Height = i;
        if (i >= ny) break;
    }

    auto* pNewBitmap = new BITMAP_t;
    memset(pNewBitmap, 0, sizeof(BITMAP_t));

    pNewBitmap->BitmapIndex = uiBitmapIndex;

    filename._Copy_s(pNewBitmap->FileName, MAX_BITMAP_FILE_NAME, MAX_BITMAP_FILE_NAME);

    pNewBitmap->Width = (float)Width;
    pNewBitmap->Height = (float)Height;
    pNewBitmap->Components = 4;
    pNewBitmap->Ref = 1;

    size_t BufferSize = Width * Height * pNewBitmap->Components;
    pNewBitmap->Buffer = (unsigned char*)new BYTE[BufferSize];

    m_dwUsedTextureMemory += BufferSize;

    for (int y = 0; y < ny; y++)
    {
        unsigned char* src = &PakBuffer[index];
        index += nx * 4;
        unsigned char* dst = &pNewBitmap->Buffer[(ny - 1 - y) * Width * pNewBitmap->Components];

        for (int x = 0; x < nx; x++)
        {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            src += 4;
            dst += pNewBitmap->Components;
        }
    }
    SAFE_DELETE_ARRAY(PakBuffer);

    m_mapBitmap.insert(type_bitmap_map::value_type(uiBitmapIndex, pNewBitmap));

    glGenTextures(1, &(pNewBitmap->TextureNumber));

    glBindTexture(GL_TEXTURE_2D, pNewBitmap->TextureNumber);

    glTexImage2D(GL_TEXTURE_2D, 0, 4, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pNewBitmap->Buffer);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, uiFilter);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uiFilter);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, uiWrapMode);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, uiWrapMode);

    return true;
}

void CGlobalBitmap::SplitFileName(IN const std::wstring& filepath, OUT std::wstring& filename, bool bIncludeExt)
{
    wchar_t __fname[_MAX_FNAME] = { 0, };
    wchar_t __ext[_MAX_EXT] = { 0, };
    _wsplitpath(filepath.c_str(), NULL, NULL, __fname, __ext);
    filename = __fname;
    if (bIncludeExt)
        filename += __ext;
}
void CGlobalBitmap::SplitExt(IN const std::wstring& filepath, OUT std::wstring& ext, bool bIncludeDot)
{
    wchar_t __ext[_MAX_EXT] = { 0, };
    _wsplitpath(filepath.c_str(), NULL, NULL, NULL, __ext);
    if (bIncludeDot) {
        ext = __ext;
    }
    else {
        if ((__ext[0] == '.') && __ext[1])
            ext = __ext + 1;
    }
}
void CGlobalBitmap::ExchangeExt(IN const std::wstring& in_filepath, IN const std::wstring& ext, OUT std::wstring& out_filepath)
{
    wchar_t __drive[_MAX_DRIVE] = { 0, };
    wchar_t __dir[_MAX_DIR] = { 0, };
    wchar_t __fname[_MAX_FNAME] = { 0, };
    _wsplitpath(in_filepath.c_str(), __drive, __dir, __fname, NULL);

    out_filepath = __drive;
    out_filepath += __dir;
    out_filepath += __fname;
    out_filepath += '.';
    out_filepath += ext;
}

bool CGlobalBitmap::Convert_Format(const std::wstring& filename)
{
    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    wchar_t fname[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];

    ::_wsplitpath(filename.c_str(), drive, dir, fname, ext);

    std::wstring strPath = drive; strPath += dir;
    std::wstring strName = fname;

    if (_wcsicmp(ext, L".jpg") == 0)
    {
        auto strSaveName = strPath + strName + L".OZJ";
        return Save_Image(filename, strSaveName.c_str(), 24);
    }
    else if (_wcsicmp(ext, L".tga") == 0)
    {
        auto strSaveName = strPath + strName + L".OZT";
        return Save_Image(filename, strSaveName.c_str(), 4);
    }
    else if (_wcsicmp(ext, L".bmp") == 0)
    {
        auto strSaveName = strPath + strName + L".OZB";
        return Save_Image(filename, strSaveName.c_str(), 4);
    }
    else
    {
    }

    return false;
}

bool CGlobalBitmap::Save_Image(const std::wstring& src, const std::wstring& dest, int cDumpHeader)
{
    FILE* fp = _wfopen(src.c_str(), L"rb");
    if (fp == NULL)
    {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* pTempBuf = new char[size];
    fread(pTempBuf, 1, size, fp);
    fclose(fp);

    fp = _wfopen(dest.c_str(), L"wb");
    if (fp == NULL)
        return false;

    fwrite(pTempBuf, 1, cDumpHeader, fp);
    fwrite(pTempBuf, 1, size, fp);
    fclose(fp);

    delete[] pTempBuf;

    return true;
}
