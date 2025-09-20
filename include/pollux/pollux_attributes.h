#ifndef POLLUX_ATTRIBUTES_H
#define POLLUX_ATTRIBUTES_H

#ifdef _WIN32
#  ifndef pollux_api
#    ifdef POLLUX_BUILDING
#      ifdef POLLUX_WIN_DLL
#        define pollux_api __declspec(dllexport)
#      else
#        define pollux_api
#      endif
#    else
#      ifdef POLLUX_WIN_DLL
#        define pollux_api __declspec(dllimport)
#      else
#        define pollux_api
#      endif
#    endif
#  endif
#else
#  ifndef pollux_api
#    if (defined(__GNUC__) && __GNUC__ > 4) || defined(__clang__)
#      define pollux_api __attribute__((visibility("default")))
#    else
#      define pollux_api
#    endif
#  endif
#endif

#endif // POLLUX_ATTRIBUTES_H
