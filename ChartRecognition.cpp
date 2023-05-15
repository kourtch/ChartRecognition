#include <iostream>
using namespace std;
#include <map>
#include <vector>
#include <set>
#include <numeric>
#include <algorithm>

#include <windows.h>
#include <wingdi.h>
//#include <gdiplusheaders.h>

void errhandler(const char * message, HWND hwnd)
{
    cout << message << endl;
    abort();
}

// https://learn.microsoft.com/en-us/windows/win32/gdi/storing-an-image
PBITMAPINFO CreateBitmapInfoStruct(HWND hwnd, HBITMAP hBmp)
{
    BITMAP bmp;
    PBITMAPINFO pbmi;
    WORD    cClrBits;

    // Retrieve the bitmap color format, width, and height.  
    if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp))
        errhandler("GetObject", hwnd);

    // Convert the color format to a count of bits.  
    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
    if (cClrBits == 1)
        cClrBits = 1;
    else if (cClrBits <= 4)
        cClrBits = 4;
    else if (cClrBits <= 8)
        cClrBits = 8;
    else if (cClrBits <= 16)
        cClrBits = 16;
    else if (cClrBits <= 24)
        cClrBits = 24;
    else cClrBits = 32;

    // Allocate memory for the BITMAPINFO structure. (This structure  
    // contains a BITMAPINFOHEADER structure and an array of RGBQUAD  
    // data structures.)  

    if (cClrBits < 24)
        pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
            sizeof(BITMAPINFOHEADER) +
            sizeof(RGBQUAD) * (1 << cClrBits));

    // There is no RGBQUAD array for these formats: 24-bit-per-pixel or 32-bit-per-pixel 

    else
        pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
            sizeof(BITMAPINFOHEADER));

    // Initialize the fields in the BITMAPINFO structure.  

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = bmp.bmWidth;
    pbmi->bmiHeader.biHeight = bmp.bmHeight;
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
    if (cClrBits < 24)
        pbmi->bmiHeader.biClrUsed = (1 << cClrBits);

    // If the bitmap is not compressed, set the BI_RGB flag.  
    pbmi->bmiHeader.biCompression = BI_RGB;

    // Compute the number of bytes in the array of color  
    // indices and store the result in biSizeImage.  
    // The width must be DWORD aligned unless the bitmap is RLE 
    // compressed. 
    pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits + 31) & ~31) / 8
        * pbmi->bmiHeader.biHeight;
    // Set biClrImportant to 0, indicating that all of the  
    // device colors are important.  
    pbmi->bmiHeader.biClrImportant = 0;
    return pbmi;
}

// https://learn.microsoft.com/en-us/windows/win32/gdi/storing-an-image
void CreateBMPFile(HWND hwnd, LPTSTR pszFile, PBITMAPINFO pbi,
    HBITMAP hBMP, HDC hDC)
{
    HANDLE hf;                 // file handle  
    BITMAPFILEHEADER hdr;       // bitmap file-header  
    PBITMAPINFOHEADER pbih;     // bitmap info-header  
    LPBYTE lpBits;              // memory pointer  
    DWORD dwTotal;              // total count of bytes  
    DWORD cb;                   // incremental count of bytes  
    BYTE* hp;                   // byte pointer  
    DWORD dwTmp;

    pbih = (PBITMAPINFOHEADER)pbi;
    lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

    if (!lpBits)
        errhandler("GlobalAlloc", hwnd);

    // Retrieve the color table (RGBQUAD array) and the bits  
    // (array of palette indices) from the DIB.  
    if (!GetDIBits(hDC, hBMP, 0, (WORD)pbih->biHeight, lpBits, pbi,
        DIB_RGB_COLORS))
    {
        errhandler("GetDIBits", hwnd);
    }

    // Create the .BMP file.  
    hf = CreateFile(pszFile,
        GENERIC_READ | GENERIC_WRITE,
        (DWORD)0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        (HANDLE)NULL);
    if (hf == INVALID_HANDLE_VALUE)
        errhandler("CreateFile", hwnd);
    hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"  
    // Compute the size of the entire file.  
    hdr.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) +
        pbih->biSize + pbih->biClrUsed
        * sizeof(RGBQUAD) + pbih->biSizeImage);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;

    // Compute the offset to the array of color indices.  
    hdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) +
        pbih->biSize + pbih->biClrUsed
        * sizeof(RGBQUAD);

    // Copy the BITMAPFILEHEADER into the .BMP file.  
    if (!WriteFile(hf, (LPVOID)&hdr, sizeof(BITMAPFILEHEADER),
        (LPDWORD)&dwTmp, NULL))
    {
        errhandler("WriteFile", hwnd);
    }

    // Copy the BITMAPINFOHEADER and RGBQUAD array into the file.  
    if (!WriteFile(hf, (LPVOID)pbih, sizeof(BITMAPINFOHEADER)
        + pbih->biClrUsed * sizeof(RGBQUAD),
        (LPDWORD)&dwTmp, (NULL)))
        errhandler("WriteFile", hwnd);

    // Copy the array of color indices into the .BMP file.  
    dwTotal = cb = pbih->biSizeImage;
    hp = lpBits;
    if (!WriteFile(hf, (LPSTR)hp, (int)cb, (LPDWORD)&dwTmp, NULL))
        errhandler("WriteFile", hwnd);

    // Close the .BMP file.  
    if (!CloseHandle(hf))
        errhandler("CloseHandle", hwnd);

    // Free memory.  
    GlobalFree((HGLOBAL)lpBits);
}

bool findColumn(HDC dc, BITMAP bitmap, COLORREF color, set<pair<int, int>>& output)
{
    set<pair<int, int>> activePoints;
    // 
    auto findSeed = [&]()
    {
        for (int i = 0; i < bitmap.bmWidth; ++i)
            for (int j = 0; j < bitmap.bmHeight; ++j)
            {
                COLORREF c = GetPixel(dc, i, j);
                if (c == color)
                {
                    activePoints.insert(make_pair(i, j));
                    return true;
                }
            }
        return false;
    };

    findSeed();
    
    // flood fill
    set<pair<int, int>> visitedPoints;
    int count = 0;
    while (activePoints.size() > 0)
    {
        set<pair<int, int>> nextPoints;
        for (auto p : activePoints)
        {
            visitedPoints.insert(p);
            SetPixel(dc, p.first, p.second, RGB(255, 0, 0));
            
            auto validIdx = [](int idx, int max_idx)
            {
                return idx >= 0 && idx < max_idx;
            };
            
            for (int di : {-1, 0, 1})
            {
                int i = p.first + di;
                if (i < 0 || i >= bitmap.bmWidth)
                    continue;
                for (int dj : { -1, 0, 1 })
                {
                    if (di == 0 && dj == 0)
                        continue;
                    int j = p.second + dj;
                    if (j < 0 || j >= bitmap.bmHeight)
                        continue;
                    pair<int, int> p1 = make_pair(i, j);
                    if (visitedPoints.find(p1) == visitedPoints.end() && activePoints.find(p1) == activePoints.end())
                    {
                        COLORREF c = GetPixel(dc, i, j);
                        if (c == color)
                        {
                            nextPoints.insert(p1);
                        }
                    }
                }
            }
        }
        activePoints = nextPoints;

        count++;
    }
    cout << "count  " << count << endl;
    cout << "size " << output.size() << endl;
    output = visitedPoints;
    return output.size() > 0;
}


int main(int argc, char* argv[]) 
{
    HBITMAP hBitmap = 0;
    BITMAP Bitmap;
    hBitmap = (HBITMAP)LoadImage(0, TEXT("Chart.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

    BITMAP bitmap;
    GetObject(hBitmap, sizeof(bitmap), (LPVOID)&bitmap);

    cout << bitmap.bmType << endl;
    cout << bitmap.bmWidth << endl;
    cout << bitmap.bmHeight << endl;
    cout << bitmap.bmWidthBytes << endl;
    cout << bitmap.bmPlanes << endl;
    cout << bitmap.bmBitsPixel << endl;

    // create a hdc and select the source bitmap names 'hBitmap'
    HDC dc = CreateCompatibleDC(NULL);
    SelectObject(dc, hBitmap);

    cout << endl;

    COLORREF c = GetPixel(dc, 0, 0);

    map<int,int> stats;

    for (int i = 0; i < bitmap.bmWidth; ++i)
        for (int j = 0; j < bitmap.bmHeight; ++j)
        {
            COLORREF c = GetPixel(dc, i, j);
            if (stats.find(c) == stats.end())
            {
                stats[c] = 1;
            }
            else
            {
                stats[c]++;
            }
    }

    cout << stats.size() << endl;

    vector<int> colors;
    vector<int> counts;
    for (auto it : stats)
    {
        cout << it.first << "\t" << it.second << std::endl;
        colors.push_back(it.first);
        counts.push_back(it.second);
    }

    cout << endl;

    vector<int> indices(stats.size());
    iota(begin(indices), end(indices), static_cast<size_t>(0));
    sort(indices.begin(), indices.end(), [&](const int idx1, const int idx2) { return counts[idx1] > counts[idx2]; });

    cout << indices[0] << "\t" << colors[indices[0]] << endl;
    cout << indices[1] << "\t" << colors[indices[1]] << endl;
    cout << indices[2] << "\t" << colors[indices[2]] << endl;

    cout << endl;

    struct THE_COLUMN
    {
        int min_i;
        int min_j;
        int max_i;
        int max_j;
    };
    
    vector<THE_COLUMN> the_columns;
    set<pair<int, int>> output;
    while(findColumn(dc, bitmap, colors[indices[1]], output))
    {
        if (output.size() == 0)
            break;
        THE_COLUMN the_column;
        the_column.min_i = min_element(output.begin(), output.end(), 
            [&](const auto p1, const auto p2) { return p1.first < p2.first; })->first;
        the_column.max_i = max_element(output.begin(), output.end(),
            [&](const auto p1, const auto p2) { return p1.first < p2.first; })->first;
        the_column.min_j = min_element(output.begin(), output.end(),
            [&](const auto p1, const auto p2) { return p1.second < p2.second; })->second;
        the_column.max_j = max_element(output.begin(), output.end(),
            [&](const auto p1, const auto p2) { return p1.second < p2.second; })->second;
        the_columns.push_back(the_column);

        cout << "(" << the_column.min_i << "," << the_column.max_i << ")"
             << "(" << the_column.min_j << "," << the_column.max_j << ")" << endl;
    }
    cout << endl;
    cout << "No Of Columns " << the_columns.size() << endl;

    cout << endl;

    sort(the_columns.begin(), the_columns.end(), 
        [&](auto c1, const auto c2) { return c1.min_i  < c2.min_i;});

    FILE* f = fopen("tmp.csv","wt");
    for (auto c : the_columns)
    {
        cout << "(" << c.min_i << "," << c.max_i << ")"
            << "(" << c.min_j << "," << c.max_j << ")" << endl;
        fprintf(f, "%d\n", c.max_j - c.min_j);
    }
    fclose(f);


    PBITMAPINFO pbi = CreateBitmapInfoStruct(0, hBitmap);
    CreateBMPFile(0, LPTSTR(L"tmp.bmp"), pbi, hBitmap, dc);
}
