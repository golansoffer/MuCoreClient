// GlobalBitmap.h: interface for the CGlobalBitmap class.
//
//////////////////////////////////////////////////////////////////////
#pragma once

#pragma warning(disable : 4786)

#include <map>
#include <deque>
#include <string>
#include <setjmp.h>
#include "./Time/Timer.h"

#define MAX_BITMAP_FILE_NAME 256

#pragma pack(push, 1)
typedef struct
{
    GLuint	BitmapIndex;
    wchar_t FileName[MAX_BITMAP_FILE_NAME];
    float	Width;
    float	Height;
    char	Components;
    GLuint	TextureNumber;
    BYTE	Ref;
    bool    IsSkin;
    bool    IsHair;
    BYTE* Buffer;

private:
    friend class CBitmapCache;
    DWORD	dwCallCount;
} BITMAP_t;
#pragma pack(pop)

// This cache uses direct array indexing for O(1) bitmap lookup
// - Single flat array for maximum performance
// - Direct indexing with no categorization
// - Preallocated to handle all possible bitmap indices
class CBitmapCache
{
    enum
    {
        CACHE_SIZE = 100000  // Large enough to handle all possible bitmap indices
    };

    BITMAP_t** m_ppBitmapCache;  // Single flat array for O(1) access
    BITMAP_t* m_pNullBitmap;
    CTimer2 m_ManageTimer;

public:
    CBitmapCache();
    ~CBitmapCache();

    bool Create();
    void Release();

    void Add(GLuint uiBitmapIndex, BITMAP_t* pBitmap);
    void Remove(GLuint uiBitmapIndex);
    void RemoveAll();

    size_t GetCacheSize();
    void Update();
    bool Find(GLuint uiBitmapIndex, BITMAP_t** ppBitmap);
};

class CGlobalBitmap
{
    enum
    {
        MAX_WIDTH = 1024,
        MAX_HEIGHT = 1024,
    };

    typedef std::map<GLuint, BITMAP_t*, std::less<GLuint> >	type_bitmap_map;
    typedef std::list<GLuint> type_index_list;

    type_bitmap_map	m_mapBitmap;
    type_index_list m_listNonamedIndex;

    GLuint	m_uiAlternate, m_uiTextureIndexStream;
    DWORD	m_dwUsedTextureMemory;

    CBitmapCache	m_BitmapCache;
#ifdef DEBUG_BITMAP_CACHE
    CTimer2				m_DebugOutputTimer;
#endif // DEBUG_BITMAP_CACHE

    void Init();

public:
    CGlobalBitmap();
    virtual ~CGlobalBitmap();

    GLuint LoadImage(const std::wstring& filename, GLuint uiFilter = GL_NEAREST, GLuint uiWrapMode = GL_CLAMP_TO_EDGE);
    bool LoadImage(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter = GL_NEAREST, GLuint uiWrapMode = GL_CLAMP_TO_EDGE);
    void UnloadImage(GLuint uiBitmapIndex, bool bForce = false);
    void UnloadAllImages();

    BITMAP_t* GetTexture(GLuint uiBitmapIndex);
    BITMAP_t* FindTexture(GLuint uiBitmapIndex);
    BITMAP_t* FindTexture(const std::wstring& filename);
    BITMAP_t* FindTextureByName(const std::wstring& name);

    DWORD GetUsedTextureMemory() const;
    size_t GetNumberOfTexture() const;

    bool Convert_Format(const std::wstring& filename);

    void Manage();

    inline BITMAP_t& operator [] (GLuint uiBitmapIndex) { return *GetTexture(uiBitmapIndex); }

protected:
    GLuint GenerateTextureIndex();
    GLuint FindAvailableTextureIndex(GLuint uiSeed);

    bool OpenJpegTurbo(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter = GL_NEAREST, GLuint uiWrapMode = GL_CLAMP_TO_EDGE);
    bool OpenTga(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter = GL_NEAREST, GLuint uiWrapMode = GL_CLAMP_TO_EDGE);
    void SplitFileName(IN const std::wstring& filepath, OUT std::wstring& filename, bool bIncludeExt);
    void SplitExt(IN const std::wstring& filepath, OUT std::wstring& ext, bool bIncludeDot);
    void ExchangeExt(IN const std::wstring& in_filepath, IN const std::wstring& ext, OUT std::wstring& out_filepath);

    bool Save_Image(const std::wstring& src, const std::wstring& dest, int cDumpHeader);
};

extern CGlobalBitmap Bitmaps;
