#ifndef _INC_SDKDDKVER
#define _INC_SDKDDKVER

/* Version constants */
#define _WIN32_WINNT_NT4                    0x0400
#define _WIN32_WINNT_WIN2K                  0x0500
#define _WIN32_WINNT_WINXP                  0x0501
#define _WIN32_WINNT_WS03                   0x0502
#define _WIN32_WINNT_LONGHORN               0x0600
#define _WIN32_IE_IE20                      0x0200
#define _WIN32_IE_IE30                      0x0300
#define _WIN32_IE_IE302                     0x0302
#define _WIN32_IE_IE40                      0x0400
#define _WIN32_IE_IE401                     0x0401
#define _WIN32_IE_IE50                      0x0500
#define _WIN32_IE_IE501                     0x0501
#define _WIN32_IE_IE55                      0x0550
#define _WIN32_IE_IE60                      0x0600
#define _WIN32_IE_IE60SP1                   0x0601
#define _WIN32_IE_IE60SP2                   0x0603
#define _WIN32_IE_IE70                      0x0700
#define _WIN32_IE_NT4                       _WIN32_IE_IE20
#define _WIN32_IE_NT4SP1                    _WIN32_IE_IE20
#define _WIN32_IE_NT4SP2                    _WIN32_IE_IE20
#define _WIN32_IE_NT4SP3                    _WIN32_IE_IE302
#define _WIN32_IE_NT4SP4                    _WIN32_IE_IE401
#define _WIN32_IE_NT4SP5                    _WIN32_IE_IE401
#define _WIN32_IE_NT4SP6                    _WIN32_IE_IE50
#define _WIN32_IE_WIN98                     _WIN32_IE_IE401
#define _WIN32_IE_WIN98SE                   _WIN32_IE_IE50
#define _WIN32_IE_WINME                     _WIN32_IE_IE55
#define _WIN32_IE_WIN2K                     _WIN32_IE_IE501
#define _WIN32_IE_WIN2KSP1                  _WIN32_IE_IE501
#define _WIN32_IE_WIN2KSP2                  _WIN32_IE_IE501
#define _WIN32_IE_WIN2KSP3                  _WIN32_IE_IE501
#define _WIN32_IE_WIN2KSP4                  _WIN32_IE_IE501
#define _WIN32_IE_XP                        _WIN32_IE_IE60
#define _WIN32_IE_XPSP1                     _WIN32_IE_IE60SP1
#define _WIN32_IE_XPSP2                     _WIN32_IE_IE60SP2
#define _WIN32_IE_WS03                      0x0602
#define _WIN32_IE_WS03SP1                   _WIN32_IE_IE60SP2
#define _WIN32_IE_LONGHORN                  _WIN32_IE_IE70
#define NTDDI_WIN2K                         0x05000000
#define NTDDI_WIN2KSP1                      0x05000100
#define NTDDI_WIN2KSP2                      0x05000200
#define NTDDI_WIN2KSP3                      0x05000300
#define NTDDI_WIN2KSP4                      0x05000400
#define NTDDI_WINXP                         0x05010000
#define NTDDI_WINXPSP1                      0x05010100
#define NTDDI_WINXPSP2                      0x05010200
#define NTDDI_WS03                          0x05020000
#define NTDDI_WS03SP1                       0x05020100
#define NTDDI_LONGHORN                      0x06000000

/* Macros to get the NTDDI for a given WIN32 */
#define NTDDI_VERSION_FROM_WIN32_WINNT2(ver)    ver##0000
#define NTDDI_VERSION_FROM_WIN32_WINNT(ver)     NTDDI_VERSION_FROM_WIN32_WINNT2(ver)

/* Select Default WIN32_WINNT Value */
#if !defined(_WIN32_WINNT) && !defined(_CHICAGO_)
#define  _WIN32_WINNT   0x0600
#endif

/* Choose NTDDI Version */
#ifndef NTDDI_VERSION
#ifdef _WIN32_WINNT
#define NTDDI_VERSION   NTDDI_VERSION_FROM_WIN32_WINNT(_WIN32_WINNT)
#else
#define NTDDI_VERSION   0x06000000
#endif
#endif

/* Choose WINVER Value */
#ifndef WINVER
#ifdef _WIN32_WINNT
#define WINVER          _WIN32_WINNT
#else
#define WINVER          0x0600
#endif
#endif

/* Choose IE Version */
#ifndef _WIN32_IE
#ifdef _WIN32_WINNT
#if (_WIN32_WINNT <= _WIN32_WINNT_NT4)
#define _WIN32_IE       _WIN32_IE_IE50
#elif (_WIN32_WINNT <= _WIN32_WINNT_WIN2K)
#define _WIN32_IE       _WIN32_IE_IE501
#elif (_WIN32_WINNT <= _WIN32_WINNT_WINXP)
#define _WIN32_IE       _WIN32_IE_IE60
#elif (_WIN32_WINNT <= _WIN32_WINNT_WS03)
#define _WIN32_IE       0x0602
#else
#define _WIN32_IE       0x0700
#endif
#else
#define _WIN32_IE       0x0700
#endif
#endif

#endif
